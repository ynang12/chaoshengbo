#include "ht32.h"
#include <stdint.h>

/*
 * HT32F52352 + HC-SR04 + PC14 LED
 * 条件：距离 <= 3cm 时点亮 PC14
 *
 * 引脚：
 *   TRIG -> PA0
 *   ECHO -> PA1 (注意分压，HC-SR04 ECHO 为 5V)
 *   LED  -> PC14
 */

#define HCSR04_TRIG_PORT      HT_GPIOA
#define HCSR04_TRIG_PIN       GPIO_PIN_0
#define HCSR04_ECHO_PORT      HT_GPIOA
#define HCSR04_ECHO_PIN       GPIO_PIN_1

#define LED_GPIO_PORT         HT_GPIOC
#define LED_GPIO_PIN          GPIO_PIN_14
#define LED_ON_THRESHOLD_CM   3.0f
/* ESK 板载 LED 常见为低电平点亮：1=低电平亮, 0=高电平亮 */
#define LED_ACTIVE_LOW        1

#define MEASURE_TIMEOUT_US    30000U
#define LOOP_PERIOD_MS        60U

static void delay_cycles(volatile uint32_t n)
{
  while (n--) __NOP();
}

static void delay_us(uint32_t us)
{
  while (us--) delay_cycles(12);  // 48MHz 经验值
}

static void delay_ms(uint32_t ms)
{
  while (ms--) delay_us(1000);
}

static void board_init(void)
{
  CKCU_PeripClockConfig_TypeDef ck = {{0}};
  ck.Bit.PA    = 1;
  ck.Bit.PC    = 1;
  ck.Bit.AFIO  = 1;
  ck.Bit.BFTM0 = 1;
  CKCU_PeripClockConfig(ck, ENABLE);

  GPIO_DirectionConfig(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN, GPIO_DIR_OUT);
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);

  GPIO_DirectionConfig(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN, GPIO_DIR_IN);
  GPIO_InputConfig(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN, ENABLE);

  GPIO_DirectionConfig(LED_GPIO_PORT, LED_GPIO_PIN, GPIO_DIR_OUT);
  /* 上电先灭灯 */
#if LED_ACTIVE_LOW
  GPIO_SetOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#else
  GPIO_ClearOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#endif

  /* 不使用 PSC，兼容“无 PSC 字段”的 HT32 库版本 */
  HT_BFTM0->CR   = 0;
  HT_BFTM0->CNTR = 0;
  HT_BFTM0->CMP  = 0xFFFFFFFFU;
  HT_BFTM0->CR  |= 0x1U;
}

static uint32_t micros(void)
{
  /* 48MHz: 48tick = 1us */
  return HT_BFTM0->CNTR / 48U;
}

static void hcsr04_trigger(void)
{
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
  delay_us(2);
  GPIO_SetOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
  delay_us(10);
  GPIO_ClearOutBits(HCSR04_TRIG_PORT, HCSR04_TRIG_PIN);
}

static float hcsr04_read_cm(void)
{
  uint32_t t0;

  hcsr04_trigger();

  t0 = micros();
  while (GPIO_ReadInBit(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == RESET)
  {
    if ((micros() - t0) > MEASURE_TIMEOUT_US) return -1.0f;
  }

  HT_BFTM0->CNTR = 0;

  while (GPIO_ReadInBit(HCSR04_ECHO_PORT, HCSR04_ECHO_PIN) == SET)
  {
    if (micros() > MEASURE_TIMEOUT_US) return -2.0f;
  }

  return (float)micros() / 58.0f;
}

int main(void)
{
  float d_cm;
  board_init();

  while (1)
  {
    d_cm = hcsr04_read_cm();

    if (d_cm > 0.0f && d_cm <= LED_ON_THRESHOLD_CM)
    {
#if LED_ACTIVE_LOW
      GPIO_ClearOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#else
      GPIO_SetOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#endif
    }
    else
    {
#if LED_ACTIVE_LOW
      GPIO_SetOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#else
      GPIO_ClearOutBits(LED_GPIO_PORT, LED_GPIO_PIN);
#endif
    }

    delay_ms(LOOP_PERIOD_MS);
  }
}
