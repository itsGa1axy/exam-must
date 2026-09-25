#ifndef BOARD_TRANSPORT_H
#define BOARD_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "stm32f10x.h"

/* USART1 460800 8N1：接收中断入队，发送由 DMA 完成。 */
void BoardTransport_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                         GPIO_TypeDef *rx_port, uint16_t rx_pin,
                         FunctionalState remap_usart1);
int BoardTransport_Send(const uint8_t *data, size_t length);
int BoardTransport_ReadByte(uint8_t *byte);
void BoardTransport_RxIrqHandler(void);
void BoardTransport_TxDmaIrqHandler(void);

#endif
