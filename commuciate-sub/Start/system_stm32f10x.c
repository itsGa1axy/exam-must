/**
  ******************************************************************************
  * @file    system_stm32f10x.c
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    11-March-2011
  * @brief   CMSIS Cortex-M3 器件外设访问层系统源文件。
  *
  * 1.  本文件提供两个函数和一个全局变量，供用户应用程序调用：
  *      - SystemInit()：设置系统时钟（系统时钟源、PLL 倍频因子、
  *                      AHB/APBx 预分频器和 Flash 设置）。
  *                      本函数在启动时、复位之后、跳转到主程序之前被调用。
  *                      该调用在 "startup_stm32f10x_xx.s" 文件内部完成。
  *
  *      - SystemCoreClock 变量：包含内核时钟（HCLK），用户应用程序
  *                                 可用它来设置 SysTick 定时器或配置其它参数。
  *
  *      - SystemCoreClockUpdate()：更新 SystemCoreClock 变量，每当程序
  *                                 执行过程中内核时钟发生变化时都必须调用。
  *
  * 2. 每次器件复位后，HSI（8 MHz）被用作系统时钟源。随后在
  *    "startup_stm32f10x_xx.s" 文件中调用 SystemInit() 函数，
  *    在跳转到主程序之前配置系统时钟。
  *
  * 3. 如果用户选择的系统时钟源启动失败，SystemInit() 函数将不做任何
  *    处理，HSI 仍被用作系统时钟源。用户可以在 SetSysClock() 函数
  *    内部添加一些代码来处理该问题。
  *
  * 4. HSE 晶振的默认值设为 8 MHz（或 25 MHz，取决于所用产品），
  *    参见 "stm32f10x.h" 文件中的 "HSE_VALUE" 宏定义。
  *    当 HSE 直接或通过 PLL 用作系统时钟源，而所用的晶振不同时，
  *    必须根据自身配置调整 HSE 值。
  *
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
  
/** @addtogroup STM32F10x_System_Private_Includes  系统私有包含文件
  * @{
  */

#include "stm32f10x.h"

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_TypesDefinitions  系统私有类型定义
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_Defines  系统私有宏定义
  * @{
  */

/*!< 取消注释与所需系统时钟（SYSCLK）频率相对应的那一行
   （复位后 HSI 被用作 SYSCLK 源）

   重要说明：
   ==============
   1. 每次器件复位后，HSI 被用作系统时钟源。

   2. 请确保所选的系统时钟不超过器件的最高频率。

   3. 如果下面的宏定义一个都未使能，则 HSI 被用作系统时钟源。

   4. 本文件提供的系统时钟配置函数假定：
        - 对于小容量、中容量和大容量超值型器件，使用外部 8MHz
          晶振驱动系统时钟。
        - 对于小容量、中容量和大容量器件，使用外部 8MHz 晶振
          驱动系统时钟。
        - 对于互联型器件，使用外部 25MHz 晶振驱动
          系统时钟。
     如果所用的晶振不同，必须相应地调整这些函数。
    */
    
#if defined (STM32F10X_LD_VL) || (defined STM32F10X_MD_VL) || (defined STM32F10X_HD_VL)
/* #define SYSCLK_FREQ_HSE    HSE_VALUE */
 #define SYSCLK_FREQ_24MHz  24000000
#else
/* #define SYSCLK_FREQ_HSE    HSE_VALUE */
/* #define SYSCLK_FREQ_24MHz  24000000 */ 
/* #define SYSCLK_FREQ_36MHz  36000000 */
/* #define SYSCLK_FREQ_48MHz  48000000 */
/* #define SYSCLK_FREQ_56MHz  56000000 */
#define SYSCLK_FREQ_72MHz  72000000
#endif

/*!< 如果需要将安装在 STM3210E-EVAL 板（STM32 大容量和超大容量器件）
     或 STM32100E-EVAL 板（STM32 大容量超值型器件）上的外部 SRAM
     用作数据存储器，请取消注释下面这一行 */ 
#if defined (STM32F10X_HD) || (defined STM32F10X_XL) || (defined STM32F10X_HD_VL)
/* #define DATA_IN_ExtSRAM */
#endif

/*!< 如果需要将向量表重定位到内部 SRAM 中，
     请取消注释下面这一行。 */ 
/* #define VECT_TAB_SRAM */
#define VECT_TAB_OFFSET  0x0 /*!< 向量表基址偏移字段。
                                  该值必须是 0x200 的倍数。 */


/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_Macros  系统私有宏
  * @{
  */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_Variables  系统私有变量
  * @{
  */

/*******************************************************************************
*  时钟定义
*******************************************************************************/
#ifdef SYSCLK_FREQ_HSE
  uint32_t SystemCoreClock         = SYSCLK_FREQ_HSE;        /*!< 系统时钟频率（内核时钟） */
#elif defined SYSCLK_FREQ_24MHz
  uint32_t SystemCoreClock         = SYSCLK_FREQ_24MHz;        /*!< 系统时钟频率（内核时钟） */
#elif defined SYSCLK_FREQ_36MHz
  uint32_t SystemCoreClock         = SYSCLK_FREQ_36MHz;        /*!< 系统时钟频率（内核时钟） */
#elif defined SYSCLK_FREQ_48MHz
  uint32_t SystemCoreClock         = SYSCLK_FREQ_48MHz;        /*!< 系统时钟频率（内核时钟） */
#elif defined SYSCLK_FREQ_56MHz
  uint32_t SystemCoreClock         = SYSCLK_FREQ_56MHz;        /*!< 系统时钟频率（内核时钟） */
#elif defined SYSCLK_FREQ_72MHz
  uint32_t SystemCoreClock         = SYSCLK_FREQ_72MHz;        /*!< 系统时钟频率（内核时钟） */
#else /*!< 选择 HSI 作为系统时钟源 */
  uint32_t SystemCoreClock         = HSI_VALUE;        /*!< 系统时钟频率（内核时钟） */
#endif

__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_FunctionPrototypes  系统私有函数原型
  * @{
  */

static void SetSysClock(void);

#ifdef SYSCLK_FREQ_HSE
  static void SetSysClockToHSE(void);
#elif defined SYSCLK_FREQ_24MHz
  static void SetSysClockTo24(void);
#elif defined SYSCLK_FREQ_36MHz
  static void SetSysClockTo36(void);
#elif defined SYSCLK_FREQ_48MHz
  static void SetSysClockTo48(void);
#elif defined SYSCLK_FREQ_56MHz
  static void SetSysClockTo56(void);  
#elif defined SYSCLK_FREQ_72MHz
  static void SetSysClockTo72(void);
#endif

#ifdef DATA_IN_ExtSRAM
  static void SystemInit_ExtMemCtl(void); 
#endif /* DATA_IN_ExtSRAM */

/**
  * @}
  */

/** @addtogroup STM32F10x_System_Private_Functions  系统私有函数
  * @{
  */

/**
  * @brief  设置微控制器系统
  *         初始化嵌入式 Flash 接口、PLL，并更新
  *         SystemCoreClock 变量。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
void SystemInit (void)
{
  /* 将 RCC 时钟配置复位为默认复位状态（用于调试目的） */
  /* 设置 HSION 位 */
  RCC->CR |= (uint32_t)0x00000001;

  /* 复位 SW、HPRE、PPRE1、PPRE2、ADCPRE 和 MCO 位 */
#ifndef STM32F10X_CL
  RCC->CFGR &= (uint32_t)0xF8FF0000;
#else
  RCC->CFGR &= (uint32_t)0xF0FF0000;
#endif /* STM32F10X_CL */   
  
  /* 复位 HSEON、CSSON 和 PLLON 位 */
  RCC->CR &= (uint32_t)0xFEF6FFFF;

  /* 复位 HSEBYP 位 */
  RCC->CR &= (uint32_t)0xFFFBFFFF;

  /* 复位 PLLSRC、PLLXTPRE、PLLMUL 和 USBPRE/OTGFSPRE 位 */
  RCC->CFGR &= (uint32_t)0xFF80FFFF;

#ifdef STM32F10X_CL
  /* 复位 PLL2ON 和 PLL3ON 位 */
  RCC->CR &= (uint32_t)0xEBFFFFFF;

  /* 关闭所有中断并清除挂起位  */
  RCC->CIR = 0x00FF0000;

  /* 复位 CFGR2 寄存器 */
  RCC->CFGR2 = 0x00000000;
#elif defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || (defined STM32F10X_HD_VL)
  /* 关闭所有中断并清除挂起位  */
  RCC->CIR = 0x009F0000;

  /* 复位 CFGR2 寄存器 */
  RCC->CFGR2 = 0x00000000;      
#else
  /* 关闭所有中断并清除挂起位  */
  RCC->CIR = 0x009F0000;
#endif /* STM32F10X_CL */
    
#if defined (STM32F10X_HD) || (defined STM32F10X_XL) || (defined STM32F10X_HD_VL)
  #ifdef DATA_IN_ExtSRAM
    SystemInit_ExtMemCtl(); 
  #endif /* DATA_IN_ExtSRAM */
#endif 

  /* 配置系统时钟频率、HCLK、PCLK2 和 PCLK1 预分频器 */
  /* 配置 Flash 等待周期并使能预取缓冲区 */
  SetSysClock();

#ifdef VECT_TAB_SRAM
  SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* 在内部 SRAM 中重定位向量表。 */
#else
  SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* 在内部 FLASH 中重定位向量表。 */
#endif 
}

/**
  * @brief  根据时钟寄存器值更新 SystemCoreClock 变量。
  *         SystemCoreClock 变量包含内核时钟（HCLK），用户应用程序
  *         可用它来设置 SysTick 定时器或配置其它参数。
  *
  * @note   每当内核时钟（HCLK）发生变化时，都必须调用本函数来更新
  *         SystemCoreClock 变量的值。否则，任何基于该变量的配置
  *         都将不正确。
  *
  * @note   - 本函数计算出的系统频率并非芯片中的真实频率。它是根据
  *           预定义常量和所选的时钟源计算得出的：
  *
  *           - 如果 SYSCLK 源为 HSI，SystemCoreClock 将包含 HSI_VALUE(*)
  *
  *           - 如果 SYSCLK 源为 HSE，SystemCoreClock 将包含 HSE_VALUE(**)
  *
  *           - 如果 SYSCLK 源为 PLL，SystemCoreClock 将包含 HSE_VALUE(**)
  *             或 HSI_VALUE(*) 乘以 PLL 因子。
  *
  *         (*) HSI_VALUE 是 stm32f1xx.h 文件中定义的常量（默认值
  *             8 MHz），但真实值可能随电压和温度的变化而改变。
  *
  *         (**) HSE_VALUE 是 stm32f1xx.h 文件中定义的常量（默认值
  *              8 MHz 或 25 MHz，取决于所用产品），用户必须确保
  *              HSE_VALUE 与所用晶振的真实频率相同。
  *              否则本函数可能得到错误的结果。
  *
  *         - 当 HSE 晶振使用小数频率值时，
  *           本函数的结果可能不正确。
  * @param  无
  * @retval 无
  */
void SystemCoreClockUpdate (void)
{
  uint32_t tmp = 0, pllmull = 0, pllsource = 0;

#ifdef  STM32F10X_CL
  uint32_t prediv1source = 0, prediv1factor = 0, prediv2factor = 0, pll2mull = 0;
#endif /* STM32F10X_CL */

#if defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || (defined STM32F10X_HD_VL)
  uint32_t prediv1factor = 0;
#endif /* STM32F10X_LD_VL or STM32F10X_MD_VL or STM32F10X_HD_VL */
    
  /* 获取 SYSCLK 源 -------------------------------------------------------*/
  tmp = RCC->CFGR & RCC_CFGR_SWS;
  
  switch (tmp)
  {
    case 0x00:  /* HSI 用作系统时钟 */
      SystemCoreClock = HSI_VALUE;
      break;
    case 0x04:  /* HSE 用作系统时钟 */
      SystemCoreClock = HSE_VALUE;
      break;
    case 0x08:  /* PLL 用作系统时钟 */

      /* 获取 PLL 时钟源和倍频系数 ----------------------*/
      pllmull = RCC->CFGR & RCC_CFGR_PLLMULL;
      pllsource = RCC->CFGR & RCC_CFGR_PLLSRC;
      
#ifndef STM32F10X_CL      
      pllmull = ( pllmull >> 18) + 2;
      
      if (pllsource == 0x00)
      {
        /* 选择 HSI 振荡器时钟除以 2 作为 PLL 时钟输入 */
        SystemCoreClock = (HSI_VALUE >> 1) * pllmull;
      }
      else
      {
 #if defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || (defined STM32F10X_HD_VL)
       prediv1factor = (RCC->CFGR2 & RCC_CFGR2_PREDIV1) + 1;
       /* 选择 HSE 振荡器时钟作为 PREDIV1 时钟输入 */
       SystemCoreClock = (HSE_VALUE / prediv1factor) * pllmull; 
 #else
        /* 选择 HSE 作为 PLL 时钟输入 */
        if ((RCC->CFGR & RCC_CFGR_PLLXTPRE) != (uint32_t)RESET)
        {/* HSE 振荡器时钟除以 2 */
          SystemCoreClock = (HSE_VALUE >> 1) * pllmull;
        }
        else
        {
          SystemCoreClock = HSE_VALUE * pllmull;
        }
 #endif
      }
#else
      pllmull = pllmull >> 18;
      
      if (pllmull != 0x0D)
      {
         pllmull += 2;
      }
      else
      { /* PLL 倍频系数 = PLL 输入时钟 * 6.5 */
        pllmull = 13 / 2; 
      }
            
      if (pllsource == 0x00)
      {
        /* 选择 HSI 振荡器时钟除以 2 作为 PLL 时钟输入 */
        SystemCoreClock = (HSI_VALUE >> 1) * pllmull;
      }
      else
      {/* 选择 PREDIV1 作为 PLL 时钟输入 */
        
        /* 获取 PREDIV1 时钟源和分频系数 */
        prediv1source = RCC->CFGR2 & RCC_CFGR2_PREDIV1SRC;
        prediv1factor = (RCC->CFGR2 & RCC_CFGR2_PREDIV1) + 1;
        
        if (prediv1source == 0)
        { 
          /* 选择 HSE 振荡器时钟作为 PREDIV1 时钟输入 */
          SystemCoreClock = (HSE_VALUE / prediv1factor) * pllmull;          
        }
        else
        {/* 选择 PLL2 时钟作为 PREDIV1 时钟输入 */
          
          /* 获取 PREDIV2 分频系数和 PLL2 倍频系数 */
          prediv2factor = ((RCC->CFGR2 & RCC_CFGR2_PREDIV2) >> 4) + 1;
          pll2mull = ((RCC->CFGR2 & RCC_CFGR2_PLL2MUL) >> 8 ) + 2; 
          SystemCoreClock = (((HSE_VALUE / prediv2factor) * pll2mull) / prediv1factor) * pllmull;                         
        }
      }
#endif /* STM32F10X_CL */ 
      break;

    default:
      SystemCoreClock = HSI_VALUE;
      break;
  }
  
  /* 计算 HCLK 时钟频率 ----------------*/
  /* 获取 HCLK 预分频器 */
  tmp = AHBPrescTable[((RCC->CFGR & RCC_CFGR_HPRE) >> 4)];
  /* HCLK 时钟频率 */
  SystemCoreClock >>= tmp;  
}

/**
  * @brief  配置系统时钟频率、HCLK、PCLK2 和 PCLK1 预分频器。
  * @param  无
  * @retval 无
  */
static void SetSysClock(void)
{
#ifdef SYSCLK_FREQ_HSE
  SetSysClockToHSE();
#elif defined SYSCLK_FREQ_24MHz
  SetSysClockTo24();
#elif defined SYSCLK_FREQ_36MHz
  SetSysClockTo36();
#elif defined SYSCLK_FREQ_48MHz
  SetSysClockTo48();
#elif defined SYSCLK_FREQ_56MHz
  SetSysClockTo56();  
#elif defined SYSCLK_FREQ_72MHz
  SetSysClockTo72();
#endif
 
 /* 如果上面的宏定义一个都未使能，则 HSI 被用作系统时钟源
   （复位后的默认值） */ 
}

/**
  * @brief  设置外部存储器控制器。在 startup_stm32f10x.s 中
  *          跳转到 __main 之前被调用
  * @param  无
  * @retval 无
  */ 
#ifdef DATA_IN_ExtSRAM
/**
  * @brief  设置外部存储器控制器。
  *         在 startup_stm32f10x_xx.s/.c 中跳转到 main 之前被调用。
  * 	      本函数配置安装在 STM3210E-EVAL 板上的外部 SRAM
  *         （STM32 大容量器件）。该 SRAM 将用作程序数据存储器
  *         （包括堆和栈）。
  * @param  无
  * @retval 无
  */ 
void SystemInit_ExtMemCtl(void) 
{
/*!< STM3210E-EVAL 使用 FSMC Bank1 NOR/SRAM3，如果需要使用其它 Bank，
  则需调整寄存器地址 */

  /* 使能 FSMC 时钟 */
  RCC->AHBENR = 0x00000114;
  
  /* 使能 GPIOD、GPIOE、GPIOF 和 GPIOG 时钟 */  
  RCC->APB2ENR = 0x000001E0;
  
/* ---------------  SRAM 数据线、NOE 和 NWE 配置 ---------------*/
/*----------------  SRAM 地址线配置 -------------------------*/
/*----------------  NOE 和 NWE 配置 --------------------------------*/  
/*----------------  NE3 配置 ----------------------------------------*/
/*----------------  NBL0、NBL1 配置 ---------------------------------*/
  
  GPIOD->CRL = 0x44BB44BB;  
  GPIOD->CRH = 0xBBBBBBBB;

  GPIOE->CRL = 0xB44444BB;  
  GPIOE->CRH = 0xBBBBBBBB;

  GPIOF->CRL = 0x44BBBBBB;  
  GPIOF->CRH = 0xBBBB4444;

  GPIOG->CRL = 0x44BBBBBB;  
  GPIOG->CRH = 0x44444B44;
   
/*----------------  FSMC 配置 ---------------------------------------*/  
/*----------------  使能 FSMC Bank1_SRAM Bank ------------------------------*/
  
  FSMC_Bank1->BTCR[4] = 0x00001011;
  FSMC_Bank1->BTCR[5] = 0x00000200;
}
#endif /* DATA_IN_ExtSRAM */

#ifdef SYSCLK_FREQ_HSE
/**
  * @brief  选择 HSE 作为系统时钟源，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockToHSE(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/    
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {

#if !defined STM32F10X_LD_VL && !defined STM32F10X_MD_VL && !defined STM32F10X_HD_VL
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 0 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);

#ifndef STM32F10X_CL
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_0;
#else
    if (HSE_VALUE <= 24000000)
	{
      FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_0;
	}
	else
	{
      FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_1;
	}
#endif /* STM32F10X_CL */
#endif
 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV1;
    
    /* 选择 HSE 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_HSE;    

    /* 等待直到 HSE 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x04)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  }  
}
#elif defined SYSCLK_FREQ_24MHz
/**
  * @brief  将系统时钟频率设为 24MHz，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockTo24(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/    
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {
#if !defined STM32F10X_LD_VL && !defined STM32F10X_MD_VL && !defined STM32F10X_HD_VL 
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 0 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_0;    
#endif
 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV1;
    
#ifdef STM32F10X_CL
    /* 配置 PLL ------------------------------------------------------*/
    /* PLL 配置：PLLCLK = PREDIV1 * 6 = 24 MHz */ 
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
                            RCC_CFGR_PLLMULL6); 

    /* PLL2 配置：PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 配置：PREDIV1CLK = PLL2 / 10 = 4 MHz */       
    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV10);
  
    /* 使能 PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* 等待直到 PLL2 就绪 */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }   
#elif defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || defined (STM32F10X_HD_VL)
    /*  PLL 配置： = (HSE / 2) * 6 = 24 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1_Div2 | RCC_CFGR_PLLMULL6);
#else    
    /*  PLL 配置： = (HSE / 2) * 6 = 24 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLXTPRE_HSE_Div2 | RCC_CFGR_PLLMULL6);
#endif /* STM32F10X_CL */

    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* 等待直到 PLL 就绪 */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* 选择 PLL 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

    /* 等待直到 PLL 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  } 
}
#elif defined SYSCLK_FREQ_36MHz
/**
  * @brief  将系统时钟频率设为 36MHz，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockTo36(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/    
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 1 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_1;    
 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV1;
    
#ifdef STM32F10X_CL
    /* 配置 PLL ------------------------------------------------------*/
    
    /* PLL 配置：PLLCLK = PREDIV1 * 9 = 36 MHz */ 
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
                            RCC_CFGR_PLLMULL9); 

	/*!< PLL2 配置：PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 配置：PREDIV1CLK = PLL2 / 10 = 4 MHz */
        
    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV10);
  
    /* 使能 PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* 等待直到 PLL2 就绪 */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }
    
#else    
    /*  PLL 配置：PLLCLK = (HSE / 2) * 9 = 36 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLXTPRE_HSE_Div2 | RCC_CFGR_PLLMULL9);
#endif /* STM32F10X_CL */

    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* 等待直到 PLL 就绪 */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* 选择 PLL 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

    /* 等待直到 PLL 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  } 
}
#elif defined SYSCLK_FREQ_48MHz
/**
  * @brief  将系统时钟频率设为 48MHz，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockTo48(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/    
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 1 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_1;    
 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;
    
#ifdef STM32F10X_CL
    /* 配置 PLL ------------------------------------------------------*/
    /* PLL2 配置：PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 配置：PREDIV1CLK = PLL2 / 5 = 8 MHz */
        
    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV5);
  
    /* 使能 PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* 等待直到 PLL2 就绪 */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }
    
   
    /* PLL 配置：PLLCLK = PREDIV1 * 6 = 48 MHz */ 
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
                            RCC_CFGR_PLLMULL6); 
#else    
    /*  PLL 配置：PLLCLK = HSE * 6 = 48 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL6);
#endif /* STM32F10X_CL */

    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* 等待直到 PLL 就绪 */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* 选择 PLL 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

    /* 等待直到 PLL 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  } 
}

#elif defined SYSCLK_FREQ_56MHz
/**
  * @brief  将系统时钟频率设为 56MHz，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockTo56(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/   
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 2 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;    
 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

#ifdef STM32F10X_CL
    /* 配置 PLL ------------------------------------------------------*/
    /* PLL2 配置：PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 配置：PREDIV1CLK = PLL2 / 5 = 8 MHz */
        
    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV5);
  
    /* 使能 PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* 等待直到 PLL2 就绪 */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }
    
   
    /* PLL 配置：PLLCLK = PREDIV1 * 7 = 56 MHz */ 
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
                            RCC_CFGR_PLLMULL7); 
#else     
    /* PLL 配置：PLLCLK = HSE * 7 = 56 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL7);

#endif /* STM32F10X_CL */

    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* 等待直到 PLL 就绪 */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }

    /* 选择 PLL 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

    /* 等待直到 PLL 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  } 
}

#elif defined SYSCLK_FREQ_72MHz
/**
  * @brief  将系统时钟频率设为 72MHz，并配置 HCLK、PCLK2
  *         和 PCLK1 预分频器。
  * @note   本函数只应在复位之后使用。
  * @param  无
  * @retval 无
  */
static void SetSysClockTo72(void)
{
  __IO uint32_t StartUpCounter = 0, HSEStatus = 0;
  
  /* SYSCLK、HCLK、PCLK2 和 PCLK1 配置 ---------------------------*/    
  /* 使能 HSE */    
  RCC->CR |= ((uint32_t)RCC_CR_HSEON);
 
  /* 等待 HSE 就绪，如果达到超时则退出 */
  do
  {
    HSEStatus = RCC->CR & RCC_CR_HSERDY;
    StartUpCounter++;  
  } while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

  if ((RCC->CR & RCC_CR_HSERDY) != RESET)
  {
    HSEStatus = (uint32_t)0x01;
  }
  else
  {
    HSEStatus = (uint32_t)0x00;
  }  

  if (HSEStatus == (uint32_t)0x01)
  {
    /* 使能预取缓冲区 */
    FLASH->ACR |= FLASH_ACR_PRFTBE;

    /* Flash 2 等待周期 */
    FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
    FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;    

 
    /* HCLK = SYSCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;
      
    /* PCLK2 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;
    
    /* PCLK1 = HCLK */
    RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

#ifdef STM32F10X_CL
    /* 配置 PLL ------------------------------------------------------*/
    /* PLL2 配置：PLL2CLK = (HSE / 5) * 8 = 40 MHz */
    /* PREDIV1 配置：PREDIV1CLK = PLL2 / 5 = 8 MHz */
        
    RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
                              RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
    RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
                             RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV5);
  
    /* 使能 PLL2 */
    RCC->CR |= RCC_CR_PLL2ON;
    /* 等待直到 PLL2 就绪 */
    while((RCC->CR & RCC_CR_PLL2RDY) == 0)
    {
    }
    
   
    /* PLL 配置：PLLCLK = PREDIV1 * 9 = 72 MHz */ 
    RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
                            RCC_CFGR_PLLMULL9); 
#else    
    /*  PLL 配置：PLLCLK = HSE * 9 = 72 MHz */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
                                        RCC_CFGR_PLLMULL));
    RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);
#endif /* STM32F10X_CL */

    /* 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;

    /* 等待直到 PLL 就绪 */
    while((RCC->CR & RCC_CR_PLLRDY) == 0)
    {
    }
    
    /* 选择 PLL 作为系统时钟源 */
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

    /* 等待直到 PLL 被用作系统时钟源 */
    while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08)
    {
    }
  }
  else
  { /* 如果 HSE 启动失败，应用程序将获得错误的时钟
         配置。用户可以在此处添加代码来处理该错误 */
  }
}
#endif

/**
  * @}
  */

/**
  * @}
  */
  
/**
  * @}
  */    
/******************* (C) COPYRIGHT 2011 STMicroelectronics *****文件结束****/
