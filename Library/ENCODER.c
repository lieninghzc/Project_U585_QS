/**
  ******************************************************************************
  * @file    ENCODER.c
  * @brief   EC11 旋转编码器驱动 (基于 TIM3 编码器模式)
  *          - PA6: TIM3_CH1 (Encoder A)
  *          - PA7: TIM3_CH2 (Encoder B)
  *          - 顺时针旋转: 计数器递减; 逆时针旋转: 计数器递增
  ******************************************************************************
  */
#include "ENCODER.h"

/* 记录初始化时的计数器值, 用于计算相对偏移 */
static int32_t encoder_initial_count = 0;

/**
  * @brief  启动编码器并记录初始位置
  * @note   调用前须确保 MX_TIM3_Init() 已完成
  */
void ENCODER_Init (void)
{
    /* 启动 TIM3 编码器模式 (双通道) */
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);

    /* 记录初始计数器值, 后续所有 Delta 均以此为基准 */
    encoder_initial_count = (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
}

/**
  * @brief  获取编码器当前原始计数值
  * @retval 当前 TIM3->CNT 值 (有符号 32 位)
  */
int32_t ENCODER_GetPosition (void)
{
    return (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
}

/**
  * @brief  获取相对于初始化位置的偏移量
  * @retval 正数 = 逆时针累计步数; 负数 = 顺时针累计步数
  */
int32_t ENCODER_GetDelta (void)
{
    int32_t raw = encoder_initial_count - (int32_t)__HAL_TIM_GET_COUNTER(&htim3);
    return raw / 2; /* EC11 每格产生 1 个完整周期 (2边沿), ÷2 = 每格±1 */
}
