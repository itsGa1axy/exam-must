#ifndef BOARD_TRANSPORT_H
#define BOARD_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>

/* USART1 460800 8N1：接收中断入队，发送由 DMA 完成。 */
void BoardTransport_Init(void);
int BoardTransport_Send(const uint8_t *data, size_t length);
int BoardTransport_ReadByte(uint8_t *byte);
void BoardTransport_RxIrqHandler(void);
void BoardTransport_TxDmaIrqHandler(void);

#endif
