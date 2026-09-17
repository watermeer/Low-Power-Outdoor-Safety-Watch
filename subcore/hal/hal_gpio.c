/* hal_gpio.c — GPIO 汇总配置 + 调试口
 *
 * 全板引脚的初始化都收在一个文件里，省得散得到处都是。
 * 低功耗的账从这儿开始算：脚悬着就漏电。
 *
 * 调试串口实际是 USART1（PA2/PA3），头文件里还写着 USART2，没改。
 */

#include "hal_gpio.h"
#include "gd32l23x.h"

/* ---- 各端口已用引脚掩码 ----
 * 只写不读，当初留着排查引脚冲突的，到现在也没用上 */
static uint32_t s_used_pins_gpioa = 0;
static uint32_t s_used_pins_gpiob = 0;
static uint32_t s_used_pins_gpioc = 0;
static uint32_t s_used_pins_gpiof = 0;

/**
 * @brief 调试 LED 开关
 */
void HalGpio_LedSet(bool on)
{
    if (on) {
        gpio_bit_reset(HAL_LED_PORT, HAL_LED_PIN);  /* 低电平点亮 */
    } else {
        gpio_bit_set(HAL_LED_PORT, HAL_LED_PIN);
    }
}

void HalGpio_LedToggle(void)
{
    gpio_bit_toggle(HAL_LED_PORT, HAL_LED_PIN);
}

/* ★ 设计关键点：未用引脚配模拟输入，防浮空漏电 */
/**
 * @brief 未用引脚全配成模拟输入
 * @note  模拟模式连施密特触发器都关了，功耗最低的一档。
 *        原来写的 GPIO_MODE_AIN 是 STM32 叫法，GD32 这边叫 ANALOG
 */
void HalGpio_AnalogUnused(uint32_t port, uint32_t used_pins)
{
    uint32_t pin;

    for (pin = GPIO_PIN_0; pin <= GPIO_PIN_15; pin <<= 1) {
        if (0U == (used_pins & pin)) {
            gpio_mode_set(port, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, pin);
        }
    }

    /* 记录已用引脚 */
    if (port == GPIOA) {
        s_used_pins_gpioa = used_pins;
    } else if (port == GPIOB) {
        s_used_pins_gpiob = used_pins;
    } else if (port == GPIOC) {
        s_used_pins_gpioc = used_pins;
    } else if (port == GPIOF) {
        s_used_pins_gpiof = used_pins;
    }
}

/**
 * @brief 调 LED 引脚
 */
static void led_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_mode_set(HAL_LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, HAL_LED_PIN);
    gpio_output_options_set(HAL_LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, HAL_LED_PIN);
    gpio_bit_set(HAL_LED_PORT, HAL_LED_PIN);  /* 默认熄灭 */
}

/**
 * @brief 调串口（USART1, 115200-8-N-1）
 */
static void uart_debug_init(void)
{
    /* 使能 GPIOA 和 USART1 时钟 */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_USART1);

    /* PA2 = TX, PA3 = RX — 复用推挽模式 */
    /* GD32L23x: PA2/PA3 走 USART1，复用号是 AF7（不是 AF4） */
    gpio_af_set(GPIOA, GPIO_AF_7, HAL_UART_TX_PIN | HAL_UART_RX_PIN);
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, HAL_UART_TX_PIN | HAL_UART_RX_PIN);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                            HAL_UART_TX_PIN | HAL_UART_RX_PIN);

    /* USART1 基础配置: 115200-8-N-1 */
    usart_deinit(USART1);
    usart_baudrate_set(USART1, 115200U);
    usart_word_length_set(USART1, USART_WL_8BIT);
    usart_stop_bit_set(USART1, USART_STB_1BIT);
    usart_parity_config(USART1, USART_PM_NONE);
    usart_hardware_flow_rts_config(USART1, USART_RTS_DISABLE);
    usart_hardware_flow_cts_config(USART1, USART_CTS_DISABLE);
    usart_receive_config(USART1, USART_RECEIVE_ENABLE);
    usart_transmit_config(USART1, USART_TRANSMIT_ENABLE);
    usart_enable(USART1);
}

/**
 * @brief 全板 GPIO 一次配完
 * @note  套路是"先把脚全压成模拟，谁用谁再抢回去"，
 *        所以 I2C/电源都得排在这之后，顺序别调
 */
ErrorCode_t HalGpio_InitAll(void)
{
    /* ---- 1. 调试 LED ---- */
    led_init();

    /* ---- 2. 调试串口 ---- */
    uart_debug_init();

    /* ---- 3. 标记各端口已用引脚 ---- */

    /* GPIOA: PA2(TX), PA3(RX), PA5(LED) */
    HalGpio_AnalogUnused(GPIOA, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5);

    /* PB6/PB7 是老分配，I2C0 早挪到 PF6/PF7 了，掩码忘了跟着改。
     * 不影响功能，先记着 */
    HalGpio_AnalogUnused(GPIOB, GPIO_PIN_6 | GPIO_PIN_7);

    /* GPIOC: 全部模拟输入（暂无外设使用） */
    HalGpio_AnalogUnused(GPIOC, 0);

    /* GPIOF 全压模拟，包括 I2C0 要用的 PF6/PF7 —— 靠的是 I2C0 在后面
     * 才初始化把它们抢回来。顺序要是反了，总线就通不了 */
    HalGpio_AnalogUnused(GPIOF, 0);

    return ERR_OK;
}

/*==============================================================================
 * printf 重定向到 USART1（调试用）
 *
 * ★ 设计关键点：GCC/MDK 各一套 printf
 *============================================================================*/

#ifdef __GNUC__
/* GCC/ARM 工具链: 重定向 _write 系统调用 */
int _write(int fd, char *ptr, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        while (RESET == usart_flag_get(USART1, USART_FLAG_TBE)) {
            /* 等待发送缓冲为空 */
        }
        usart_data_transmit(USART1, (uint8_t)ptr[i]);
    }
    return len;
}
#else
/* MDK/IAR 工具链: 重定向 fputc */
int fputc(int ch, FILE *f)
{
    (void)f;
    while (RESET == usart_flag_get(USART1, USART_FLAG_TBE)) {
        /* 等待发送缓冲为空 */
    }
    usart_data_transmit(USART1, (uint8_t)ch);
    return ch;
}
#endif
