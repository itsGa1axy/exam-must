#include "board_link.h"
#include "pc_console.h"

/* 主机 USART1 默认引脚：PA9 为 TX、PA10 为 RX。 */
#define HOST_UART_TX_PORT GPIOA
#define HOST_UART_TX_PIN GPIO_Pin_9
#define HOST_UART_RX_PORT GPIOA
#define HOST_UART_RX_PIN GPIO_Pin_10
#define HOST_USART1_REMAP DISABLE

/* USB-TTL 默认连接 USART2：PA2 为 TX、PA3 为 RX，串口参数 460800 8N1。 */
#define PC_UART_TX_PORT GPIOA
#define PC_UART_TX_PIN GPIO_Pin_2
#define PC_UART_RX_PORT GPIOA
#define PC_UART_RX_PIN GPIO_Pin_3
#define PC_USART2_REMAP DISABLE

int main(void)
{
    BoardLink_Attitude attitude;
    uint16_t gyro_weight_q15;

    SystemCoreClockUpdate();
    BoardLink_Init(HOST_UART_TX_PORT, HOST_UART_TX_PIN,
                   HOST_UART_RX_PORT, HOST_UART_RX_PIN,
                   HOST_USART1_REMAP);
    PcConsole_Init(PC_UART_TX_PORT, PC_UART_TX_PIN,
                   PC_UART_RX_PORT, PC_UART_RX_PIN,
                   PC_USART2_REMAP);

    while (1)
    {
        BoardLink_Process();
        PcConsole_Process();

        if (PcConsole_TakeFilterWeight(&gyro_weight_q15))
            (void)BoardLink_SendFilterWeight(gyro_weight_q15);

        /* CRC 已在板间接收模块验证，原始帧交给电脑端解析。 */
        if (BoardLink_GetLatest(&attitude))
            (void)PcConsole_SendFrame(attitude.raw_frame,
                                      sizeof(attitude.raw_frame));
    }
}
