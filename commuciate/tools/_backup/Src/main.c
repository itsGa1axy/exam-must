#include "stm32f10x.h"

int main(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = GPIO_Pin_13;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;

    GPIO_Init(GPIOC, &gpio);
    GPIO_ResetBits(GPIOC, GPIO_Pin_13);

    while (1)
    {
    }
}