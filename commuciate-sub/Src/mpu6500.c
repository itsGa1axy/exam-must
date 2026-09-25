#include "mpu6500.h"
#include "inv_mpu.h"
#include "mpu6500_port.h"

static float gyro_sensitivity;
static unsigned short accel_sensitivity;
static float gyro_scale;
static float accel_scale;
static uint8_t initialized;
static uint8_t timestamp_valid;
static uint32_t last_timestamp_ms;

/* 初始化 MPU6500，并开启 1 kHz 加速度计/陀螺仪 FIFO。 */
int Mpu6500_Init(GPIO_TypeDef *scl_port, uint16_t scl_pin,
                 GPIO_TypeDef *sda_port, uint16_t sda_pin,
                 FunctionalState remap_i2c1)
{
    unsigned char who_am_i = 0u;

    initialized = 0u;
    timestamp_valid = 0u;
    if (Mpu6500_PortInit(scl_port, scl_pin, sda_port, sda_pin,
                         remap_i2c1) != 0)
        return MPU6500_INIT_ERR_PORT;
    /* 先用官方寄存器接口确认 I2C 可读，且连接的是 MPU6500。 */
    if (mpu_read_reg(0x75u, &who_am_i) != 0)
        return MPU6500_INIT_ERR_WHOAMI_READ;
    if (who_am_i != 0x70u)
        return MPU6500_INIT_ERR_WHOAMI_ID;
    if (mpu_init(0) != 0)
        return MPU6500_INIT_ERR_DRIVER;
    if (mpu_set_sensors(INV_XYZ_ACCEL | INV_XYZ_GYRO) != 0 ||
        mpu_set_gyro_fsr(500) != 0 || mpu_set_accel_fsr(4) != 0 ||
        mpu_set_sample_rate(1000) != 0 || mpu_set_lpf(98) != 0 ||
        mpu_get_gyro_sens(&gyro_sensitivity) != 0 ||
        mpu_get_accel_sens(&accel_sensitivity) != 0 ||
        mpu_configure_fifo(INV_XYZ_ACCEL | INV_XYZ_GYRO) != 0)
    {
        initialized = 0u;
        return MPU6500_INIT_ERR_CONFIG;
    }
    if (gyro_sensitivity <= 0.0f || accel_sensitivity == 0u)
        return MPU6500_INIT_ERR_CONFIG;
    /* 灵敏度在初始化后固定，预先求倒数，避免每帧做六次软件浮点除法。 */
    gyro_scale = 1.0f / gyro_sensitivity;
    accel_scale = 1.0f / (float)accel_sensitivity;
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
    uint32_t timestamp_ms;
    uint8_t axis;
    int fifo_status;

    if (data == 0 || initialized == 0u)
        return -1;
    fifo_status = mpu_read_fifo(gyro_raw, accel_raw, &timestamp, &sensors, &more);
    if (fifo_status == -2)
        return -3; /* 官方库检测到 FIFO 溢出并尝试复位 FIFO。 */
    if (fifo_status != 0)
        return -2;
    if (sensors == 0u)
        return 1; /* 当前没有新采样 */

    for (axis = 0u; axis < 3u; ++axis)
    {
        sample.accel_g[axis] = (float)accel_raw[axis] * accel_scale;
        sample.gyro_dps[axis] = (float)gyro_raw[axis] * gyro_scale;
    }
    sample.temperature_c = 0.0f; /* 姿态解算不依赖温度，避免每帧额外 I2C 读取 */
    /* 官方库给的是读取时刻；用 FIFO 剩余包数估计当前样本的采样时刻。 */
    timestamp_ms = (uint32_t)timestamp - (uint32_t)more;
    if (timestamp_valid != 0u)
    {
        uint32_t elapsed_ms = timestamp_ms - last_timestamp_ms;

        /* 毫秒时钟与 FIFO 计数不同步时，两个真实样本可能算出相同或倒退的时间戳。
         * 按模 2^32 比较前后关系；保留真正间断后的墙钟时间，异常时至少前进 1 ms。 */
        if (elapsed_ms == 0u || elapsed_ms >= 0x80000000u)
            timestamp_ms = last_timestamp_ms + 1u;
    }
    last_timestamp_ms = timestamp_ms;
    timestamp_valid = 1u;
    sample.timestamp_ms = timestamp_ms;
    *data = sample;
    return 0;
}
