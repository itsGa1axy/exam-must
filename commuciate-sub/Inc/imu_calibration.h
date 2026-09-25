#ifndef IMU_CALIBRATION_H
#define IMU_CALIBRATION_H

#include "mpu6500.h"

/* 清除上电校准累积量；校准期间应保持传感器静止。 */
void ImuCalibration_Init(void);

/* 输入 MPU 原始物理量并输出已扣除零偏的数据。
 * 返回 0 表示仍在上电校准，返回 1 表示输出有效校准数据，负数表示参数无效。 */
int ImuCalibration_Update(const Mpu6500_Data *raw, Mpu6500_Data *corrected);

#endif
