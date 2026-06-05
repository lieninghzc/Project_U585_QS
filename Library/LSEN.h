#ifndef __LSEN_H
#define __LSEN_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

/* ========== BH1750 I2C 地址 (7-bit, ADDR=GND) ========== */
#define BH1750_I2C_ADDR 0x23

/* ========== BH1750 命令 ========== */
#define BH1750_CMD_POWER_ON 0x01    /* 上电 */
#define BH1750_CMD_POWER_OFF 0x00   /* 断电 */
#define BH1750_CMD_RESET 0x07       /* 复位 */
#define BH1750_CMD_CONT_H_RES 0x10  /* 连续高分辨率模式, 1lx, 120ms */
#define BH1750_CMD_CONT_H_RES2 0x11 /* 连续高分辨率模式2, 0.5lx, 120ms */
#define BH1750_CMD_CONT_L_RES 0x13  /* 连续低分辨率模式, 4lx, 16ms */
#define BH1750_CMD_ONE_H_RES 0x20   /* 单次高分辨率模式, 1lx, 120ms */
#define BH1750_CMD_ONE_H_RES2 0x21  /* 单次高分辨率模式2, 0.5lx, 120ms */
#define BH1750_CMD_ONE_L_RES 0x23   /* 单次低分辨率模式, 4lx, 16ms */

/* ========== 全局变量声明 ========== */
extern float lux;         /* 光照度 (lx) */
extern uint8_t BH1750_SR; /* 状态: 0=正常, 1=I2C错误 */

/* ========== 函数声明 ========== */
void BH1750_Init (void);
void LSEN_READ (void);
float BH1750_CalcLux (uint16_t raw);

#endif
