#ifndef PC_CONSOLE_H
#define PC_CONSOLE_H

#include "stm32f10x.h"
#include "board_protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* 初始化连接 USB-TTL 的 USART2；引脚由 main.c 配置并传入。 */
void PcConsole_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart2);
/* 将 CRC 帧原样转发到电脑；电脑端负责解码四元数并计算欧拉角。 */
void PcConsole_SendAttitude(uint16_t sequence,
                            uint32_t timestamp_ms,
                            const BoardProtocol_Quaternion *quaternion);
/* 主循环解析电脑发来的 K,0.98 命令。 */
void PcConsole_Process(void);
bool PcConsole_TakeFilterWeight(uint16_t *gyro_weight_q15);
void PcConsole_RxIrqHandler(void);

#endif
