/**
  ******************************************************************************
  * @file    FLASH_EEPROM.c
  * @brief   内部 Flash 模拟 EEPROM — 实现
  *
  * @note    U585 Flash 写粒度 128-bit (quad-word), 按 16 字节对齐。
  *          擦除粒度 8KB/page, 寿命 ~1 万次擦除。
  *          写策略: 空载 3s 或进入休眠时写入, 避免频繁擦除。
  ******************************************************************************
  */

#include "FLASH_EEPROM.h"
#include "stm32u5xx_hal.h"
#include "Heating.h"
#include "Wet.h"
#include "LED.h"
#include "MENU.h"

/* ======================== Flash 页定义 ======================== */
/*
 * U585 2MB Flash, 双 Bank, 每 Bank 128 页 × 8KB
 * 使用最后 1 页: Bank2 Page127, 地址 0x081FE000
 *     0x08000000 + 1MB(Bank2起始) + 127×8KB = 0x081FE000
 */
#define EE_PAGE_ADDR        0x081FE000UL
#define EE_PAGE_BANK        FLASH_BANK_2
#define EE_PAGE_NUM         127

/* ======================== 魔数 ======================== */

#define EE_MAGIC            0x55AA5A5AUL   /* 辨识有效数据 (非 0xFFFFFFFF) */

/* ======================== 延时写参数 ======================== */

#define EE_SAVE_DELAY_TICK  30             /* 3 秒 (每 100ms 调用一次 Task) */

/* ======================== 配置数据结构 ======================== */
/*
 * 严格 16 字节 (一个 quad-word), 16 字节对齐
 *
 * Offset | Size | Field
 * -------|------|-----------------
 *   0    |  4   | magic          — 魔数 0x55AA5A5A
 *   4    |  4   | temp_threshold — 目标温度 (float)
 *   8    |  4   | hum_threshold  — 目标湿度 (float)
 *  12    |  2   | led_target_lux — LED 目标照度 (uint16_t)
 *  14    |  1   | off_mode       — 0=自动, 1=全部关闭
 *  15    |  1   | checksum       — 字段 [4..14] XOR 校验和
 */
typedef struct __attribute__((aligned(16))) {
    uint32_t magic;
    float    temp_threshold;
    float    hum_threshold;
    uint16_t led_target_lux;
    uint8_t  off_mode;
    uint8_t  checksum;
} EE_Config_t;

/*
 * 结构体必须正好 16 字节 (一个 quad-word), 已通过手动布局保证:
 *   4(magic) + 4(temp) + 4(hum) + 2(lux) + 1(mode) + 1(cs) = 16
 */

/* ======================== 内部变量 ======================== */

static uint8_t  ee_dirty  = 0;       /* 1=有未保存的变更 */
static uint16_t ee_timer  = 0;       /* 脏后计时 (单位: Task 调用次数) */
static uint8_t  ee_loaded = 0;       /* 1=已完成上电加载 */

/* ======================== 校验和计算 ======================== */

/**
  * @brief  计算配置数据校验和 (XOR 字段 [4..14], 不含 magic 和 checksum 自身)
  */
static uint8_t ee_calc_checksum (const EE_Config_t* p)
{
    const uint8_t* b = (const uint8_t*)p;
    uint8_t cs = 0;
    /* 字节 4~14: temp_threshold(4) + hum_threshold(4) + led_target_lux(2) + off_mode(1) */
    for (uint8_t i = 4; i < 15; i++)
    {
        cs ^= b[i];
    }
    /* 避免 checksum = 0xFF (擦除态), 翻转 bit7 */
    return (cs == 0xFF) ? 0x7F : cs;
}

/* ======================== Flash 读取 ======================== */

/**
  * @brief  从 Flash 读取已保存的配置。
  * @retval 1=数据有效并已应用到各模块, 0=无有效数据 (首次上电/数据损坏)
  */
static uint8_t ee_read_from_flash (void)
{
    const EE_Config_t* p = (const EE_Config_t*)EE_PAGE_ADDR;

    /* 检查魔数 */
    if (p->magic != EE_MAGIC)
    {
        return 0;
    }

    /* 检查校验和 */
    if (p->checksum != ee_calc_checksum(p))
    {
        return 0;
    }

    /* 数据有效 → 恢复到各模块 */
    Heating_Set(p->temp_threshold);
    Wet_Set(p->hum_threshold);
    LED_SetTargetLux(p->led_target_lux);

    /* 恢复自动/关闭模式 (MENU_SetOffMode 会同步设置 voice_manual_mode 和输出状态) */
    MENU_SetOffMode(p->off_mode);

    return 1;
}

/* ======================== Flash 写入 ======================== */

/**
  * @brief  将当前配置写入 Flash。
  * @retval HAL_OK=成功, 其他=失败
  * @note   先擦除整页 (8KB), 再写一个 quad-word (16B)。
  *         操作期间 Flash 不可读 — CPU 短暂停顿 (约 30~50ms)。
  */
static HAL_StatusTypeDef ee_write_to_flash (void)
{
    HAL_StatusTypeDef status;
    uint32_t page_error = 0;

    /* 构造配置数据 */
    EE_Config_t config __attribute__((aligned(16)));
    config.magic          = EE_MAGIC;
    config.temp_threshold = Heating_GetThreshold();
    config.hum_threshold  = Wet_GetThreshold();
    config.led_target_lux = LED_GetTargetLux();
    config.off_mode       = MENU_IsOffMode();
    config.checksum       = ee_calc_checksum(&config);

    /* --- 1. 解锁 Flash 控制寄存器 --- */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) goto exit;

    /* --- 2. 擦除目标页 --- */
    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Banks     = EE_PAGE_BANK,
        .Page      = EE_PAGE_NUM,
        .NbPages   = 1,
    };
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    if (status != HAL_OK) goto lock_exit;

    /* --- 3. 写入 quad-word (128-bit) --- */
    /* 数据源必须为 32-bit 对齐的 RAM 地址, 此处 config 已 128-bit 对齐 */
    status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,
                               EE_PAGE_ADDR,
                               (uint32_t)&config);
    if (status != HAL_OK) goto lock_exit;

    /* --- 4. 清除 dirty 标志 --- */
    ee_dirty = 0;

lock_exit:
    HAL_FLASH_Lock();
exit:
    return status;
}

/* ======================== 公有函数 ======================== */

/**
  * @brief  上电初始化: 从 Flash 加载配置到各模块。
  */
void FlashEE_Init (void)
{
    ee_dirty  = 0;
    ee_timer  = 0;
    ee_loaded = 1;

    if (ee_read_from_flash())
    {
        /* 成功从 Flash 恢复配置, 各模块已更新 */
    }
    /* 否则保持模块默认值 (Heating 25°C, Wet 60%, LED 1000lux, off_mode=0) */
}

/**
  * @brief  立即保存当前配置到 Flash (若 dirty)。
  */
void FlashEE_Save (void)
{
    if (!ee_loaded) return;   /* 尚未初始化, 跳过 */
    if (!ee_dirty)  return;   /* 无变更, 跳过 */

    ee_write_to_flash();
    /* dirty 在 ee_write_to_flash 内部清除 */
}

/**
  * @brief  标记配置已变更。
  */
void FlashEE_MarkDirty (void)
{
    if (!ee_loaded) return;

    ee_dirty = 1;
    ee_timer = 0;             /* 重置延时计时器 */
}

/**
  * @brief  后台任务: 延时 3 秒后自动写入。
  *         在主循环中每 100ms 调用一次。
  */
void FlashEE_Task (void)
{
    if (!ee_loaded) return;
    if (!ee_dirty)  return;

    ee_timer++;
    if (ee_timer >= EE_SAVE_DELAY_TICK)
    {
        ee_write_to_flash();
        /* dirty 清除, timer 不再增长 */
    }
}
