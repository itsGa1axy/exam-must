#include "stm32f10x.h"
#include "attitude.h"
#include "imu_calibration.h"
#include "mpu6500.h"
#include "mpu6500_port.h"

static Mpu6500_Data raw_data;
static Mpu6500_Data calibrated_data;
static Attitude_Result attitude;

int main(void)
{
    SystemCoreClockUpdate();

    if (Mpu6500_Init() != 0)
    {
        while (1)
        {
        }
    }

    ImuCalibration_Init();
    Attitude_Init();

    while (1)
    {
        static uint32_t last_tick_ms;
        uint32_t now_ms;

        /* 仅在 1 ms SysTick 节拍后读取一次，避免主循环过快重复读取同一组传感器数据。 */
        do
        {
            now_ms = Mpu6500_PortGetMs();
        } while (now_ms == last_tick_ms);
        last_tick_ms = now_ms;

        if (Mpu6500_Read(&raw_data) != 0)
            continue;

        /* 上电保持静止，校准完成后再把数据交给互补滤波器。 */
        if (ImuCalibration_Update(&raw_data, &calibrated_data) != 1)
            continue;

        /* attitude 保存最新 Pitch、Roll、Yaw 和 wxyz 四元数，供通信层读取。 */
        (void)Attitude_Update(&calibrated_data, &attitude);
    }
}
