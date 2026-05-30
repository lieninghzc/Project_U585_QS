#ifndef __HEATING_H
#define __HEATING_H

//加热具体控制方式待改变

void Heating_Set (float temperature);
void Heating_ON (void);
void Heating_OFF (void);
void Heating_Control (float current_temperature);

#endif /* __HEATING_H */
