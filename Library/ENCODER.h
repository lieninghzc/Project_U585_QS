#ifndef __ENCODER_H
#define __ENCODER_H

#include "tim.h"

void ENCODER_Init (void);
int32_t ENCODER_GetPosition (void);
int32_t ENCODER_GetDelta (void);

#endif
