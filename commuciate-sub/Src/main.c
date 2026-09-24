#include "stm32f10x.h"
#include "attitude.h"
#include "board_link.h"
#include "imu_calibration.h"
#include "mpu6500.h"

/* 从机板卡引脚统一在此配置；USART1 重映射时使用 PB6/PB7。 */
#define SLAVE_UART_TX_PORT       GPIOA
#define SLAVE_UART_TX_PIN        GPIO_Pin_9
#define SLAVE_UART_RX_PORT       GPIOA
#define SLAVE_UART_RX_PIN        GPIO_Pin_10
#define SLAVE_USART1_REMAP       DISABLE

/* I2C1 默认映射为 PB6=SCL、PB7=SDA；重映射时使用 PB8/PB9。 */
#define MPU6500_SCL_PORT         GPIOB
#define MPU6500_SCL_PIN          GPIO_Pin_6
#define MPU6500_SDA_PORT         GPIOB
#define MPU6500_SDA_PIN          GPIO_Pin_7
#define MPU6500_I2C1_REMAP       DISABLE

static Mpu6500_Data raw_data;
static Mpu6500_Data calibrated_data;
static Attitude_Result attitude;

int main(void)
{
    SystemCoreClockUpdate();
    if (Mpu6500_Init(MPU6500_SCL_PORT, MPU6500_SCL_PIN,
                     MPU6500_SDA_PORT, MPU6500_SDA_PIN,
                     MPU6500_I2C1_REMAP) != 0)
    {
        while (1) { }
    }

    ImuCalibration_Init();
    Attitude_Init();
    BoardLink_Init(SLAVE_UART_TX_PORT, SLAVE_UART_TX_PIN,
                   SLAVE_UART_RX_PORT, SLAVE_UART_RX_PIN,
                   SLAVE_USART1_REMAP);

    while (1)
    {
        int read_status;
        BoardLink_Process();
        read_status = Mpu6500_Read(&raw_data);
        if (read_status != 0)
            continue;

        /* 每个 FIFO 样本独立完成校准和互补滤波，处理成功后发出一帧。 */
        if (ImuCalibration_Update(&raw_data, &calibrated_data) != 1)
            continue;
        if (Attitude_Update(&calibrated_data, &attitude) == 1)
            (void)BoardLink_SendAttitude(&attitude);
    }
}
