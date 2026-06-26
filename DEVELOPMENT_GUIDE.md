# 永雏塔菲智能环境管家 — 完整开发指南

> 芯片：STM32U585CIU6 + ASRPRO (天问)  
> 项目路径：`d:\STM32project\project_U585_QSnew`  
> 更新：2026-06-20 最终版（全功能就绪）

---

## 一、硬件接线

| ASRPRO | STM32U585 | 说明 |
|--------|-----------|------|
| PB5 (UART0_TX) | **PA1** (UART4_RX) | 串口数据（ASR → STM） ✅ |
| PB6 (UART0_RX) | **PA0** (UART4_TX) | 串口数据（STM → ASR） ✅ |
| GND | GND | **必须共地！** |

> UART 必须 TX↔RX 交叉连接，**不要 TX 接 TX**。

---

## 二、通信参数与帧协议

### 2.1 通信参数

| 参数 | 值 |
|------|-----|
| 波特率 | **9600** |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 (8N1) |
| 硬件流控 | **Disable** |
| STM32 UART | UART4 (Asynchronous, PA0=TX PA1=RX) |
| ASRPRO UART | UART0 (PB5=TX PB6=RX) |

### 2.2 ASRPRO → STM32 命令帧（4 字节）

```
[CMD, ARG, 0x00, 0x00]
```

### 2.3 命令码表

| CMD | 名称 | ARG | 类型 |
|:---:|------|:---:|:---:|
| `0x10` | 查询温度 | — | 查询 |
| `0x11` | 查询湿度 | — | 查询 |
| `0x12` | 查询光照 | — | 查询 |
| `0x13` | 查询全部 | — | 查询 |
| `0x20` | 温度调高 | `+1` | 控制 |
| `0x21` | 温度调低 | `-1` | 控制 |
| `0x30` | 湿度调高 | `+5` | 控制 |
| `0x31` | 湿度调低 | `-5` | 控制 |
| `0x40` | 开灯 | — | 控制 |
| `0x41` | 关灯 | — | 控制 |
| `0x42` | 灯光调亮 | `+10` | 控制 |
| `0x43` | 灯光调暗 | `-10` | 控制 |
| `0x50` | 开加热 | — | 控制 |
| `0x51` | 关加热 | — | 控制 |
| `0x52` | 开加湿 | — | 控制 |
| `0x53` | 关加湿 | — | 控制 |
| `0x54` | 自动模式 | — | 控制 |

### 2.4 STM32 → ASRPRO 传感器回传（2 字节大端 int16）

| 命令 | 回传内容 |
|------|---------|
| 0x10 查询温度 | `temperature_c × 10` |
| 0x11 查询湿度 | `humidity × 10` |
| 0x12 查询光照 | `lux` |
| 0x13 查询全部 | 温 2B + 湿 2B + 光 2B = 6 字节 |

---

## 三、STM32U585 — CubeMX 配置

### 3.1 UART4

| 选项卡 | 配置项 | 值 |
|--------|--------|-----|
| **Pinout** | UART4 | 勾选，PA0=TX, PA1=RX |
| **Mode** | Mode | **Asynchronous** |
| | Hardware Flow Control | **Disable** |
| **Parameter Settings** | Baud Rate | 9600 Bits/s |
| | Word Length | 8 Bits |
| | Parity | None |
| | Stop Bits | 1 |
| | Data Direction | Receive and Transmit |
| **NVIC Settings** | UART4 global interrupt | ✅ 勾选 |

### 3.2 初始化调用顺序

```c
MX_UART4_Init();     // CubeMX 自动生成
VOICE_Init();        // 启动 UART4 中断接收
```

> 不需要在 `stm32u5xx_it.c` 写中断处理，`VOICE.c` 已覆盖 `HAL_UART_RxCpltCallback`。

---

## 四、STM32 — 工程结构与 API

### 4.1 工程结构

| 路径 | 用途 |
|------|------|
| `Core/Src/main.c` | 主程序：OLED 显示、传感器、温湿控、VOICE_Process |
| `Core/Inc/main.h` | 引脚宏：`Heating_Pin=PB0` `Wet_Pin=PB1` `LED_Pin=PB7` |
| `Core/Src/usart.c` | UART4 9600 8N1 CubeMX 自动生成 |
| `Core/Src/gpio.c` | GPIO 初始化（**勿手动改**） |
| `Core/Src/stm32u5xx_it.c` | 中断向量，含 `UART4_IRQHandler` |
| `Library/VOICE.c` | UART4 中断接收 + 命令解析 + 传感器回传 |
| `Library/VOICE.h` | 命令码宏 + API 声明 |
| `Library/Heating.c/h` | 加热滞回控制（PB0 高低电平） |
| `Library/Wet.c/h` | 加湿滞回控制（PB1 高低电平） |

### 4.2 新增模块方法

1. 在 `Library/` 下创建 `模块.c` + `模块.h`
2. 在 `main.c` 开头 `/* USER CODE BEGIN Includes */` 区 `#include "模块.h"`
3. 在 `main.c` 初始化区 `/* USER CODE BEGIN 2 */` 调用 `模块_Init()`
4. 在 `while(1)` 中调用控制函数
5. **不要动 CubeMX 生成的 `Core/Src/*.c`**

### 4.3 主循环结构（main.c）

```c
while (1) {
    SENS_Read();                    // 读传感器
    OLED_Clear();
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    Heating_Control(temperature_c); // 温控（滞回）
    Wet_Control(humidity);          // 湿控（滞回）

    /* OLED 第1行: T:+27.39S:25.5H */
    /* OLED 第2行: H:44.23%WS:60 W */
    /* OLED 第3行: L:1000lx        */
    /* OLED 第4行: ENC:0           */

    VOICE_Process();   // ★ 非阻塞 UART 命令处理
    HAL_Delay(1000);
}
```

### 4.4 VOICE 模块 API

| 函数 | 说明 |
|------|------|
| `VOICE_Init()` | 使能 UART4 中断，启动首次接收 |
| `VOICE_Process()` | `while(1)` 非阻塞状态机 |
| `VOICE_IsBusy()` | 查询通信状态 |

VOICE_Process 状态机：

```
IDLE → 启动 HAL_UART_Receive_IT(4字节)
      ↓ ISR 置 voice_rx_done
RX_WAIT → 校验同步字节 → VOICE_ParseCommand()
      ↓ 立即重启 HAL_UART_Receive_IT
CMD_READY → 回到 IDLE
```

回传函数 `VOICE_SendInt16(val)` — 直接写 `UART4->TDR`，2 字节大端序，阻塞发送。

---

### 4.5 OLED 显示布局（16列）

```
第1行: T:+27.39S:25.5H   (T=温度 S=设定值 H=加热中)
第2行: H:44.23%WS:60 W   (H=湿度 WS=湿度设定 W=加湿中)
第3行: L:1000lx          (光照度)
第4行: ENC:0             (编码器)
```

### 4.6 手动/自动模式

- `voice_manual_mode`（`VOICE.c` 全局变量）
  - `0` = 自动模式：`Heating_Control` / `Wet_Control` 根据滞回自动调节
  - `1` = 手动模式：语音开关命令覆写，自动控制不干预
- "打开加热"/"关闭加热"/"打开加湿"/"关闭加湿" → 强制设为手动
- "自动模式" → 恢复为 0

---

## 五、ASRPRO — 完整开发指南

### 5.1 快速开始

```
天问 Block → 主板选 ASRPRO → 字符编程模式 → 粘贴代码 → 生成模型 → 编译下载
编译模式：SSOP24=2M, QFN40=4M
```

### 5.2 必胜法则

| # | 规则 | 原因 |
|---|------|------|
| 1 | `#include "myLib/asr_event.h"` | `play_audio` 声明 |
| 2 | `Serial.begin(9600)` 放 `hardware_init()` | `setup()` 可能不执行 |
| 3 | **弃用 `play_num`，纯 `play_audio` 拼数字** | `play_num` 导致 ASRPRO 死机 |
| 4 | 数字用 `10084 + n` 播放（10084=零,10085=一…） | 预录音频 ID 偏移 |
| 5 | 控制类分 2 次 `Serial.write` 发 4 字节 | 避免 32 位写字节序问题 |
| 6 | 查询类 `delay(1200)` 后读回传 | 等 STM32 主循环处理（1s） |
| 7 | `snid` 读后立即清零 | `uint32_t sid = snid; snid = 0;` |
| 8 | 查询块内**不加** `enter_wakeup`，顶部 `set_state_enter_wakeup(10000)` | 播完重新延长，不用反复唤醒 |
| 9 | **别手写 IOREUSE 寄存器** | `Serial.begin` 自动配置 |
| 10 | `while(Serial.available()) Serial.read()` 清残留 | 避免读到上一帧数据 |

### 5.3 完整 ASRPRO 代码（2026-06-20 最终版）

```cpp
#include "asr.h"
extern "C"{ void * __dso_handle = 0 ;}
#include "setup.h"
#include "HardwareSerial.h"
#include "myLib/asr_event.h"

uint32_t snid;
void ASR_CODE();

//{speak:小英-高兴,vol:10,speed:10,platform:haohaodada,version:V3}
//{playid:10001,voice:欢迎使用永雏塔菲智能管家，用塔菲塔菲唤醒我喵。}
//{playid:10002,voice:塔菲退下了喵…主人要想塔菲哦，用塔菲塔菲唤醒我喵。}
//{playid:10003,voice:当前温度为}
//{playid:10004,voice:当前湿度为}
//{playid:10005,voice:当前光照为}
//{playid:10006,voice:点}
//{playid:10007,voice:摄氏度}
//{playid:10008,voice:百分之}
//{playid:10009,voice:勒克斯}
//{playid:10084,voice:零}
//{playid:10085,voice:一}
//{playid:10086,voice:二}
//{playid:10087,voice:三}
//{playid:10088,voice:四}
//{playid:10089,voice:五}
//{playid:10090,voice:六}
//{playid:10091,voice:七}
//{playid:10092,voice:八}
//{playid:10093,voice:九}
//{playid:10094,voice:十}
//{playid:10095,voice:百}
//{playid:10096,voice:千}
//{playid:10097,voice:万}
//{playid:10098,voice:亿}
//{playid:10099,voice:负}

//{ID:1,keyword:"唤醒词",ASR:"塔菲塔菲",ASRTO:"喵~ 塔菲在这里呢！"}
//{ID:2,keyword:"命令词",ASR:"当前温度",ASRTO:""}
//{ID:3,keyword:"命令词",ASR:"当前湿度",ASRTO:""}
//{ID:4,keyword:"命令词",ASR:"当前光照",ASRTO:""}
//{ID:5,keyword:"命令词",ASR:"环境状态",ASRTO:""}
//{ID:6,keyword:"命令词",ASR:"温度调高",ASRTO:"喵~ 调高了！"}
//{ID:7,keyword:"命令词",ASR:"温度调低",ASRTO:"喵~ 降低了！"}
//{ID:8,keyword:"命令词",ASR:"湿度调高",ASRTO:"喵呜~ 湿度调高了！"}
//{ID:9,keyword:"命令词",ASR:"湿度调低",ASRTO:"喵~ 湿度降低了！"}
//{ID:10,keyword:"命令词",ASR:"打开灯光",ASRTO:"喵~ 灯亮了！"}
//{ID:11,keyword:"命令词",ASR:"关闭灯光",ASRTO:"喵…灯灭了！"}
//{ID:12,keyword:"命令词",ASR:"灯光调亮",ASRTO:"喵呜~ 更亮了！"}
//{ID:13,keyword:"命令词",ASR:"灯光调暗",ASRTO:"喵~ 暗一点了…"}
//{ID:14,keyword:"命令词",ASR:"打开加热",ASRTO:"喵~ 加热已开启！"}
//{ID:15,keyword:"命令词",ASR:"关闭加热",ASRTO:"喵~ 加热关闭了！"}
//{ID:16,keyword:"命令词",ASR:"打开加湿",ASRTO:"喵呜~ 加湿器启动了！"}
//{ID:17,keyword:"命令词",ASR:"关闭加湿",ASRTO:"喵~ 加湿器关掉了！"}
//{ID:18,keyword:"命令词",ASR:"自动模式",ASRTO:"喵~ 自动温湿度控制！"}
//{ID:19,keyword:"退出词",ASR:"退下吧",ASRTO:"喵…塔菲会想你的…"}

/* ====== 纯 play_audio 拼数字播报 — 不用 play_num ====== */
void speak(int16_t n) {
    if (n < 0) { play_audio(10099); n = -n; }
    if (n == 0) { play_audio(10084); return; }
    if (n >= 1000) { play_audio(10084 + n/1000); play_audio(10096); n %= 1000; }
    if (n >= 100)  { play_audio(10084 + n/100);  play_audio(10095); n %= 100; }
    if (n >= 10)   {
        if (n/10 > 1) play_audio(10084 + n/10);
        play_audio(10094); n %= 10;
    }
    if (n > 0) play_audio(10084 + n);
}

void speak_lux(int16_t n) {
    if (n == 0) { play_audio(10084); return; }
    if (n >= 10000) { speak(n/10000); play_audio(10097); n %= 10000; }
    speak(n);
}

void hardware_init() {
    Serial.begin(9600);
    vTaskDelete(NULL);
}

void ASR_CODE() {
    set_state_enter_wakeup(10000);
    uint32_t sid = snid; snid = 0;

    /* 控制类 — 分 2 次发 4 字节 */
    if (sid >= 6 && sid <= 18) {
        if(sid == 6)  { Serial.write(0x20); Serial.write(0x01); }
        if(sid == 7)  { Serial.write(0x21); Serial.write(0xFF); }
        if(sid == 8)  { Serial.write(0x30); Serial.write(0x05); }
        if(sid == 9)  { Serial.write(0x31); Serial.write(0xFB); }
        if(sid == 10) { Serial.write(0x40); Serial.write(0x00); }
        if(sid == 11) { Serial.write(0x41); Serial.write(0x00); }
        if(sid == 12) { Serial.write(0x42); Serial.write(0x0A); }
        if(sid == 13) { Serial.write(0x43); Serial.write(0xF6); }
        if(sid == 14) { Serial.write(0x50); Serial.write(0x00); }
        if(sid == 15) { Serial.write(0x51); Serial.write(0x00); }
        if(sid == 16) { Serial.write(0x52); Serial.write(0x00); }
        if(sid == 17) { Serial.write(0x53); Serial.write(0x00); }
        if(sid == 18) { Serial.write(0x54); Serial.write(0x00); }
        Serial.write(0x00); Serial.write(0x00);
        return;
    }

    /* 查询类 — 清残留 + delay(1200) + 纯 play_audio 播报 */
    if (sid >= 2 && sid <= 5) {
        uint8_t cmd = (sid==2)?0x10 : (sid==3)?0x11 : (sid==4)?0x12 : 0x13;
        uint8_t need = (sid == 5) ? 6 : 2;
        while(Serial.available()) Serial.read();
        Serial.write(cmd); Serial.write(0x00); Serial.write(0x00); Serial.write(0x00);
        delay(1200);

        uint8_t b[6], n = 0;
        while (n < need && Serial.available()) b[n++] = (uint8_t)Serial.read();

        if (n >= 2) {
            int16_t v = ((int16_t)b[0] << 8) | b[1];
            if (sid == 2 || sid == 5) {
                play_audio(10003); speak(v / 10);
                play_audio(10006); speak(v % 10);
                play_audio(10007);
            }
            if (sid == 3 || sid == 5) {
                if(sid==5){v=((int16_t)b[2]<<8)|b[3];}
                play_audio(10004); play_audio(10008);
                speak(v / 10); play_audio(10006); speak(v % 10);
            }
            if (sid == 4 || sid == 5) {
                if(sid==5) v=((int16_t)b[4]<<8)|b[5];
                play_audio(10005); speak_lux(v);
                play_audio(10009);
            }
            set_state_enter_wakeup(10000);  // 播完延长 10 秒
        }
    }
}

void setup() {}
```

### 5.4 添加新命令词

**ASRPRO：**

1. 注释区加词条：`//{ID:20,keyword:"命令词",ASR:"新命令",ASRTO:"回应！"}`
2. 新查询类：照 `sid == 2` 模板复制
3. 新控制类：`if(sid == 20) { Serial.write(CMD); Serial.write(ARG); Serial.write(0x00); Serial.write(0x00); }`

**STM32：**

1. `VOICE.h` 加 `#define VOICE_CMD_XXX 0xNN`
2. `VOICE.c` → `VOICE_ParseCommand()` 的 `switch(cmd)` 加 `case`

---

## 六、ASRPRO 动态播报机制（纯 play_audio 拼数字）

### 6.1 核心 API

| 函数 | 用途 |
|------|------|
| `play_audio(ID)` | 播放预录固定语音 |
| `speak(n)` | 播报整数 n（自动拼千百十个） |
| `speak_lux(n)` | 播报光照值（支持万位） |
| `set_state_enter_wakeup(10000)` | 唤醒 10 秒，查询播完后重调延长 |

> **禁用 `play_num`** — 会导致 ASRPRO 死机。

### 6.2 播报浮点数技巧

```cpp
// STM32 发 273 = 27.3°C，ASRPRO 收到 v=273
speak(v / 10);   // "二十七"
play_audio(10006); // "点"
speak(v % 10);   // "三"
// → "二十七点三摄氏度"
```

### 6.3 数字 ID 映射

```
10084 = 零    10085 = 一    10086 = 二    10087 = 三    10088 = 四
10089 = 五    10090 = 六    10091 = 七    10092 = 八    10093 = 九
10094 = 十    10095 = 百    10096 = 千    10097 = 万    10099 = 负
```

`speed(n)` 内公式：`10084 + n` 映射数字 0-9 到正确音频 ID。

---

## 七、ASRPRO 外设寄存器参考（仅供理解，实战用 Serial API）

### 7.1 外设基址

| 外设 | 基址 |
|------|------|
| UART0 | `0x40022000` |
| UART1 | `0x40023000` |
| UART2 | `0x40024000` |
| DPMU | `0x40030000` |

### 7.2 UART0 数据发送寄存器

- `UART_WDATA`（偏移 0x04）— 32 位写入，低 8 位为数据
- 波特率：PCLK = 50MHz, 9600bps → IBRD = 325, FBRD = 33
- LCR: `0x00000070` (8N1 FIFO 开), CR: `0x00000301` (TX+RX+UART 使能)

### 7.3 IO 复用（DPMU）

- `CFG_LOCK_CFG`（偏移 0x00）— 先写 `0x51AC0FFE` 解锁
- `IOREUSE_CFG0`（0x140）— PB0-PB5 复用控制（2bit/引脚）
- `IOREUSE_CFG1`（0x144）— PB6-PB7 复用控制

> **实战中不要手写这些寄存器，`Serial.begin(9600)` 自动完成全部配置。**

---

## 八、编译烧录

```
STM32：
D:\Keil\UV4\UV4.exe -f d:\STM32project\project_U585_QSnew\MDK-ARM\project_U585_QS.uvprojx -j0 -t project_U585_QS

ASRPRO：
天问 Block → 选主板 ASRPRO → 字符编程 → 生成模型 → 编译下载
SSOP24 选 2M / QFN40 选 4M
```

---

## 九、故障排查

| 现象 | 可能原因 | 检查方法 |
|------|----------|----------|
| 语音无反应 | 命令词没触发 | ASRPRO 代码确认 `ASRTO` 有台词设定 |
| STM32 收不到数据 | 接线交叉错 / GND 未共地 | PB5→PA1, PB6→PA0, 万用表量 GND |
| OLED 收到但 CMD=0x00 | ASRPRO IO 复用未生效 | 确保 `Serial.begin` 在 `hardware_init()` |
| 帧校验失败 | 波特率偏差 | 示波器看波形 |
| UART4 中断不触发 | NVIC 未勾选 | CubeMX NVIC 选项卡确认 |
| 播报后卡死 | 用了 `play_num` | 改用纯 `play_audio` + `speak()` 函数 |
| 播报数字不对（偏一位） | ID 偏移错 | `10084 + n` 不是 `10085 + n` |
| 查询后立即退出 | 查询块内调了 `enter_wakeup` | 删掉，播完用 `set_state_enter_wakeup(10000)` |
| ASRPRO 编译报错 | 缺少头文件 | `#include "myLib/asr_event.h"` 和 `#include "HardwareSerial.h"` |
| 烧录失败 | 未登录/实名 / 编译模式错 | 登录 + 实名 + 选对 2M/4M |
| 只能发不能收 | Hardware Flow Control | CubeMX 选 **Disable** |

---

## 十、参考

- 天问 ASRPRO 芯片手册：`information/asr_pro_core.pdf`
- 天问 ASRPRO 编程手册：`information/asr_pro.pdf`
- STM32U585 参考手册：RM0456
- 天问 Block：https://www.haohaodada.com
- 项目内存档：`/memories/session/project_U585_QS_migration.md`
