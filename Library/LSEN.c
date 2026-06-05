/**
  ******************************************************************************
  * @file    LSEN.c
  * @brief   BH1750 光照传感器驱动 (I2C1, ADDR=GND → 0x23)
  *          - 连续高分辨率模式, 分辨率 1 lx, 测量时间 ~120ms
  ******************************************************************************
  */
#include "LSEN.h"
#include "i2c.h"

/* === 全局变量实现 === */
float lux = 0.0f;      /* 光照度 (lx) */
uint8_t BH1750_SR = 0; /* 状态: 0=正常, 1=I2C错误 */

/* ========== BH1750 初始化 ========== */
void BH1750_Init (void)
{
    uint8_t cmd;
    HAL_StatusTypeDef ret;

    /* 1. 上电 */
    cmd = BH1750_CMD_POWER_ON;
    ret = HAL_I2C_Master_Transmit(&hi2c1, BH1750_I2C_ADDR << 1, &cmd, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        BH1750_SR = 1;
        return;
    }

    /* 2. 设置为连续高分辨率模式 */
    cmd = BH1750_CMD_CONT_H_RES;
    ret = HAL_I2C_Master_Transmit(&hi2c1, BH1750_I2C_ADDR << 1, &cmd, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        BH1750_SR = 1;
        return;
    }

    BH1750_SR = 0;
}

/* ========== 光照度换算: lx = raw / 1.2 ========== */
float BH1750_CalcLux (uint16_t raw)
{
    return (float)raw / 1.2f;
}

/* ========== 主循环调用: 读取光照度 ========== */
void LSEN_READ (void)
{
    uint8_t buf[2] = {0};
    uint16_t raw;
    HAL_StatusTypeDef ret;

    /* 读取 2 字节: [高字节, 低字节] */
    ret = HAL_I2C_Master_Receive(&hi2c1, BH1750_I2C_ADDR << 1, buf, 2, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        BH1750_SR = 1;
        return;
    }

    /* 组装原始值并换算 */
    raw = ((uint16_t)buf[0] << 8) | buf[1];
    lux = BH1750_CalcLux(raw);

    BH1750_SR = 0;
}
