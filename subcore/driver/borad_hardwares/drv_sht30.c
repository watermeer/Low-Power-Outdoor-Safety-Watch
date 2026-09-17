/* drv_sht30.c — SHT30 环境温湿度，板级绑定
 *
 * 给体温补偿那层供环境温度，走 I2C0，ADDR 接地 → 0x44（7 位）。
 *
 * 它跟另外两个不是一路货：命令式器件，没有寄存器地址，
 * interface 里 read 的 reg 参数是废的，看代码时别被它绕进去。
 */

#include "driver_sht30.h"
#include "driver_sht30_interface.h"

/* 就一个句柄 */
static sht30_handle_t g_sht30_handle;

/* 绑句柄 + 设地址 + init，最后把重复性定成高 */
void SHT30_Init(void)
{
    /* 句柄清零 */
    DRIVER_SHT30_LINK_INIT(&g_sht30_handle, sht30_handle_t);

    /* 回调绑一遍，命令式器件没有中断回调 */
    DRIVER_SHT30_LINK_IIC_INIT(&g_sht30_handle, sht30_interface_iic_init);
    DRIVER_SHT30_LINK_IIC_DEINIT(&g_sht30_handle, sht30_interface_iic_deinit);
    DRIVER_SHT30_LINK_IIC_READ(&g_sht30_handle, sht30_interface_iic_read);
    DRIVER_SHT30_LINK_IIC_WRITE(&g_sht30_handle, sht30_interface_iic_write);
    DRIVER_SHT30_LINK_DELAY_MS(&g_sht30_handle, sht30_interface_delay_ms);
    DRIVER_SHT30_LINK_DEBUG_PRINT(&g_sht30_handle, sht30_interface_debug_print);

    /* ADDR 接地，0x44 */
    (void)sht30_set_addr_pin(&g_sht30_handle, SHT30_ADDRESS_ADDR_LOW);
    if (0U != sht30_init(&g_sht30_handle)) {
        return; /* 失败就退，连个错误码都没有 */
    }

    /* ★ 设计关键点：高重复性单次测量，15ms 换精度
     * 测完自己回空闲，功耗 ≈0.2μA，正好配这块表 */
    (void)sht30_set_repeatability(&g_sht30_handle, SHT30_REPEATABILITY_HIGH);
}

/* 读环境温湿度，一次拿到两个：温 °C×100、湿 %×100，都是整数。
 * 差一个参数传 NULL 也行，但读还是照读 */
void SHT30_Read(int16_t *temp_ambient, uint16_t *humidity)
{
    float temperature = 0.0f;
    float hum         = 0.0f;

    (void)sht30_read_temperature_humidity(&g_sht30_handle, &temperature, &hum);

    if (NULL != temp_ambient) {
        /* ★ 设计关键点：跨层只传定点，float 烂在驱动 */
        *temp_ambient = (int16_t)(temperature * 100.0f);
    }
    if (NULL != humidity) {
        *humidity = (uint16_t)(hum * 100.0f);
    }
}
