#include "stm32u585xx.h"
#include "stm32u5xx_hal.h"
#include "gpio.h"

float temperature_threshold = 25.0;  // 设定温度阈值

void Heating_Set (float temperature)
{
    temperature_threshold = temperature;
}

void Heating_ON (void)
{
    // 这里可以添加实际的加热控制代码，例如控制GPIO引脚
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);  // PB0控制加热器
}

void Heating_OFF (void)
{
    // 这里可以添加实际的加热控制代码，例如控制GPIO引脚
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);  // PB0控制加热器
}

void Heating_Control (float current_temperature)
{
    if (temperature_threshold - 0.5 > current_temperature)  // 如果当前温度低于设定阈值0.5度以上，开启加热
    {
        Heating_ON();
    }
    else
    {
        Heating_OFF();
    }
}
