/* hal_rtc.h — RTC 闹钟 + 备份域
 *
 * 副核唯一的"时间感"，也是唯一的持久化手段，都在这儿。
 * 备份域就 5 个 32 位，省着用。
 */

#ifndef HAL_RTC_H
#define HAL_RTC_H

#include "global_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * RTC 备份域寄存器索引（GD32L233 有 5 个 32 位备份寄存器 BKP0~BKP4）
 *
 * BKP 由 VBAT 域供电，Standby 期间 SRAM 全丢，就剩它能留东西。
 * VBAT 也掉 → 全清零 → 当冷启动处理。
 *
 *   BKP0  当前告警等级 A/B/C/D
 *   BKP1  体温采样间隔（秒）—— 只有体温是动态的，别的固定不用存
 *   BKP2  累计步数低 16 位，Standby 前把 DMP 增量累加进去
 *   BKP3  运行魔数，用来认冷启动还是唤醒
 *   BKP4  空着，没找到非它不可的用途
 *============================================================================*/

/* ★ 设计关键点：BKP 只有 5 个，只存不能丢的 */

/* 告警等级 A/B/C/D */
#define RTC_BKP_ALARM_LEVEL      0U

/* 体温采样间隔（秒） */
#define RTC_BKP_SAMPLE_INTERVAL  1U

/* 累计步数低 16 位 */
#define RTC_BKP_STEP_COUNT_L     2U

/* 系统运行标记 */
#define RTC_BKP_RUN_MARKER       3U

/* 魔数，选这个值纯粹因为好看好认 */
#define RTC_RUN_MAGIC  0xDEADBEEFU

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化 RTC
 * @note    时钟源是 IRC32K。这行以前写的是 LSI —— STM32 的叫法，
 *          移植时照抄吃了亏，.c 开头有记录
 * @return  ERR_OK 成功; ERR_HARDWARE 失败
 */
ErrorCode_t HalRtc_Init(void);

/* ★ 设计关键点：闹钟是匹配时刻，跨天 %24 兜住 */
/**
 * @brief   设置 RTC 闹钟（相对时间）
 * @param   seconds 从当前时刻起多少秒后唤醒
 * @return  ERR_OK 成功
 * @note    硬件只会比时刻，所以内部先读当前时间再加秒数
 *          改采样间隔前先 Cancel，不然旧闹钟还会把人叫醒一次
 */
ErrorCode_t HalRtc_SetAlarm(uint32_t seconds);

/**
 * @brief   取消当前的 RTC 闹钟
 * @note    只关中断位，不清 RTC 计数
 */
void HalRtc_CancelAlarm(void);

/**
 * @brief   取"今天已经过了多少秒"
 * @return  0 ~ 86399，过零点回绕到 0
 * @note    上面那句以前写的是"距上次设闹钟的已流逝时间"，是错的！
 *          算时间差的地方都得处理回绕（窗口那边就补过一次）
 */
uint32_t HalRtc_GetElapsedSeconds(void);

/* ---- 备份域操作 ---- */

/**
 * @brief   写备份域寄存器
 * @param   index 索引 0~4，越界直接丢
 * @param   value 写入值
 */
void HalRtc_BackupWrite(uint8_t index, uint32_t value);

/**
 * @brief   读备份域寄存器
 * @param   index 索引 0~4，越界返回 0
 * @return  寄存器值
 */
uint32_t HalRtc_BackupRead(uint8_t index);

/* ★ 设计关键点：魔数、闹钟标志都查，缺一当冷启动 */
/**
 * @brief   是不是冷启动（非 Standby 唤醒）
 * @return  true=冷启动（首次上电/复位）, false=唤醒
 * @note    查 BKP3 里魔数在不在，再确认闹钟标志
 */
bool HalRtc_IsColdBoot(void);

/**
 * @brief   打上"已运行"魔数
 * @note    冷启动初始化做完才调。提前调的话，半路休眠会被当成唤醒
 */
void HalRtc_MarkRunning(void);

/**
 * @brief   这次唤醒是不是闹钟干的
 * @return  true=闹钟唤醒
 */
bool HalRtc_IsWakeupFromAlarm(void);

/**
 * @brief   RTC 闹钟中断（由 RTC_Alarm_IRQHandler 转发）
 * @note    只清标志。真活儿留给主循环 —— 中断里干活会跟 I2C1 抢时间
 */
void HalRtc_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_RTC_H */
