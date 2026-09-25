#ifndef SAMPLE_TIMER_H
#define SAMPLE_TIMER_H

#include <stdint.h>

/* 初始化 1 kHz 采样定时器；定时中断只登记采样任务，不在中断中访问 I2C。 */
int SampleTimer_Init1kHz(void);

/* 主循环每次取走一个到期的采样任务。 */
int SampleTimer_TakeTick(void);

/* 由 TIM2 中断服务函数调用。 */
void SampleTimer_OnInterrupt(void);

#endif
