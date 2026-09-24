#ifndef MPU6500_H
#define MPU6500_H

#include <stdint.h>

typedef struct
{
    float accel_g[3];
    float gyro_dps[3];
    float temperature_c;
    uint32_t timestamp_ms;
} Mpu6500_Data;

/* 初始化 I2C1 和 MPU6500，并将加速度计/陀螺仪配置为 1 kHz FIFO 采样。 */
int Mpu6500_Init(void);

/* 读取官方 FIFO 中最早的一帧同步样本；返回 1 表示 FIFO 暂无新样本。 */
int Mpu6500_Read(Mpu6500_Data *data);

#endif
