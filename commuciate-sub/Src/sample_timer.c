#include "sample_timer.h"

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"

#define SAMPLE_TIMER_COUNTER_HZ 10000u
#define SAMPLE_TIMER_TICKS_PER_SAMPLE 10u

static volatile uint8_t pending_ticks;

int SampleTimer_Init1kHz(void)
{
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef nvic;
    uint32_t timer_clock_hz;
    uint32_t prescaler;

    RCC_GetClocksFreq(&clocks);
    timer_clock_hz = clocks.PCLK1_Frequency;
    /* APB1 分频不为 1 时，通用定时器时钟为 PCLK1 的两倍。 */
    if ((RCC->CFGR & 0x00000700u) != 0u)
        timer_clock_hz *= 2u;

    if (timer_clock_hz < SAMPLE_TIMER_COUNTER_HZ ||
        (timer_clock_hz / SAMPLE_TIMER_COUNTER_HZ) == 0u)
        return -1;

    prescaler = timer_clock_hz / SAMPLE_TIMER_COUNTER_HZ;
    pending_ticks = 0u;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_Cmd(TIM2, DISABLE);
    TIM_DeInit(TIM2);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(prescaler - 1u);
    timer.TIM_CounterMode = TIM_CounterMode_Up;
    timer.TIM_Period = SAMPLE_TIMER_COUNTER_HZ / 1000u - 1u;
    timer.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInit(TIM2, &timer);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    nvic.NVIC_IRQChannel = TIM2_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 3u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    TIM_Cmd(TIM2, ENABLE);
    return 0;
}

int SampleTimer_TakeTick(void)
{
    uint32_t primask;
    int available;

    /* 保留原中断屏蔽状态，避免取任务时与 TIM2 中断竞争。 */
    __asm volatile ("mrs %0, primask" : "=r" (primask));
    __asm volatile ("cpsid i" ::: "memory");
    available = (pending_ticks != 0u);
    if (available)
        --pending_ticks;
    __asm volatile ("msr primask, %0" : : "r" (primask) : "memory");
    return available;
}

void SampleTimer_OnInterrupt(void)
{
    /* 饱和计数可保留短暂主循环延迟期间积压的采样任务。 */
    if (pending_ticks != 0xFFu)
        ++pending_ticks;
}
