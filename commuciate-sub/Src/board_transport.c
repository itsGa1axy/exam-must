#include "board_transport.h"
#include "stm32f10x.h"
#include <string.h>

#define TX_QUEUE_COUNT 4u
#define TX_FRAME_CAPACITY 32u
#define RX_QUEUE_SIZE 128u

typedef struct
{
    uint8_t bytes[TX_FRAME_CAPACITY];
    uint8_t length;
} TxSlot;

static TxSlot tx_queue[TX_QUEUE_COUNT];
static volatile uint8_t tx_read;
static volatile uint8_t tx_write;
static volatile uint8_t tx_active;
static volatile uint8_t rx_queue[RX_QUEUE_SIZE];
static volatile uint8_t rx_read;
static volatile uint8_t rx_write;

static uint32_t gpio_clock_for_port(GPIO_TypeDef *port)
{
    if (port == GPIOA) return RCC_APB2Periph_GPIOA;
    if (port == GPIOB) return RCC_APB2Periph_GPIOB;
    if (port == GPIOC) return RCC_APB2Periph_GPIOC;
    if (port == GPIOD) return RCC_APB2Periph_GPIOD;
    return 0u;
}

/* GCC/ARM GCC 下保存并恢复中断屏蔽状态，避免临界区误开原本关闭的中断。 */
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

static void start_dma(void)
{
    if (tx_active != 0u || tx_read == tx_write)
        return;
    tx_active = 1u;

    /* 通道固定参数已在初始化时设置；每帧只更新发送缓冲区和字节数。 */
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA1_Channel4->CMAR = (uint32_t)tx_queue[tx_read].bytes;
    DMA_SetCurrDataCounter(DMA1_Channel4, tx_queue[tx_read].length);
    DMA_ClearITPendingBit(DMA1_IT_TC4 | DMA1_IT_TE4);
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

void BoardTransport_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                         GPIO_TypeDef *rx_port, uint16_t rx_pin,
                         FunctionalState remap_usart1)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;
    DMA_InitTypeDef dma;

    tx_read = tx_write = tx_active = 0u;
    rx_read = rx_write = 0u;
    RCC_APB2PeriphClockCmd(gpio_clock_for_port(tx_port) |
                           gpio_clock_for_port(rx_port) |
                           RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, remap_usart1);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = tx_pin;
    GPIO_Init(tx_port, &gpio);
    gpio.GPIO_Pin = rx_pin;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(rx_port, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 460800u;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);

    /* DMA 通道的外设地址、方向和数据宽度不随帧变化，只配置一次。 */
    DMA_DeInit(DMA1_Channel4);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)tx_queue[0].bytes;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = 1u;
    dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode = DMA_Mode_Normal;
    dma.DMA_Priority = DMA_Priority_High;
    dma.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel4, &dma);
    DMA_ClearITPendingBit(DMA1_IT_TC4 | DMA1_IT_TE4);
    DMA_ITConfig(DMA1_Channel4, DMA_IT_TC, ENABLE);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = DMA1_Channel4_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

int BoardTransport_Send(const uint8_t *data, size_t length)
{
    uint8_t next;
    uint32_t primask;
    if (data == 0 || length == 0u || length > TX_FRAME_CAPACITY)
        return -1;
    primask = enter_critical();
    next = (uint8_t)((tx_write + 1u) % TX_QUEUE_COUNT);
    if (next == tx_read)
    {
        exit_critical(primask);
        return -2;
    }
    memcpy(tx_queue[tx_write].bytes, data, length);
    tx_queue[tx_write].length = (uint8_t)length;
    tx_write = next;
    start_dma();
    exit_critical(primask);
    return 0;
}

int BoardTransport_ReadByte(uint8_t *byte)
{
    if (byte == 0 || rx_read == rx_write)
        return 0;
    *byte = rx_queue[rx_read];
    rx_read = (uint8_t)((rx_read + 1u) % RX_QUEUE_SIZE);
    return 1;
}

void BoardTransport_RxIrqHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
        uint8_t next = (uint8_t)((rx_write + 1u) % RX_QUEUE_SIZE);
        if (next != rx_read)
        {
            rx_queue[rx_write] = byte;
            rx_write = next;
        }
    }
}

void BoardTransport_TxDmaIrqHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC4) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TC4);
        DMA_Cmd(DMA1_Channel4, DISABLE);
        tx_read = (uint8_t)((tx_read + 1u) % TX_QUEUE_COUNT);
        tx_active = 0u;
        start_dma();
    }
    if (DMA_GetITStatus(DMA1_IT_TE4) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_TE4);
        DMA_Cmd(DMA1_Channel4, DISABLE);
        tx_active = 0u;
        start_dma();
    }
}
