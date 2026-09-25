/**
 ******************************************************************************
 * @file      sysmem.c
 * @author    由 STM32CubeIDE for Visual Studio Code 扩展自动生成
 * @brief     GCC 系统内存调用文件
 *
 *            有关哪些 C 函数需要哪些底层函数的
 *            更多信息，请查阅 Newlib 或 Picolibc 的 libc 手册
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

/* 包含头文件 */
#include <errno.h>
#include <stdint.h>
#include <stddef.h>

/**
 * 指向当前堆使用水位高点的指针
 */
static uint8_t *__sbrk_heap_end = NULL;

/**
 * @brief _sbrk() 为 newlib 堆分配内存，供 C 库的 malloc
 *        及其他函数使用
 *
 * @verbatim
 * ############################################################################
 * #  .data  #  .bss  #       newlib heap       #          MSP stack          #
 * #         #        #                         # Reserved by _Min_Stack_Size #
 * ############################################################################
 * ^-- RAM start      ^-- _end                             _estack, RAM end --^
 * @endverbatim
 *
 * 本实现从 '_end' 链接符号处开始分配内存
 * '_Min_Stack_Size' 链接符号为 MSP 栈保留内存
 * 本实现将 '_estack' 链接符号视为 RAM 末尾
 * 注意：如果 MSP 栈在执行过程中任意时刻增长到超过所
 * 保留的大小，请增大 '_Min_Stack_Size'。
 *
 * @param incr 内存大小
 * @return 指向已分配内存的指针
 */
void *_sbrk(ptrdiff_t incr)
{
  extern uint8_t _end; /* 在链接脚本中定义的符号 */
  extern uint8_t _estack; /* 在链接脚本中定义的符号 */
  extern uint32_t _Min_Stack_Size; /* 在链接脚本中定义的符号 */
  const uint32_t stack_limit = (uint32_t)&_estack - (uint32_t)&_Min_Stack_Size;
  const uint8_t *max_heap = (uint8_t *)stack_limit;
  uint8_t *prev_heap_end;

  /* 首次调用时初始化堆末尾 */
  if (NULL == __sbrk_heap_end)
  {
    __sbrk_heap_end = &_end;
  }

  /* 防止堆增长到保留的 MSP 栈中 */
  if (__sbrk_heap_end + incr > max_heap)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  prev_heap_end = __sbrk_heap_end;
  __sbrk_heap_end += incr;

  return (void *)prev_heap_end;
}

#if defined(__PICOLIBC__)
  // Picolibc 期望系统调用不带前导下划线。
  // 这会创建一个强别名，使得
  // 对 `sbrk()` 的调用解析到我们的 `_sbrk()` 实现。
  __strong_reference(_sbrk, sbrk);
#endif
