#include "stm32u585xx.h"
#include "stm32u5xx_hal.h"
#include "gpio.h"

static float temperature_threshold = 25.0f; /* 设定温度阈值 */
static uint8_t heating_state = 0;           /* 0=关闭, 1=加热中 */

/* 初始化加热控制引脚 PB0 */
void Heating_Init (void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PB0 推挽输出, 初始低电平(关闭加热) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Heating_Set (float temperature)
{
    temperature_threshold = temperature;
}

void Heating_ON (void)
{
    heating_state = 1;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

void Heating_OFF (void)
{
    heating_state = 0;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

void Heating_Control (float current_temperature)
{
    /* 滞回控制: 低于阈值0.5°C开启, 距阈值0.1°C提前关闭 (利用热惯性) */
    if (temperature_threshold - 0.5f > current_temperature)
    {
        Heating_ON();
    }
    else if (current_temperature > temperature_threshold - 0.1f)
    {
        Heating_OFF();
    }
    /* 在 [threshold-0.5, threshold-0.1] 区间保持原状态 (滞回) */
}

uint8_t Heating_GetState (void)
{
    return heating_state;
}

float Heating_GetThreshold (void)
{
    return temperature_threshold;
}
