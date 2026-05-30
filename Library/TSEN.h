#ifndef __TSEN_H
#define __TSEN_H

#include "stm32u5xx_hal.h"
#include <stdint.h>

/* ========== SHT40 I2C 地址 (7-bit) ========== */
#define SHT40_I2C_ADDR 0x44

/* ========== SHT40 命令 ========== */
#define SHT40_CMD_MEASURE_HIGH 0xFD   /* 高精度测量, 8.3ms */
#define SHT40_CMD_MEASURE_MEDIUM 0xF6 /* 中精度测量, 4.5ms */
#define SHT40_CMD_MEASURE_LOW 0xE0    /* 低精度测量, 1.7ms */
#define SHT40_CMD_SERIAL_NUM 0x89     /* 读取序列号 */
#define SHT40_CMD_SOFT_RESET 0x94     /* 软复位 */

/* ========== CRC-8 多项式 (x^8 + x^5 + x^4 + 1) ========== */
#define SHT40_CRC_POLY 0x31
#define SHT40_CRC_INIT 0xFF

/* ========== 全局变量声明 (保持与main.c兼容) ========== */
extern uint16_t rtd_raw;    /* 温度原始ADC值 */
extern uint16_t hum_raw;    /* 湿度原始ADC值 */
extern uint8_t SR;          /* 状态/错误码 */
extern uint8_t testData;    /* 测试数据 */
extern float temperature_c; /* 摄氏温度 */
extern float humidity;      /* 相对湿度 %RH */

/* ========== 函数声明 ========== */
void SHT40_Init (void);
void TSEN_READ (void);
uint8_t SHT40_CRC8 (const uint8_t* data, uint8_t len);
float SHT40_CalcTemperature (uint16_t raw);
float SHT40_CalcHumidity (uint16_t raw);

#endif
