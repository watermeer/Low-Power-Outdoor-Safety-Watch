/* alarm_manager.c — ABCD 四级告警状态机
 *
 * 副核的命根子。动这里之前先想清楚，动完必须把 A→B→C→D→A 整条路径重跑一遍。
 *
 * 改过三版，留个记录免得以后忘了为什么长这样：
 *   7月 只看阈值        → 传感器抖一下就叫，误报多到没法用
 *   8月 加趋势方向      → 阈值 AND 同向趋势，勉强能看
 *   9月 加卡尔曼+加权斜率 → 噪声压下去，才敢叫它"趋势预警"
 *
 * 现在体温走的是六层：
 *   环境补偿 → 卡尔曼 → 带时间戳窗口 → 加权斜率 → 双条件 → 防抖
 */

#include "alarm_manager.h"
#include "kalman_filter.h"    /* 体温管线第2层 */
#include "sliding_window.h"
#include "trend_algorithm.h"
#include "sample_scheduler.h"
#include "hal_rtc.h"
#include "hal_power.h"
#include "hal_system.h"
#include "protocol.h"
#include "hal_i2c1.h"
#include <stddef.h>

/*==============================================================================
 * 内部状态
 *============================================================================*/

static SlidingWindow_t s_tempWindow;   /* 体温窗口 */
static SlidingWindow_t s_spo2Window;   /* 血氧窗口 */

static Kalman1D_t   s_kalmanTemp;      /* 体温滤波器 */
static float        s_filteredTemp;    /* 滤波后体温（判定用这个） */

static AlarmLevel_t s_level     = ALARM_LEVEL_A;
static AlarmType_t  s_alarmType = ALARM_TYPE_NONE;
static float        s_lastTemp  = 36.5f;   /* 存原始值，上报给主核 */
static float        s_lastSpO2  = 98.0f;
static uint32_t     s_lastDAlarmTime = 0;

/* 升级防抖计数，每个等级一个 */
static uint8_t s_levelB_confirm = 0;
static uint8_t s_levelC_confirm = 0;
static uint8_t s_levelD_confirm = 0;

#define ALARM_CONFIRM_COUNT  2   /* 连2次才认，单次不算 */

/*==============================================================================
 * 内部函数声明
 *============================================================================*/

static float apply_env_compensation(float skin_temp, float amb_temp);
static void  evaluate_temp_level(float filtered_temp);
static void  evaluate_spo2_level(float spo2);
static void  try_upgrade_level(AlarmLevel_t target, AlarmType_t type);
static void  try_downgrade_level(void);

/*==============================================================================
 * 初始化
 *============================================================================*/

void AlarmMgr_Init(void)
{
    Window_Init(&s_tempWindow, TEMP_WINDOW_LEN);
    Window_Init(&s_spo2Window, SPO2_WINDOW_LEN);

    /* Q<P 是有意的：宁可相信"体温不变"这个物理模型，
     * 也别跟着传感器单点跳变走 */
    Kalman1D_Init(&s_kalmanTemp,
                  KALMAN_TEMP_Q,
                  KALMAN_TEMP_R,
                  KALMAN_TEMP_INIT_P);
    s_filteredTemp = 36.5f;  /* 先按正常体温起步 */

    /* 等级从备份域捞回来 —— Standby 唤醒后状态不能丢 */
    uint32_t saved = HalRtc_BackupRead(RTC_BKP_ALARM_LEVEL);
    if (saved <= ALARM_LEVEL_D) {
        s_level = (AlarmLevel_t)saved;
    } else {
        s_level = ALARM_LEVEL_A;   /* 数据被写花了，退回安全态 */
    }

    s_alarmType       = ALARM_TYPE_NONE;
    s_levelB_confirm  = 0;
    s_levelC_confirm  = 0;
    s_levelD_confirm  = 0;
    s_lastDAlarmTime  = 0;
}

/*==============================================================================
 * 数据输入
 *============================================================================*/

void AlarmMgr_FeedTemp(float temp, float amb_temp)
{
    s_lastTemp = temp;   /* 原始值留给主核显示，别丢 */

    /* 第1层：环境补偿 */
    float compensated = apply_env_compensation(temp, amb_temp);

    /* 第2层：卡尔曼 */
    s_filteredTemp = Kalman1D_Update(&s_kalmanTemp, compensated);

    /* 第3层：入窗（顺手记时间戳） */
    int16_t val = (int16_t)(s_filteredTemp * 10.0f);
    Window_Push(&s_tempWindow, val);

    /* 第4~6层都在这个函数里 */
    evaluate_temp_level(s_filteredTemp);
}

void AlarmMgr_FeedSpO2(float spo2)
{
    s_lastSpO2 = spo2;

    /* 存成 %×10，跟温度窗口统一格式 */
    int16_t val = (int16_t)(spo2 * 10.0f);
    Window_Push(&s_spo2Window, val);

    evaluate_spo2_level(spo2);
}

/*==============================================================================
 * 体温等级评估
 *============================================================================*/

static void evaluate_temp_level(float filtered_temp)
{
    int16_t temp_x10 = (int16_t)(filtered_temp * 10.0f);
    /* 用加权版：采样间隔会变(600s/120s/30s)，等权版斜率没物理意义 */
    TrendDirection_t trend = Trend_GetTempDirectionWeighted(&s_tempWindow,
                                                             KALMAN_TAU_SEC);

    /* ---- D 级：≤34.5°C 或 ≥39.5°C ---- */
    /* ★ 设计关键点：兜底直通：≤34.0 不等趋势 */
    if (temp_x10 <= TEMP_THRESHOLD_D_LOW &&
        (trend == TREND_FALLING || temp_x10 <= 340)) {
        try_upgrade_level(ALARM_LEVEL_D, ALARM_TYPE_TEMP_LOW);
        return;
    }
    if (temp_x10 >= TEMP_THRESHOLD_D_HIGH &&
        (trend == TREND_RISING || temp_x10 >= 400)) {
        try_upgrade_level(ALARM_LEVEL_D, ALARM_TYPE_TEMP_HIGH);
        return;
    }

    /* ---- C 级：34.5~35.0 或 38.5~39.5 ---- */
    if (temp_x10 <= TEMP_THRESHOLD_C_LOW &&
        temp_x10 > TEMP_THRESHOLD_D_LOW) {
        if (trend == TREND_FALLING) {
            try_upgrade_level(ALARM_LEVEL_C, ALARM_TYPE_TEMP_LOW);
            return;
        }
    }
    if (temp_x10 >= TEMP_THRESHOLD_C_HIGH &&
        temp_x10 < TEMP_THRESHOLD_D_HIGH) {
        if (trend == TREND_RISING) {
            try_upgrade_level(ALARM_LEVEL_C, ALARM_TYPE_TEMP_HIGH);
            return;
        }
    }

    /* ---- 速率告警：绝对值正常但掉得太快 ----
     * ★ 设计关键点：第二条触发路径，绕开阈值只看变化速率
     * 温度还在 A 级范围里，但斜率已经陡到不正常 → 提前给 B 级
     */
    {
        float weighted_slope = Window_CalcSlopeWeighted(&s_tempWindow,
                                                         KALMAN_TAU_SEC);
        if (weighted_slope < TREND_TEMP_RAPID_FALLING_PER_SEC) {
            try_upgrade_level(ALARM_LEVEL_B, ALARM_TYPE_TEMP_LOW);
            return;
        }
        if (weighted_slope > TREND_TEMP_RAPID_RISING_PER_SEC) {
            try_upgrade_level(ALARM_LEVEL_B, ALARM_TYPE_TEMP_HIGH);
            return;
        }
    }

    /* ---- B 级：35.0~35.5 或 38.0~38.8 ---- */
    if (temp_x10 <= TEMP_THRESHOLD_B_LOW &&
        temp_x10 > TEMP_THRESHOLD_C_LOW) {
        if (trend == TREND_FALLING) {
            try_upgrade_level(ALARM_LEVEL_B, ALARM_TYPE_TEMP_LOW);
            return;
        }
    }
    if (temp_x10 >= TEMP_THRESHOLD_B_HIGH &&
        temp_x10 < TEMP_THRESHOLD_C_HIGH) {
        if (trend == TREND_RISING) {
            try_upgrade_level(ALARM_LEVEL_B, ALARM_TYPE_TEMP_HIGH);
            return;
        }
    }

    /* 落回 A 级范围 */
    try_downgrade_level();
}

/*==============================================================================
 * 血氧等级评估
 *============================================================================*/

static void evaluate_spo2_level(float spo2)
{
    int16_t spo2_x10 = (int16_t)(spo2 * 10.0f);
    TrendDirection_t trend = Trend_GetSpO2Direction(&s_spo2Window);

    /* 血氧只防掉不防高，所以只判 FALLING */

    if (spo2_x10 <= SPO2_THRESHOLD_DANGER && trend == TREND_FALLING) {
        try_upgrade_level(ALARM_LEVEL_D, ALARM_TYPE_SPO2_LOW);
        return;
    }
    if (spo2_x10 <= SPO2_THRESHOLD_LOW && trend == TREND_FALLING) {
        try_upgrade_level(ALARM_LEVEL_C, ALARM_TYPE_SPO2_LOW);
        return;
    }
    if (spo2_x10 <= SPO2_THRESHOLD_NORMAL && trend == TREND_FALLING) {
        /* 90~95 且在掉 → 先给 B 级盯着 */
        try_upgrade_level(ALARM_LEVEL_B, ALARM_TYPE_SPO2_LOW);
        return;
    }

    try_downgrade_level();
}

/*==============================================================================
 * 升级
 *============================================================================*/

static void try_upgrade_level(AlarmLevel_t target, AlarmType_t type)
{
    /* 已经在同级或更高 → 什么都不做 */
    if (target <= s_level) {
        return;
    }

    /* ★ 设计关键点：禁止跳级，再急也只能一级一级爬
     * 理由：跳级会让中间等级的采样加密/震动提醒被跳过，
     * 用户从"没感觉"直接跳到"全屏告警"，体验上很突兀 */
    if (target > (AlarmLevel_t)(s_level + 1)) {
        target = (AlarmLevel_t)(s_level + 1);
    }

    /* ★ 设计关键点：连续 N 次确认才升级，压掉误报 */
    uint8_t *confirm_counter = NULL;
    switch (target) {
        case ALARM_LEVEL_B: confirm_counter = &s_levelB_confirm; break;
        case ALARM_LEVEL_C: confirm_counter = &s_levelC_confirm; break;
        case ALARM_LEVEL_D: confirm_counter = &s_levelD_confirm; break;
        default: return;
    }

    (*confirm_counter)++;
    if (*confirm_counter < ALARM_CONFIRM_COUNT) {
        return;   /* 再攒一次 */
    }

    s_level     = target;
    s_alarmType = type;

    /* 升级了就把所有计数器清掉，避免残留影响下一轮 */
    s_levelB_confirm = 0;
    s_levelC_confirm = 0;
    s_levelD_confirm = 0;

    /* 存备份域，Standby 不丢 */
    HalRtc_BackupWrite(RTC_BKP_ALARM_LEVEL, (uint32_t)s_level);

    /* ★ 设计关键点：动态采样 —— 越可疑看得越勤
     * A:10min → B:2min → C/D:30s，用功耗换响应速度 */
    switch (s_level) {
        case ALARM_LEVEL_B:
            SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_ENCRYPT);
            break;
        case ALARM_LEVEL_C:
            SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_FAST);
            break;
        case ALARM_LEVEL_D:
            SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_FAST);
            break;
        default:
            break;
    }
}

/*==============================================================================
 * 降级
 *
 * 比升级麻烦：升级只看"够不够坏"，降级得看"是不是真好了"，
 * 而且不同告警类型的"好"标准不一样，只能按类型分开判。
 *============================================================================*/

/* 降级确认计数（放文件级，之前写块里 static 把自己坑了） */
static uint8_t s_downgrade_confirm = 0;

/**
 * @brief 当前告警对应的指标，是否已恢复到目标等级的安全区
 * @param target 想降到哪一级（= 当前等级 - 1）
 * @return true 可以降
 */
static bool can_downgrade_to(AlarmLevel_t target)
{
    int16_t latest;
    TrendDirection_t trend;

    switch (s_alarmType) {
        case ALARM_TYPE_TEMP_LOW:
            /* 低温告警 → 要看到回升 */
            latest = Window_GetLatest(&s_tempWindow);
            trend  = Trend_GetTempDirection(&s_tempWindow);
            switch (target) {
                case ALARM_LEVEL_C:
                    return (latest > TEMP_THRESHOLD_D_LOW && trend != TREND_FALLING);
                case ALARM_LEVEL_B:
                    return (latest > TEMP_THRESHOLD_C_LOW && trend != TREND_FALLING);
                case ALARM_LEVEL_A:
                    /* 回 A 级要求最严：不仅要回到 A 区间，还得站住不动 */
                    return (latest > TEMP_THRESHOLD_A_LOW && trend == TREND_STABLE);
                default: return false;
            }

        case ALARM_TYPE_TEMP_HIGH:
            /* 高温告警 → 要看到回落 */
            latest = Window_GetLatest(&s_tempWindow);
            trend  = Trend_GetTempDirection(&s_tempWindow);
            switch (target) {
                case ALARM_LEVEL_C:
                    return (latest < TEMP_THRESHOLD_D_HIGH && trend != TREND_RISING);
                case ALARM_LEVEL_B:
                    return (latest < TEMP_THRESHOLD_C_HIGH && trend != TREND_RISING);
                case ALARM_LEVEL_A:
                    return (latest < TEMP_THRESHOLD_A_HIGH && trend == TREND_STABLE);
                default: return false;
            }

        case ALARM_TYPE_SPO2_LOW:
            latest = Window_GetLatest(&s_spo2Window);
            trend  = Trend_GetSpO2Direction(&s_spo2Window);
            switch (target) {
                case ALARM_LEVEL_C:
                    return (latest > SPO2_THRESHOLD_DANGER && trend != TREND_FALLING);
                case ALARM_LEVEL_B:
                    return (latest > SPO2_THRESHOLD_LOW && trend != TREND_FALLING);
                case ALARM_LEVEL_A:
                    return (latest > SPO2_THRESHOLD_NORMAL && trend == TREND_STABLE);
                default: return false;
            }

        default:
            /* 类型未知就不敢降，保持现状比乱降安全 */
            return false;
    }
}

static void try_downgrade_level(void)
{
    if (ALARM_LEVEL_A == s_level) {
        s_levelB_confirm = 0;
        s_downgrade_confirm = 0;
        return;   /* 到底了 */
    }

    AlarmLevel_t target = (AlarmLevel_t)(s_level - 1);

    if (can_downgrade_to(target)) {
        s_downgrade_confirm++;

        /* 降级也用连2次确认 —— 恢复过程同样是抖的 */
        if (s_downgrade_confirm >= ALARM_CONFIRM_COUNT) {
            s_downgrade_confirm = 0;
            s_level = target;
            HalRtc_BackupWrite(RTC_BKP_ALARM_LEVEL, (uint32_t)s_level);

            if (ALARM_LEVEL_A == s_level) {
                s_alarmType = ALARM_TYPE_NONE;
                SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_NORMAL);
            } else if (ALARM_LEVEL_B == s_level) {
                SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_ENCRYPT);
            } else if (ALARM_LEVEL_C == s_level) {
                SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_FAST);
            }
        }
    } else {
        /* 条件不满足就把计数清零，必须连续满足才算 */
        s_downgrade_confirm = 0;
    }
}

/*==============================================================================
 * 告警动作
 *============================================================================*/

void AlarmMgr_ProcessAlarmAction(void)
{
    uint32_t now = HalRtc_GetElapsedSeconds();

    switch (s_level) {
        case ALARM_LEVEL_A:
            /* 静默采样，什么都不做 */
            break;

        case ALARM_LEVEL_B:
            /* 采样频率在升级时已经调过了，这里不用重复做 */
            break;

        case ALARM_LEVEL_C:
            /* 副核自己震一下提醒，还不值得叫醒主核 */
            HalPower_VibratePulseShort();  /* 100ms 短震 */
            HalSystem_DelayMs(200);
            break;

        case ALARM_LEVEL_D:
            /* ★ 设计关键点：30 秒冷却 —— 防止主核被反复上电
             * 主核上电要 500ms + 一整套显示流程，短时间连叫几次很伤电 */
            if ((now - s_lastDAlarmTime) < MAINCORE_ALARM_COOLDOWN_S) {
                return;
            }
            s_lastDAlarmTime = now;

            /* 1. 先给主核供电 */
            HalPower_MainCoreOn();

            /* 2. 报警帧发过去，主核上电后会来主动拉状态 */
            PayloadAlarm_t alarm = {
                .alarm_level = (uint8_t)ALARM_LEVEL_D,
                .alarm_type  = (uint8_t)s_alarmType,
                .value       = (uint16_t)(s_lastTemp * 10.0f)
            };

            CommFrame_t frame;
            if (ERR_OK == Protocol_PackFrame(CMD_TRIGGER_ALARM,
                                              (uint8_t *)&alarm,
                                              sizeof(alarm),
                                              &frame)) {
                HalI2c1_SlaveSend((uint8_t *)&frame,
                                  PROTOCOL_MIN_FRAME_LEN + frame.length);
            }

            /* 3. 长震，这个用户能直接感觉到 */
            HalPower_VibratePulseLong();
            break;

        default:
            break;
    }
}

/*==============================================================================
 * 环境温度补偿（管线第1层）
 *
 * 皮肤温度 ≠ 核心温度，而环境温度是最大的混淆变量。
 * 这里只是粗修正，不是医学级体核温度估算，别写文档里吹。
 *
 * ★ 设计关键点：用热梯度做混淆变量修正
 *   高温(>30) + 皮环温差小(<2) → 散热受阻 → 体核可能更高 → +1.0
 *   低温(<10) + 皮环温差大(>10) → 散热快 → 体核高于皮温 → +0.5
 *   常温 10~30 → 不动
 *   ≤0 或 >60 → 传感器还没就绪，别瞎补
 *
 * 局限（面试被问到就老实说）：
 *   真正的体核估算要热平衡模型 + 个体标定，我们做不到。
 *   这一层的价值是"识别到了这个混淆变量并做了处理"，不是"消除了它"。
 *============================================================================*/
static float apply_env_compensation(float skin_temp, float amb_temp)
{
    if (amb_temp <= 0.0f || amb_temp > 60.0f) {
        return skin_temp;   /* 无效值，不补 */
    }

    float gradient = skin_temp - amb_temp;

    /* 闷热环境，散热不畅 */
    if (amb_temp > 30.0f && gradient < 2.0f) {
        return skin_temp + 1.0f;
    }

    /* 寒冷环境，散热太快 */
    if (amb_temp < 10.0f && gradient > 10.0f) {
        return skin_temp + 0.5f;
    }

    /* 常温，梯度正常 */
    return skin_temp;
}

/*==============================================================================
 * 查询接口
 *============================================================================*/

AlarmLevel_t AlarmMgr_GetCurrentLevel(void)
{
    return s_level;
}

AlarmType_t AlarmMgr_GetCurrentAlarmType(void)
{
    return s_alarmType;
}

void AlarmMgr_GetStatus(AlarmMgr_Status_t *status)
{
    if (NULL == status) return;

    status->current_level  = s_level;
    status->current_alarm_type = s_alarmType;
    status->last_temp      = s_lastTemp;
    status->last_spo2      = s_lastSpO2;
    status->temp_slope     = Window_CalcSlope(&s_tempWindow);
    status->last_d_alarm_time = s_lastDAlarmTime;
    status->maincore_on    = HalPower_IsMainCoreOn();
}

void AlarmMgr_GetTempHistory(uint8_t *count, int16_t *data)
{
    if (NULL == count || NULL == data) return;

    *count = Window_GetCount(&s_tempWindow);
    if (*count > TEMP_WINDOW_LEN) *count = TEMP_WINDOW_LEN;

    /* 约定：data[0] 最新，data[count-1] 最旧 */
    for (uint8_t i = 0; i < *count; i++) {
        data[i] = Window_GetAt(&s_tempWindow, i);
    }
}

void AlarmMgr_ResetToLevelA(void)
{
    s_level            = ALARM_LEVEL_A;
    s_alarmType        = ALARM_TYPE_NONE;
    s_levelB_confirm   = 0;
    s_levelC_confirm   = 0;
    s_levelD_confirm   = 0;
    s_lastDAlarmTime   = 0;   /* 冷却计时也清掉 */

    HalRtc_BackupWrite(RTC_BKP_ALARM_LEVEL, (uint32_t)ALARM_LEVEL_A);
    SampleSched_SetTempInterval(SAMPLE_INTERVAL_TEMP_NORMAL);

    /* 主核那边答应断电了，这边把电源和震动都收掉 */
    HalPower_VibrateSet(false);
    HalPower_MainCoreOff();
}
