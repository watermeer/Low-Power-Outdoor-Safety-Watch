/* alarm_manager.h — ABCD 四级告警状态机
 *
 * A静默 → B加密采样 → C本地震动 → D唤醒主核
 *
 * 对外只有 Feed/Get 两组接口，等级怎么爬、什么时候降都是内部的事，
 * 上层（sample_scheduler）只管把采样值喂进来，别的不用操心。
 */

#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include "global_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 状态快照（调试 / 主核查询用）
 *============================================================================*/

typedef struct {
    AlarmLevel_t current_level;      /* 当前等级 */
    AlarmType_t  current_alarm_type; /* 无告警时 NONE */
    float        last_temp;          /* 最近原始体温 (°C) */
    float        last_spo2;          /* 最近血氧 (%) */
    float        temp_slope;         /* 体温斜率，调试看趋势 */
    uint32_t     last_d_alarm_time;  /* 上次D级时间，算冷却用 */
    bool         maincore_on;        /* 主核当前有没有供电 */
} AlarmMgr_Status_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @note    从备份域恢复等级 —— Standby 唤醒后不能从 A 级重来，
 *          不然用户在 C 级被震醒，一觉醒来又变回 A 了
 */
void AlarmMgr_Init(void);

/**
 * @brief   喂体温
 * @param   temp     体温 (°C)
 * @param   amb_temp 环境温度 (°C)，补偿用
 * @note    内部走完六层管线，调用方不用管
 */
void AlarmMgr_FeedTemp(float temp, float amb_temp);

/**
 * @brief   喂血氧
 * @param   spo2 血氧 (%)
 */
void AlarmMgr_FeedSpO2(float spo2);

/**
 * @brief   取当前等级
 */
AlarmLevel_t AlarmMgr_GetCurrentLevel(void);

/**
 * @brief   取当前告警类型
 */
AlarmType_t AlarmMgr_GetCurrentAlarmType(void);

/**
 * @brief   执行当前等级对应的动作
 * @note    每轮采样完由调度器调一次
 *          A: 无 / B: 无（频率已在升级时调过）/ C: 短震 / D: 唤醒主核
 */
void AlarmMgr_ProcessAlarmAction(void);

/**
 * @brief   取状态快照
 */
void AlarmMgr_GetStatus(AlarmMgr_Status_t *status);

/**
 * @brief   取体温历史（给主核画曲线用）
 * @param   count 出参：条数
 * @param   data  出参：°C×10 数组，data[0] 是最新的
 */
void AlarmMgr_GetTempHistory(uint8_t *count, int16_t *data);

/**
 * @brief   强制回 A 级
 * @note    主核发 CMD_SLEEP 说"我知道了"之后调用，等于用户确认过了
 */
void AlarmMgr_ResetToLevelA(void);

#ifdef __cplusplus
}
#endif

#endif /* ALARM_MANAGER_H */
