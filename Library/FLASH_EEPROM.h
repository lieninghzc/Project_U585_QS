/**
  ******************************************************************************
  * @file    FLASH_EEPROM.h
  * @brief   内部 Flash 模拟 EEPROM — 掉电保存关键配置参数
  *
  * @note    使用 U585 内部 Flash 最后一个 8KB 页 (Bank2 Page127) 存储:
  *          - 目标温度 (Heating threshold)
  *          - 目标湿度 (Wet threshold)
  *          - LED 目标照度
  *          - 自动/关闭模式
  *
  *          写入策略: 参数变化时标记脏位, 进入休眠或空闲 3s 后写入,
  *          避免频繁擦除磨损 Flash (8KB 页寿命 ~1 万次擦除)。
  ******************************************************************************
  */

#ifndef __FLASH_EEPROM_H
#define __FLASH_EEPROM_H

#include <stdint.h>

/**
  * @brief  上电初始化: 从 Flash 读取已保存的配置并应用到各模块。
  *         若 Flash 中无有效数据 (首次上电或数据损坏), 保持各模块默认值。
  * @note   必须在所有模块初始化 (Heating_Init, Wet_Init, LED_Init, MENU_Init)
  *         完成之后调用, 以便用已保存值覆盖默认值。
  */
void FlashEE_Init(void);

/**
  * @brief  将当前配置参数立即保存到 Flash。
  *         内部先检查 dirty 标志, 无变化则跳过。
  * @note   此操作会先擦除整个 8KB 页再写入, 耗时约 30~50ms。
  *         调用期间会短暂关闭全局中断, 完成后恢复。
  */
void FlashEE_Save(void);

/**
  * @brief  标记配置已变更, 稍后需要写入 Flash。
  *         在参数被调节时调用 (编码器/语音指令修改参数后)。
  *         不立即写 Flash — 由 FlashEE_Task() 延时 3s 写入,
  *         或由 FlashEE_Save() 在休眠时立即写入。
  */
void FlashEE_MarkDirty(void);

/**
  * @brief  后台任务: 在脏标记置位 3 秒后自动写入 Flash。
  *         在主循环中周期性调用 (100ms 周期, 内部计数)。
  */
void FlashEE_Task(void);

#endif /* __FLASH_EEPROM_H */
