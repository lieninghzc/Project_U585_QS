#include "stm32u585xx.h"
#include "stm32u5xx_hal.h"
#include "tim.h"
#include "LED.h"
#include <stdlib.h>

/* ======================== 底层常量 ======================== */

/* 自动重装载值: 999 → 1000 级占空比 (0~999), 每级 = 0.1%
 * PWM 频率 ≈ 16MHz / 1000 = 16kHz (无频闪) */
#define LED_ARR_VALUE           999

/* ======================== PI 控制参数 ======================== */

#define LED_KP          1.2f     /* 比例增益 (输出单位: 0.1% duty)          */
#define LED_KI          0.03f    /* 积分增益 (消除静差)                     */
#define LED_DEADBAND    50       /* 死区 (lux) — 偏差≤50lux 时不调节, 防末端震荡 */
#define LED_DUTY_MIN    0        /* 最小占空比 (0.1% 单位)      0%          */
#define LED_DUTY_MAX    1000     /* 最大占空比 (0.1% 单位)    100.0%        */
#define LED_I_LIMIT     10000    /* 积分限幅, 防饱和恢复过慢                */

/* ======================== 执行间隔与平滑 ======================== */

#define LED_CTRL_INTERVAL   5    /* 每调用 N 次才执行 PI (100ms×5=500ms)   */
#define LUX_ALPHA           0.3f /* 光照低通滤波系数 (越小越平滑)           */

/* ======================== 内部变量 ======================== */

static uint16_t target_lux = LED_DEFAULT_TARGET_LUX;  /* 目标照度 (lux) */

/* current_duty 范围 0~1000, 对应占空比 0.0%~100.0%, 每步 0.1% */
static uint16_t current_duty = 0;
static float    integral = 0.0f;
static uint8_t  ctrl_tick = 0;
static float    lux_filtered = 0.0f;
static uint8_t  force_off = 0;          /* 1=强制关闭中, PI 暂停 */
static uint8_t  led_active = 0;         /* 1=LED点亮中 (由 LED_Task 每次运行时检测) */

extern float lux;
extern uint8_t BH1750_SR;

/* ======================== 初始化 ======================== */

void LED_Init(void)
{
    HAL_TIM_PWM_Stop(&htim4, TIM_CHANNEL_2);

    __HAL_TIM_SET_AUTORELOAD(&htim4, LED_ARR_VALUE);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, 0);
    __HAL_TIM_SET_COUNTER(&htim4, 0);

    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);

    target_lux    = LED_DEFAULT_TARGET_LUX;
    current_duty  = 0;
    integral      = 0.0f;
    ctrl_tick     = 0;
    lux_filtered  = 0.0f;
    force_off     = 0;
    led_active    = 0;
}

/* ======================== 底层: 写入 CCR ======================== */

static void LED_WriteDuty(uint16_t duty_tenths)
{
    uint32_t pulse;

    if (duty_tenths == 0)
    {
        pulse = 0;
    }
    else if (duty_tenths >= 1000)
    {
        pulse = LED_ARR_VALUE + 1;  /* CCR > ARR → PWM1 输出恒高 (100%) */
    }
    else
    {
        pulse = duty_tenths;        /* 1~999 直接映射到 0.1%~99.9% */
    }

    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, pulse);
}

/* ======================== 公有函数 ======================== */

void LED_SetTargetLux(uint16_t target)
{
    target_lux = target;
}

uint16_t LED_GetTargetLux(void)
{
    return target_lux;
}

/**
  * @brief  获取当前占空比, 0~100 (百分比)
  */
uint8_t LED_GetDutyCycle(void)
{
    return (uint8_t)(current_duty / 10);  /* 0.1% → % */
}

/**
  * PI 负反馈调节任务
  *
  *  error = target_lux - lux_filtered
  *  duty  = clamp(Kp×error + Ki×Σerror, 0, 1000)   (0.1% 单位)
  *
  * 末端精细控制:
  *   根据偏差大小自适应调节速率, 越接近目标步进越小:
  *     |error| > 200lux → 每步 5.0%   (50)
  *     |error| > 100lux → 每步 3.0%   (30)
  *     |error| >  50lux → 每步 1.0%   (10)
  *     |error| >  15lux → 每步 0.5%   ( 5) — 末端无感微调
  *     |error| ≤  15lux → 死区, 不动
  */
void LED_Task(void)
{
    if (BH1750_SR != 0) return;             /* 传感器异常 */

    /* === 每次调节检测 LED 状态 === */
    led_active = (!force_off) && (current_duty > 0);

    /* === 强制关闭状态: 保持输出 0, 暂停 PI === */
    if (force_off)
    {
        if (current_duty != 0)
        {
            current_duty = 0;
            LED_WriteDuty(0);
        }
        return;
    }

    /* === 1. 光照低通滤波 === */
    if (lux_filtered < 1.0f)
        lux_filtered = lux;
    else
        lux_filtered = LUX_ALPHA * lux + (1.0f - LUX_ALPHA) * lux_filtered;

    /* === 2. 控制节流 === */
    ctrl_tick++;
    if (ctrl_tick < LED_CTRL_INTERVAL) return;
    ctrl_tick = 0;

    /* === 3. PI 运算 === */
    int16_t error = (int16_t)target_lux - (int16_t)(lux_filtered + 0.5f);
    int16_t abs_error = abs((int)error);

    /* 死区 */
    if (abs_error <= LED_DEADBAND) return;

    float p_term = LED_KP * (float)error;
    integral += LED_KI * (float)error;

    if (integral >  LED_I_LIMIT) integral =  LED_I_LIMIT;
    if (integral < -LED_I_LIMIT) integral = -LED_I_LIMIT;

    float output = p_term + integral;

    /* === 4. 限幅 === */
    int16_t new_duty = (int16_t)(output + 0.5f);
    if      (new_duty > LED_DUTY_MAX) new_duty = LED_DUTY_MAX;
    else if (new_duty < LED_DUTY_MIN) new_duty = LED_DUTY_MIN;

    /* === 5. 自适应速率限制 (末端精细) ===
     *  远→粗调, 近→精调, 确保末端无感知超调:
     *    |error| > 200 → 5.0%/步   快速接近
     *    |error| > 100 → 3.0%/步
     *    |error| >  50 → 1.0%/步
     *    |error| >  15 → 0.5%/步   末端微调 (最精细)
     *    |error| ≤  15 → 死区, 不动
     */
    uint16_t rate_limit;
    if      (abs_error > 200) rate_limit = 50;   /* 5.0%/步 */
    else if (abs_error > 100) rate_limit = 30;   /* 3.0%/步 */
    else if (abs_error > 50)  rate_limit = 10;   /* 1.0%/步 */
    else                      rate_limit = 5;    /* 0.5%/步 — 末端最精细 */

    int16_t delta = new_duty - (int16_t)current_duty;
    if      (delta >  (int16_t)rate_limit) delta = (int16_t)rate_limit;
    else if (delta < -(int16_t)rate_limit) delta = -(int16_t)rate_limit;

    current_duty = (uint16_t)((int16_t)current_duty + delta);
    LED_WriteDuty(current_duty);
}

/* ======================== 强制开关控制 ======================== */

void LED_ForceOff(void)
{
    force_off = 1;
    current_duty = 0;
    led_active = 0;
    LED_WriteDuty(0);
}

void LED_ResumeAuto(void)
{
    force_off = 0;
    led_active = 1;

    /* 重设积分, 从当前光照开始重新追踪目标 */
    integral = 0.0f;
    ctrl_tick = 0;
    lux_filtered = 0.0f;
}

uint8_t LED_IsForcedOff(void)
{
    return force_off;
}

uint8_t LED_IsActive(void)
{
    return led_active;
}
