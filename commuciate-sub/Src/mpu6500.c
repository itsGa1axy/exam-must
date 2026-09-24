#include "mpu6500.h"
#include "inv_mpu.h"
#include "mpu6500_port.h"

static float gyro_sensitivity;
static unsigned short accel_sensitivity;
static uint8_t initialized;

/* 初始化 MPU6500，并开启 1 kHz 加速度计/陀螺仪 FIFO。 */
int Mpu6500_Init(void)
{
    if (Mpu6500_PortInit() != 0)
        return -1;
    if (mpu_init(0) != 0)
        return -2;
    if (mpu_set_sensors(INV_XYZ_ACCEL | INV_XYZ_GYRO) != 0 ||
        mpu_set_gyro_fsr(500) != 0 || mpu_set_accel_fsr(4) != 0 ||
        mpu_set_sample_rate(1000) != 0 || mpu_set_lpf(98) != 0 ||
        mpu_get_gyro_sens(&gyro_sensitivity) != 0 ||
        mpu_get_accel_sens(&accel_sensitivity) != 0 ||
        mpu_configure_fifo(INV_XYZ_ACCEL | INV_XYZ_GYRO) != 0)
    {
        initialized = 0u;
        return -3;
    }
    initialized = 1u;
    return 0;
}

/* 每次从官方 FIFO 接口读取一帧同步的加速度和角速度。 */
int Mpu6500_Read(Mpu6500_Data *data)
{
    short accel_raw[3];
    short gyro_raw[3];
    unsigned long timestamp;
    unsigned char sensors;
    unsigned char more;
    Mpu6500_Data sample;
    uint8_t axis;

    if (data == 0 || initialized == 0u)
        return -1;
    if (mpu_read_fifo(gyro_raw, accel_raw, &timestamp, &sensors, &more) != 0)
        return -2;
    if (sensors == 0u)
        return 1; /* 当前没有新采样 */

    for (axis = 0u; axis < 3u; ++axis)
    {
        sample.accel_g[axis] = (float)accel_raw[axis] / (float)accel_sensitivity;
        sample.gyro_dps[axis] = (float)gyro_raw[axis] / gyro_sensitivity;
    }
    sample.temperature_c = 0.0f; /* 姿态解算不依赖温度，避免每帧额外 I2C 读取 */
    /* more 是当前包之后排队的包数，回推旧包时间以保持 FIFO 样本间隔为 1 ms。 */
    sample.timestamp_ms = (uint32_t)timestamp - (uint32_t)more;
    *data = sample;
    return 0;
}
