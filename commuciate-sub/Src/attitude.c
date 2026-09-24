#include "attitude.h"

#include <math.h>
#include <string.h>

#define ATTITUDE_PI                 3.14159265358979323846f
#define ATTITUDE_RAD_TO_DEG         (180.0f / ATTITUDE_PI)
#define ATTITUDE_DEG_TO_RAD         (ATTITUDE_PI / 180.0f)
#define ATTITUDE_FILTER_TAU_S       0.5f
#define ATTITUDE_MAX_DT_S           0.05f

typedef struct
{
    float roll_rad;
    float pitch_rad;
    float yaw_rad;
    uint32_t last_timestamp_ms;
    uint8_t initialized;
} Attitude_FilterState;

static Attitude_FilterState filter_state;

/* 将角度限制到 [-pi, pi]，使跨越边界时融合走较短方向。 */
static float wrap_pi(float angle)
{
    while (angle > ATTITUDE_PI)
        angle -= 2.0f * ATTITUDE_PI;
    while (angle < -ATTITUDE_PI)
        angle += 2.0f * ATTITUDE_PI;
    return angle;
}

/* 按 Z-Y-X（Yaw-Pitch-Roll）旋转顺序由欧拉角生成单位四元数。 */
static void make_quaternion(float roll, float pitch, float yaw, float q[4])
{
    float half_roll = 0.5f * roll;
    float half_pitch = 0.5f * pitch;
    float half_yaw = 0.5f * yaw;
    float cr = cosf(half_roll);
    float sr = sinf(half_roll);
    float cp = cosf(half_pitch);
    float sp = sinf(half_pitch);
    float cy = cosf(half_yaw);
    float sy = sinf(half_yaw);

    q[0] = cr * cp * cy + sr * sp * sy;
    q[1] = sr * cp * cy - cr * sp * sy;
    q[2] = cr * sp * cy + sr * cp * sy;
    q[3] = cr * cp * sy - sr * sp * cy;
}

/* 复制本次融合结果，供上层直接打包发送。 */
static void fill_result(const Mpu6500_Data *imu, Attitude_Result *result)
{
    result->roll_deg = filter_state.roll_rad * ATTITUDE_RAD_TO_DEG;
    result->pitch_deg = filter_state.pitch_rad * ATTITUDE_RAD_TO_DEG;
    result->yaw_deg = wrap_pi(filter_state.yaw_rad) * ATTITUDE_RAD_TO_DEG;
    result->timestamp_ms = imu->timestamp_ms;
    make_quaternion(filter_state.roll_rad,
                    filter_state.pitch_rad,
                    filter_state.yaw_rad,
                    result->quaternion_wxyz);
}

void Attitude_Init(void)
{
    memset(&filter_state, 0, sizeof(filter_state));
}

int Attitude_Update(const Mpu6500_Data *imu, Attitude_Result *result)
{
    uint32_t elapsed_ms;
    float ax;
    float ay;
    float az;
    float accel_norm;
    float accel_roll;
    float accel_pitch;

    if ((imu == 0) || (result == 0))
        return -1;

    ax = imu->accel_g[0];
    ay = imu->accel_g[1];
    az = imu->accel_g[2];
    accel_norm = sqrtf(ax * ax + ay * ay + az * az);
    if (accel_norm < 0.1f)
        return -1;

    accel_roll = atan2f(ay, az);
    accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));

    if (filter_state.initialized == 0u)
    {
        filter_state.roll_rad = accel_roll;
        filter_state.pitch_rad = accel_pitch;
        filter_state.yaw_rad = 0.0f;
        filter_state.last_timestamp_ms = imu->timestamp_ms;
        filter_state.initialized = 1u;
        fill_result(imu, result);
        return 1;
    }

    elapsed_ms = imu->timestamp_ms - filter_state.last_timestamp_ms;
    if (elapsed_ms == 0u)
        return 0;

    filter_state.last_timestamp_ms = imu->timestamp_ms;

    {
        float dt = (float)elapsed_ms * 0.001f;
        float roll = filter_state.roll_rad;
        float pitch = filter_state.pitch_rad;
        float cos_pitch = cosf(pitch);
        float sin_roll = sinf(roll);
        float cos_roll = cosf(roll);
        float gx = imu->gyro_dps[0] * ATTITUDE_DEG_TO_RAD;
        float gy = imu->gyro_dps[1] * ATTITUDE_DEG_TO_RAD;
        float gz = imu->gyro_dps[2] * ATTITUDE_DEG_TO_RAD;
        float tan_pitch;
        float roll_rate;
        float pitch_rate;
        float yaw_rate;
        float alpha;
        float accel_correction;
        float accel_error;

        if (dt > ATTITUDE_MAX_DT_S)
            dt = ATTITUDE_MAX_DT_S;
        if (cos_pitch > -0.01f && cos_pitch < 0.01f)
            cos_pitch = (cos_pitch >= 0.0f) ? 0.01f : -0.01f;
        tan_pitch = sinf(pitch) / cos_pitch;

        /* 將機體角速度換算成 Z-Y-X 欧拉角变化率。 */
        roll_rate = gx + sin_roll * tan_pitch * gy + cos_roll * tan_pitch * gz;
        pitch_rate = cos_roll * gy - sin_roll * gz;
        yaw_rate = (sin_roll * gy + cos_roll * gz) / cos_pitch;

        filter_state.roll_rad = wrap_pi(roll + roll_rate * dt);
        filter_state.pitch_rad += pitch_rate * dt;
        filter_state.yaw_rad = wrap_pi(filter_state.yaw_rad + yaw_rate * dt);

        /* 按实际 dt 调整互补系数，保持时间常数约 0.5 秒。 */
        alpha = ATTITUDE_FILTER_TAU_S / (ATTITUDE_FILTER_TAU_S + dt);
        accel_error = fabsf(accel_norm - 1.0f);
        accel_correction = 1.0f - (accel_error / 0.25f);
        if (accel_correction < 0.0f)
            accel_correction = 0.0f;
        accel_correction *= 1.0f - alpha;
        filter_state.roll_rad = wrap_pi(
            filter_state.roll_rad + accel_correction * wrap_pi(accel_roll - filter_state.roll_rad));
        filter_state.pitch_rad += accel_correction * (accel_pitch - filter_state.pitch_rad);
    }

    fill_result(imu, result);
    return 1;
}
