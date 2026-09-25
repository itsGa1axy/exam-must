#include "imu_calibration.h"

#include <math.h>
#include <string.h>

#define CALIBRATION_SAMPLE_COUNT       2000u
#define CALIBRATION_GYRO_LIMIT_DPS     5.0f
#define CALIBRATION_ACCEL_MIN_G        0.70f
#define CALIBRATION_ACCEL_MAX_G        1.30f
#define CALIBRATION_GYRO_STD_MAX_DPS   1.0f
#define CALIBRATION_ACCEL_STD_MAX_G    0.05f
#define STATIONARY_GYRO_LIMIT_DPS      0.5f
#define STATIONARY_ACCEL_ERROR_G       0.03f
#define STATIONARY_ACCEL_MIN_G         (1.0f - STATIONARY_ACCEL_ERROR_G)
#define STATIONARY_ACCEL_MAX_G         (1.0f + STATIONARY_ACCEL_ERROR_G)
#define STATIONARY_HOLD_SAMPLES        500u
#define GYRO_BIAS_TRACK_ALPHA          0.001f

typedef struct
{
    float gyro_sum[3];
    float gyro_square_sum[3];
    float accel_sum[3];
    float accel_square_sum[3];
    float gyro_bias[3];
    float accel_radial_bias[3];
    uint16_t sample_count;
    uint16_t stationary_count;
    uint8_t calibrated;
} ImuCalibration_State;

static ImuCalibration_State calibration;

/* 用于校准阶段：排除明显运动或明显不符合重力幅值的样本。 */
static int is_stationary_candidate(const Mpu6500_Data *sample)
{
    float accel_norm;
    uint8_t axis;

    accel_norm = sqrtf(sample->accel_g[0] * sample->accel_g[0] +
                       sample->accel_g[1] * sample->accel_g[1] +
                       sample->accel_g[2] * sample->accel_g[2]);
    if ((accel_norm < CALIBRATION_ACCEL_MIN_G) ||
        (accel_norm > CALIBRATION_ACCEL_MAX_G))
        return 0;

    for (axis = 0u; axis < 3u; ++axis)
    {
        if ((sample->gyro_dps[axis] < -CALIBRATION_GYRO_LIMIT_DPS) ||
            (sample->gyro_dps[axis] > CALIBRATION_GYRO_LIMIT_DPS))
            return 0;
    }

    return 1;
}

/* 清空当前累计窗口；检测到运动后从新窗口重新收集。 */
static void reset_window(void)
{
    memset(calibration.gyro_sum, 0, sizeof(calibration.gyro_sum));
    memset(calibration.gyro_square_sum, 0, sizeof(calibration.gyro_square_sum));
    memset(calibration.accel_sum, 0, sizeof(calibration.accel_sum));
    memset(calibration.accel_square_sum, 0, sizeof(calibration.accel_square_sum));
    calibration.sample_count = 0u;
}

/* 检查校准窗口方差，避免把振动或缓慢运动误认为静止零偏。 */
static int calibration_window_is_stable(void)
{
    uint8_t axis;
    float inverse_count = 1.0f / (float)calibration.sample_count;

    for (axis = 0u; axis < 3u; ++axis)
    {
        float gyro_mean = calibration.gyro_sum[axis] * inverse_count;
        float gyro_variance = calibration.gyro_square_sum[axis] * inverse_count -
                              gyro_mean * gyro_mean;
        float accel_mean = calibration.accel_sum[axis] * inverse_count;
        float accel_variance = calibration.accel_square_sum[axis] * inverse_count -
                               accel_mean * accel_mean;

        if (gyro_variance < 0.0f)
            gyro_variance = 0.0f;
        if (accel_variance < 0.0f)
            accel_variance = 0.0f;

        if ((sqrtf(gyro_variance) > CALIBRATION_GYRO_STD_MAX_DPS) ||
            (sqrtf(accel_variance) > CALIBRATION_ACCEL_STD_MAX_G))
            return 0;
    }

    return 1;
}

/* 用静止平均加速度估计重力方向，只扣除沿重力方向的径向误差。
 * 单一姿态无法完整求出三轴加速度计零偏。 */
static void estimate_accel_radial_bias(void)
{
    float mean[3];
    float norm;
    float inverse_count = 1.0f / (float)calibration.sample_count;
    uint8_t axis;

    for (axis = 0u; axis < 3u; ++axis)
        mean[axis] = calibration.accel_sum[axis] * inverse_count;

    norm = sqrtf(mean[0] * mean[0] + mean[1] * mean[1] + mean[2] * mean[2]);
    if (norm < 0.1f)
        return;

    for (axis = 0u; axis < 3u; ++axis)
        calibration.accel_radial_bias[axis] = mean[axis] - mean[axis] / norm;
}

/* 累计一组静止样本并在达到窗口长度后估算陀螺仪零偏。 */
static int collect_calibration_sample(const Mpu6500_Data *sample)
{
    uint8_t axis;

    if (!is_stationary_candidate(sample))
    {
        reset_window();
        return 0;
    }

    for (axis = 0u; axis < 3u; ++axis)
    {
        float gyro = sample->gyro_dps[axis];
        float accel = sample->accel_g[axis];

        calibration.gyro_sum[axis] += gyro;
        calibration.gyro_square_sum[axis] += gyro * gyro;
        calibration.accel_sum[axis] += accel;
        calibration.accel_square_sum[axis] += accel * accel;
    }

    ++calibration.sample_count;
    if (calibration.sample_count < CALIBRATION_SAMPLE_COUNT)
        return 0;

    if (!calibration_window_is_stable())
    {
        reset_window();
        return 0;
    }

    for (axis = 0u; axis < 3u; ++axis)
        calibration.gyro_bias[axis] = calibration.gyro_sum[axis] /
                                      (float)calibration.sample_count;

    estimate_accel_radial_bias();
    calibration.calibrated = 1u;
    calibration.stationary_count = 0u;
    return 1;
}

/* 用角速度和重力幅值判定静止，持续稳定后缓慢跟踪陀螺仪零偏。 */
static void track_gyro_bias(const Mpu6500_Data *raw)
{
    float ax;
    float ay;
    float az;
    float accel_norm_square;
    uint8_t axis;
    int stationary = 1;

    ax = raw->accel_g[0] - calibration.accel_radial_bias[0];
    ay = raw->accel_g[1] - calibration.accel_radial_bias[1];
    az = raw->accel_g[2] - calibration.accel_radial_bias[2];
    /* 模长平方与阈值平方比较，避免每个采样都进行开方运算。 */
    accel_norm_square = ax * ax + ay * ay + az * az;
    if ((accel_norm_square < STATIONARY_ACCEL_MIN_G * STATIONARY_ACCEL_MIN_G) ||
        (accel_norm_square > STATIONARY_ACCEL_MAX_G * STATIONARY_ACCEL_MAX_G))
        stationary = 0;

    for (axis = 0u; axis < 3u; ++axis)
    {
        float residual = raw->gyro_dps[axis] - calibration.gyro_bias[axis];
        if (fabsf(residual) > STATIONARY_GYRO_LIMIT_DPS)
            stationary = 0;
    }

    if (!stationary)
    {
        calibration.stationary_count = 0u;
        return;
    }

    if (calibration.stationary_count < STATIONARY_HOLD_SAMPLES)
    {
        ++calibration.stationary_count;
        return;
    }

    for (axis = 0u; axis < 3u; ++axis)
    {
        calibration.gyro_bias[axis] += GYRO_BIAS_TRACK_ALPHA *
                                       (raw->gyro_dps[axis] - calibration.gyro_bias[axis]);
    }
}

void ImuCalibration_Init(void)
{
    memset(&calibration, 0, sizeof(calibration));
}

int ImuCalibration_Update(const Mpu6500_Data *raw, Mpu6500_Data *corrected)
{
    uint8_t axis;

    if ((raw == 0) || (corrected == 0))
        return -1;

    if (calibration.calibrated == 0u)
    {
        (void)collect_calibration_sample(raw);
        if (calibration.calibrated == 0u)
            return 0;
    }
    else
    {
        track_gyro_bias(raw);
    }

    *corrected = *raw;
    for (axis = 0u; axis < 3u; ++axis)
    {
        corrected->gyro_dps[axis] = raw->gyro_dps[axis] - calibration.gyro_bias[axis];
        corrected->accel_g[axis] = raw->accel_g[axis] - calibration.accel_radial_bias[axis];
    }

    return 1;
}
