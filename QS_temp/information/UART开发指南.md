# ASRPRO + STM32U585 UART 通信开发指南

> 适用：天问 ASRPRO（QFN40/SSOP24）与 STM32U585CIU6 之间 UART 串口通信
> 项目：永雏塔菲智能环境管家（嵌赛）

---

## 一、硬件接线

| ASRPRO | STM32U585 | 说明 |
|--------|-----------|------|
| PB5 (UART0_TX) | **PA1** (UART4_RX) | 串口数据（ASR → STM） |
| PB6 (UART0_RX) | **PA0** (UART4_TX) | 串口数据（STM → ASR，预留） |
| GND | GND | **必须共地！** |

---

## 二、通信参数

| 参数 | 值 |
|------|-----|
| 波特率 | **9600** |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 (8N1) |
| 硬件流控 | **Disable**（无 CTS/RTS 引脚） |
| ASRPRO UART | UART0 (基址 `0x40022000`) |
| STM32 UART | UART4 (Asynchronous 模式) |

---

## 三、帧协议

### 3.1 ASRPRO → STM32（命令帧，4 字节）

```
字节0: CMD   命令码
字节1: ARG   有符号参数 (int8)
字节2: 0x00  同步填充
字节3: 0x00  同步填充
```

ASRPRO 侧发送（写 32 位寄存器 = 发 4 字节）：

```cpp
// 无参数命令: 发 [CMD, 0x00, 0x00, 0x00]
UART0_WDATA = (uint32_t)CMD;

// 带参数命令: 发 [CMD, ARG, 0x00, 0x00]
UART0_WDATA = (uint32_t)(((uint32_t)(uint8_t)(ARG) << 8) | (uint32_t)CMD);
```

### 3.2 命令码表

| CMD | 名称 | ARG | 说明 |
|-----|------|-----|------|
| `0x10` | 查询温度 | — | 刷新 SHT40 温度 |
| `0x11` | 查询湿度 | — | 刷新 SHT40 湿度 |
| `0x12` | 查询光照 | — | 刷新 BH1750 光照 |
| `0x13` | 查询全部 | — | 刷新全部传感器 |
| `0x20` | 温度调高 | `+1` | 设定值 +1°C |
| `0x21` | 温度调低 | `-1` | 设定值 -1°C |
| `0x30` | 湿度调高 | `+5` | 设定值 +5%RH |
| `0x31` | 湿度调低 | `-5` | 设定值 -5%RH |
| `0x40` | 开灯 | — | LED 100% PWM |
| `0x41` | 关灯 | — | LED 0% PWM |
| `0x42` | 灯光调亮 | `+10` | PWM +10% |
| `0x43` | 灯光调暗 | `-10` | PWM -10% |
| `0x50` | 开加热 | — | 强制开 PB0 |
| `0x51` | 关加热 | — | 强制关 PB0 |
| `0x52` | 开加湿 | — | 强制开 PB1 |
| `0x53` | 关加湿 | — | 强制关 PB1 |
| `0x54` | 自动模式 | — | 恢复温湿度滞回控制 |

---

## 四、STM32U585 CubeMX 配置

### 4.1 UART4 配置

| 选项卡 | 配置项 | 值 |
|--------|--------|-----|
| **Pinout** | UART4 | 勾选，引脚自动分配 **PA0=TX, PA1=RX** |
| **Mode** | Mode | **Asynchronous** |
| | Hardware Flow Control | **Disable** |
| **Configuration → Parameter Settings** | Baud Rate | 9600 Bits/s |
| | Word Length | 8 Bits (including Parity) |
| | Parity | None |
| | Stop Bits | 1 |
| | Data Direction | Receive and Transmit |
| | Over Sampling | 16 Samples |
| **NVIC Settings** | UART4 global interrupt | ✅ 勾选 |

### 4.2 代码中调用顺序

```c
// main.c 中的初始化顺序：
MX_UART4_Init();    // CubeMX 自动生成
VOICE_Init();       // 启动 UART4 中断接收
```

> 不需要在 `stm32u5xx_it.c` 里写中断处理，`VOICE.c` 已覆盖 `HAL_UART_RxCpltCallback`。

---

## 五、STM32 代码 API（VOICE.c / VOICE.h）

| 函数 | 说明 |
|------|------|
| `VOICE_Init()` | 初始化 UART4 中断接收、启动首次 RX |
| `VOICE_Process()` | 状态机主循环，放 `while(1)` 中，完全非阻塞 |
| `VOICE_IsBusy()` | 返回 0=空闲，1=通信中 |

### 架构

```
while(1) {
    SENS_Read();           // 传感器采集
    Heating_Control();     // 温控
    OLED 显示更新;
    VOICE_Process();       // ★ 检查收到的命令并执行
}
```

`VOICE_Process()` 内部状态机：

```
IDLE → 启动 HAL_UART_Receive_IT(4字节)
      ↓ ISR 置 voice_rx_done
RX_WAIT → 校验同步字节 → VOICE_ParseCommand()
      ↓ 立即重启 HAL_UART_Receive_IT
CMD_READY → 回到 IDLE
```

---

## 六、ASRPRO 外设寄存器

### 6.1 外设基址

| 外设 | 基址 |
|------|------|
| UART0 | `0x40022000` |
| UART1 | `0x40023000` |
| UART2 | `0x40024000` |
| DPMU（系统控制/IO 复用） | `0x40030000` |

### 6.2 UART 寄存器

| 偏移 | 名称 | 位宽 | 类型 | 说明 |
|------|------|------|------|------|
| `0x04` | UART_WDATA | 32 | W | 写数据寄存器，写 32 位 = 发 4 字节 |
| `0x0C` | UART_FLAG | 32 | R | 状态标志 |
| `0x10` | UART_IBRD | 32 | R/W | 波特率分频整数部分 |
| `0x14` | UART_FBRD | 32 | R/W | 波特率分频小数部分 |
| `0x18` | UART_LCR | 32 | R/W | 线控寄存器 |
| `0x1C` | UART_CR | 32 | R/W | 控制寄存器 |

#### UART_LCR 常用值

| 值 | 配置 |
|----|------|
| `0x00000060` | 8 数据位，1 停止位，无校验，FIFO 关 |
| `0x00000070` | 8 数据位，1 停止位，无校验，FIFO **开** ✅ |

#### UART_CR 常用值

| 值 | 含义 |
|----|------|
| `0x00000301` | TX 使能 + RX 使能 + UART 使能 |

#### 波特率计算公式

```
BRD = PCLK / (16 × Baud)
IBRD = 整数部分
FBRD = round(小数部分 × 64)
```

> PCLK = 50MHz, 9600bps → IBRD = 325, FBRD = 33

### 6.3 DPMU 寄存器（IO 复用）

| 偏移 | 名称 | 说明 |
|------|------|------|
| `0x00` | CFG_LOCK_CFG | 配置锁定，**必须先写 `0x51AC0FFE` 解锁** |
| `0x140` | IOREUSE_CFG0 | PA 口 IO 复用配置 |
| `0x144` | IOREUSE_CFG1 | PB 口 IO 复用配置 |

### 6.4 IO 复用配置步骤

```cpp
CFG_LOCK_CFG = 0x51AC0FFE;          // 1. 必须先解锁
IOREUSE_CFGx &= ~(0x7 << BITPOS);   // 2. 清除目标位段
IOREUSE_CFGx |=  (FUNC << BITPOS);  // 3. 写入功能号
```

| 引脚 | 寄存器 | 位段 | UART 功能号 |
|------|--------|------|:----------:|
| PB5 | IOREUSE_CFG1 | [22:20] | 2 (UART0_TX) |
| PB6 | IOREUSE_CFG1 | [2:0]   | 2 (UART0_RX) |

---

## 七、ASRPRO 完整代码（复制到天问 Block 字符编程模式）

```cpp
#include "asr.h"
extern "C"{ void * __dso_handle = 0 ;}
#include "setup.h"

uint32_t snid;
void ASR_CODE();

//{speak:小英-高兴,vol:10,speed:10,platform:haohaodada,version:V3}
//{playid:10001,voice:欢迎使用永雏塔菲智能管家，用塔菲塔菲唤醒我喵。}
//{playid:10002,voice:塔菲退下了喵…主人要想塔菲哦，用塔菲塔菲唤醒我喵。}

//{ID:1,keyword:"唤醒词",ASR:"塔菲塔菲",ASRTO:"喵~ 塔菲在这里呢！主人有什么吩咐？记得关注永雏塔菲喵~"}
//{ID:2,keyword:"命令词",ASR:"当前温度",ASRTO:"喵~ 塔菲看看温度计…屏幕上已经显示啦！关注永雏塔菲了没喵？"}
//{ID:3,keyword:"命令词",ASR:"当前湿度",ASRTO:"喵呜~ 湿度数据已更新！主人要记得让塔菲的房间也保持湿润喵~"}
//{ID:4,keyword:"命令词",ASR:"当前光照",ASRTO:"喵~ 光照出来了！太亮塔菲眼睛会不舒服…快去看屏幕喵！"}
//{ID:5,keyword:"命令词",ASR:"环境状态",ASRTO:"喵~ 全部环境数据都刷新在屏幕上啦！永雏塔菲会一直陪着主人的~"}
//{ID:6,keyword:"命令词",ASR:"温度调高",ASRTO:"喵~ 调高了！塔菲也喜欢暖暖的，像主人的怀抱一样~"}
//{ID:7,keyword:"命令词",ASR:"温度调低",ASRTO:"喵~ 降低了！主人热的话塔菲用小爪子帮你扇扇风~"}
//{ID:8,keyword:"命令词",ASR:"湿度调高",ASRTO:"喵呜~ 湿度调高了！塔菲的毛也会更柔顺哦~"}
//{ID:9,keyword:"命令词",ASR:"湿度调低",ASRTO:"喵~ 湿度降低了！干燥一点也好喵~"}
//{ID:10,keyword:"命令词",ASR:"打开灯光",ASRTO:"喵~ 灯亮了！这样主人就能看清塔菲可爱的样子啦！"}
//{ID:11,keyword:"命令词",ASR:"关闭灯光",ASRTO:"喵…灯灭了，但塔菲在黑暗中也能找到主人哦~"}
//{ID:12,keyword:"命令词",ASR:"灯光调亮",ASRTO:"喵呜~ 更亮了！塔菲的毛色在灯光下是不是更好看了？"}
//{ID:13,keyword:"命令词",ASR:"灯光调暗",ASRTO:"喵~ 暗一点更有氛围…主人要睡了喵？"}
//{ID:14,keyword:"命令词",ASR:"打开加热",ASRTO:"喵~ 加热已开启！塔菲也想一起取暖…可以蹭蹭主人吗？"}
//{ID:15,keyword:"命令词",ASR:"关闭加热",ASRTO:"喵~ 加热关闭了！主人冷的话抱紧塔菲哦，塔菲的毛很暖和的！"}
//{ID:16,keyword:"命令词",ASR:"打开加湿",ASRTO:"喵呜~ 加湿器启动了！空气湿润润的塔菲鼻子也舒服多啦~"}
//{ID:17,keyword:"命令词",ASR:"关闭加湿",ASRTO:"喵~ 加湿器关掉了！塔菲可是很关心主人健康的！"}
//{ID:18,keyword:"命令词",ASR:"自动模式",ASRTO:"喵~ 已切换到自动温湿度控制啦！永雏塔菲很靠谱的！"}
//{ID:19,keyword:"退出词",ASR:"退下吧",ASRTO:"喵…塔菲会想你的…主人要早点回来唤醒塔菲哦！"}

/* ====== UART0 寄存器 ====== */
#define UART0_WDATA  (*(volatile uint32_t*)(0x40022000 + 0x04))
#define UART0_IBRD   (*(volatile uint32_t*)(0x40022000 + 0x10))
#define UART0_FBRD   (*(volatile uint32_t*)(0x40022000 + 0x14))
#define UART0_LCR    (*(volatile uint32_t*)(0x40022000 + 0x18))
#define UART0_CR     (*(volatile uint32_t*)(0x40022000 + 0x1C))

#define CFG_LOCK_CFG (*(volatile uint32_t*)(0x40030000 + 0x00))
#define IOREUSE_CFG1 (*(volatile uint32_t*)(0x40030000 + 0x144))

/* ====== 命令码 (与 STM32 VOICE.h 一致) ====== */
#define CMD_REPORT_TEMP  0x10
#define CMD_REPORT_HUM   0x11
#define CMD_REPORT_LUX   0x12
#define CMD_REPORT_ALL   0x13
#define CMD_TEMP_UP      0x20
#define CMD_TEMP_DOWN    0x21
#define CMD_HUM_UP       0x30
#define CMD_HUM_DOWN     0x31
#define CMD_LED_ON       0x40
#define CMD_LED_OFF      0x41
#define CMD_LED_BRIGHTER 0x42
#define CMD_LED_DIMMER   0x43
#define CMD_HEAT_ON      0x50
#define CMD_HEAT_OFF     0x51
#define CMD_WET_ON       0x52
#define CMD_WET_OFF      0x53
#define CMD_AUTO_MODE    0x54

#define SEND(cmd)     (UART0_WDATA = (uint32_t)(cmd))
#define SEND_ARG(c,a) (UART0_WDATA = (uint32_t)(((uint32_t)(uint8_t)(a) << 8) | (uint32_t)(c)))

void ASR_CODE()
{
  set_state_enter_wakeup(10000);

  if(snid == 2)  SEND(CMD_REPORT_TEMP);
  if(snid == 3)  SEND(CMD_REPORT_HUM);
  if(snid == 4)  SEND(CMD_REPORT_LUX);
  if(snid == 5)  SEND(CMD_REPORT_ALL);
  if(snid == 6)  SEND_ARG(CMD_TEMP_UP,   1);
  if(snid == 7)  SEND_ARG(CMD_TEMP_DOWN, -1);
  if(snid == 8)  SEND_ARG(CMD_HUM_UP,    5);
  if(snid == 9)  SEND_ARG(CMD_HUM_DOWN, -5);
  if(snid == 10) SEND(CMD_LED_ON);
  if(snid == 11) SEND(CMD_LED_OFF);
  if(snid == 12) SEND_ARG(CMD_LED_BRIGHTER, 10);
  if(snid == 13) SEND_ARG(CMD_LED_DIMMER,  -10);
  if(snid == 14) SEND(CMD_HEAT_ON);
  if(snid == 15) SEND(CMD_HEAT_OFF);
  if(snid == 16) SEND(CMD_WET_ON);
  if(snid == 17) SEND(CMD_WET_OFF);
  if(snid == 18) SEND(CMD_AUTO_MODE);
}

void hardware_init() { vol_set(10); vTaskDelete(NULL); }

void setup()
{
  /* ====== PB5 → UART0_TX, PB6 → UART0_RX ====== */
  CFG_LOCK_CFG = 0x51AC0FFE;
  IOREUSE_CFG1 &= ~(0x7 << 20);
  IOREUSE_CFG1 |=  (0x2 << 20);
  IOREUSE_CFG1 &= ~(0x7 << 0);
  IOREUSE_CFG1 |=  (0x2 << 0);

  /* ====== UART0: 9600, 8N1, FIFO 开 ====== */
  UART0_CR   = 0;
  UART0_IBRD = 325;
  UART0_FBRD = 33;
  UART0_LCR  = 0x00000070;
  UART0_CR   = 0x00000301;
}
```

### ASRPRO 烧录步骤

1. 打开天问 Block → 选主板 **ASRPRO** → 切换到**字符编程模式**
2. 清空编辑器 → 粘贴上面全部代码
3. 编译模式：SSOP24 选 **2M**，QFN40 选 **4M**
4. 点击 **生成模型**（需登录 + 实名认证）
5. 点击 **编译下载**

---

## 八、STM32 完整代码概览

### 8.1 VOICE.h（命令码 + API）

```c
// 硬件
#define VOICE_UART_HANDLE (&huart4)    // UART4
#define VOICE_UART_BAUD 9600

// 帧
#define VOICE_FRAME_LEN 4
#define VOICE_FRAME_SYNC0 0x00
#define VOICE_FRAME_SYNC1 0x00

// 命令码 (0x10-0x5F, 与 ASRPRO 一致)
#define VOICE_CMD_REPORT_TEMP 0x10
// ... (完整定义见 Library/VOICE.h)

// API
void VOICE_Init(void);
void VOICE_Process(void);
uint8_t VOICE_IsBusy(void);
```

### 8.2 VOICE.c（状态机 + 中断回调）

- `VOICE_Init()`: 使能 UART4 NVIC + `HAL_UART_Receive_IT()` 启动首次接收
- `VOICE_Process()`: 状态机轮询, 收到 4 字节帧后校验 + `VOICE_ParseCommand()` 分发
- `HAL_UART_RxCpltCallback()`: ISR 置标, 立即重启下一帧接收
- `HAL_UART_ErrorCallback()`: 错误时 Abort + 重启接收

### 8.3 main.c 集成

```c
// 初始化区:
MX_UART4_Init();     // CubeMX
MX_TIM4_Init();      // TIM4 PWM (LED灯光)
VOICE_Init();

// 主循环 while(1):
SENS_Read();
Heating_Control(temperature_c);
// TODO: Wet_Control(humidity);  // 待加
OLED 显示;
VOICE_Process();     // ★ 非阻塞
HAL_Delay(1000);
```

---

## 九、故障排查

| 现象 | 可能原因 | 检查方法 |
|------|----------|----------|
| 语音无反应 | 命令词没触发 / UART 未发 | ASRPRO `ASR_CODE` 开头加板载 LED 翻转验证 |
| STM32 收不到数据 | 接线 / 波特率 / PA0/PA1 引脚 | 查 PB5→PA1、GND 共地、CubeMX UART4 勾选 |
| 帧校验经常失败 | 波特率偏差 | 示波器看波形, IBRD/FBRD 微调 |
| UART4 中断不触发 | NVIC 未勾选 | CubeMX NVIC 选项卡确认 |
| 只能发不能收 | Hardware Flow Control | 确认 CubeMX 选 **Disable** |
| ASRPRO IO 复用无效 | 忘记解锁 DPMU | `CFG_LOCK_CFG = 0x51AC0FFE` 必须第一行 |
| 天问 Block 编译报错 | 未登录 / 未实名 | 工具栏登录 + 更多→实名认证 |
| 天问 Block 下载失败 | 编译模式选错 | SSOP24=2M, QFN40=4M |

---

## 十、参考

- 天问 ASRPRO 芯片手册：`information/asr_pro_core.pdf`
- 天问 ASRPRO 编程手册：`information/asr_pro.pdf`
- STM32U585 参考手册：RM0456
- 天问 Block：https://www.haohaodada.com
