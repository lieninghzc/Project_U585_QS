#include "MENU.h"
#include "ENCODER.h"
#include "Heating.h"
#include "Wet.h"
#include "LED.h"
#include "VOICE.h"
#include "gpio.h"

/* ======================== 硬件引脚 ======================== */

#define KEY_PIN     GPIO_PIN_5
#define KEY_PORT    GPIOB

/* ======================== 超时参数 ======================== */

#define IDLE_TIMEOUT_MS     180000   /* 3 分钟无操作 → 休眠 */
#define LOOP_MS             100
#define IDLE_TIMEOUT_LOOPS  (IDLE_TIMEOUT_MS / LOOP_MS)  /* 1800 */

/* ======================== 按键灵敏度 ======================== */

#define ENC_THRESHOLD       2        /* 编码器步进阈值 (防抖) */

/* ======================== 调节步进 ======================== */

#define TEMP_STEP           0.5f
#define HUM_STEP            1.0f
#define LUX_STEP            100

/* ======================== 内部变量 ======================== */

static DISP_Mode_t  display_mode = DISP_MODE_SLEEP;
static MENU_Param_t selected_param = MENU_PARAM_NONE;
static uint16_t     idle_counter = 0;

static uint8_t      btn_prev = 1;
static uint8_t      btn_pressed = 0;

static int32_t      enc_accum = 0;
static uint8_t      off_mode = 0;        /* 0=自动, 1=全部关闭 */

/* ======================== 初始化 ======================== */

void MENU_Init(void)
{
    GPIO_InitTypeDef gpio = {
        .Pin    = KEY_PIN,
        .Mode   = GPIO_MODE_INPUT,
        .Pull   = GPIO_PULLUP,
        .Speed  = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(KEY_PORT, &gpio);

    display_mode   = DISP_MODE_SLEEP;
    selected_param = MENU_PARAM_NONE;
    idle_counter   = 0;
    btn_prev       = 1;
    btn_pressed    = 0;
    enc_accum      = 0;
    off_mode       = 0;
}

/* ======================== 查询函数 ======================== */

DISP_Mode_t MENU_GetDisplayMode(void)
{
    return display_mode;
}

MENU_Param_t MENU_GetSelectedParam(void)
{
    return (display_mode == DISP_MODE_ACTIVE) ? selected_param : MENU_PARAM_NONE;
}

uint8_t MENU_IsAdjusting(void)
{
    return (display_mode == DISP_MODE_ACTIVE && selected_param != MENU_PARAM_NONE);
}

uint8_t MENU_IsOffMode(void)
{
    return off_mode;
}

/* ======================== 参数调节 ======================== */

static void adjust_parameter(int8_t direction)
{
    if (direction == 0) return;

    switch (selected_param)
    {
    case MENU_PARAM_TEMP_SET:
    {
        float sp = Heating_GetThreshold();
        sp += (float)direction * TEMP_STEP;
        if (sp > 80.0f)  sp = 80.0f;
        if (sp < 10.0f)  sp = 10.0f;
        Heating_Set(sp);
        break;
    }
    case MENU_PARAM_HUM_SET:
    {
        float sp = Wet_GetThreshold();
        sp += (float)direction * HUM_STEP;
        if (sp > 95.0f)  sp = 95.0f;
        if (sp < 20.0f)  sp = 20.0f;
        Wet_Set(sp);
        break;
    }
    case MENU_PARAM_LED_TARGET:
    {
        uint16_t cur = LED_GetTargetLux();
        int32_t  new_val = (int32_t)cur + (int32_t)direction * (int32_t)LUX_STEP;
        if (new_val < 0)      new_val = 0;
        if (new_val > 65535)  new_val = 65535;
        LED_SetTargetLux((uint16_t)new_val);
        break;
    }
    /* 模式切换: 自动/关闭 */
    case MENU_PARAM_MODE_SWITCH:
        if (direction > 0)
        {
            /* → 自动模式: LED恢复PI, 温湿度恢复自动滞回 */
            LED_ResumeAuto();
            voice_manual_mode = 0;
            off_mode = 0;
        }
        else
        {
            /* → 关闭模式: 全部关闭 */
            Heating_OFF();
            Wet_OFF();
            LED_ForceOff();
            voice_manual_mode = 1;
            off_mode = 1;
        }
        break;

    default:
        break;
    }
}

/* ======================== 参数循环 ======================== */

static void cycle_selection(void)
{
    switch (selected_param)
    {
    case MENU_PARAM_NONE:         selected_param = MENU_PARAM_TEMP_SET;    break;
    case MENU_PARAM_TEMP_SET:     selected_param = MENU_PARAM_HUM_SET;     break;
    case MENU_PARAM_HUM_SET:      selected_param = MENU_PARAM_LED_TARGET;  break;
    case MENU_PARAM_LED_TARGET:   selected_param = MENU_PARAM_MODE_SWITCH; break;
    case MENU_PARAM_MODE_SWITCH:  selected_param = MENU_PARAM_NONE;        break;
    }
}

/* ======================== 主任务 ======================== */

void MENU_Task(void)
{
    /* === 1. 读取输入 === */
    uint8_t btn_now = HAL_GPIO_ReadPin(KEY_PORT, KEY_PIN);
    static int32_t enc_last = 0;
    int32_t enc_pos = ENCODER_GetPosition();
    int32_t enc_delta = enc_pos - enc_last;
    enc_last = enc_pos;

    /* === 2. 检测活动 === */
    uint8_t activity = 0;

    btn_pressed = 0;
    if (btn_prev == 1 && btn_now == 0)
    {
        btn_pressed = 1;
        activity = 1;
    }
    btn_prev = btn_now;

    enc_accum += enc_delta;
    int8_t enc_dir = 0;
    if      (enc_accum >=  ENC_THRESHOLD) { enc_dir = 1;  enc_accum -= ENC_THRESHOLD; activity = 1; }
    else if (enc_accum <= -ENC_THRESHOLD) { enc_dir = -1; enc_accum += ENC_THRESHOLD; activity = 1; }

    /* === 3. 状态机 === */

    if (display_mode == DISP_MODE_SLEEP)
    {
        if (activity)
        {
            display_mode   = DISP_MODE_ACTIVE;
            selected_param = MENU_PARAM_NONE;
            idle_counter   = 0;
        }
    }
    else /* ACTIVE */
    {
        if (activity)
        {
            idle_counter = 0;

            if (btn_pressed)
            {
                /* 按键 → 循环选择下一个参数 */
                cycle_selection();
            }
            else if (enc_dir != 0)
            {
                /* 有参数选中 → 调节/切换; 无 → 忽略 (仅维持唤醒) */
                if (selected_param != MENU_PARAM_NONE)
                {
                    adjust_parameter(enc_dir);
                }
            }
        }
        else
        {
            idle_counter++;
            if (idle_counter >= IDLE_TIMEOUT_LOOPS)
            {
                display_mode   = DISP_MODE_SLEEP;
                selected_param = MENU_PARAM_NONE;
                idle_counter   = 0;
            }
        }
    }
}
