#ifndef WATCHDOG_H
#define WATCHDOG_H

#include <stdbool.h>

/* 初始化独立看门狗；超时后由硬件复位主机。 */
void Watchdog_Init(void);

/* 主循环每轮调用一次，证明主循环仍在正常运行。 */
void Watchdog_Feed(void);

/* 返回本次启动是否由独立看门狗触发复位。 */
bool Watchdog_WasReset(void);

#endif
