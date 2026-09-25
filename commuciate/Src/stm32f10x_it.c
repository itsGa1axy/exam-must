/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   主中断服务例程。
  *          本文件为所有异常处理函数和外设中断服务
  *          例程提供模板。
  ******************************************************************************
  * @attention
  *
  * 本固件仅供指导之用，旨在为客户提供与其产品相关的编码信息，
  * 以帮助客户节省时间。因此，对于因本固件的内容和/或客户将其中所含
  * 编码信息用于其产品而产生的任何索赔所导致的任何直接、间接或
  * 后果性损害，STMicroelectronics 概不承担任何责任。
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* 头文件包含 ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "board_link.h"
#include "pc_console.h"
#include "host_log.h"

/** @addtogroup STM32F10x_StdPeriph_Template  STM32F10x 标准外设库模板
  * @{
  */

/* 私有类型定义 -----------------------------------------------------------*/
/* 私有宏定义 ------------------------------------------------------------*/
/* 私有宏 -------------------------------------------------------------*/
/* 私有变量 ---------------------------------------------------------*/
/* 私有函数原型 -----------------------------------------------*/
/* 私有函数 ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 处理器异常处理函数                         */
/******************************************************************************/

/**
  * @brief  本函数处理 NMI 异常。
  * @param  无
  * @retval 无
  */
void NMI_Handler(void)
{
}

/**
  * @brief  本函数处理 Hard Fault 异常。
  * @param  无
  * @retval 无
  */
void HardFault_Handler(void)
{
  /* 发生 Hard Fault 异常时进入无限循环 */
  while (1)
  {
  }
}

/**
  * @brief  本函数处理 Memory Manage 异常。
  * @param  无
  * @retval 无
  */
void MemManage_Handler(void)
{
  /* 发生 Memory Manage 异常时进入无限循环 */
  while (1)
  {
  }
}

/**
  * @brief  本函数处理 Bus Fault 异常。
  * @param  无
  * @retval 无
  */
void BusFault_Handler(void)
{
  /* 发生 Bus Fault 异常时进入无限循环 */
  while (1)
  {
  }
}

/**
  * @brief  本函数处理 Usage Fault 异常。
  * @param  无
  * @retval 无
  */
void UsageFault_Handler(void)
{
  /* 发生 Usage Fault 异常时进入无限循环 */
  while (1)
  {
  }
}

/**
  * @brief  本函数处理 SVCall 异常。
  * @param  无
  * @retval 无
  */
void SVC_Handler(void)
{
}

/**
  * @brief  本函数处理 Debug Monitor 异常。
  * @param  无
  * @retval 无
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  本函数处理 PendSVC 异常。
  * @param  无
  * @retval 无
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  本函数处理 SysTick 处理函数。
  * @param  无
  * @retval 无
  */
void SysTick_Handler(void)
{
  HostLog_Tick1ms();
}

/* USART 接收中断只搬运字节，帧解析和 CRC 在主循环完成。 */
void USART1_IRQHandler(void)
{
  BoardLink_RxIrqHandler();
}

void USART2_IRQHandler(void)
{
  PcConsole_RxIrqHandler();
}

/* USART2 的 DMA 完成中断启动队列里的下一帧。 */
void DMA1_Channel7_IRQHandler(void)
{
  PcConsole_TxDmaIrqHandler();
}

/******************************************************************************/
/*                 STM32F10x 外设中断处理函数                   */
/*  在此添加所用外设（PPP）的中断处理函数，可用的  */
/*  外设中断处理函数名称请参阅启动 */
/*  文件（startup_stm32f10x_xx.s）。                                            */
/******************************************************************************/

/**
  * @brief  本函数处理 PPP 中断请求。
  * @param  无
  * @retval 无
  */
/*void PPP_IRQHandler(void)
{
}*/

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****文件结束****/
