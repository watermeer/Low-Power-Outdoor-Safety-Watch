/* drv_mlx90614.c — MLX90614 红外体温，板级绑定
 *
 * 挂 I2C0，出厂地址 0x5A，EEPROM 没动过。
 * PEC 校验驱动里做了，这层不用管。
 *
 * 三个传感器里它最省事：没有参数要配，init 就是读一次环境温度
 * 确认通信。失败还静默，读出来是 0.0 —— 上层自己判合不合理。
 */

#include "driver_mlx90614.h"
#include "driver_mlx90614_interface.h"

/* 就一个句柄 */
static mlx90614_handle_t g_mlx90614_handle;

/* 绑句柄 + 设地址 + init，没有别的要配 */
void MLX90614_Init(void)
{
    /* 句柄清零 */
    DRIVER_MLX90614_LINK_INIT(&g_mlx90614_handle, mlx90614_handle_t);

    /* 回调绑一遍，它没有中断回调这种接口 */
    DRIVER_MLX90614_LINK_IIC_INIT(&g_mlx90614_handle, mlx90614_interface_iic_init);
    DRIVER_MLX90614_LINK_IIC_DEINIT(&g_mlx90614_handle, mlx90614_interface_iic_deinit);
    DRIVER_MLX90614_LINK_IIC_READ(&g_mlx90614_handle, mlx90614_interface_iic_read);
    DRIVER_MLX90614_LINK_IIC_WRITE(&g_mlx90614_handle, mlx90614_interface_iic_write);
    DRIVER_MLX90614_LINK_DELAY_MS(&g_mlx90614_handle, mlx90614_interface_delay_ms);
    DRIVER_MLX90614_LINK_DEBUG_PRINT(&g_mlx90614_handle, mlx90614_interface_debug_print);

    /* 出厂地址 0x5A，没改过 */
    (void)mlx90614_set_addr_pin(&g_mlx90614_handle, MLX90614_ADDRESS_DEFAULT);
    /* init 里读一次环境温度当验证；返回码丢掉，挂了也是静默 */
    (void)mlx90614_init(&g_mlx90614_handle);
}

/* 读体温，走物体 1 通道。
 * 拿到的是皮温，不是体核温 —— 补偿那步在 app 层做 */
void MLX90614_Read(int16_t *temp_body)
{
    float object_temp = 0.0f;

    /* 失败静默，object_temp 保持 0 */
    (void)mlx90614_read_temperature(&g_mlx90614_handle, MLX90614_TEMP_OBJECT1, &object_temp);

    if (NULL != temp_body) {
        /* ★ 设计关键点：出驱动转定点，上层不碰 float */
        *temp_body = (int16_t)(object_temp * 100.0f);
    }
}
