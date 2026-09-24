#ifndef BOARD_LINK_H
#define BOARD_LINK_H

#include "attitude.h"
#include <stdint.h>

void BoardLink_Init(void);
void BoardLink_Process(void);
int BoardLink_SendAttitude(const Attitude_Result *attitude);
void BoardLink_RxIrqHandler(void);
void BoardLink_TxDmaIrqHandler(void);

#endif
