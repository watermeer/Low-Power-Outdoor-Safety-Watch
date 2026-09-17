/* hal_power.c — 主核电源 + 震动马达
 *
 * ★ 设计关键点：主核的电源开关握在副核手里
 *   两块 MCU 不是一个电源域。副核常驻、微安级；主核（STM32F103 + OLED）
 *   是耗电大户，平时被一个 P-MOSFET 彻底掐电，只有 D 级告警才放它出来跑 500ms。
 *   这比让主核进 Stop 模式省得多 —— Stop 还有漏电，断电就是真的一点不耗。
 *
 * 电平别记反了（我反过一次，主核一直上电，三天耗光电池）：
 *   MAINCORE: reset(LOW) = 导通 = 上电    set(HIGH) = 截止 = 断电
 *   VIBRATE : set(HIGH)  = 震             reset(LOW) = 停
 */

#include "hal_power.h"
#include "gd32l23x.h"

/*==============================================================================
 * 内部状态
 *============================================================================*/

static bool s_mainCoreOn   = false;
static bool s_vibrateOn    = false;

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t HalPower_Init(void)
{
    rcu_periph_clock_enable(RCU_GPIOB);

    /* 主核电源：默认输出高 = P-MOSFET 截止 = 断电 */
    gpio_mode_set(HAL_POWER_MAINCORE_PORT,
                  GPIO_MODE_OUTPUT,
                  GPIO_PUPD_NONE,
                  HAL_POWER_MAINCORE_PIN);
    gpio_output_options_set(HAL_POWER_MAINCORE_PORT,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_2MHZ,
                            HAL_POWER_MAINCORE_PIN);
    gpio_bit_set(HAL_POWER_MAINCORE_PORT, HAL_POWER_MAINCORE_PIN);

    /* 震动：默认低 = 停 */
    gpio_mode_set(HAL_POWER_VIBRATE_PORT,
                  GPIO_MODE_OUTPUT,
                  GPIO_PUPD_NONE,
                  HAL_POWER_VIBRATE_PIN);
    gpio_output_options_set(HAL_POWER_VIBRATE_PORT,
                            GPIO_OTYPE_PP,
                            GPIO_OSPEED_2MHZ,
                            HAL_POWER_VIBRATE_PIN);
    gpio_bit_reset(HAL_POWER_VIBRATE_PORT, HAL_POWER_VIBRATE_PIN);

    s_mainCoreOn = false;
    s_vibrateOn  = false;

    return ERR_OK;
}

/*==============================================================================
 * 主核电源
 *============================================================================*/

void HalPower_MainCoreOn(void)
{
    gpio_bit_reset(HAL_POWER_MAINCORE_PORT, HAL_POWER_MAINCORE_PIN);
    s_mainCoreOn = true;

    /* 等主核上电稳住。
     * 本来该用 HalSystem_DelayMs(10)，但进 Standby 前后 SysTick 状态不保险，
     * 就先用空循环硬等。160000 次 @16MHz ≈ 10ms，实测够。
     * TODO 有空换成 SysTick 版，这个数字跟主频绑死了，降频后会变。 */
    for (volatile uint32_t d = 0; d < 160000; d++) {
        __NOP();
    }
}

void HalPower_MainCoreOff(void)
{
    gpio_bit_set(HAL_POWER_MAINCORE_PORT, HAL_POWER_MAINCORE_PIN);
    s_mainCoreOn = false;
}

bool HalPower_IsMainCoreOn(void)
{
    return s_mainCoreOn;
}

/*==============================================================================
 * 震动
 *============================================================================*/

void HalPower_VibrateSet(bool on)
{
    if (on) {
        gpio_bit_set(HAL_POWER_VIBRATE_PORT, HAL_POWER_VIBRATE_PIN);
    } else {
        gpio_bit_reset(HAL_POWER_VIBRATE_PORT, HAL_POWER_VIBRATE_PIN);
    }
    s_vibrateOn = on;
}

void HalPower_VibratePulseShort(void)
{
    /* C 级：震 100ms 就够，目的是"引起注意"不是"把人吓一跳"。
     * 同样是空循环，1600000 @16MHz ≈ 100ms。 */
    HalPower_VibrateSet(true);
    for (volatile uint32_t d = 0; d < 1600000; d++) {
        __NOP();
    }
    HalPower_VibrateSet(false);
}

void HalPower_VibratePulseLong(void)
{
    /* D 级：一直震，等主核那边用户按了键才停。
     * 别在这里加超时 —— 昏过去的人不会按键，持续震本身就是求救信号。 */
    HalPower_VibrateSet(true);
}
