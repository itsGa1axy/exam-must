/**
 ******************************************************************************
 * @file      syscalls.c
 * @author    由 STM32CubeIDE for Visual Studio Code 扩展自动生成
 * @brief     最小系统调用文件
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
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/times.h>


/* 变量 */
extern int __io_putchar(int ch) __attribute__((weak));
extern int __io_getchar(void) __attribute__((weak));


char *__env[1] = { 0 };
char **environ = __env;


/* 函数 */
void initialise_monitor_handles(void)
{
}

int _getpid(void)
{
  return 1;
}

int _kill(int pid, int sig)
{
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

void _exit (int status)
{
  _kill(status, -1);
  while (1) {}    /* 确保在此处挂起 */
}

__attribute__((weak)) int _read(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    *ptr++ = __io_getchar();
  }

  return len;
}

__attribute__((weak)) int _write(int file, char *ptr, int len)
{
  (void)file;
  int DataIdx;

  for (DataIdx = 0; DataIdx < len; DataIdx++)
  {
    __io_putchar(*ptr++);
  }
  return len;
}

int _close(int file)
{
  (void)file;
  return -1;
}


int _fstat(int file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _isatty(int file)
{
  (void)file;
  return 1;
}

int _lseek(int file, int ptr, int dir)
{
  (void)file;
  (void)ptr;
  (void)dir;
  return 0;
}

int _open(char *path, int flags, ...)
{
  (void)path;
  (void)flags;
  /* 假装总是失败 */
  return -1;
}

int _wait(int *status)
{
  (void)status;
  errno = ECHILD;
  return -1;
}

int _unlink(char *name)
{
  (void)name;
  errno = ENOENT;
  return -1;
}

clock_t _times(struct tms *buf)
{
  (void)buf;
  return -1;
}

int _stat(const char *file, struct stat *st)
{
  (void)file;
  st->st_mode = S_IFCHR;
  return 0;
}

int _link(char *old, char *new)
{
  (void)old;
  (void)new;
  errno = EMLINK;
  return -1;
}

int _fork(void)
{
  errno = EAGAIN;
  return -1;
}

int _execve(char *name, char **argv, char **env)
{
  (void)name;
  (void)argv;
  (void)env;
  errno = ENOMEM;
  return -1;
}

// --- Picolibc 专用部分 ---
#if defined(__PICOLIBC__)

/**
 * @brief Picolibc 辅助函数，用于向 FILE 流输出一个字符。
 *        它将输出重定向到低层的 __io_putchar 函数。
 * @param c 要写入的字符。
 * @param file FILE 流指针（忽略）。
 * @retval int 写入的字符。
 */
static int starm_putc(char c, FILE *file)
{
	(void) file;
  __io_putchar(c);
	return c;
}

/**
 * @brief Picolibc 辅助函数，用于从 FILE 流输入一个字符。
 *        它将输入重定向到低层的 __io_getchar 函数。
 * @param file FILE 流指针（忽略）。
 * @retval int 读取的字符，先转换为 unsigned char 再转换为 int。
 */
static int starm_getc(FILE *file)
{
	unsigned char c;
	(void) file;
  c = __io_getchar();
	return c;
}

// 为 Picolibc 定义并初始化标准 I/O 流。
// FDEV_SETUP_STREAM 将 starm_putc 和 starm_getc 辅助函数连接到 FILE 结构。
// _FDEV_SETUP_RW 表示该流用于读取和写入。
static FILE __stdio = FDEV_SETUP_STREAM(starm_putc,
					starm_getc,
					NULL,
					_FDEV_SETUP_RW);

// 将标准流指针（stdin、stdout、stderr）赋给已初始化的流。
// Picolibc 使用这些指针进行标准 I/O 操作（printf、scanf 等）。
FILE *const stdin = &__stdio;
__strong_reference(stdin, stdout);
__strong_reference(stdin, stderr);

// 创建强别名，将标准 C 库函数名（不带下划线）
// 映射到已实现的系统调用桩（带下划线）。Picolibc 在内部使用这些
// 标准名称，因此需要此链接。
__strong_reference(_read, read);
__strong_reference(_write, write);
__strong_reference(_times, times);
__strong_reference(_execve, execve);
__strong_reference(_fork, fork);
__strong_reference(_link, link);
__strong_reference(_unlink, unlink);
__strong_reference(_stat, stat);
__strong_reference(_wait, wait);
__strong_reference(_open, open);
__strong_reference(_close, close);
__strong_reference(_lseek, lseek);
__strong_reference(_isatty, isatty);
__strong_reference(_fstat, fstat);
__strong_reference(_exit, exit);
__strong_reference(_kill, kill);
__strong_reference(_getpid, getpid);

#endif //__PICOLIBC__
