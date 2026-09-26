#include "watchdog.h"

#include "stm32f10x_iwdg.h"
#include "stm32f10x_rcc.h"

/* LSI 标称约 40 kHz；64 分频、1250 次计数约为 2 秒。 */
#define WATCHDOG_PRESCALER IWDG_Prescaler_64
#define WATCHDOG_RELOAD    1250u

static bool watchdog_reset;
static bool watchdog_enabled;

void Watchdog_Init(void)
{
    uint32_t timeout = 100000u;

    watchdog_enabled = false;
    /* 复位标志必须在清除前读取，否则无法判断上次复位来源。 */
    watchdog_reset = (RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET);
    RCC_ClearFlag();

    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
    IWDG_SetPrescaler(WATCHDOG_PRESCALER);
    IWDG_SetReload(WATCHDOG_RELOAD);

    /* 等待分频和重装载值同步到低速时钟域。 */
    while ((IWDG_GetFlagStatus(IWDG_FLAG_PVU) != RESET ||
            IWDG_GetFlagStatus(IWDG_FLAG_RVU) != RESET) && timeout-- != 0u)
    {
    }

    /* 同步异常时不能把主循环永久锁死；本次启动跳过看门狗。 */
    if (timeout == 0u)
        return;

    IWDG_ReloadCounter();
    IWDG_Enable();
    watchdog_enabled = true;
}

void Watchdog_Feed(void)
{
    if (watchdog_enabled)
        IWDG_ReloadCounter();
}

bool Watchdog_WasReset(void)
{
    return watchdog_reset;
}
