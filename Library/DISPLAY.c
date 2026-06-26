#include "DISPLAY.h"
#include "OLED.h"
#include "TSEN.h"
#include "LSEN.h"
#include "Heating.h"
#include "Wet.h"
#include "ENCODER.h"
#include "LED.h"
#include <string.h>

/* ======================== 常量 ======================== */

#define LINE_LEN    16                             /* 每行字符数 */
#define LINE_COUNT  4                              /* 总行数 */

/* ======================== 阴影缓冲 ========================
 * shadow[line][col] 记录 OLED 当前实际显示的字符
 * DISPLAY_Update() 逐一对比, 仅变化位置才发起 I2C 写入,
 * 彻底消除无效刷新和闪烁.
 */
static char shadow[LINE_COUNT][LINE_LEN];
static char line_buf[LINE_LEN];                    /* 行格式化暂存 */

/* ======================== 底层: 刷新一行到 OLED ======================== */

/**
  * @brief  将 line_buf[] 与 shadow[line] 逐字符对比,
  *         将变化位置写入 OLED 并更新 shadow.
  * @param  line  行索引 0~3
  */
static void flush_line(uint8_t line)
{
    for (uint8_t col = 0; col < LINE_LEN; col++)
    {
        if (shadow[line][col] != line_buf[col])
        {
            OLED_ShowChar(line + 1, col + 1, line_buf[col]);
            shadow[line][col] = line_buf[col];
        }
    }
}

/* ======================== 行格式化 ======================== */

/**
  * 行1 — 温度 / 设定值 / 加热指示
  *
  * 格式: T:+25.39CS:25.5H  (16 列)
  *       T:+25.39CS:25.5   (不加热时末位空格)
  */
static void format_line_1(void)
{
    memset(line_buf, ' ', LINE_LEN);

    int temp_int = (int)(temperature_c * 100.0f);
    uint8_t i = 0;

    line_buf[i++] = 'T';
    line_buf[i++] = ':';
    if (temp_int < 0)
    {
        line_buf[i++] = '-';
        temp_int = -temp_int;
    }
    else
    {
        line_buf[i++] = '+';
    }
    line_buf[i++] = '0' + (temp_int / 100) / 10;
    line_buf[i++] = '0' + (temp_int / 100) % 10;
    line_buf[i++] = '.';
    line_buf[i++] = '0' + (temp_int % 100) / 10;
    line_buf[i++] = '0' + (temp_int % 100) % 10;
    line_buf[i++] = 'C';

    /* 设定值 S:XX.X */
    {
        float sp = Heating_GetThreshold();
        int sp_int = (int)(sp * 10.0f + (sp >= 0 ? 0.05f : -0.05f));
        line_buf[i++] = 'S';
        line_buf[i++] = ':';
        line_buf[i++] = '0' + (sp_int / 10) / 10;
        line_buf[i++] = '0' + (sp_int / 10) % 10;
        line_buf[i++] = '.';
        line_buf[i++] = '0' + (sp_int % 10 + 10) % 10;
    }

    /* 加热指示 (最后1列) */
    line_buf[i] = Heating_GetState() ? 'H' : ' ';

    flush_line(0);
}

/**
  * 行2 — 湿度 / 设定值 / 加湿指示
  *
  * 格式: H: 44.23%WS:60W   (16 列)
  */
static void format_line_2(void)
{
    memset(line_buf, ' ', LINE_LEN);

    int hum_int = (int)(humidity * 100.0f);
    uint8_t i = 0;

    line_buf[i++] = 'H';
    line_buf[i++] = ':';

    /* 湿度整数 3 位 (高位补空格, ShowNum 风格) */
    uint32_t hum_int_part = hum_int / 100;
    if (hum_int_part >= 100) line_buf[i++] = '0' + (hum_int_part / 100) % 10;
    else                     line_buf[i++] = ' ';
    if (hum_int_part >= 10)  line_buf[i++] = '0' + (hum_int_part / 10) % 10;
    else                     line_buf[i++] = ' ';
    line_buf[i++] = '0' + hum_int_part % 10;
    line_buf[i++] = '.';
    line_buf[i++] = '0' + (hum_int % 100) / 10;
    line_buf[i++] = '0' + (hum_int % 100) % 10;
    line_buf[i++] = '%';

    /* 设定值 WS:XX */
    {
        float wsp = Wet_GetThreshold();
        uint32_t wsp_int = (uint32_t)(wsp + 0.5f);
        line_buf[i++] = 'W';
        line_buf[i++] = 'S';
        line_buf[i++] = ':';
        line_buf[i++] = '0' + (wsp_int / 10) % 10;
        line_buf[i++] = '0' + wsp_int % 10;
    }

    /* 加湿指示 */
    line_buf[i] = Wet_GetState() ? 'W' : ' ';

    flush_line(1);
}

/**
  * 行3 — 光照度 (BH1750) + 目标照度
  *
  * 格式: L:12345lx  500   (16 列, 正常)
  *       L:  500lx  500   (16 列)
  *       L:12345lxERR     (16 列, 传感器错误)
  */
static void format_line_3(void)
{
    memset(line_buf, ' ', LINE_LEN);

    uint8_t i = 0;
    line_buf[i++] = 'L';
    line_buf[i++] = ':';

    /* 5 位光度值 (高位补空格) */
    uint32_t lux_int = (uint32_t)lux;
    char digits[5];
    for (int d = 4; d >= 0; d--)
    {
        digits[d] = '0' + lux_int % 10;
        lux_int /= 10;
    }
    uint8_t start = 0;
    while (start < 4 && digits[start] == '0') digits[start++] = ' ';
    for (uint8_t d = start; d < 5; d++) line_buf[i++] = digits[d];

    /* "lx" */
    line_buf[i++] = 'l';
    line_buf[i++] = 'x';
    i++;  /* 列 9: 分隔空格 */

    /* 目标照度 (右对齐 5 位, 列 10~14) */
    if (BH1750_SR == 0)
    {
        uint16_t tgt = LED_GetTargetLux();
        char tbuf[5];
        for (int d = 4; d >= 0; d--)
        {
            tbuf[d] = '0' + tgt % 10;
            tgt /= 10;
        }
        uint8_t skip = 0;
        while (skip < 4 && tbuf[skip] == '0')
        {
            line_buf[10 + skip] = ' ';
            skip++;
        }
        for (uint8_t d = skip; d < 5; d++)
            line_buf[10 + d] = tbuf[d];
    }
    else
    {
        line_buf[10] = 'E';
        line_buf[11] = 'R';
        line_buf[12] = 'R';
    }

    flush_line(2);
}

/**
  * 行4 — 编码器增量
  *
  * 格式: ENC:+    1234     (16 列)
  *       ENC:-      42
  */
static void format_line_4(void)
{
    memset(line_buf, ' ', LINE_LEN);

    int32_t delta = ENCODER_GetDelta();
    uint8_t i = 0;

    line_buf[i++] = 'E';
    line_buf[i++] = 'N';
    line_buf[i++] = 'C';
    line_buf[i++] = ':';

    /* 符号位 */
    uint32_t abs_val;
    if (delta < 0)
    {
        line_buf[i++] = '-';
        abs_val = (uint32_t)(-delta);
    }
    else
    {
        line_buf[i++] = '+';
        abs_val = (uint32_t)delta;
    }

    /* 8 位数字 (右对齐, 高位补空格) — 模拟 OLED_ShowSignedNum */
    char num_str[8];
    for (int d = 7; d >= 0; d--)
    {
        num_str[d] = '0' + abs_val % 10;
        abs_val /= 10;
    }
    uint8_t start = 0;
    while (start < 7 && num_str[start] == '0') num_str[start++] = ' ';
    for (uint8_t d = start; d < 8; d++) line_buf[i++] = num_str[d];

    flush_line(3);
}

/* ======================== 公有函数 ======================== */

/**
  * @brief  初始化 OLED + 显示启动画面
  */
void DISPLAY_Init(void)
{
    OLED_Init();
    OLED_Clear();

    /* 清空阴影缓冲, 保证首次 DISPLAY_Update 全量写入 */
    memset(shadow, 0, sizeof(shadow));

    /* 启动画面 */
    OLED_ShowString(2, 4, "U585 OK!");
    HAL_Delay(500);
    OLED_Clear();

    /* 再次清空阴影, 使下一轮全量刷新 */
    memset(shadow, 0, sizeof(shadow));
}

/**
  * @brief  刷新所有显示行
  *
  *         需确保调用前 SENS_Read() 已执行,
  *         各传感器数值为最新.
  *
  *         内部按行逐一格式化 → 对比阴影缓冲 →
  *         仅变化字符写入 OLED.
  */
void DISPLAY_Update(void)
{
    format_line_1();
    format_line_2();
    format_line_3();
    format_line_4();
}

/**
  * @brief  强制刷新指定行
  * @param  line  行号 1~4
  */
void DISPLAY_FlushLine(uint8_t line)
{
    if (line < 1 || line > LINE_COUNT) return;
    uint8_t idx = line - 1;

    /* 清空该行阴影标记, 使下次 format 全量写入 */
    memset(shadow[idx], 0, LINE_LEN);

    /* 按行号重新格式化 */
    switch (idx)
    {
    case 0: format_line_1(); break;
    case 1: format_line_2(); break;
    case 2: format_line_3(); break;
    case 3: format_line_4(); break;
    }
}
