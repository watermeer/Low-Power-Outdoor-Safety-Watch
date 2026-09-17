/* maincore_vibrate.h — 震动马达接口（TIM3_CH3 / PB0）
 *
 * 三种震法：短震、模式震、持续震。主核现在只用 PATTERN。
 * 全都是阻塞调用，调用方心里有个数。
 */

#ifndef MAINCORE_VIBRATE_H
#define MAINCORE_VIBRATE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 震动模式 */
typedef enum {
    VIBRATE_OFF     = 0,  /**< 关闭 */
    VIBRATE_SHORT   = 1,  /**< 短震 (100ms) — C级提示 */
    VIBRATE_PATTERN = 2,  /**< 模式震 (200ms×3) — D级提醒 */
    VIBRATE_CONTINUOUS = 3, /**< 持续震动 — D级告警 */
} VibrateMode_t;

/** @brief 初始化 TIM3_CH3 PWM，初始不震 */
void        MainCore_VibrateInit(void);

/** @brief 直接开关（自己管时序时用） */
void        MainCore_VibrateSet(bool on);

/** @brief 按模式震一次 —— 阻塞，PATTERN 要 1 秒多 */
void        MainCore_VibratePulse(VibrateMode_t mode);

/** @brief 立即停 */
void        MainCore_VibrateStop(void);

/** @brief 查状态，暂时没人调 */
bool        MainCore_IsVibrating(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINCORE_VIBRATE_H */
