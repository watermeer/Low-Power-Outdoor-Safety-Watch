/* drv_max30102.c — MAX30102 心率/血氧，板级绑定
 *
 * 底下是 LibDriver 那套句柄 + 函数指针，这层只干绑线的活：
 * 句柄、AD0 地址、采样参数、FIFO 配置都在这定死，上层直接调。
 *
 * 这层全是 void，出错静默 return，什么都不往上报 ——
 * 调不通先量 I2C 波形，别一上来就怀疑算法。
 *
 * Read 还是个空壳，PPG 那块没写，现在只负责把 FIFO 搬出来。
 */

#include "driver_max30102.h"
#include "driver_max30102_interface.h"

/* 全项目就这一个句柄 */
static max30102_handle_t g_max30102_handle;

/* 初始化 + 把参数配到本项目要的那套。调完芯片就开始采了 */
void MAX30102_Init(void)
{
    /* 句柄先清零 */
    DRIVER_MAX30102_LINK_INIT(&g_max30102_handle, max30102_handle_t);

    /* 回调一次绑完：I2C 读写 + 延时 + 打印 */
    DRIVER_MAX30102_LINK_IIC_INIT(&g_max30102_handle, max30102_interface_iic_init);
    DRIVER_MAX30102_LINK_IIC_DEINIT(&g_max30102_handle, max30102_interface_iic_deinit);
    DRIVER_MAX30102_LINK_IIC_READ(&g_max30102_handle, max30102_interface_iic_read);
    DRIVER_MAX30102_LINK_IIC_WRITE(&g_max30102_handle, max30102_interface_iic_write);
    DRIVER_MAX30102_LINK_DELAY_MS(&g_max30102_handle, max30102_interface_delay_ms);
    DRIVER_MAX30102_LINK_DEBUG_PRINT(&g_max30102_handle, max30102_interface_debug_print);
    /* 中断回调绑了，但现在还是轮询读 FIFO */
    DRIVER_MAX30102_LINK_RECEIVE_CALLBACK(&g_max30102_handle, max30102_interface_receive_callback);

    /* AD0 接地 → 0xAE，板上就这么接的 */
    (void)max30102_set_addr_pin(&g_max30102_handle, MAX30102_ADDRESS_AD0_LOW);
    if (0U != max30102_init(&g_max30102_handle)) {
        return; /* 挂了就退，不吭声 —— 外面看不出来 */
    }

    /* ★ 设计关键点：411us 脉宽换 18 位分辨率
     * 配 ±8192nA 量程 + 100Hz 采样 */
    (void)max30102_set_spo2_config(&g_max30102_handle,
                                    MAX30102_ADC_RANGE_8192,
                                    MAX30102_SAMPLE_RATE_100_HZ,
                                    MAX30102_PULSE_WIDTH_411_US);

    /* LED 两路都 0x3F ≈ 12mA，戴松/肤色深得往上加 */
    (void)max30102_set_led_pulse_amplitude(&g_max30102_handle, MAX30102_LED_RED, 0x3F);
    (void)max30102_set_led_pulse_amplitude(&g_max30102_handle, MAX30102_LED_IR,  0x3F);

    /* ★ 设计关键点：片内 16 点平均，攒 17 个才中断
     * 平均在芯片里做完，MCU 拿到的就是平滑过的数 */
    (void)max30102_set_fifo_config(&g_max30102_handle,
                                    MAX30102_SAMPLE_AVG_16,
                                    MAX30102_BOOL_TRUE,
                                    17U);

    /* 使能 PPG 数据就绪中断 */
    (void)max30102_set_interrupt(&g_max30102_handle, MAX30102_INTERRUPT_PPG_RDY, MAX30102_BOOL_TRUE);

    /* 双 LED 血氧模式，红光 + 红外 */
    (void)max30102_set_mode(&g_max30102_handle, MAX30102_MODE_SPO2);
}

/* 读一次：spo2 是百分比，hr 是 bpm。
 * 现在只把 FIFO 搬出来，算法没写，两个输出都是 0 */
void MAX30102_Read(uint8_t *spo2, uint8_t *hr)
{
    max30102_fifo_data_t samples[32];
    uint16_t read_count = 0U;

    /* 32 深 FIFO 一次搬空；read_count 拿到手没往下用，先留着
     * 栈上 256 字节 + 驱动里还有 192 字节 raw，别在中断里调 */
    (void)max30102_read_fifo(&g_max30102_handle, samples, 32U, &read_count);

    /* TODO: PPG 算法还没写（DC 滤波 → 峰谷检测 → R 值 → SpO2）
     *       算不出来就先返 0，上层看到 0 别当有效值用 */
    if (NULL != spo2) { *spo2 = 0U; }   /* 占位，等算法 */
    if (NULL != hr)   { *hr   = 0U; }   /* 占位，等算法 */
}
