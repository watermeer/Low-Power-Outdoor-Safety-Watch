/* bsp_mlx90614.c — MLX90614 红外非接触测温
 *
 * 体温这条线的源头，读数直接喂给告警状态机，单位别记错：°C×100。
 * 这颗没有 ID 寄存器，只能读一次环境温度来确认它还活着。
 *
 * 睡是 deinit 顺带发的 SLEEP 命令（~2μA），代价是句柄的 inited 被清掉，
 * 醒过来谁要用它得先重新 Init 一遍，不然读出来全是错误码。
 */

#include "bsp_mlx90614.h"
#include "driver_mlx90614.h"
#include "driver_mlx90614_interface.h"
#include "hal_i2c0.h"

static mlx90614_handle_t g_mlx90614;

/* LibDriver → ErrorCode_t
 * 注意驱动里 PEC 校验失败也返回 1，到这儿就变成 ERR_TIMEOUT 了，
 * 头文件里写的 ERR_CHECKSUM 实际到不了，照实记一笔 */
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

ErrorCode_t BSP_MLX90614_Init(void)
{
    uint8_t ret;

    DRIVER_MLX90614_LINK_INIT(&g_mlx90614, mlx90614_handle_t);
    DRIVER_MLX90614_LINK_IIC_INIT(&g_mlx90614, mlx90614_interface_iic_init);
    DRIVER_MLX90614_LINK_IIC_DEINIT(&g_mlx90614, mlx90614_interface_iic_deinit);
    DRIVER_MLX90614_LINK_IIC_READ(&g_mlx90614, mlx90614_interface_iic_read);
    DRIVER_MLX90614_LINK_IIC_WRITE(&g_mlx90614, mlx90614_interface_iic_write);
    DRIVER_MLX90614_LINK_DELAY_MS(&g_mlx90614, mlx90614_interface_delay_ms);
    DRIVER_MLX90614_LINK_DEBUG_PRINT(&g_mlx90614, mlx90614_interface_debug_print);

    ret = mlx90614_set_addr_pin(&g_mlx90614, MLX90614_ADDRESS_DEFAULT);
    if (0U != ret) return to_hal(ret);

    ret = mlx90614_init(&g_mlx90614);
    if (0U != ret) return to_hal(ret);

    /* ★ 设计关键点：没 ID 寄存器，环境温度证明在场 */
    float amb = 0.0f;
    ret = mlx90614_read_temperature(&g_mlx90614, MLX90614_TEMP_AMBIENT, &amb);
    if (0U != ret) return ERR_NOT_FOUND;

    return ERR_OK;
}

/*==============================================================================
 * 休眠 / 读取
 *============================================================================*/

ErrorCode_t BSP_MLX90614_Sleep(void)
{
    /* ★ 设计关键点：SLEEP 压 2μA，无 MOS */
    /* deinit 内部会发 SMBus SLEEP，顺手把 inited 也清了
     * —— 再读就是 ERR_NOT_INIT，要用得重新 Init */
    uint8_t ret = mlx90614_deinit(&g_mlx90614);
    return to_hal(ret);
}

ErrorCode_t BSP_MLX90614_ReadTemp(int16_t *temp_body)
{
    if (NULL == temp_body) return ERR_NULL_POINTER;

    float object_temp = 0.0f;   /* Object1 = 正对的那片视野 */
    uint8_t ret = mlx90614_read_temperature(&g_mlx90614,
                                             MLX90614_TEMP_OBJECT1,
                                             &object_temp);
    if (0U != ret) return to_hal(ret);

    *temp_body = (int16_t)(object_temp * 100.0f);   /* 统一 ×100，上层再除回来 */
    return ERR_OK;
}

ErrorCode_t BSP_MLX90614_ReadAmbientTemp(int16_t *temp_ambient)
{
    if (NULL == temp_ambient) return ERR_NULL_POINTER;

    float amb_temp = 0.0f;   /* 环境温度 —— 体温补偿那一层要它 */
    uint8_t ret = mlx90614_read_temperature(&g_mlx90614,
                                             MLX90614_TEMP_AMBIENT,
                                             &amb_temp);
    if (0U != ret) return to_hal(ret);

    *temp_ambient = (int16_t)(amb_temp * 100.0f);
    return ERR_OK;
}
