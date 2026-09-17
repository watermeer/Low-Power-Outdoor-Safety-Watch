/* bsp_max30102.c — MAX30102 心率 / 血氧
 *
 * 驱动这边配得挺全（SpO2 模式、双 LED、FIFO 16 次平均、PPG 就绪中断），
 * 但算法一行没写：FIFO 读出来的 PPG 数据看完就扔，返回的是写死的占位值。
 * 血氧那条告警链现在跑的就是假数据，别拿它当真。详见下面两个 @warning。
 */

#include "bsp_max30102.h"
#include "driver_max30102.h"
#include "driver_max30102_interface.h"
#include "hal_i2c0.h"

static max30102_handle_t g_max30102;

/* 还是那套映射，这份多两个码（4 复位 / 5 ID） */
static ErrorCode_t to_hal(uint8_t e)
{
    switch (e) {
        case 0:  return ERR_OK;
        case 1:  return ERR_TIMEOUT;
        case 2:  return ERR_NULL_POINTER;
        case 3:  return ERR_NOT_INIT;
        case 4:  return ERR_HARDWARE;
        case 5:  return ERR_NOT_FOUND;
        default: return ERR_HARDWARE;
    }
}

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t BSP_MAX30102_Init(void)
{
    uint8_t ret;

    DRIVER_MAX30102_LINK_INIT(&g_max30102, max30102_handle_t);
    DRIVER_MAX30102_LINK_IIC_INIT(&g_max30102, max30102_interface_iic_init);
    DRIVER_MAX30102_LINK_IIC_DEINIT(&g_max30102, max30102_interface_iic_deinit);
    DRIVER_MAX30102_LINK_IIC_READ(&g_max30102, max30102_interface_iic_read);
    DRIVER_MAX30102_LINK_IIC_WRITE(&g_max30102, max30102_interface_iic_write);
    DRIVER_MAX30102_LINK_DELAY_MS(&g_max30102, max30102_interface_delay_ms);
    DRIVER_MAX30102_LINK_DEBUG_PRINT(&g_max30102, max30102_interface_debug_print);
    DRIVER_MAX30102_LINK_RECEIVE_CALLBACK(&g_max30102, max30102_interface_receive_callback);

    ret = max30102_set_addr_pin(&g_max30102, MAX30102_ADDRESS_AD0_LOW);
    if (0U != ret) return to_hal(ret);

    ret = max30102_init(&g_max30102);
    if (0U != ret) return to_hal(ret);

    /* 配置 SpO2 模式 */
    (void)max30102_set_spo2_config(&g_max30102,
                                    MAX30102_ADC_RANGE_8192,
                                    MAX30102_SAMPLE_RATE_100_HZ,
                                    MAX30102_PULSE_WIDTH_411_US);

    /* LED 电流：红光 ~12mA, 红外 ~12mA */
    (void)max30102_set_led_pulse_amplitude(&g_max30102, MAX30102_LED_RED, 0x3F);
    (void)max30102_set_led_pulse_amplitude(&g_max30102, MAX30102_LED_IR,  0x3F);

    /* ★ 设计关键点：16 次平均丢片内，MCU 不掺和 */
    /* FIFO: 16样本平均 + 翻转模式 + 满阈值=17 */
    (void)max30102_set_fifo_config(&g_max30102,
                                    MAX30102_SAMPLE_AVG_16,
                                    MAX30102_BOOL_TRUE,
                                    17U);

    /* 使能 PPG 就绪中断 */
    (void)max30102_set_interrupt(&g_max30102, MAX30102_INTERRUPT_PPG_RDY, MAX30102_BOOL_TRUE);

    /* 进入 SpO2 模式 */
    (void)max30102_set_mode(&g_max30102, MAX30102_MODE_SPO2);

    return ERR_OK;
}

/*==============================================================================
 * 断电 / 读取
 *============================================================================*/

ErrorCode_t BSP_MAX30102_Shutdown(void)
{
    /* deinit 里会写 Shutdown 位，LED 和采样全停 */
    uint8_t ret = max30102_deinit(&g_max30102);
    return to_hal(ret);
}

ErrorCode_t BSP_MAX30102_ReadSpO2(uint8_t *spo2)
{
    if (NULL == spo2) return ERR_NULL_POINTER;

    /* @warning SpO2 算法没写：FIFO 读出来就扔掉，返回写死的 98。
     * 上层那条血氧告警链是拿假数据在跑，别当真。
     * 补的时候从这儿接：DC 滤波 → 峰谷检测 → R 值 → SpO2 = 110 - 25R。
     * 目标不是"测准血氧"，是"看出它在往下掉"，所以别死抠绝对值。
     */

    max30102_fifo_data_t samples[32];   /* 32 × 8B = 256B，全在栈上，栈水位留神 */
    uint16_t read_count = 0U;

    uint8_t ret = max30102_read_fifo(&g_max30102, samples, 32U, &read_count);
    if (0U != ret) {
        *spo2 = 0U;
        return to_hal(ret);
    }

    /* ★ 设计关键点：先返占位值，把上层整条链路跑通 */
    if (read_count > 0U) {
        *spo2 = 98U;  /* 占位：SpO2 算法未实现，返回假数据 */
    } else {
        *spo2 = 0U;
    }

    return ERR_OK;
}

ErrorCode_t BSP_MAX30102_ReadHeartRate(uint8_t *hr)
{
    if (NULL == hr) return ERR_NULL_POINTER;

    /* @warning 心率算法也没写，返回写死的 0。
     * 和上面共用同一批 FIFO 数据：红光通道 → 带通(0.5~5Hz) → 峰检测 →
     * 峰间隔 → HR = 60 / 间隔。两条一起在 Phase 4 补。
     */
    *hr = 0U;  /* 占位：算法未实现 */
    return ERR_OK;
}
