#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>
#include "stm32f10x.h"

typedef struct
{
    float accel_g[3];
    float gyro_dps[3];
    float temperature_c;
    uint32_t timestamp_ms;
} Mpu6500_Data;

/* 初始化失败码，便于串口日志区分故障阶段。 */
#define MPU6500_INIT_ERR_PORT          (-1)
#define MPU6500_INIT_ERR_WHOAMI_READ   (-2)
#define MPU6500_INIT_ERR_WHOAMI_ID     (-3)
#define MPU6500_INIT_ERR_DRIVER        (-4)
#define MPU6500_INIT_ERR_CONFIG        (-5)

/* 初始化 I2C1 和 MPU6500，并将加速度计/陀螺仪配置为 1 kHz FIFO 采样。 */
int Mpu6500_Init(GPIO_TypeDef *scl_port, uint16_t scl_pin,
                 GPIO_TypeDef *sda_port, uint16_t sda_pin,
                 FunctionalState remap_i2c1);

/* 读取官方 FIFO 中最早的一帧同步样本。
 * 返回 0 表示成功，1 表示暂无新样本，-1 表示未初始化或参数错误，
 * -2 表示底层读取失败，-3 表示官方库检测到 FIFO 溢出并尝试复位。 */
int Mpu6500_Read(Mpu6500_Data *data);

#endif
