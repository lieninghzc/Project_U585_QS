#ifndef __MENU_H
#define __MENU_H

#include <stdint.h>

/* ======================== 显示模式 ======================== */

typedef enum {
    DISP_MODE_SLEEP  = 0,   /* 休眠: 仅显示温湿度光照 */
    DISP_MODE_ACTIVE = 1,   /* 激活: 完整详细显示 */
} DISP_Mode_t;

/* ======================== 可调参数 ======================== */

typedef enum {
    MENU_PARAM_NONE         = 0,   /* 无选中, 正常显示 */
    MENU_PARAM_TEMP_SET     = 1,   /* 温度设定值 */
    MENU_PARAM_HUM_SET      = 2,   /* 湿度设定值 */
    MENU_PARAM_LED_TARGET   = 3,   /* LED 目标照度 */
    MENU_PARAM_HEAT_SWITCH  = 4,   /* 加热开关 (编码器切换) */
    MENU_PARAM_HUM_SWITCH   = 5,   /* 加湿开关 (编码器切换) */
    MENU_PARAM_LED_SWITCH   = 6,   /* LED 开关 (编码器切换) */
} MENU_Param_t;

/* ======================== 函数接口 ======================== */

/**
  * @brief  初始化菜单模块 (配置 PB5 按键输入)
  */
void MENU_Init(void);

/**
  * @brief  菜单主任务 — 在主循环中调用
  *
  *         处理:
  *           - 按键检测 (PB5, 下降沿触发)
  *           - 编码器旋转检测
  *           - 休眠/激活状态切换
  *           - 3 分钟空闲超时自动休眠
  *           - 参数循环选择与调节
  */
void MENU_Task(void);

/**
  * @brief  获取当前显示模式
  */
DISP_Mode_t MENU_GetDisplayMode(void);

/**
  * @brief  获取当前选中的可调参数
  */
MENU_Param_t MENU_GetSelectedParam(void);

/**
  * @brief  获取当前是否处于参数调节状态
  * @retval 1=有参数被选中待调节, 0=无
  */
uint8_t MENU_IsAdjusting(void);

#endif /* __MENU_H */
