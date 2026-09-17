/* hal_gpio.h — GPIO 汇总配置
 *
 * 引脚宏都在这儿，换板子只动这个文件（别处别再写死引脚号）。
 * UART 那块写着 USART2_TX，实际是 USART1 —— 老注释，没改。
 */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include "global_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 调试引脚宏定义（可按实际 PCB 修改）
 *============================================================================*/

/** @brief LED 调试引脚 — PA5（低电平点亮） */
#define HAL_LED_PORT    GPIOA
#define HAL_LED_PIN     GPIO_PIN_5

/* PA2/PA3 走的是 USART1（AF7），下面那句 USART2_TX 是旧的。
 * HAL_UART_PORT 也没人引用，.c 里都是直接写 GPIOA */
/** @brief 串口调试 TX — PA2 (USART2_TX) */
#define HAL_UART_PORT   GPIOA
#define HAL_UART_TX_PIN GPIO_PIN_2
#define HAL_UART_RX_PIN GPIO_PIN_3

/*==============================================================================
 * API
 *============================================================================*/

/* ★ 设计关键点：初始化顺序：全压模拟，外设抢回脚 */
/**
 * @brief   全板 GPIO 一次配完
 * @note    LED 推挽 + 串口复用推挽 + 剩下全模拟输入。
 *          顺序别换，换了 I2C0 的脚会被压掉
 * @return  ERR_OK 成功
 */
ErrorCode_t HalGpio_InitAll(void);

/**
 * @brief   调试 LED 开关
 * @param   on true=亮, false=灭（低电平点亮）
 */
void HalGpio_LedSet(bool on);

/**
 * @brief   调试 LED 翻转（跑起来闪的那种）
 */
void HalGpio_LedToggle(void);

/* ★ 设计关键点：未用引脚模拟输入，待机电流第一防线 */
/**
 * @brief   把没用的脚全压成模拟输入
 * @param   port GPIO 端口基址
 * @param   used_pins 已用引脚的位掩码
 * @note    浮空脚会漏电，进低功耗前这步不能省
 */
void HalGpio_AnalogUnused(uint32_t port, uint32_t used_pins);

#ifdef __cplusplus
}
#endif

#endif /* HAL_GPIO_H */
