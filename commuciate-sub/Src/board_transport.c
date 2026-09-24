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
    DMA_InitTypeDef dma;
    if (tx_active != 0u || tx_read == tx_write)
        return;
    tx_active = 1u;
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA_DeInit(DMA1_Channel4);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DR;
    dma.DMA_MemoryBaseAddr = (uint32_t)tx_queue[tx_read].bytes;
    dma.DMA_DIR = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize = tx_queue[tx_read].length;
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
    DMA_Cmd(DMA1_Channel4, ENABLE);
}

void BoardTransport_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    tx_read = tx_write = tx_active = 0u;
    rx_read = rx_write = 0u;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = 460800u;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);

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
