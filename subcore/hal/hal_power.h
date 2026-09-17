/* hal_power.h — 主核电源开关 + 震动马达
 *
 * 就俩引脚，主核的命攥在这儿。
 * 高低电平对应什么，看 .c —— 猜反过一次，代价是电池。
 */

#ifndef HAL_POWER_H
#define HAL_POWER_H

#include "global_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 硬件引脚宏（按实际 PCB 修改）
 *
 * 引脚号是照打样那块板填的，画新板记得同步过来
 *============================================================================*/

/** @brief 主核电源控制引脚 — PB1 */
#define HAL_POWER_MAINCORE_PORT   GPIOB
#define HAL_POWER_MAINCORE_PIN    GPIO_PIN_1

/** @brief 震动马达控制引脚 — PB0 */
#define HAL_POWER_VIBRATE_PORT    GPIOB
#define HAL_POWER_VIBRATE_PIN     GPIO_PIN_0

/* P-MOS 低电平导通。这宏加上就没被引用过，代码里都是直接
 * gpio_bit_reset/set —— 留着当个说明书 */
#define HAL_POWER_PMOS_ON_LEVEL   false

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化电源控制引脚
 * @note    两个脚都配推挽输出，默认主核断电、震动关
 * @return  ERR_OK 成功
 */
ErrorCode_t HalPower_Init(void);

/* ★ 设计关键点：主核电源副核门控，比 Stop 省 */
/**
 * @brief   主核上电
 * @note    栅极拉低。里面约 10ms 忙等，是阻塞的，别在中断里调
 */
void HalPower_MainCoreOn(void);

/**
 * @brief   主核断电
 * @note    栅极拉高，硬断。主核会先求 CMD_SLEEP，这边回完 OK 才断
 */
void HalPower_MainCoreOff(void);

/**
 * @brief   查主核在不在上电
 * @return  true=已上电, false=已断电
 */
bool HalPower_IsMainCoreOn(void);

/**
 * @brief   震动开关（底下就是拉高拉低）
 * @param   on true=震, false=停
 */
void HalPower_VibrateSet(bool on);

/* ★ 设计关键点：震动两档：C 级点一下，D 级求救 */
/**
 * @brief   短震，C 级用
 * @note    底下是死等 100ms，不是"立即返回"，这句老注释写错了
 *          （TODO 顺手改掉）
 */
void HalPower_VibratePulseShort(void);

/**
 * @brief   长震，D 级用
 * @note    不自己停，得有人调 HalPower_VibrateSet(false)。
 *          故意不留超时 —— 理由写在 .c 里
 */
void HalPower_VibratePulseLong(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_POWER_H */
