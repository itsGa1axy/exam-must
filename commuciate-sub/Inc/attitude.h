#ifndef ATTITUDE_H
#define ATTITUDE_H

#include "mpu6500.h"

typedef struct
{
    float pitch_deg;
    float roll_deg;
    float yaw_deg;
    float quaternion_wxyz[4];
    uint32_t timestamp_ms;
} Attitude_Result;

/* 清空互补滤波器状态；启动校准完成后调用。 */
void Attitude_Init(void);
/* 设置互补滤波陀螺仪权重，范围 0.0 到 1.0，返回 0 表示成功。 */
int Attitude_SetGyroWeight(float weight);

/* 使用校准后的加速度和角速度更新姿态。
 * 返回 1 表示已输出新姿态，0 表示时间戳未变化，负数表示参数无效。 */
int Attitude_Update(const Mpu6500_Data *imu, Attitude_Result *result);

#endif
