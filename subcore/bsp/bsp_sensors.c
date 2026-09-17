/* bsp_sensors.c — 四个传感器一起开、一起睡
 *
 * 开机 InitAll，进 Standby 之前 SleepAll。
 * 容错是有偏向的：只有体温那颗失败才往上报，别的坏了就坏了 ——
 * 没血氧照样能徒步，没体温这表就没意义了。
 */

#include "bsp_sensors.h"
#include "hal_i2c0.h"

ErrorCode_t BSP_Sensors_InitAll(void)
{
    ErrorCode_t err;

    /* 1. 先把 I2C0 起起来
     * 注：每颗传感器 init 时驱动接口还会把 I2C0 重配一遍，
     * 这一句主要是先把 PF6/PF7 那对复用脚配好 */
    err = HalI2c0_MasterInit();
    if (ERR_OK != err) return err;

    /* 2. 一颗一颗来，谁坏了都不拦别人 */

    err = BSP_MPU6050_Init();
    if (ERR_OK != err) {
        /* 计步器坏了不致命，继续 */
    }

    /* ★ 设计关键点：只有体温算硬依赖，其余坏了不拦启动 */
    err = BSP_MLX90614_Init();
    if (ERR_OK != err) {
        /* 体温是命根子，直接往上报 */
        return err;
    }

    err = BSP_SHT30_Init();
    if (ERR_OK != err) {
        /* 环境温湿度坏了不致命 */
    }

    err = BSP_MAX30102_Init();
    if (ERR_OK != err) {
        /* 血氧坏了也不致命（算法本来就没写） */
    }

    /* 能走到这儿说明硬依赖都活着；err 早被覆盖了，别去读它 */
    return ERR_OK;
}

void BSP_Sensors_SleepAll(void)
{
    /* ★ 设计关键点：先器件后总线，总线一关器件就睡不下 */
    BSP_MPU6050_Sleep();
    BSP_MLX90614_Sleep();
    BSP_SHT30_Sleep();
    BSP_MAX30102_Shutdown();
    /* 四颗各关一次总线，重复关也没什么后果 */

    /* 总线也关掉，睡就睡干净 */
    HalI2c0_DeInit();
}
