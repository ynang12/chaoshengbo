# HT32F52352（ESK32-30501）+ HC-SR04：HT32Init 全页签配置 + Keil 代码

> 你给的 6 张图就是 HT32Init 的全部功能页签：`Clock / AFIO / GPIO / EXTI / TM / SCTM`。  
> 下面我按 **“每个页签具体该怎么选”** 给你一份可直接照着配的清单。

---

## 0. 接线先固定（建议）

- `TRIG` → `PA0`
- `ECHO` → `PA1`
- `VCC` → `5V`
- `GND` → `GND`

⚠️ HC-SR04 的 `ECHO` 是 5V，`PA1` 前必须分压/电平转换（例：15k 上拉到 ECHO、10k 下拉到 GND，中点进 PA1）。

---

## 1. Clock 页签（你第 2 张图）该怎么配

### 1.1 Clock 区块

- `Enable HSE[4~16]`：✅ 勾选，填 `8 MHz`
- `Enable PLL`：✅ 勾选
- `PLL NF2`：`6`
- `PLL NO2`：`1`
- `Clock Divider`：`1`
- 目标 `fcpu = 48MHz`

### 1.2 BFTM0 / BFTM1 区块

- `Enable BFTM0`：❌ 不勾（用代码自己配 1MHz，更准）
- `Enable BFTM1`：❌ 不勾

> 说明：HT32Init 里 BFTM 的 “Cycle time ms” 主要是周期中断模板，不适合我们做 HC-SR04 的微秒计时。

### 1.3 SysTick / WDT / RTC / CRC / USB / LVD / Misc

为了先跑通测距，建议：

- `Enable SysTick`：✅ 可勾（1ms 默认即可）
- `Enable WDT`：❌ 不勾
- `Enable RTC`：❌ 不勾
- `Enable CRC`：❌ 不勾
- `Enable USB`：❌ 不勾
- `Enable LVD/BOR`：❌ 不勾
- `Software Division / Rand`：❌ 不勾

> 总原则：先只开必要项，避免比赛现场被无关中断干扰。

---

## 2. AFIO 页签（你第 1 张图）该怎么配

这是你当前最关键的错误点：

- 你图里 `PA1` 被设成了 `AF4 TM -> GT1_CH1`（蓝色），这会把 PA1 变成定时器复用，**ECHO 读不到正常 GPIO 电平**。

### 正确设置

- `PA0`：保持 `AF0 Default`（GPIO）
- `PA1`：保持 `AF0 Default`（GPIO）

### 操作方法

1. 点 `PA1` 这一行。
2. 把当前 `GT1_CH1` 取消。
3. 选回 `AF0 Default`。

---

## 3. GPIO 页签（你第 3 张图）该怎么配

按你表格列配置：

### PA0（TRIG）

- `Direction`：`Output`
- `Input Enable`：`Disable`
- `Pull State`：`No Pull`
- `Output State`：`Low`
- `Open Drain`：`Disable`（推挽）
- `Current`：`4mA`（够用）

### PA1（ECHO）

- `Direction`：`Input`
- `Input Enable`：`Enable`（关键）
- `Pull State`：`No Pull`（有外部分压时推荐）
- `Open Drain`：`Disable`
- `Current`：默认即可

> 说明：你当前图里 PA1 的 `Input Enable` 还是 `Disable`，建议改成 `Enable`。

---

## 4. EXTI 页签（你第 4 张图）该怎么配

本方案是 **轮询法**（不走中断），因此：

- EXTI0~EXTI15 全部保持默认，不额外配置。
- `Port` 列无需绑定 PA1。
- `Interrupt Trigger Type` 保持默认即可。

> 这样可避免中断向量名、线号、双边沿配置差异导致的坑。

---

## 5. TM 页签（你第 5 张图）该怎么配

本方案不用 GPTM/MCTM 输入捕获，因此：

- `Enable Timer`：❌ 不勾
- `Output PWM Mode`：❌ 不勾
- `Input Capture CH0~CH3`：❌ 都不勾

保持默认即可。

---

## 6. SCTM 页签（你第 6 张图）该怎么配

同样不用 SCTM：

- `Enable Timer`：❌ 不勾
- `Output PWM Mode`：❌ 不勾
- `Input Capture Mode`：❌ 不勾

保持默认。

---

## 7. 生成工程（Keil）

1. `File -> Save` 保存 `.ioc`（或 HT32Init 工程文件）。
2. 工具栏点生成代码。
3. Toolchain 选 `MDK-ARM (Keil)`。
4. 打开生成的 `.uvprojx`。

---

## 8. Keil 直接可用代码（轮询 + BFTM0 1MHz）

> 这份代码与上面配置一一对应。  
> 核心思路：TRIG 发 10us 脉冲，轮询 ECHO 高电平宽度（单位 us），距离 = us / 58。

```c
#include "ht32.h"
#include <stdint.h>

#define HCSR04_TRIG_PORT   HT_GPIOA
#define HCSR04_TRIG_PIN    GPIO_PIN_0
#define HCSR04_ECHO_PORT   HT_GPIOA
#define HCSR04_ECHO_PIN    GPIO_PIN_1
#define HCSR04_TIMEOUT_US  30000U

static void delay_cycles(volatile uint32_t n)
{
  while (n--) __NOP();
}

static void delay_us(uint32_t us)
{
  while (us--) delay_cycles(12);  // 48MHz 粗略值，够触发用
}

static void hcsr04_hw_init(void)
{
  CKCU_PeripClockConfig_TypeDef ck = {{0}};
  ck.Bit.PA    = 1;
  ck.Bit.AFIO  = 1;
  ck.Bit.BFTM0 = 1;
  CKCU_PeripClockConfig(ck, ENABLE);

  GPIO_DirectionConfig(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_DIR_OUT);
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);

  GPIO_DirectionConfig(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN, GPIO_DIR_IN);
  GPIO_InputConfig(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN, ENABLE);

  /* BFTM0: 1MHz free-run (1 tick = 1us) */
  HT_BFTM0->CR   = 0;
  HT_BFTM0->CNTR = 0;
  HT_BFTM0->CMP  = 0xFFFFFFFFU;
  HT_BFTM0->PSC  = 47U;   // 48MHz/(47+1)=1MHz
  HT_BFTM0->CR  |= 0x1U;
}

static uint32_t micros(void)
{
  return HT_BFTM0->CNTR;
}

static void hcsr04_trigger(void)
{
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
  delay_us(2);
  GPIO_SetOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
  delay_us(10);
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
}

float hcsr04_read_cm(void)
{
  uint32_t t0;

  hcsr04_trigger();

  /* 等待 ECHO 拉高 */
  t0 = micros();
  while (GPIO_ReadInBit(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == RESET)
  {
    if ((micros() - t0) > HCSR04_TIMEOUT_US)
      return -1.0f;
  }

  HT_BFTM0->CNTR = 0;  // 高电平开始计时

  /* 等待 ECHO 拉低 */
  while (GPIO_ReadInBit(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == SET)
  {
    if (micros() > HCSR04_TIMEOUT_US)
      return -1.0f;
  }

  return (float)micros() / 58.0f;
}

int main(void)
{
  float d;
  hcsr04_hw_init();

  while (1)
  {
    d = hcsr04_read_cm();
    // printf("%.2f cm\r\n", d);
    (void)d;
    delay_us(60000);   // >=60ms 测一次
  }
}
```

---

## 9. 你现在马上改这 5 项，就能跑

1. AFIO：`PA1` 从 `GT1_CH1` 改回 `AF0 Default`。  
2. GPIO：`PA1 Input Enable = Enable`。  
3. GPIO：`PA0 Output + Low`。  
4. Clock：保持 48MHz。  
5. Keil 用上面代码，先看串口/变量是否有距离值。



## 13. 直接可复制的完整代码文件

已提供完整替换代码：`main_complete_ht32f52352.c`。

- 直接把此文件内容覆盖到 Keil 工程 `User/main.c`。
- 已包含：HC-SR04 测距 + `d<=3cm` 点亮 `PC14` + 兼容无 `PSC` 字段。


## 14. 新报错修复：`identifier "inline" is undefined`

这是 Keil/ARMCC 使用 C90 模式时的常见报错。

直接改法：

```c
/* 错误写法（在部分 Keil 配置下会报错） */
static inline uint32_t micros(void)

/* 兼容写法 */
static uint32_t micros(void)
```

我提供的 `main_complete_ht32f52352.c` 已经按兼容写法处理。


---



## 15. 新问题：板子“一直亮灯”怎么改

这是开发板 LED 极性问题（高电平亮/低电平亮）导致的。

在 `main_complete_ht32f52352.c` 里我已加了可配置宏：

```c
#define LED_ACTIVE_LOW        1   // 1=低电平亮, 0=高电平亮
```

- 如果你现在“始终亮灯”，先保持 `LED_ACTIVE_LOW = 1` 试跑。
- 若变成“始终灭灯”，改成 `0` 再试。

代码已经自动按这个宏切换 `GPIO_SetOutBits`/`GPIO_ClearOutBits`，不需要你再手动改判断逻辑。
