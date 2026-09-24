#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include "attitude.h"
#include <stdint.h>
#include "stm32f10x.h"

void BoardLink_Init(GPIO_TypeDef *tx_port, uint16_t tx_pin,
                    GPIO_TypeDef *rx_port, uint16_t rx_pin,
                    FunctionalState remap_usart1);
void BoardLink_Process(void);
int BoardLink_SendAttitude(const Attitude_Result *attitude);
void BoardLink_RxIrqHandler(void);
void BoardLink_TxDmaIrqHandler(void);

#endif
