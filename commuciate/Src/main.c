#include "board_link.h"

/* 主机 USART1 默认引脚：PA9 为 TX、PA10 为 RX。 */
#define HOST_UART_TX_PORT GPIOA
#define HOST_UART_TX_PIN GPIO_Pin_9
#define HOST_UART_RX_PORT GPIOA
#define HOST_UART_RX_PIN GPIO_Pin_10
#define HOST_USART1_REMAP DISABLE

int main(void)
{
    BoardLink_Attitude attitude;

    SystemCoreClockUpdate();
    BoardLink_Init(HOST_UART_TX_PORT, HOST_UART_TX_PIN,
                   HOST_UART_RX_PORT, HOST_UART_RX_PIN,
                   HOST_USART1_REMAP);

    while (1)
    {
        BoardLink_Process();
        if (BoardLink_GetLatest(&attitude))
        {
            /* CRC 校验通过的最新姿态数据可在此交给主机应用逻辑。 */
            (void)attitude;
        }
    }
}
