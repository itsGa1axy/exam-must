#include "stm32f10x.h"
#include "mpu6500.h"

static Mpu6500_Data sensor_data;

int main(void)
{
    SystemCoreClockUpdate();

    if (Mpu6500_Init() != 0)
    {
        while (1)
        {
        }
    }

    while (1)
    {
        (void)Mpu6500_Read(&sensor_data);
    }
}
