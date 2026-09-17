/* sample_scheduler.h — 采样调度
 *
 * 副核每次醒来都得先问它：这次该采谁？
 *
 * 设计上刻意不做"统一节拍全采一遍" —— 计步要 5 秒级实时，
 * 体温 10 分钟就够，全按 5 秒采的话电池两天就没。
 * 所以每个传感器各算各的到期时间，谁到点了采谁。
 */

#ifndef SAMPLE_SCHEDULER_H
#define SAMPLE_SCHEDULER_H

#include "global_types.h"
#include "global_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 唤醒原因
 *============================================================================*/

typedef enum {
    SAMPLE_EVENT_NONE      = 0,  /* 没事件 */
    SAMPLE_EVENT_RTC       = 1,  /* 闹钟到点 */
    SAMPLE_EVENT_MOTION    = 2,  /* 用户动了（抬腕/走路） */
    SAMPLE_EVENT_I2C       = 3,  /* 主核来找我们说话 */
    SAMPLE_EVENT_COLD_BOOT = 4,  /* 冷启动，得全采一遍 */
} SampleEvent_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @note    会从 BKP1 把上次的体温间隔捞回来 ——
 *          Standby 醒来不能退回 10 分钟，不然 D 级白报了
 */
void SampleSched_Init(void);

/**
 * @brief   跑一轮调度
 * @param   event 这次是谁把我叫醒的
 * @note    流程：判断各传感器到期没 → 采 → 喂给告警机 → 定下次闹钟
 *          → 这轮啥也没采就直接进 Standby
 */
void SampleSched_Process(SampleEvent_t event);

/**
 * @brief   改体温采样间隔
 * @note    告警等级一变就会调这个。会立刻重设闹钟，不是等下轮生效
 */
void SampleSched_SetTempInterval(uint16_t interval_sec);

/**
 * @brief   改血氧采样间隔
 * @note    目前没人调，血氧一直是固定 30 分钟
 */
void SampleSched_SetSpO2Interval(uint16_t interval_sec);

/**
 * @brief   强制全量采一次
 * @note    冷启动和 D 级告警用
 */
void SampleSched_UpdateAllSample(void);

/**
 * @brief   当前体温采样间隔
 */
uint16_t SampleSched_GetTempInterval(void);

/**
 * @brief   这个传感器该采了吗
 */
bool SampleSched_NeedSample(SensorType_t sensor_type);

/**
 * @brief   上次采它是什么时候（RTC 秒）
 */
uint32_t SampleSched_GetLastSampleTime(SensorType_t sensor_type);

/**
 * @brief   累计步数
 * @note    存在 BKP2 里。MPU6050 进 Standby 会断电、DMP 计数清零，
 *          所以每次睡前把当轮步数累加进去，醒来接着数
 */
uint32_t SampleSched_GetAccumulatedSteps(void);

#ifdef __cplusplus
}
#endif

#endif /* SAMPLE_SCHEDULER_H */
