/* bsp_mlx90614.h — MLX90614 红外非接触测温
 *
 * 体温主线就靠它，两个接口单位统一 °C×100。
 * Sleep 之后再读会一路报错，得重新 Init —— 驱动 deinit 的副作用。
 */

#ifndef BSP_MLX90614_H
#define BSP_MLX90614_H

#include "global_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @note    绑 I2C0，再读一次环境温度当通信验证（这颗没有 ID 寄存器）
 * @return  ERR_OK 成功; ERR_NOT_FOUND 读不到; ERR_HARDWARE 初始化失败
 */
ErrorCode_t BSP_MLX90614_Init(void);

/**
 * @brief   进休眠（SMBus SLEEP 命令，~2μA）
 * @note    唤醒不用管，来一次 I2C 通信它自己就醒；
 *          但睡醒后第一次读之前要留稳定时间，不然读出来不准
 * @return  ERR_OK 成功
 */
ErrorCode_t BSP_MLX90614_Sleep(void);

/**
 * @brief   读人体温度
 * @param   temp_body 出参：°C × 100（3650 = 36.50°C）
 * @return  ERR_OK 成功; 失败基本只见到 ERR_TIMEOUT
 * @note    PEC 校验在驱动里做，挂了也返回 1 → 这边翻成 ERR_TIMEOUT，
 *          原来头文件写的 ERR_CHECKSUM 到不了，别照着判
 */
ErrorCode_t BSP_MLX90614_ReadTemp(int16_t *temp_body);

/**
 * @brief   读环境温度
 * @param   temp_ambient 出参：°C × 100
 * @return  ERR_OK 成功; ERR_TIMEOUT 失败
 * @note    体温补偿（管线第 1 层）要用它，别把这个接口停了
 */
ErrorCode_t BSP_MLX90614_ReadAmbientTemp(int16_t *temp_ambient);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MLX90614_H */
