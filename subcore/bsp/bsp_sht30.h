/* bsp_sht30.h — SHT30 环境温湿度
 *
 * 它量的是环境，不是体温，主要看湿度。
 * 单次测量、时钟拉伸，读到的是 °C×100 和 %×100。
 */

#ifndef BSP_SHT30_H
#define BSP_SHT30_H

#include "global_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @note    绑 I2C0 → 软复位（init 里自带）→ 重复性设 HIGH
 * @return  ERR_OK 成功; ERR_HARDWARE 初始化失败
 */
ErrorCode_t BSP_SHT30_Init(void);

/**
 * @brief   进休眠
 * @note    它单次测完本来就回空闲（~0.2μA），这里主要是清 inited + 关总线；
 *          睡完要读得重新 Init
 * @return  ERR_OK 成功
 */
ErrorCode_t BSP_SHT30_Sleep(void);

/**
 * @brief   读环境温湿度
 * @param   temp 出参：°C × 100（2550 = 25.50°C）
 * @param   humi 出参：% × 100（6000 = 60.00%）
 * @return  ERR_OK 成功; ERR_TIMEOUT 通信失败
 * @note    CRC 挂了在驱动里就被拦下了，返回的也是 1 → ERR_TIMEOUT，
 *          没有单独的 ERR_CHECKSUM 给你
 */
ErrorCode_t BSP_SHT30_ReadData(int16_t *temp, uint16_t *humi);

#ifdef __cplusplus
}
#endif

#endif /* BSP_SHT30_H */
