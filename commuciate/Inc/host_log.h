#ifndef HOST_LOG_H
#define HOST_LOG_H

#include "board_link.h"
#include <stdint.h>

/* 初始化 1 ms 日志时基并输出上电信息。 */
void HostLog_Init(void);
/* SysTick 中断只更新时间，不在中断中发送串口。 */
void HostLog_Tick1ms(void);
/* 主循环记录最近收到的姿态帧，并每秒输出一次链路统计。 */
void HostLog_OnAttitude(const BoardLink_Attitude *attitude);
void HostLog_Process(void);
/* 记录电脑下发滤波系数后的板间发送结果。 */
void HostLog_FilterWeight(uint16_t weight_q15, int send_result);

#endif
