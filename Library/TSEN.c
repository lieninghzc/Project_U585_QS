#include "TSEN.h"
#include "i2c.h"
#include <math.h>

/* === 全局变量实现 === */
uint16_t rtd_raw = 0;       /* 温度原始ADC值 */
uint16_t hum_raw = 0;       /* 湿度原始ADC值 */
uint8_t SR = 0;             /* 状态/错误码: 0=正常, 1=I2C错误, 2=CRC错误 */
uint8_t testData = 0;       /* CRC校验结果备用 */
float temperature_c = 0.0f; /* 摄氏温度 */
float humidity = 0.0f;      /* 相对湿度 %RH */

/* ========== CRC-8 校验 (多项式 x^8+x^5+x^4+1, 初值 0xFF) ========== */
uint8_t SHT40_CRC8 (const uint8_t* data, uint8_t len)
{
    uint8_t crc = SHT40_CRC_INIT;
    while (len--)
    {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++)
        {
            if (crc & 0x80)
                crc = (uint8_t)((crc << 1) ^ SHT40_CRC_POLY);
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ========== SHT40 初始化 ========== */
void SHT40_Init (void)
{
    uint8_t reset_cmd = SHT40_CMD_SOFT_RESET;

    /* 发送软复位命令 */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(&hi2c1, SHT40_I2C_ADDR << 1, &reset_cmd, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        SR = 1; /* I2C通信错误 */
        return;
    }

    /* 软复位后等待 1ms */
    HAL_Delay(1);

    SR = 0;
}

/* ========== 上电初始化兼容入口 (保持与原有调用一致) ========== */
__attribute__((weak)) void Project_U585_MAX31865_Init (void)
{
    SHT40_Init();
}

/* ========== 温度换算: T[°C] = -45 + 175 * (St / 65535) ========== */
float SHT40_CalcTemperature (uint16_t raw)
{
    return -45.0f + 175.0f * ((float)raw / 65535.0f);
}

/* ========== 湿度换算: RH[%] = -6 + 125 * (Srh / 65535), 钳位 [0,100] ========== */
float SHT40_CalcHumidity (uint16_t raw)
{
    float rh = -6.0f + 125.0f * ((float)raw / 65535.0f);
    if (rh < 0.0f)
        rh = 0.0f;
    if (rh > 100.0f)
        rh = 100.0f;
    return rh;
}

/* ========== 主循环调用: 测量 + 读取温湿度 ========== */
void TSEN_READ (void)
{
    uint8_t cmd = SHT40_CMD_MEASURE_HIGH;
    uint8_t buf[6] = {0};
    HAL_StatusTypeDef ret;

    /* ---- 1. 发送测量命令 ---- */
    ret = HAL_I2C_Master_Transmit(&hi2c1, SHT40_I2C_ADDR << 1, &cmd, 1, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        SR = 1;
        return;
    }

    /* ---- 2. 等待转换完成 (高精度需要 8.3ms, 这里取 10ms) ---- */
    HAL_Delay(10);

    /* ---- 3. 读取 6 字节: [T_MSB, T_LSB, T_CRC, H_MSB, H_LSB, H_CRC] ---- */
    ret = HAL_I2C_Master_Receive(&hi2c1, SHT40_I2C_ADDR << 1, buf, 6, HAL_MAX_DELAY);
    if (ret != HAL_OK)
    {
        SR = 1;
        return;
    }

    /* ---- 4. CRC 校验温度数据 ---- */
    if (SHT40_CRC8(&buf[0], 2) != buf[2])
    {
        SR = 2; /* 温度 CRC 错误 */
        return;
    }

    /* ---- 5. CRC 校验湿度数据 ---- */
    if (SHT40_CRC8(&buf[3], 2) != buf[5])
    {
        SR = 2; /* 湿度 CRC 错误 */
        return;
    }

    /* ---- 6. 组装原始值 ---- */
    rtd_raw = ((uint16_t)buf[0] << 8) | buf[1];
    hum_raw = ((uint16_t)buf[3] << 8) | buf[4];

    /* ---- 7. 换算为物理量 ---- */
    temperature_c = SHT40_CalcTemperature(rtd_raw);
    humidity = SHT40_CalcHumidity(hum_raw);

    /* ---- 8. 状态 OK ---- */
    SR = 0;
}
