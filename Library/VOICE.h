#ifndef __VOICE_H
#define __VOICE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"

/* ==================== 硬件配置 ==================== */
/* UART4: PA0=TX, PA1=RX (由 CubeMX 配置)          */
#define VOICE_UART_HANDLE (&huart4)
#define VOICE_UART_BAUD 9600

/* ==================== 帧格式 (ASRPRO -> STM32) ==================== */
/* ASRPRO 写 UART0_WDATA = (0<<24)|(0<<16)|(ARG<<8)|CMD               */
/* 发送 4 字节: CMD, ARG, 0x00, 0x00                                  */
/* STM32 接收 4 字节, 校验 buf[2]==0&&buf[3]==0 为有效帧              */
#define VOICE_FRAME_LEN 4  /* 帧长度 */
#define VOICE_FRAME_SYNC0 0x00 /* 同步填充字节 */
#define VOICE_FRAME_SYNC1 0x00

/* ==================== 语音命令码 ==================== */
/* ---- 查询类 (0x10-0x1F) ---- */
#define VOICE_CMD_REPORT_TEMP 0x10  /* 播报当前温度 */
#define VOICE_CMD_REPORT_HUM 0x11   /* 播报当前湿度 */
#define VOICE_CMD_REPORT_LUX 0x12   /* 播报当前光照 */
#define VOICE_CMD_REPORT_ALL 0x13   /* 播报环境状态 */

/* ---- 温度控制 (0x20-0x2F), ARG=int8 步进值 ---- */
#define VOICE_CMD_TEMP_UP 0x20      /* 设定温度 +ARG */
#define VOICE_CMD_TEMP_DOWN 0x21    /* 设定温度 -ARG */

/* ---- 湿度控制 (0x30-0x3F), ARG=int8 步进值 ---- */
#define VOICE_CMD_HUM_UP 0x30       /* 设定湿度 +ARG */
#define VOICE_CMD_HUM_DOWN 0x31     /* 设定湿度 -ARG */

/* ---- 灯光控制 (0x40-0x4F) ---- */
#define VOICE_CMD_LED_ON 0x40       /* 打开灯光 */
#define VOICE_CMD_LED_OFF 0x41      /* 关闭灯光 */
#define VOICE_CMD_LED_BRIGHTER 0x42 /* 灯光调亮 +ARG */
#define VOICE_CMD_LED_DIMMER 0x43   /* 灯光调暗 -ARG */

/* ---- 加热/加湿开关 (0x50-0x5F) ---- */
#define VOICE_CMD_HEAT_ON 0x50      /* 强制开加热 */
#define VOICE_CMD_HEAT_OFF 0x51     /* 强制关加热 */
#define VOICE_CMD_WET_ON 0x52       /* 强制开加湿 */
#define VOICE_CMD_WET_OFF 0x53      /* 强制关加湿 */
#define VOICE_CMD_AUTO_MODE 0x54    /* 恢复自动温湿度控制 */

/* ==================== 非阻塞 API ==================== */
void VOICE_Init (void);
void VOICE_Process (void);   /* 状态机主循环, 放到 while(1) 中 */
uint8_t VOICE_IsBusy (void); /* 查询 UART 通信是否进行中 */

extern uint8_t voice_manual_mode; /* 0=自动模式, 1=手动模式 */

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_H */