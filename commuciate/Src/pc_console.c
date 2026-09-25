#include "pc_console.h"
#include "board_protocol.h"

#include <string.h>

#define PC_RX_RING_SIZE 64u
#define PC_COMMAND_SIZE 24u
#define PC_TX_QUEUE_COUNT 8u

typedef struct
{
    uint8_t bytes[BOARD_PROTOCOL_MAX_FRAME_SIZE];
} PcTxSlot;

static volatile uint8_t rx_ring[PC_RX_RING_SIZE];
static volatile uint8_t rx_write;
static volatile uint8_t rx_read;
static char command[PC_COMMAND_SIZE];
static uint8_t command_length;
static uint16_t pending_weight_q15;
static bool weight_pending;
static PcTxSlot tx_queue[PC_TX_QUEUE_COUNT];
static volatile uint8_t tx_read;
static volatile uint8_t tx_write;
static volatile uint8_t tx_active;

/* 主循环和 DMA 中断共同操作发送队列，需要保护队列指针。 */
static uint32_t enter_critical(void)
{
    uint32_t primask;
    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("cpsid i" ::: "memory");
    return primask;
}

static void exit_critical(uint32_t primask)
{
    __asm volatile ("msr primask, %0" : : "r" (primask) : "memory");
}

static void start_tx_dma(void)
{
    if (tx_active != 0u || tx_read == tx_write)
        return;

    tx_active = 1u;
    /* 固定参数只配置一次，每帧更新源地址和长度。 */
    DMA_Cmd(DMA1_Channel7, DISABLE);
    DMA1_Channel7->CMAR = (uint32_t)tx_queue[tx_read].bytes;
    DMA_SetCurrDataCounter(DMA1_Channel7, BOARD_PROTOCOL_MAX_FRAME_SIZE);
    DMA_ClearITPendingBit(DMA1_IT_TC7 | DMA1_IT_TE7);
    DMA_Cmd(DMA1_Channel7, ENABLE);
}

static uint32_t gpio_clock_for_port(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0u;
}

static void usart2_init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                        GPIO_TypeDef *rx_port, uint16_t rx_pin,
                        FunctionalState remap_usart2)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;
    DMA_InitTypeDef dma;
    uint32_t gpio_clocks = gpio_clock_for_port(tx_port) |
                           gpio_clock_for_port(rx_port);

    RCC_APB2PeriphClockCmd(gpio_clocks | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, remap_usart2);

    gpio.GPIO_Pin = tx_pin;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(tx_port, &gpio);
    gpio.GPIO_Pin = rx_pin;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(rx_port, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 460800u;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART2, &usart);

    /* USART2_TX 使用 DMA1 通道 7，发送 29 字节完整协议帧。 */
    DMA_DeInit(DMA1_Channel7);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)tx_queue[0].bytes;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = BOARD_PROTOCOL_MAX_FRAME_SIZE;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel7, &dma);
    DMA_ClearITPendingBit(DMA1_IT_TC7 | DMA1_IT_TE7);
    DMA_ITConfig(DMA1_Channel7, DMA_IT_TC | DMA_IT_TE, ENABLE);

    nvic.NVIC_IRQChannel = USART2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = DMA1_Channel7_IRQn;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    NVIC_Init(&nvic);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART2, ENABLE);
}

static bool parse_filter_command(const char *text, uint16_t *weight_q15)
{
    uint32_t whole = 0u;
    uint32_t fraction = 0u;
    uint32_t scale = 1u;
    uint8_t index = 2u;
    uint8_t fraction_digits = 0u;
    bool has_digit = false;

    if (text[0] != 'K' || (text[1] != ',' && text[1] != '='))
        return false;

    if (text[index] < '0' || text[index] > '1')
        return false;
    whole = (uint32_t)(text[index++] - '0');
    if (text[index] == '.')
    {
        ++index;
        while (text[index] >= '0' && text[index] <= '9')
        {
            if (fraction_digits >= 4u)
                return false;
            fraction = fraction * 10u + (uint32_t)(text[index++] - '0');
            scale *= 10u;
            ++fraction_digits;
            has_digit = true;
        }
        if (!has_digit)
            return false;
    }
    if (text[index] != '\0' || (whole == 1u && fraction != 0u))
        return false;

    *weight_q15 = (uint16_t)((whole * scale + fraction) * 32767u / scale);
    return true;
}

static void handle_command(void)
{
    uint16_t weight;
    command[command_length] = '\0';
    if (parse_filter_command(command, &weight))
    {
        pending_weight_q15 = weight;
        weight_pending = true;
    }
    command_length = 0u;
}

void PcConsole_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart2)
{
    rx_write = 0u;
    rx_read = 0u;
    command_length = 0u;
    weight_pending = false;
    tx_read = tx_write = tx_active = 0u;
    usart2_init(tx_port, tx_pin, rx_port, rx_pin, remap_usart2);
}

bool PcConsole_SendFrame(const uint8_t *frame, size_t length)
{
    uint8_t next;
    uint32_t primask;

    if (frame == 0 || length != BOARD_PROTOCOL_MAX_FRAME_SIZE)
        return false;

    primask = enter_critical();
    next = (uint8_t)((tx_write + 1u) % PC_TX_QUEUE_COUNT);
    if (next == tx_read)
    {
        exit_critical(primask);
        return false;
    }
    memcpy(tx_queue[tx_write].bytes, frame, length);
    tx_write = next;
    start_tx_dma();
    exit_critical(primask);
    return true;
}

void PcConsole_WriteLine(const char *line)
{
    if (line == 0)
        return;
    while (*line != '\0')
    {
        while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) { }
        USART_SendData(USART2, (uint8_t)*line++);
    }
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) { }
    USART_SendData(USART2, '\r');
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) { }
    USART_SendData(USART2, '\n');
}

void PcConsole_Process(void)
{
    while (rx_read != rx_write)
    {
        char byte = (char)rx_ring[rx_read];
        rx_read = (uint8_t)((rx_read + 1u) % PC_RX_RING_SIZE);
        if (byte == '\r')
            continue;
        if (byte == '\n')
        {
            handle_command();
            continue;
        }
        if (command_length < (PC_COMMAND_SIZE - 1u))
            command[command_length++] = byte;
        else
            command_length = 0u;
    }
}

bool PcConsole_TakeFilterWeight(uint16_t *gyro_weight_q15)
{
    if (gyro_weight_q15 == 0 || !weight_pending)
        return false;
    *gyro_weight_q15 = pending_weight_q15;
    weight_pending = false;
    return true;
}

void PcConsole_RxIrqHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART2);
        uint8_t next = (uint8_t)((rx_write + 1u) % PC_RX_RING_SIZE);
        if (next != rx_read)
        {
            rx_ring[rx_write] = byte;
            rx_write = next;
        }
    }
}

void PcConsole_TxDmaIrqHandler(void)
{
    /* 传输错误时丢弃当前帧，避免错误帧残留在发送队列中。 */
    if (DMA_GetITStatus(DMA1_IT_TE7) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC7 | DMA1_IT_TE7);
        DMA_Cmd(DMA1_Channel7, DISABLE);
        tx_read = (uint8_t)((tx_read + 1u) % PC_TX_QUEUE_COUNT);
        tx_active = 0u;
        start_tx_dma();
        return;
    }
    if (DMA_GetITStatus(DMA1_IT_TC7) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC7);
        DMA_Cmd(DMA1_Channel7, DISABLE);
        tx_read = (uint8_t)((tx_read + 1u) % PC_TX_QUEUE_COUNT);
        tx_active = 0u;
        start_tx_dma();
    }
}
