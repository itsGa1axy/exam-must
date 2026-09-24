/**
  ******************************************************************************
  * @file    mpu6500_port.h
  * @brief   MPU6500 驱动的 STM32F10x 移植层接口。
  *
  *          移植层是驱动与具体单片机之间的适配层，只做三件事：
  *            1. 提供毫秒级时基（SysTick 中断累加），供初始化延时与时间戳使用；
  *            2. 配置 I2C1（PB6 = SCL，PB7 = SDA）为 400 kHz 快速模式；
  *            3. 实现 InvenSense 官方库所需的 inv_i2c_read / inv_i2c_write
  *               两个回调（声明在 Src/mpu6500_port.c 中，不属于本头文件）。
  *
  *          更换 MCU 时只需重写 Src/mpu6500_port.c，上层 mpu6500.c 无需改动。
  ******************************************************************************
  */

#ifndef MPU6500_PORT_H
#define MPU6500_PORT_H

#include <stdint.h>

/**
  * @brief  初始化移植层：SysTick 毫秒时基、I2C1 时钟与引脚、I2C 外设。
  *
  * @param  无
  * @retval  0  初始化成功
  * @retval -1  SysTick 配置失败（重装值超出 24 位范围或时钟异常）
  *
  * @note   本函数会调用 SysTick_Config，从此 SysTick 中断开始产生，
  *         全局毫秒计数由 system_millis 维护。
  */
int Mpu6500_PortInit(void);

/**
  * @brief  毫秒时基递增 1，须在 SysTick 中断服务函数中调用。
  *
  * @param  无
  * @retval 无
  *
  * @note   本函数在中断上下文执行，实现只是对 volatile 变量的自增，
  *         没有任何分支或函数调用，因此中断开销极小。
  */
void Mpu6500_PortTick1ms(void);

/**
  * @brief  获取当前毫秒时基，即上电以来的毫秒数。
  *
  * @param  无
  * @retval 上电后经过的毫秒数
  *
  * @note   返回值在约 49.7 天后回绕；若需长时间记录，请自行处理回绕，
  *         比较两个时刻的间隔时应使用无符号数相减的写法。
  */
uint32_t Mpu6500_PortGetMs(void);

#endif /* MPU6500_PORT_H */
