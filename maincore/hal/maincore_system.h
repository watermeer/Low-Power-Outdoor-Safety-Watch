/* maincore_system.h — 时钟 / GPIO / SysTick 对外接口
 *
 * 能用到的就下面这几个，没有隐藏技能。
 * 500ms 那条线是 MAINCORE_MAX_RUN_MS，主循环拿它兜底。
 */

#ifndef MAINCORE_SYSTEM_H
#define MAINCORE_SYSTEM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 硬件参数宏（按实际 PCB 修改）
 *============================================================================*/

/** @brief 系统时钟频率 (Hz) — 8MHz HSE × 9 */
#define MAINCORE_SYSCLK_HZ     72000000U

/** @brief 最大运行时长 (ms) — 超时强制请求断电 */
#define MAINCORE_MAX_RUN_MS    500U

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   系统时钟初始化
 * @note    HSE 8MHz → PLL ×9 → 72MHz
 *          AHB=72MHz, APB1=36MHz, APB2=72MHz
 */
void MainCore_ClockInit(void);

/**
 * @brief   GPIO 初始化
 * @note    实际只开端口时钟，引脚各模块自己配
 */
void MainCore_GpioInit(void);

/**
 * @brief   SysTick 初始化（1ms 滴答）
 */
void MainCore_SysTickInit(void);

/**
 * @brief   毫秒延时
 * @param   ms 毫秒数
 * @note    WFI 等滴答，别在中断里调
 */
void MainCore_DelayMs(uint32_t ms);

/**
 * @brief   获取上电后运行时间（毫秒）
 * @return  运行毫秒数
 * @note    超时控制全靠它：> MAINCORE_MAX_RUN_MS 强制 SLEEP
 */
uint32_t MainCore_GetUptimeMs(void);

/**
 * @brief   系统软复位
 * @note    HardFault 兜底用，正常流程走不到
 */
void MainCore_SoftReset(void);

/**
 * @brief   SysTick 中断服务（由 SysTick_Handler 转发）
 * @note    只累加毫秒计数，别在这里干别的
 */
void MainCore_SysTickHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINCORE_SYSTEM_H */
