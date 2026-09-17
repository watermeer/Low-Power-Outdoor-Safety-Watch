/* bsp_sht30.c — SHT30 环境温湿度
 *
 * 单次测量走时钟拉伸：命令发完就直接 read，硬件自己等它测完，
 * 高重复性一次 ~15ms，所以在高频路径上别塞它。
 *
 * 温湿度两个 CRC-8 都在驱动里验，验不过这边只会看到一个通信错误码。
 */

#include "bsp_sht30.h"
#include "driver_sht30.h"
#include "driver_sht30_interface.h"
#include "hal_i2c0.h"

static sht30_handle_t g_sht30;

/* 又是这套映射，抄的时候别把 1 和 2 抄反 */
static ErrorCode_t to_hal(uint8_t e)
{
    switch (e) {
        case 0:  return ERR_OK;
        case 1:  return ERR_TIMEOUT;
        case 2:  return ERR_NULL_POINTER;
        case 3:  return ERR_NOT_INIT;
        default: return ERR_HARDWARE;
    }
}

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t BSP_SHT30_Init(void)
{
    uint8_t ret;

    DRIVER_SHT30_LINK_INIT(&g_sht30, sht30_handle_t);
    DRIVER_SHT30_LINK_IIC_INIT(&g_sht30, sht30_interface_iic_init);
    DRIVER_SHT30_LINK_IIC_DEINIT(&g_sht30, sht30_interface_iic_deinit);
    DRIVER_SHT30_LINK_IIC_READ(&g_sht30, sht30_interface_iic_read);
    DRIVER_SHT30_LINK_IIC_WRITE(&g_sht30, sht30_interface_iic_write);
    DRIVER_SHT30_LINK_DELAY_MS(&g_sht30, sht30_interface_delay_ms);
    DRIVER_SHT30_LINK_DEBUG_PRINT(&g_sht30, sht30_interface_debug_print);

    ret = sht30_set_addr_pin(&g_sht30, SHT30_ADDRESS_ADDR_LOW);
    if (0U != ret) return to_hal(ret);

    /* init 里自带软复位，复位不成功返回 4 */
    ret = sht30_init(&g_sht30);
    if (0U != ret) return to_hal(ret);

    /* ★ 设计关键点：精度优先，重复性直接拉满 HIGH */
    /* 高重复性一次 ~15ms；驱动 init 里设过了，这里再明确一遍 */
    ret = sht30_set_repeatability(&g_sht30, SHT30_REPEATABILITY_HIGH);
    (void)ret;   /* 这句只是改结构体字段，失败了也还有默认值 */

    return ERR_OK;
}

/*==============================================================================
 * 休眠 / 读取
 *============================================================================*/

ErrorCode_t BSP_SHT30_Sleep(void)
{
    /* SHT30 闲着自己就够省（~0.2μA），这里没发什么命令，
     * 只是清掉 inited 再关总线 —— 醒过来要读也得重新 Init */
    (void)sht30_deinit(&g_sht30);
    return ERR_OK;
}

ErrorCode_t BSP_SHT30_ReadData(int16_t *temp, uint16_t *humi)
{
    if (NULL == temp || NULL == humi) return ERR_NULL_POINTER;

    float temperature = 0.0f;
    float humidity    = 0.0f;

    /* ★ 设计关键点：单次测量 + 时钟拉伸，测完回空闲 */
    /* 温湿度一条命令一起回来，省一次总线往返 */
    uint8_t ret = sht30_read_temperature_humidity(&g_sht30,
                                                   &temperature, &humidity);
    if (0U != ret) return to_hal(ret);

    *temp = (int16_t)(temperature * 100.0f);
    *humi = (uint16_t)(humidity * 100.0f);

    return ERR_OK;
}
