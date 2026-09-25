/**
 ******************************************************************************
 * @brief     STM32F103xx 器件向量表，用于 GCC 工具链。
 *            本模块执行以下操作：
 *                - 设置初始 SP
 *                - 设置初始 PC == Reset_Handler，
 *                - 用异常 ISR 地址设置向量表表项
 *                - 跳转到 C 库中的 main（其最终
 *                  会调用 main()）。
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * 保留所有权利。
 *
 * 本软件按 LICENSE 文件中的条款授权，该文件位于
 * 本软件组件的根目录下。
 * 若本软件未附带 LICENSE 文件，则按“原样”提供。
 *
 ******************************************************************************
 */

.syntax unified
.cpu cortex-m3
.fpu softvfp
.thumb

.global g_pfnVectors
.global Default_Handler

/* .data 段初始化值的起始地址。
在链接脚本中定义 */
.word _sidata
/* .data 段的起始地址。在链接脚本中定义 */
.word _sdata
/* .data 段的结束地址。在链接脚本中定义 */
.word _edata
/* .bss 段的起始地址。在链接脚本中定义 */
.word _sbss
/* .bss 段的结束地址。在链接脚本中定义 */
.word _ebss

/**
 * @brief  这是处理器在复位事件后首次开始执行时
 *          被调用的代码。其中只执行绝对必要的
 *          设置，之后便调用应用提供的 main() 例程。
 * @param  无
 * @retval : 无
*/

  .section .text.Reset_Handler
  .weak Reset_Handler
  .type Reset_Handler, %function
Reset_Handler:
  ldr   r0, =_estack
  mov   sp, r0          /* 设置栈指针 */
/* 调用时钟系统初始化函数。*/
  bl  SystemInit

/* 将数据段初值从 Flash 复制到 SRAM */
  ldr r0, =_sdata
  ldr r1, =_edata
  ldr r2, =_sidata
  movs r3, #0
  b LoopCopyDataInit

CopyDataInit:
  ldr r4, [r2, r3]
  str r4, [r0, r3]
  adds r3, r3, #4

LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit

/* 将 bss 段清零填充。 */
  ldr r2, =_sbss
  ldr r4, =_ebss
  movs r3, #0
  b LoopFillZerobss

FillZerobss:
  str  r3, [r2]
  adds r2, r2, #4

LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss

/* 调用静态构造函数 */
  bl __libc_init_array
/* 调用应用程序入口点。*/
  bl main

LoopForever:
  b LoopForever

  .size Reset_Handler, .-Reset_Handler

/**
 * @brief  这是处理器收到意外中断时被调用的代码。
 *         它只是进入一个无限循环，以保留系统状态
 *         供调试器检查。
 *
 * @param  无
 * @retval : 无
*/
  .section .text.Default_Handler,"ax",%progbits
Default_Handler:
Infinite_Loop:
  b Infinite_Loop
  .size Default_Handler, .-Default_Handler

/******************************************************************************
*
* STM32F103xx 向量表。注意必须在此放置相应的构造，
* 以确保它最终位于物理地址 0x0000.0000 处。
*
******************************************************************************/
  .section .isr_vector,"a",%progbits
  .type g_pfnVectors, %object

g_pfnVectors:
  .word _estack
  .word Reset_Handler
  .word NMI_Handler
  .word HardFault_Handler
  .word	MemManage_Handler
  .word	BusFault_Handler
  .word	UsageFault_Handler
  .word	0
  .word	0
  .word	0
  .word	0
  .word	SVC_Handler
  .word	DebugMon_Handler
  .word	0
  .word	PendSV_Handler
  .word	SysTick_Handler
  .word	WWDG_IRQHandler           			/* 窗口看门狗中断                        */
  .word	PVD_IRQHandler            			/* PVD 通过 EXTI 线路检测中断        */
  .word	TAMPER_IRQHandler         			/* 防篡改中断                                 */
  .word	RTC_IRQHandler            			/* RTC 全局中断                             */
  .word	FLASH_IRQHandler          			/* Flash 全局中断                           */
  .word	RCC_IRQHandler            			/* RCC 全局中断                             */
  .word	EXTI0_IRQHandler          			/* EXTI 线路 0 中断                             */
  .word	EXTI1_IRQHandler          			/* EXTI 线路 1 中断                             */
  .word	EXTI2_IRQHandler          			/* EXTI 线路 2 中断                             */
  .word	EXTI3_IRQHandler          			/* EXTI 线路 3 中断                             */
  .word	EXTI4_IRQHandler          			/* EXTI 线路 4 中断                             */
  .word	DMA1_Channel1_IRQHandler  			/* DMA1 通道 1 全局中断                   */
  .word	DMA1_Channel2_IRQHandler  			/* DMA1 通道 2 全局中断                   */
  .word	DMA1_Channel3_IRQHandler  			/* DMA1 通道 3 全局中断                   */
  .word	DMA1_Channel4_IRQHandler  			/* DMA1 通道 4 全局中断                   */
  .word	DMA1_Channel5_IRQHandler  			/* DMA1 通道 5 全局中断                   */
  .word	DMA1_Channel6_IRQHandler  			/* DMA1 通道 6 全局中断                   */
  .word	DMA1_Channel7_IRQHandler  			/* DMA1 通道 7 全局中断                   */
  .word	ADC1_2_IRQHandler         			/* ADC1 和 ADC2 全局中断                   */
  .word	USB_HP_CAN_TX_IRQHandler  			/* USB 高优先级或 CAN 发送中断           */
  .word	USB_LP_CAN_RX0_IRQHandler 			/* USB 低优先级或 CAN 接收 0 中断           */
  .word	CAN_RX1_IRQHandler        			/* CAN 接收 1 中断                                */
  .word	CAN_SCE_IRQHandler        			/* CAN SCE 中断                                */
  .word	EXTI9_5_IRQHandler        			/* EXTI 线路 [9:5] 中断                        */
  .word	TIM1_BRK_IRQHandler       			/* TIM1 刹车中断                             */
  .word	TIM1_UP_IRQHandler        			/* TIM1 更新中断                            */
  .word	TIM1_TRG_COM_IRQHandler   			/* TIM1 触发和换向中断          */
  .word	TIM1_CC_IRQHandler        			/* TIM1 捕获比较中断                   */
  .word	TIM2_IRQHandler           			/* TIM2 全局中断                            */
  .word	TIM3_IRQHandler           			/* TIM3 全局中断                            */
  .word	TIM4_IRQHandler           			/* TIM4 全局中断                            */
  .word	I2C1_EV_IRQHandler        			/* I2C1 事件中断                             */
  .word	I2C1_ER_IRQHandler        			/* I2C1 错误中断                             */
  .word	I2C2_EV_IRQHandler        			/* I2C2 事件中断                             */
  .word	I2C2_ER_IRQHandler        			/* I2C2 错误中断                             */
  .word	SPI1_IRQHandler           			/* SPI1 全局中断                            */
  .word	SPI2_IRQHandler           			/* SPI2 全局中断                            */
  .word	USART1_IRQHandler         			/* USART1 全局中断                          */
  .word	USART2_IRQHandler         			/* USART2 全局中断                          */
  .word	USART3_IRQHandler         			/* USART3 全局中断                          */
  .word	EXTI15_10_IRQHandler      			/* EXTI 线路 [15:10] 中断                      */
  .word	RTCAlarm_IRQHandler       			/* RTC 闹钟通过 EXTI 线路中断           */
  .word	0                         			/* 保留                                         */
  .word	TIM8_BRK_IRQHandler       			/* TIM8 刹车中断                             */
  .word	TIM8_UP_IRQHandler        			/* TIM8 更新中断                            */
  .word	TIM8_TRG_COM_IRQHandler   			/* TIM8 触发和换向中断          */
  .word	TIM8_CC_IRQHandler        			/* TIM8 捕获比较中断                   */
  .word	ADC3_IRQHandler           			/* ADC3 全局中断                            */
  .word	FSMC_IRQHandler           			/* FSMC 全局中断                            */
  .word	SDIO_IRQHandler           			/* SDIO 全局中断                            */
  .word	TIM5_IRQHandler           			/* TIM5 全局中断                            */
  .word	SPI3_IRQHandler           			/* SPI3 全局中断                            */
  .word	UART4_IRQHandler          			/* UART4 全局中断                           */
  .word	UART5_IRQHandler          			/* UART5 全局中断                           */
  .word	TIM6_IRQHandler           			/* TIM6 全局中断                            */
  .word	TIM7_IRQHandler           			/* TIM7 全局中断                            */
  .word	DMA2_Channel1_IRQHandler  			/* DMA2 通道 1 全局中断                   */
  .word	DMA2_Channel2_IRQHandler  			/* DMA2 通道 2 全局中断                   */
  .word	DMA2_Channel3_IRQHandler  			/* DMA2 通道 3 全局中断                   */
  .word	DMA2_Channel4_5_IRQHandler			/* DMA2 通道 4 和 DMA2 通道 5 全局中断 */
  .size g_pfnVectors, .-g_pfnVectors

/*******************************************************************************
*
* 为每个异常处理函数提供指向 Default_Handler 的弱别名。
* 由于它们是弱别名，任何同名函数都会覆盖
* 此定义。
*
*******************************************************************************/

	.weak	NMI_Handler
	.thumb_set NMI_Handler,Default_Handler

	.weak	HardFault_Handler
	.thumb_set HardFault_Handler,Default_Handler

	.weak	MemManage_Handler
	.thumb_set MemManage_Handler,Default_Handler

	.weak	BusFault_Handler
	.thumb_set BusFault_Handler,Default_Handler

	.weak	UsageFault_Handler
	.thumb_set UsageFault_Handler,Default_Handler

	.weak	SVC_Handler
	.thumb_set SVC_Handler,Default_Handler

	.weak	DebugMon_Handler
	.thumb_set DebugMon_Handler,Default_Handler

	.weak	PendSV_Handler
	.thumb_set PendSV_Handler,Default_Handler

	.weak	SysTick_Handler
	.thumb_set SysTick_Handler,Default_Handler

	.weak	WWDG_IRQHandler
	.thumb_set WWDG_IRQHandler,Default_Handler

	.weak	PVD_IRQHandler
	.thumb_set PVD_IRQHandler,Default_Handler

	.weak	TAMPER_IRQHandler
	.thumb_set TAMPER_IRQHandler,Default_Handler

	.weak	RTC_IRQHandler
	.thumb_set RTC_IRQHandler,Default_Handler

	.weak	FLASH_IRQHandler
	.thumb_set FLASH_IRQHandler,Default_Handler

	.weak	RCC_IRQHandler
	.thumb_set RCC_IRQHandler,Default_Handler

	.weak	EXTI0_IRQHandler
	.thumb_set EXTI0_IRQHandler,Default_Handler

	.weak	EXTI1_IRQHandler
	.thumb_set EXTI1_IRQHandler,Default_Handler

	.weak	EXTI2_IRQHandler
	.thumb_set EXTI2_IRQHandler,Default_Handler

	.weak	EXTI3_IRQHandler
	.thumb_set EXTI3_IRQHandler,Default_Handler

	.weak	EXTI4_IRQHandler
	.thumb_set EXTI4_IRQHandler,Default_Handler

	.weak	DMA1_Channel1_IRQHandler
	.thumb_set DMA1_Channel1_IRQHandler,Default_Handler

	.weak	DMA1_Channel2_IRQHandler
	.thumb_set DMA1_Channel2_IRQHandler,Default_Handler

	.weak	DMA1_Channel3_IRQHandler
	.thumb_set DMA1_Channel3_IRQHandler,Default_Handler

	.weak	DMA1_Channel4_IRQHandler
	.thumb_set DMA1_Channel4_IRQHandler,Default_Handler

	.weak	DMA1_Channel5_IRQHandler
	.thumb_set DMA1_Channel5_IRQHandler,Default_Handler

	.weak	DMA1_Channel6_IRQHandler
	.thumb_set DMA1_Channel6_IRQHandler,Default_Handler

	.weak	DMA1_Channel7_IRQHandler
	.thumb_set DMA1_Channel7_IRQHandler,Default_Handler

	.weak	ADC1_2_IRQHandler
	.thumb_set ADC1_2_IRQHandler,Default_Handler

	.weak	USB_HP_CAN_TX_IRQHandler
	.thumb_set USB_HP_CAN_TX_IRQHandler,Default_Handler

	.weak	USB_LP_CAN_RX0_IRQHandler
	.thumb_set USB_LP_CAN_RX0_IRQHandler,Default_Handler

	.weak	CAN_RX1_IRQHandler
	.thumb_set CAN_RX1_IRQHandler,Default_Handler

	.weak	CAN_SCE_IRQHandler
	.thumb_set CAN_SCE_IRQHandler,Default_Handler

	.weak	EXTI9_5_IRQHandler
	.thumb_set EXTI9_5_IRQHandler,Default_Handler

	.weak	TIM1_BRK_IRQHandler
	.thumb_set TIM1_BRK_IRQHandler,Default_Handler

	.weak	TIM1_UP_IRQHandler
	.thumb_set TIM1_UP_IRQHandler,Default_Handler

	.weak	TIM1_TRG_COM_IRQHandler
	.thumb_set TIM1_TRG_COM_IRQHandler,Default_Handler

	.weak	TIM1_CC_IRQHandler
	.thumb_set TIM1_CC_IRQHandler,Default_Handler

	.weak	TIM2_IRQHandler
	.thumb_set TIM2_IRQHandler,Default_Handler

	.weak	TIM3_IRQHandler
	.thumb_set TIM3_IRQHandler,Default_Handler

	.weak	TIM4_IRQHandler
	.thumb_set TIM4_IRQHandler,Default_Handler

	.weak	I2C1_EV_IRQHandler
	.thumb_set I2C1_EV_IRQHandler,Default_Handler

	.weak	I2C1_ER_IRQHandler
	.thumb_set I2C1_ER_IRQHandler,Default_Handler

	.weak	I2C2_EV_IRQHandler
	.thumb_set I2C2_EV_IRQHandler,Default_Handler

	.weak	I2C2_ER_IRQHandler
	.thumb_set I2C2_ER_IRQHandler,Default_Handler

	.weak	SPI1_IRQHandler
	.thumb_set SPI1_IRQHandler,Default_Handler

	.weak	SPI2_IRQHandler
	.thumb_set SPI2_IRQHandler,Default_Handler

	.weak	USART1_IRQHandler
	.thumb_set USART1_IRQHandler,Default_Handler

	.weak	USART2_IRQHandler
	.thumb_set USART2_IRQHandler,Default_Handler

	.weak	USART3_IRQHandler
	.thumb_set USART3_IRQHandler,Default_Handler

	.weak	EXTI15_10_IRQHandler
	.thumb_set EXTI15_10_IRQHandler,Default_Handler

	.weak	RTCAlarm_IRQHandler
	.thumb_set RTCAlarm_IRQHandler,Default_Handler

	.weak	TIM8_BRK_IRQHandler
	.thumb_set TIM8_BRK_IRQHandler,Default_Handler

	.weak	TIM8_UP_IRQHandler
	.thumb_set TIM8_UP_IRQHandler,Default_Handler

	.weak	TIM8_TRG_COM_IRQHandler
	.thumb_set TIM8_TRG_COM_IRQHandler,Default_Handler

	.weak	TIM8_CC_IRQHandler
	.thumb_set TIM8_CC_IRQHandler,Default_Handler

	.weak	ADC3_IRQHandler
	.thumb_set ADC3_IRQHandler,Default_Handler

	.weak	FSMC_IRQHandler
	.thumb_set FSMC_IRQHandler,Default_Handler

	.weak	SDIO_IRQHandler
	.thumb_set SDIO_IRQHandler,Default_Handler

	.weak	TIM5_IRQHandler
	.thumb_set TIM5_IRQHandler,Default_Handler

	.weak	SPI3_IRQHandler
	.thumb_set SPI3_IRQHandler,Default_Handler

	.weak	UART4_IRQHandler
	.thumb_set UART4_IRQHandler,Default_Handler

	.weak	UART5_IRQHandler
	.thumb_set UART5_IRQHandler,Default_Handler

	.weak	TIM6_IRQHandler
	.thumb_set TIM6_IRQHandler,Default_Handler

	.weak	TIM7_IRQHandler
	.thumb_set TIM7_IRQHandler,Default_Handler

	.weak	DMA2_Channel1_IRQHandler
	.thumb_set DMA2_Channel1_IRQHandler,Default_Handler

	.weak	DMA2_Channel2_IRQHandler
	.thumb_set DMA2_Channel2_IRQHandler,Default_Handler

	.weak	DMA2_Channel3_IRQHandler
	.thumb_set DMA2_Channel3_IRQHandler,Default_Handler

	.weak	DMA2_Channel4_5_IRQHandler
	.thumb_set DMA2_Channel4_5_IRQHandler,Default_Handler

	.weak	SystemInit

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
