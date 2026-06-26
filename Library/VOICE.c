/**
  ******************************************************************************
  * @file    VOICE.c
  * @brief   ASRPRO 语音模块 UART 驱动 (中断接收, STM32U585 HAL)
  *
  * @details ASRPRO UART0_TX(PB5) -> STM32 UART4_RX(PA1)
  *          ASRPRO UART0_RX(PB6) <- STM32 UART4_TX(PA0) (预留回传)
  *
  *          帧格式 (ASRPRO->STM32): 4 字节
  *            [CMD, ARG, 0x00, 0x00]
  *          STM32 通过 HAL_UART_Receive_IT 接收,解析后执行动作
  *
  *          ASRPRO 按 snid 主动发送命令帧,无需 STM32 轮询
  ******************************************************************************
  */

#include "VOICE.h"
#include "TSEN.h"    /* temperature_c, humidity */
#include "LSEN.h"    /* lux, BH1750_SR */
#include "Heating.h" /* Heating_Set, Heating_GetThreshold, Heating_ON/OFF */
#include "Wet.h"     /* Wet_ON, Wet_OFF, Wet_Set, Wet_GetState */
#include "tim.h"     /* htim4 */
#include "LED.h"     /* LED_ForceOff, LED_ResumeAuto, LED_SetTargetLux */

/* ==================== 外部引用 ==================== */
extern UART_HandleTypeDef huart4;       /* CubeMX 生成的 UART4 句柄 */
extern TIM_HandleTypeDef htim4;         /* TIM4 PWM (给 LED 模块用) */

/* ==================== 状态机 ==================== */
typedef enum
{
    VOICE_ST_IDLE,        /* 空闲,等待接收完成 */
    VOICE_ST_RX_WAIT,     /* 已启动 IT 接收,等待回调 */
    VOICE_ST_CMD_READY,   /* 收到完整帧,可解析 */
    VOICE_ST_ERROR,       /* 帧校验失败,丢弃并重启接收 */
} VOICE_State_t;

/* ==================== 模块级变量 ==================== */
static VOICE_State_t voice_state = VOICE_ST_IDLE;
static uint8_t voice_rx_buf[VOICE_FRAME_LEN]; /* 接收帧缓冲 */
static volatile uint8_t voice_rx_done = 0;     /* 接收完成标记 (ISR 置位) */
static volatile uint8_t voice_rx_err = 0;      /* 接收错误标记 (ISR 置位) */
static volatile uint8_t voice_busy = 0;         /* 忙标记 */

uint8_t voice_manual_mode = 0; /* 0=自动, 1=手动 (外部可读写) */

/* ==================== 内部函数声明 ==================== */
static void VOICE_ParseCommand (uint8_t cmd, int8_t arg);
static void VOICE_RestartRX (void);
static void VOICE_SendInt16 (int16_t val);  /* 回传传感器数值 */

/* ==================== 初始化 ==================== */

/**
  * @brief  VOICE 模块初始化
  * @note   MX_UART4_Init() 须先于本函数调用
  */
void VOICE_Init (void)
{
    voice_state = VOICE_ST_IDLE;
    voice_rx_done = 0;
    voice_rx_err = 0;
    voice_busy = 0;

    /* 使能 UART4 中断 */
    HAL_NVIC_SetPriority(UART4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);

    /* 启动首次中断接收 */
    VOICE_RestartRX();
}

/**
  * @brief  查询 UART 是否正在通信
  */
uint8_t VOICE_IsBusy (void)
{
    return voice_busy;
}

/* ==================== 接收管理 ==================== */

/**
  * @brief  启动/重启 UART 中断接收
  */
static void VOICE_RestartRX (void)
{
    voice_rx_done = 0;
    voice_rx_err = 0;
    voice_state = VOICE_ST_RX_WAIT;

    if (HAL_UART_Receive_IT(VOICE_UART_HANDLE, voice_rx_buf, VOICE_FRAME_LEN) != HAL_OK)
    {
        voice_state = VOICE_ST_IDLE;
    }
}

/* ==================== 主状态机 ==================== */

/**
  * @brief  VOICE 状态机主循环 —— 放到 while(1) 中调用,完全非阻塞
  */
void VOICE_Process (void)
{
    switch (voice_state)
    {
        case VOICE_ST_IDLE:
            VOICE_RestartRX();
            break;

        case VOICE_ST_RX_WAIT:
            if (voice_rx_done)
            {
                voice_rx_done = 0;
                voice_state = VOICE_ST_CMD_READY;
            }
            else if (voice_rx_err)
            {
                voice_rx_err = 0;
                HAL_UART_AbortReceive_IT(VOICE_UART_HANDLE);
                VOICE_RestartRX();
            }
            break;

        case VOICE_ST_CMD_READY:
        {
            uint8_t cmd = voice_rx_buf[0];
            int8_t arg = (int8_t)voice_rx_buf[1];

            if (voice_rx_buf[2] == VOICE_FRAME_SYNC0 && voice_rx_buf[3] == VOICE_FRAME_SYNC1)
            {
                if (cmd != 0x00)
                {
                    voice_busy = 1;
                    VOICE_ParseCommand(cmd, arg);
                    voice_busy = 0;
                }
            }
            VOICE_RestartRX();
            break;
        }

        case VOICE_ST_ERROR:
            HAL_UART_AbortReceive_IT(VOICE_UART_HANDLE);
            VOICE_RestartRX();
            break;

        default:
            voice_state = VOICE_ST_IDLE;
            break;
    }
}

/* ==================== 传感器数值回传 ==================== */

/**
  * @brief  向 ASRPRO 回传 int16 传感器数值 (绕过 HAL, 直接操作寄存器)
  * @param  val : 数值 × 10
  */
static void VOICE_SendInt16 (int16_t val)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)((uint16_t)val >> 8);
    buf[1] = (uint8_t)((uint16_t)val & 0xFF);

    for (int i = 0; i < 2; i++)
    {
        while (!(UART4->ISR & USART_ISR_TXE_TXFNF));  /* 等 TXE */
        UART4->TDR = buf[i];
    }
    while (!(UART4->ISR & USART_ISR_TC));  /* 等发送完成 */
}

/* ==================== 命令解析与执行 ==================== */

static void VOICE_ParseCommand (uint8_t cmd, int8_t arg)
{
    switch (cmd)
    {
        /* ========== 查询播报类 ========== */
        case VOICE_CMD_REPORT_TEMP:
            VOICE_SendInt16((int16_t)(temperature_c * 10.0f));
            break;

        case VOICE_CMD_REPORT_HUM:
            VOICE_SendInt16((int16_t)(humidity * 10.0f));
            break;

        case VOICE_CMD_REPORT_LUX:
            VOICE_SendInt16((int16_t)lux);
            break;

        case VOICE_CMD_REPORT_ALL:
            VOICE_SendInt16((int16_t)(temperature_c * 10.0f));
            VOICE_SendInt16((int16_t)(humidity * 10.0f));
            VOICE_SendInt16((int16_t)lux);
            break;

        /* ========== 温度控制 ========== */
        case VOICE_CMD_TEMP_UP:
        {
            float sp = Heating_GetThreshold();
            float new_sp = sp + (float)arg * 0.5f;
            if (new_sp > 80.0f) new_sp = 80.0f;
            if (new_sp < 10.0f) new_sp = 10.0f;
            Heating_Set(new_sp);
            break;
        }

        case VOICE_CMD_TEMP_DOWN:
        {
            float sp = Heating_GetThreshold();
            float new_sp = sp + (float)arg * 0.5f;
            if (new_sp > 80.0f) new_sp = 80.0f;
            if (new_sp < 10.0f) new_sp = 10.0f;
            Heating_Set(new_sp);
            break;
        }

        /* ========== 湿度控制 ========== */
        case VOICE_CMD_HUM_UP:
        {
            float sp = Wet_GetThreshold();
            float new_sp = sp + (float)arg;
            if (new_sp > 95.0f) new_sp = 95.0f;
            if (new_sp < 20.0f) new_sp = 20.0f;
            Wet_Set(new_sp);
            break;
        }

        case VOICE_CMD_HUM_DOWN:
        {
            float sp = Wet_GetThreshold();
            float new_sp = sp + (float)arg;
            if (new_sp > 95.0f) new_sp = 95.0f;
            if (new_sp < 20.0f) new_sp = 20.0f;
            Wet_Set(new_sp);
            break;
        }

        /* ========== 灯光控制 ========== */
        case VOICE_CMD_LED_ON:
            LED_ResumeAuto();
            break;

        case VOICE_CMD_LED_OFF:
            LED_ForceOff();
            break;

        case VOICE_CMD_LED_BRIGHTER:
        {
            int16_t new_val = (int16_t)LED_GetTargetLux() + (int16_t)arg * 100;
            if (new_val < 0)      new_val = 0;
            if (new_val > 65535)  new_val = 65535;
            LED_SetTargetLux((uint16_t)new_val);
            break;
        }

        case VOICE_CMD_LED_DIMMER:
        {
            int16_t new_val = (int16_t)LED_GetTargetLux() + (int16_t)arg * 100;
            if (new_val < 0)      new_val = 0;
            if (new_val > 65535)  new_val = 65535;
            LED_SetTargetLux((uint16_t)new_val);
            break;
        }

        /* ========== 开关控制 ========== */
        case VOICE_CMD_HEAT_ON:
            voice_manual_mode = 1;
            Heating_ON();
            break;

        case VOICE_CMD_HEAT_OFF:
            voice_manual_mode = 1;
            Heating_OFF();
            break;

        case VOICE_CMD_WET_ON:
            voice_manual_mode = 1;
            Wet_ON();
            break;

        case VOICE_CMD_WET_OFF:
            voice_manual_mode = 1;
            Wet_OFF();
            break;

        case VOICE_CMD_AUTO_MODE:
            voice_manual_mode = 0;
            break;

        default:
            break;
    }
}

/* =================================================================== */
/*                  HAL UART 中断回调 (弱函数覆盖)                      */
/* =================================================================== */

void HAL_UART_RxCpltCallback (UART_HandleTypeDef* huart)
{
    if (huart->Instance == UART4)
    {
        voice_rx_done = 1;
        voice_state = VOICE_ST_CMD_READY;
    }
}

void HAL_UART_ErrorCallback (UART_HandleTypeDef* huart)
{
    if (huart->Instance == UART4)
    {
        voice_rx_err = 1;
    }
}