#include "stm32f10x.h"

/* 烧录发送板时设为 1，烧录接收板时设为 0。 */
#ifndef BOARD_IS_SENDER
#define BOARD_IS_SENDER 0
#endif

/*
 * 最小单线协议（1000 bit/s）：
 *   空闲位(1) | 起始位(0) | 8 位数据(低位先发) | 停止位(1)
 *
 * 接线：发送板 PA0 <-> 接收板 PA0，两个板子的 GND 必须相连。
 */
#define LINK_GPIO       GPIOA
#define LINK_GPIO_PIN   GPIO_Pin_0
#define LINK_BIT_US     1000u

static void delay_us(uint32_t us)
{
    uint32_t ticks = (SystemCoreClock / 1000000u) * us;

    SysTick->CTRL = 0u;
    SysTick->LOAD = ticks - 1u;
    SysTick->VAL = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u)
    {
    }

    SysTick->CTRL = 0u;
}

#if BOARD_IS_SENDER

static void delay_ms(uint32_t ms)
{
    while (ms > 0u)
    {
        delay_us(1000u);
        --ms;
    }
}

static void link_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* 开漏输出：写 1 时释放总线，写 0 时拉低总线。 */
    GPIO_SetBits(LINK_GPIO, LINK_GPIO_PIN);
    gpio.GPIO_Pin = LINK_GPIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(LINK_GPIO, &gpio);
}

static void link_send_byte(uint8_t data)
{
    uint8_t bit;

    /* 起始位。 */
    GPIO_ResetBits(LINK_GPIO, LINK_GPIO_PIN);
    delay_us(LINK_BIT_US);

    /* 数据位：最低位先发送。 */
    for (bit = 0u; bit < 8u; ++bit)
    {
        if ((data & (1u << bit)) != 0u)
        {
            GPIO_SetBits(LINK_GPIO, LINK_GPIO_PIN);
        }
        else
        {
            GPIO_ResetBits(LINK_GPIO, LINK_GPIO_PIN);
        }

        delay_us(LINK_BIT_US);
    }

    /* 停止位，同时让总线回到空闲高电平。 */
    GPIO_SetBits(LINK_GPIO, LINK_GPIO_PIN);
    delay_us(LINK_BIT_US);
}

#else

static volatile uint8_t link_start_pending = 0u;
static volatile uint8_t link_receiving = 0u;

static void link_init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    gpio.GPIO_Pin = LINK_GPIO_PIN;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(LINK_GPIO, &gpio);

    /* PA0 映射到 EXTI0，检测起始位的下降沿。 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);

    EXTI_StructInit(&exti);
    exti.EXTI_Line = EXTI_Line0;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    EXTI_ClearITPendingBit(EXTI_Line0);

    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    nvic.NVIC_IRQChannel = EXTI0_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void EXTI0_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line0) != RESET)
    {
        if (link_receiving == 0u)
        {
            link_start_pending = 1u;
        }

        EXTI_ClearITPendingBit(EXTI_Line0);
    }
}

static uint8_t link_receive_byte(uint8_t *data)
{
    uint8_t bit;
    uint8_t value = 0u;

    /* 先等总线空闲，再等待起始位。 */
    if (link_start_pending == 0u)
    {
        return 0u;
    }

    link_receiving = 1u;
    link_start_pending = 0u;

    /* 接收一帧期间关闭 EXTI，避免数据位的下降沿被误认为新起始位。 */
    EXTI->IMR &= ~EXTI_Line0;
    EXTI_ClearITPendingBit(EXTI_Line0);

    /* 在起始位中央再次确认，过滤很短的低电平毛刺。 */
    delay_us(LINK_BIT_US / 2u);
    if (GPIO_ReadInputDataBit(LINK_GPIO, LINK_GPIO_PIN) != Bit_RESET)
    {
        link_receiving = 0u;
        EXTI_ClearITPendingBit(EXTI_Line0);
        EXTI->IMR |= EXTI_Line0;
        return 0u;
    }

    /* 移到第一个数据位中央，随后每隔一个位时间采样。 */
    delay_us(LINK_BIT_US);
    for (bit = 0u; bit < 8u; ++bit)
    {
        if (GPIO_ReadInputDataBit(LINK_GPIO, LINK_GPIO_PIN) != Bit_RESET)
        {
            value |= (uint8_t)(1u << bit);
        }

        delay_us(LINK_BIT_US);
    }

    /* 此时位于停止位中央；停止位必须为高。 */
    if (GPIO_ReadInputDataBit(LINK_GPIO, LINK_GPIO_PIN) == Bit_RESET)
    {
        link_receiving = 0u;
        EXTI_ClearITPendingBit(EXTI_Line0);
        EXTI->IMR |= EXTI_Line0;
        return 0u;
    }

    *data = value;
    link_receiving = 0u;
    EXTI_ClearITPendingBit(EXTI_Line0);
    EXTI->IMR |= EXTI_Line0;
    return 1u;
}

static void led_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
    GPIO_SetBits(GPIOC, GPIO_Pin_13);

    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOC, &gpio);
}

#endif

int main(void)
{
    SystemCoreClockUpdate();
    link_init();

#if BOARD_IS_SENDER
    while (1)
    {
        link_send_byte(0x55u);
        delay_ms(500u);
    }
#else
    uint8_t received;

    led_init();

    while (1)
    {
        if ((link_receive_byte(&received) != 0u) && (received == 0x55u))
        {
            if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_13) != Bit_RESET)
            {
                GPIO_ResetBits(GPIOC, GPIO_Pin_13);
            }
            else
            {
                GPIO_SetBits(GPIOC, GPIO_Pin_13);
            }
        }
    }
#endif
}
