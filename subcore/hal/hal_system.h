/* hal_system.h — 时钟 + 低功耗模式
 *
 * 副核的电源管理入口。所有模块默认休眠，有任务才醒 —— 这就是这个项目
 * 能撑住电池的根本原因。
 *
 * 三种状态：
 *   NORMAL    16MHz，采样/通信时用
 *   LOW_POWER  2MHz，降功耗但还能跑
 *   STANDBY    SRAM 全丢，只有 RTC 闹钟和 I2C 地址匹配能唤醒，
 *              醒来是从复位向量重跑，不是函数返回
 */

#ifndef HAL_SYSTEM_H
#define HAL_SYSTEM_H

#include "global_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   时钟初始化
 * @note    IRC16M 直驱 16MHz，不开 PLL。
 *          AHB=16M / APB1=8M / APB2=16M
 * @return  永远返回 ERR_OK（留个返回值以后加晶振检测方便）
 */
ErrorCode_t HalSystem_ClockInit(void);

/**
 * @brief   降到 2MHz（AHB ÷8）
 */
void HalSystem_EnterLowPowerMode(void);

/**
 * @brief   升回 16MHz
 */
void HalSystem_EnterNormalMode(void);

/**
 * @brief   当前在哪个模式
 */
SubCoreMode_t HalSystem_GetMode(void);

/**
 * @brief   进 Standby 深度休眠
 * @note    顺序不能变：存状态 → 断主核电 → 清中断 → 设唤醒源 → 睡。
 *          ⚠️ 正常情况下这个函数不返回，醒了等于重新上电
 */
void HalSystem_EnterStandby(void);

/**
 * @brief   这次是 Standby 唤醒还是冷启动
 */
bool HalSystem_IsStandbyWakeup(void);

/**
 * @brief   毫秒延时（SysTick 驱动，中间 WFI）
 */
void HalSystem_DelayMs(uint32_t ms);

/**
 * @brief   微秒延时
 * @note    空循环凑的，精度一般，别用在要求高的地方
 */
void HalSystem_DelayUs(uint32_t us);

/**
 * @brief   上电至今的毫秒数
 */
uint32_t HalSystem_GetTickMs(void);

/**
 * @brief   SysTick 中断服务（由 SysTick_Handler 转发）
 * @note    只加计数，中断里别干别的
 */
void HalSystem_SysTickHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SYSTEM_H */
