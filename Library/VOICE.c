/**
  ******************************************************************************
  * @file    VOICE.c
  * @brief   ASRPRO 语音模块 I2C 驱动 (非阻塞中断版, 基于 STM32U585 HAL)
  *
  * @details 通过硬件 I2C1 (PB6=SCL, PB3=SDA) 与 ASRPRO 语音模块通信。
  *          采用 HAL_I2C_Master_Receive_IT / Master_Transmit_IT 中断方式，
  *          主循环调用 VOICE_Process() 驱动状态机，不阻塞。
  *
  *          协议: STM32 周期性读取 ASRPRO 的命令字节(7位地址 0x66)，
  *          解析后执行相应操作（修改设定值或回传传感器数据）。
  ******************************************************************************
  */

#include "VOICE.h"
#include "TSEN.h"    /* temperature_c, humidity */
#include "LSEN.h"    /* lux, BH1750_SR */
#include "Heating.h" /* Heating_Set, Heating_GetThreshold, Heating_ON/OFF */
#include "OLED.h"    /* OLED 显示语音指令反馈 */

/* ==================== 外部引用 ==================== */
extern I2C_HandleTypeDef hi2c1;

/* ==================== 状态机 ==================== */
typedef enum
{
    VOICE_ST_IDLE,        /* 空闲，等待轮询间隔 */
    VOICE_ST_READ_CMD,    /* 已发起 IT 读取命令字节 */
    VOICE_ST_CMD_RX_DONE, /* 命令字节已收到，待处理 */
    VOICE_ST_WRITING,     /* 已发起 IT 回写 */
} VOICE_State_t;

/* ==================== 模块级变量 ==================== */
static VOICE_State_t voice_state = VOICE_ST_IDLE;
static uint32_t voice_timer = 0;           /* 上次发起通信的 tick */
static uint8_t voice_rx_buf = 0;           /* 接收命令缓冲区 */
static uint8_t voice_tx_buf[4];            /* 发送数据缓冲区 */
static uint8_t voice_tx_len = 0;           /* 待发送字节数 */
static volatile uint8_t voice_rx_done = 0; /* IT 接收完成标记 (ISR 置位) */
static volatile uint8_t voice_tx_done = 0; /* IT 发送完成标记 (ISR 置位) */
static volatile uint8_t voice_busy = 0;    /* 总线忙标记 */

/* ==================== 内部函数声明 ==================== */
static void VOICE_ParseCommand (uint8_t cmd);

/**
  * @brief  VOICE 模块初始化
  * @note   I2C1 硬件由 MX_I2C1_Init() 完成，此处仅复位状态机。
  *         需要在 HAL_Init() 之后、while(1) 之前调用。
  */
void VOICE_Init (void)
{
    voice_state = VOICE_ST_IDLE;
    voice_timer = HAL_GetTick();
    voice_rx_done = 0;
    voice_tx_done = 0;
    voice_busy = 0;

    /* 使能 I2C1 中断 (NVIC) —— 非阻塞 IT 通信依赖此配置 */
    HAL_NVIC_SetPriority(I2C1_EV_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
    HAL_NVIC_SetPriority(I2C1_ER_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
}

/**
  * @brief  查询 I2C 总线是否正在被 VOICE 模块占用
  * @retval 0 = 空闲, 1 = 通信进行中
  */
uint8_t VOICE_IsBusy (void)
{
    return voice_busy;
}

/**
  * @brief  VOICE 状态机主循环 —— 放到 while(1) 中调用，完全非阻塞
  *
  *  @note   调用频率无严格限制（内部有计时器控制轮询间隔）。
  *          当 voice_busy==0 时不会影响其他 I2C 设备（SHT40/BH1750）。
  */
void VOICE_Process (void)
{
    uint32_t now = HAL_GetTick();

    switch (voice_state)
    {
        /* ---------------------------------------------------------------- */
        case VOICE_ST_IDLE:
            /* 等待下一轮轮询时刻，且 I2C 空闲 */
            if ((now - voice_timer) >= VOICE_POLL_INTERVAL)
            {
                voice_timer = now;

                /* 检查 I2C 总线是否被其他模块占用 (简单判忙) */
                if (hi2c1.State != HAL_I2C_STATE_READY)
                {
                    break; /* 总线忙，下一周期再试 */
                }

                voice_busy = 1;
                voice_rx_done = 0;

                /* 发起非阻塞读取: 从 ASRPRO 读 1 字节命令 */
                if (HAL_I2C_Master_Receive_IT(ASRPRO_I2C_HANDLE, (uint16_t)(ASRPRO_I2C_ADDR << 1), &voice_rx_buf, 1) == HAL_OK)
                {
                    voice_state = VOICE_ST_READ_CMD;
                }
                else
                {
                    /* 发起失败 (总线忙等)，回退空闲 */
                    voice_busy = 0;
                    voice_state = VOICE_ST_IDLE;
                }
            }
            break;

        /* ---------------------------------------------------------------- */
        case VOICE_ST_READ_CMD:
            /* 等待 IT 回调置位 voice_rx_done */
            if (voice_rx_done)
            {
                voice_rx_done = 0;
                voice_state = VOICE_ST_CMD_RX_DONE;
            }
            break;

        /* ---------------------------------------------------------------- */
        case VOICE_ST_CMD_RX_DONE:
            /* 解析命令 (可能触发回写) */
            VOICE_ParseCommand(voice_rx_buf);
            voice_state = VOICE_ST_IDLE; /* 默认回到空闲 */
            voice_busy = 0;

            /* 如果有待发送数据，进入发送流程 */
            if (voice_tx_len > 0)
            {
                voice_busy = 1;
                voice_tx_done = 0;
                if (HAL_I2C_Master_Transmit_IT(ASRPRO_I2C_HANDLE, (uint16_t)(ASRPRO_I2C_ADDR << 1), voice_tx_buf, voice_tx_len) == HAL_OK)
                {
                    voice_state = VOICE_ST_WRITING;
                }
                else
                {
                    voice_tx_len = 0;
                    voice_busy = 0;
                    voice_state = VOICE_ST_IDLE;
                }
            }
            break;

        /* ---------------------------------------------------------------- */
        case VOICE_ST_WRITING:
            /* 等待 IT 回调置位 voice_tx_done */
            if (voice_tx_done)
            {
                voice_tx_done = 0;
                voice_tx_len = 0;
                voice_busy = 0;
                voice_state = VOICE_ST_IDLE;
            }
            break;

        /* ---------------------------------------------------------------- */
        default:
            voice_state = VOICE_ST_IDLE;
            voice_busy = 0;
            break;
    }
}

/**
  * @brief  解析 ASRPRO 发来的命令字节并执行
  * @param  cmd : 命令码 (0x00=无命令)
  *
  *  @note   查询类命令会填充 voice_tx_buf 准备回传给 ASRPRO 做 TTS 播报。
  *          修改类命令直接操作 Heating 设定值。
  */
static void VOICE_ParseCommand (uint8_t cmd)
{
    voice_tx_len = 0; /* 默认不回写 */

    if (cmd == VOICE_CMD_NONE)
    {
        return;
    }

    switch (cmd)
    {
        /* ---- 设定温度 +1°C ---- */
        case VOICE_CMD_TEMP_UP:
        {
            float sp = Heating_GetThreshold();
            if (sp < 80.0f)
            { /* 上限保护 */
                Heating_Set(sp + 1.0f);
            }
            OLED_ShowString(4, 1, "V:Temp+1  "); /* OLED 反馈 */
            break;
        }

        /* ---- 设定温度 -1°C ---- */
        case VOICE_CMD_TEMP_DOWN:
        {
            float sp = Heating_GetThreshold();
            if (sp > 10.0f)
            { /* 下限保护 */
                Heating_Set(sp - 1.0f);
            }
            OLED_ShowString(4, 1, "V:Temp-1  ");
            break;
        }

        /* ---- 播报当前温度 ---- */
        case VOICE_CMD_REPORT_TEMP:
        {
            int16_t t = (int16_t)(temperature_c * 10.0f); /* 温度×10, 有符号 */
            voice_tx_buf[0] = (uint8_t)(t >> 8);
            voice_tx_buf[1] = (uint8_t)(t & 0xFF);
            voice_tx_len = 2;
            break;
        }

        /* ---- 播报当前湿度 ---- */
        case VOICE_CMD_REPORT_HUM:
        {
            int16_t h = (int16_t)(humidity * 10.0f); /* 湿度×10 */
            voice_tx_buf[0] = (uint8_t)(h >> 8);
            voice_tx_buf[1] = (uint8_t)(h & 0xFF);
            voice_tx_len = 2;
            break;
        }

        /* ---- 播报当前光照度 ---- */
        case VOICE_CMD_REPORT_LUX:
        {
            uint16_t lx = (uint16_t)lux;
            voice_tx_buf[0] = (uint8_t)(lx >> 8);
            voice_tx_buf[1] = (uint8_t)(lx & 0xFF);
            voice_tx_len = 2;
            break;
        }

        /* ---- 手动开关加热 ---- */
        case VOICE_CMD_TOGGLE_HEAT:
        {
            if (Heating_GetState())
            {
                Heating_OFF();
                OLED_ShowString(4, 1, "V:HeatOFF ");
            }
            else
            {
                Heating_ON();
                OLED_ShowString(4, 1, "V:HeatON  ");
            }
            break;
        }

        default:
            /* 未知命令, 忽略 */
            break;
    }
}

/* =================================================================== */
/*                     HAL I2C 中断回调 (弱函数覆盖)                    */
/* =================================================================== */

/**
  * @brief  I2C 主机接收完成回调 (由 HAL_I2C_EV_IRQHandler 调用)
  */
void HAL_I2C_MasterRxCpltCallback (I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        voice_rx_done = 1;
    }
}

/**
  * @brief  I2C 主机发送完成回调 (由 HAL_I2C_EV_IRQHandler 调用)
  */
void HAL_I2C_MasterTxCpltCallback (I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        voice_tx_done = 1;
    }
}

/**
  * @brief  I2C 错误回调 —— 复位状态机，释放总线
  */
void HAL_I2C_ErrorCallback (I2C_HandleTypeDef* hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        voice_rx_done = 2; /* 2 = 出错标记 */
        voice_tx_done = 2;
        voice_busy = 0;
        voice_tx_len = 0;
    }
}
