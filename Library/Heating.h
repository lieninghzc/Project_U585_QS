#ifndef __HEATING_H
#define __HEATING_H

#include <stdint.h>

void Heating_Init (void);
void Heating_Set (float temperature);
void Heating_ON (void);
void Heating_OFF (void);
void Heating_Control (float current_temperature);
uint8_t Heating_GetState (void);   /* 返回 1=加热中, 0=关闭 */
float Heating_GetThreshold (void); /* 返回当前设定阈值 */

#endif /* __HEATING_H */
