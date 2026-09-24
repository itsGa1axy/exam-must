#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include "board_protocol.h"
#include "stm32f10x.h"
#include <stdbool.h>

typedef struct
{
    uint16_t sequence;
    uint32_t sample_time_ms;
    BoardProtocol_Quaternion quaternion;
} BoardLink_Attitude;

typedef struct
{
    uint32_t valid_frames;
    uint32_t crc_errors;
    uint32_t invalid_frames;
    uint32_t rx_overflows;
    uint32_t retransmit_requests;
} BoardLink_Stats;

/* 初始化 USART1；端口和引脚由 main.c 中的宏传入。 */
void BoardLink_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart1);
/* 主循环调用：取出中断接收的字节、组帧并检查 CRC。 */
void BoardLink_Process(void);
bool BoardLink_GetLatest(BoardLink_Attitude *attitude);
void BoardLink_GetStats(BoardLink_Stats *stats);
/* 经 USART1 向从机发送新的陀螺仪权重，Q15 范围为 0 到 32767。 */
int BoardLink_SendFilterWeight(uint16_t gyro_weight_q15);
/* USART1 中断只负责读取数据寄存器并将字节放入环形缓冲区。 */
void BoardLink_RxIrqHandler(void);

#endif
