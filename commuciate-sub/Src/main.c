#include "stm32f10x.h"

/* USART1 默认引脚：PA9 = TX，PA10 = RX。 */
#define LINK_USART       USART1
#define LINK_BAUD_RATE   460800u

static void usart1_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    /* 打开 GPIOA 和 USART1 的外设时钟。 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_USART1,
                           ENABLE);

    /* PA9：USART1_TX，复用推挽输出。 */
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    /* PA10：USART1_RX，浮空输入。 */
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* 460800 波特率、8 个数据位、1 个停止位、无校验。 */
    USART_StructInit(&usart);
    usart.USART_BaudRate = LINK_BAUD_RATE;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    USART_Init(LINK_USART, &usart);
    USART_Cmd(LINK_USART, ENABLE);
}

static void usart_send_byte(uint8_t data)
{
    /* TXE 为 1，表示发送数据寄存器已经空了。 */
    while (USART_GetFlagStatus(LINK_USART, USART_FLAG_TXE) == RESET)
    {
    }

    USART_SendData(LINK_USART, data);
}

static uint8_t usart_receive_byte(void)
{
    /* RXNE 为 1，表示接收数据寄存器里有新数据。 */
    while (USART_GetFlagStatus(LINK_USART, USART_FLAG_RXNE) == RESET)
    {
    }

    return (uint8_t)USART_ReceiveData(LINK_USART);
}

int main(void)
{
    uint8_t received;

    SystemCoreClockUpdate();
    usart1_init();

    while (1)
    {
        /* 最小回显测试：收到什么，就原样发回什么。 */
        received = usart_receive_byte();
        usart_send_byte(received);
    }
}
