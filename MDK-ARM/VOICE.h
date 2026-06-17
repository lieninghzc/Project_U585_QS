#ifndef __VOICE_H
#define __VOICE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "main.h"
#include "i2c.h"

/* ==================== ASRPRO 语音模块硬件宏 ==================== */
#define ASRPRO_I2C_ADDR 0x66       /* ASRPRO 7位 I2C 从机地址 */
#define ASRPRO_I2C_HANDLE (&hi2c1) /* I2C1 外设句柄 */
#define ASRPRO_I2C_TIMEOUT 50      /* I2C 超时 (ms)，非阻塞模式下仅作保底 */

#define VOICE_POLL_INTERVAL 300 /* 命令轮询间隔 (ms) */

/* ==================== 语音命令码 (与 ASRPRO 固件协定) ==================== */
#define VOICE_CMD_NONE 0x00        /* 无命令 */
#define VOICE_CMD_TEMP_UP 0x01     /* 设定温度 +1°C */
#define VOICE_CMD_TEMP_DOWN 0x02   /* 设定温度 -1°C */
#define VOICE_CMD_REPORT_TEMP 0x03 /* 播报当前温度 */
#define VOICE_CMD_REPORT_HUM 0x04  /* 播报当前湿度 */
#define VOICE_CMD_REPORT_LUX 0x05  /* 播报当前光照度 */
#define VOICE_CMD_TOGGLE_HEAT 0x06 /* 手动开关加热 */

    /* ==================== 非阻塞 API ==================== */
    void VOICE_Init (void);
    void VOICE_Process (void);   /* 状态机主循环, 放到 while(1) 中 */
    uint8_t VOICE_IsBusy (void); /* 查询 I2C 是否正在通信 */

#ifdef __cplusplus
}
#endif

#endif /* __VOICE_H */
