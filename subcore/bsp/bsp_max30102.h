/* bsp_max30102.h — MAX30102 心率 / 血氧
 *
 * 驱动层配齐了，算法层是空的：两个读接口现在都返回占位值。
 * 上层按正常接口调就行，Phase 4 把 PPG 那套补上时接口不用动。
 */

#ifndef BSP_MAX30102_H
#define BSP_MAX30102_H

#include "global_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @note    绑 I2C0 → 查 PART_ID(0x15) → 配 SpO2 模式(红+红外，100Hz)
 *          → FIFO(16 平均) → 开 PPG 就绪中断
 * @return  ERR_OK 成功; ERR_NOT_FOUND ID 不对; ERR_HARDWARE 初始化失败
 */
ErrorCode_t BSP_MAX30102_Init(void);

/**
 * @brief   完全断电休眠
 * @note    置 Shutdown 位，LED 驱动和采样全关，功耗从 ~20mA 掉到 <1μA
 * @return  ERR_OK 成功
 */
ErrorCode_t BSP_MAX30102_Shutdown(void);

/**
 * @brief   读血氧
 * @param   spo2 出参：%（0~100；读不到是 0）
 * @return  ERR_OK 成功; ERR_TIMEOUT 通信失败
 * @warning 算法未实现 —— 固定返回占位值 98，上层拿到的不是真数据，
 *          血氧告警那条链现在跑的是假值，等 Phase 4 把 PPG 管线补上
 */
ErrorCode_t BSP_MAX30102_ReadSpO2(uint8_t *spo2);

/**
 * @brief   读心率
 * @param   hr 出参：BPM（无效是 0）
 * @return  ERR_OK 成功
 * @note    同样没实现，固定返回 0 —— 算法和 SpO2 一起在 Phase 4 做
 */
ErrorCode_t BSP_MAX30102_ReadHeartRate(uint8_t *hr);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MAX30102_H */
