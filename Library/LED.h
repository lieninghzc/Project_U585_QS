#ifndef __LED_H
#define __LED_H

#include <stdint.h>

/* ======================== 目标光照度定义 ========================
 * 默认目标光照度 (单位: lux)
 * 修改此处即可调整系统默认值, 无需改动其他代码
 */
#define LED_DEFAULT_TARGET_LUX  1000

/* ======================== 函数接口 ======================== */

/**
  * @brief  初始化 LED PWM (TIM4_CH2, PB7)
  *         ARR = 999 → 0.1% 占空比步进精度 (1000 级)
  */
void LED_Init(void);

/**
  * @brief  设置目标光照度 (lux)
  *         LED_Task() 通过 PI 负反馈自动调节 PWM 占空比以达到此目标
  * @param  target_lux  目标照度值 (0~65535)
  */
void LED_SetTargetLux(uint16_t target_lux);

/**
  * @brief  获取目标光照度
  * @retval 目标照度 (lux)
  */
uint16_t LED_GetTargetLux(void);

/**
  * @brief  获取当前 PWM 占空比百分比 (0~100)
  * @retval 占空比 0~100 (%)
  */
uint8_t LED_GetDutyCycle(void);

/**
  * @brief  光照负反馈调节任务 — 在主循环中周期性调用
  *
  *         流程:
  *           1. 读取 BH1750 实际照度 (extern float lux)
  *           2. 计算偏差: error = target_lux - lux
  *           3. PI 控制器计算输出占空比 (0.1% 精度)
  *           4. 自适应速率限制 (远端粗调/近端精调)
  *           5. 写 PWM 占空比
  *
  *         PI 参数在 .c 中可调, 默认 Kp=1.2, Ki=0.03, 死区=15lux
  *         输出步进 0.1%, 末端最小速率 0.5%/步
  */
void LED_Task(void);

/**
  * @brief  强制关闭 LED (占空比强制为 0)
  *         PI 控制器暂停, 直到 LED_ResumeAuto() 被调用
  */
void LED_ForceOff(void);

/**
  * @brief  恢复自动 PI 控制
  *         重设积分项, 从当前光照开始重新调节到目标值
  */
void LED_ResumeAuto(void);

/**
  * @brief  查询 LED 是否处于强制关闭状态
  * @retval 1=已强制关闭, 0=自动调节中
  */
uint8_t LED_IsForcedOff(void);

/**
  * @brief  检测 LED 当前是否点亮
  * @retval 1=点亮中 (duty>0 且非强制关闭),
  *         0=熄灭 (duty=0 或强制关闭)
  * @note   由 LED_Task 每次运行时更新, 可用于显示或状态判断
  */
uint8_t LED_IsActive(void);

#endif /* __LED_H */
