#include "stm32f10x.h"
#include "attitude.h"
#include "board_link.h"
#include "imu_calibration.h"
#include "mpu6500.h"

static Mpu6500_Data raw_data;
static Mpu6500_Data calibrated_data;
static Attitude_Result attitude;

int main(void)
{
    SystemCoreClockUpdate();
    if (Mpu6500_Init() != 0)
    {
        while (1) { }
    }

    ImuCalibration_Init();
    Attitude_Init();
    BoardLink_Init();

    while (1)
    {
        int read_status;
        BoardLink_Process();
        read_status = Mpu6500_Read(&raw_data);
        if (read_status != 0)
            continue;

        /* 每个 FIFO 样本独立完成校准和互补滤波，处理成功后发出一帧。 */
        if (ImuCalibration_Update(&raw_data, &calibrated_data) != 1)
            continue;
        if (Attitude_Update(&calibrated_data, &attitude) == 1)
            (void)BoardLink_SendAttitude(&attitude);
    }
}
