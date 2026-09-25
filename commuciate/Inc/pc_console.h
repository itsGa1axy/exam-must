#ifndef PC_CONSOLE_H
#define PC_CONSOLE_H

#include "stm32f10x.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* 初始化连接 USB-TTL 的 USART2；引脚由 main.c 配置并传入。 */
void PcConsole_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart2);
/* 将已校验的 29 字节姿态帧交给 USART2 DMA 发送；队列满时返回 false。 */
bool PcConsole_SendFrame(const uint8_t *frame, size_t length);
/* 调试用文本输出；数据模式不得调用，以免与二进制姿态帧混流。 */
void PcConsole_WriteLine(const char *line);
/* 主循环解析电脑发来的 K,0.98 命令。 */
void PcConsole_Process(void);
bool PcConsole_TakeFilterWeight(uint16_t *gyro_weight_q15);
void PcConsole_RxIrqHandler(void);
void PcConsole_TxDmaIrqHandler(void);

#endif
