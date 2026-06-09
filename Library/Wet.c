#include "stm32u585xx.h"
#include "stm32u5xx_hal.h"
#include "gpio.h"

static float humidity_threshold = 60.0f; /* 设定湿度阈值 %RH */
static uint8_t wet_state = 0;            /* 0=关闭, 1=加湿中 */

/* 初始化加湿控制引脚 PB1 */
void Wet_Init (void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PB1 推挽输出, 初始低电平(关闭加湿) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

void Wet_Set (float humidity)
{
    humidity_threshold = humidity;
}

void Wet_ON (void)
{
    wet_state = 1;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
}

void Wet_OFF (void)
{
    wet_state = 0;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
}

void Wet_Control (float current_humidity)
{
    /* 滞回控制: 低于阈值10%RH开启, 距阈值3%RH提前关闭 (利用湿惯性) */
    if (humidity_threshold - 10.0f > current_humidity)
    {
        Wet_ON();
    }
    else if (current_humidity > humidity_threshold - 3.0f)
    {
        Wet_OFF();
    }
    /* 在 [threshold-10, threshold-3] 区间保持原状态 (滞回) */
}

uint8_t Wet_GetState (void)
{
    return wet_state;
}

float Wet_GetThreshold (void)
{
    return humidity_threshold;
}
