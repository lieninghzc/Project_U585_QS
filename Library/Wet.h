#ifndef __WET_H
#define __WET_H

#include <stdint.h>

void Wet_Init (void);
void Wet_Set (float humidity);
void Wet_ON (void);
void Wet_OFF (void);
void Wet_Control (float current_humidity);
uint8_t Wet_GetState (void);   /* 返回 1=加湿中, 0=关闭 */
float Wet_GetThreshold (void); /* 返回当前设定湿度阈值 */

#endif /* __WET_H */
