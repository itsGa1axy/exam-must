#include "stm32f10x.h"
#include "attitude.h"
#include "board_link.h"
#include "imu_calibration.h"
#include "mpu6500.h"
#include "mpu6500_port.h"
#include "sample_timer.h"

/* 从机板卡引脚统一在此配置；USART1 用于向主机发送姿态帧。 */
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
    uint16_t gyro_weight_q15;
    int read_status;
    int calibration_status;
    int attitude_status;

    SystemCoreClockUpdate();
    if (Mpu6500_Init(MPU6500_SCL_PORT, MPU6500_SCL_PIN,
                     MPU6500_SDA_PORT, MPU6500_SDA_PIN,
                     MPU6500_I2C1_REMAP) != 0)
        while (1) { }

    ImuCalibration_Init();
    Attitude_Init();
    BoardLink_Init(SLAVE_UART_TX_PORT, SLAVE_UART_TX_PIN,
                   SLAVE_UART_RX_PORT, SLAVE_UART_RX_PIN,
                   SLAVE_USART1_REMAP);
    if (SampleTimer_Init1kHz() != 0)
        while (1) { }

    while (1)
    {
        BoardLink_Process();
        if (BoardLink_TakeFilterWeight(&gyro_weight_q15) != 0)
            (void)Attitude_SetGyroWeight((float)gyro_weight_q15 / 32767.0f);

        if (!SampleTimer_TakeTick())
            continue;

        read_status = Mpu6500_Read(&raw_data);
        if (read_status != 0)
            continue;

        calibration_status = ImuCalibration_Update(&raw_data, &calibrated_data);
        if (calibration_status <= 0)
            continue;

        attitude_status = Attitude_Update(&calibrated_data, &attitude);
        if (attitude_status > 0)
            (void)BoardLink_SendAttitude(&attitude);
    }
}
