/**
  ******************************************************************************
  * @file    system_stm32f10x.h
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    11-March-2011
  * @brief   CMSIS Cortex-M3 器件外设访问层系统头文件。
  ******************************************************************************
  * @attention
  *
  * 本固件仅供指导之用，其唯一目的是向客户提供有关其产品的编码信息，
  * 以便客户节省时间。因此，对于因本固件内容和/或客户将本文所含编码
  * 信息用于其产品而产生的任何索赔所导致的任何直接、间接或后果性
  * 损害，STMicroelectronics 概不承担责任。
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/** @addtogroup CMSIS   CMSIS 分组
  * @{
  */

/** @addtogroup stm32f10x_system  系统
  * @{
  */  
  
/**
  * @brief 用于防止重复包含的宏定义
  */
#ifndef __SYSTEM_STM32F10X_H
#define __SYSTEM_STM32F10X_H

#ifdef __cplusplus
 extern "C" {
#endif 

/** @addtogroup STM32F10x_System_Includes  系统包含文件
  * @{
  */

/**
  * @}
  */


/** @addtogroup STM32F10x_System_Exported_types  系统导出类型
  * @{
  */

extern uint32_t SystemCoreClock;          /*!< 系统时钟频率（内核时钟） */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Exported_Constants  系统导出常量
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Exported_Macros  系统导出宏
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Exported_Functions  系统导出函数
  * @{
  */
  
extern void SystemInit(void);
extern void SystemCoreClockUpdate(void);
/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /*__SYSTEM_STM32F10X_H */

/**
  * @}
  */
  
/**
  * @}
  */  
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****文件结束****/
