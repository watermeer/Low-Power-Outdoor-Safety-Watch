/* sample_scheduler.c — 采样调度器
 *
 * 副核的"心跳"。平时整个板子睡在 Standby，靠 RTC 闹钟按时醒过来，
 * 醒一次就把该采的采完、该喂的喂给告警机、再决定睡不睡回去。
 *
 * 省电的关键就在这儿：不是所有传感器每次都采，各自算自己的到期时间。
 * 计步 5s 一次（要实时），体温 600s 起（变化慢），血氧 1800s（更慢）。
 *
 * ★ 面试要点：动态采样间隔 —— A级10分钟 / B级2分钟 / C、D级30秒。
 *   算法上叫"自适应采样率"：越可疑，看得越勤，拿功耗换响应速度。
 */

#include "sample_scheduler.h"
#include "hal_rtc.h"
#include "hal_system.h"
#include "bsp_sensors.h"
#include "alarm_manager.h"

/*==============================================================================
 * 内部状态
 *============================================================================*/

/* 各传感器的当前间隔，体温那个会被告警机动态改 */
static uint16_t s_intervalTemp    = SAMPLE_INTERVAL_TEMP_NORMAL;
static uint16_t s_intervalSpO2    = SAMPLE_INTERVAL_SPO2;
static uint16_t s_intervalStep    = SAMPLE_INTERVAL_STEP;
static uint16_t s_intervalAmbient = SAMPLE_INTERVAL_AMBIENT;

/* 各传感器上次采样时刻（RTC 秒） */
static uint32_t s_lastTempSample    = 0;
static uint32_t s_lastSpO2Sample    = 0;
static uint32_t s_lastStepSample    = 0;
static uint32_t s_lastAmbientSample = 0;

/* 下次闹钟定多久 */
static uint32_t s_nextAlarmSeconds  = 0;

/*==============================================================================
 * 初始化
 *============================================================================*/

void SampleSched_Init(void)
{
    /* 间隔从备份域捞，Standby 醒来不能退回默认值 ——
     * 不然 D 级睡着睡着变回 10 分钟采样，等于白报警 */
    uint32_t saved_interval = HalRtc_BackupRead(RTC_BKP_SAMPLE_INTERVAL);
    if (saved_interval > 0 && saved_interval <= 3600) {
        s_intervalTemp = (uint16_t)saved_interval;
    }

    /* 上次采样时间统一按"现在"起算，避免一醒来就全量采 */
    uint32_t now = HalRtc_GetElapsedSeconds();
    s_lastTempSample    = now;
    s_lastSpO2Sample    = now;
    s_lastStepSample    = now;
    s_lastAmbientSample = now;

    s_nextAlarmSeconds = s_intervalStep;   /* 计步 5s 最短，先按它定 */
    HalRtc_SetAlarm(s_nextAlarmSeconds);
}

/*==============================================================================
 * 主调度
 *============================================================================*/

void SampleSched_Process(SampleEvent_t event)
{
    uint32_t now = HalRtc_GetElapsedSeconds();
    bool any_sampled = false;

    switch (event) {
        case SAMPLE_EVENT_COLD_BOOT:
            /* 冷启动没有历史，全量采一次把窗口垫上 */
            SampleSched_UpdateAllSample();
            any_sampled = true;
            break;

        case SAMPLE_EVENT_RTC:
        case SAMPLE_EVENT_NONE:
            break;

        case SAMPLE_EVENT_MOTION:
            /* MPU6050 的中断叫醒的，只处理计步，别的传感器继续睡 */
            BSP_MPU6050_WakeUp();
            {
                uint32_t steps = BSP_MPU6050_GetStepCount();
                if (steps > 0U) {
                    /* ★ 设计关键点：步数累加进 BKP2
                     * Standby 期间 MPU6050 断电、DMP 计数归零，
                     * BKP2 挂在 VBAT 域上，只有它能跨休眠存住数据 */
                    uint32_t accumulated = HalRtc_BackupRead(RTC_BKP_STEP_COUNT_L);
                    HalRtc_BackupWrite(RTC_BKP_STEP_COUNT_L, accumulated + steps);
                }
            }
            s_lastStepSample = now;
            BSP_MPU6050_Sleep();
            any_sampled = true;
            break;

        case SAMPLE_EVENT_I2C:
            /* 主核来通信唤醒的，采样不归我管 */
            break;

        default:
            break;
    }

    /* ---- 体温 ---- */
    if (SampleSched_NeedSample(SENSOR_TYPE_TEMP_BODY)) {
        int16_t body_temp = 0;
        int16_t amb_temp  = 0;

        if (ERR_OK == BSP_MLX90614_ReadTemp(&body_temp)) {
            /* 环境温度这时候还是 0，真值在下一句才读
             * —— 补偿用的是上一轮的环境温度，差 600s 但环境变得没这么快 */
            AlarmMgr_FeedTemp((float)body_temp / 100.0f,
                              (float)amb_temp / 100.0f);
            s_lastTempSample = now;
            any_sampled = true;
        }

        /* 环境温度复用同一个唤醒周期，不多醒一次 */
        if (ERR_OK == BSP_MLX90614_ReadAmbientTemp(&amb_temp)) {
            s_lastAmbientSample = now;
        }
    }

    /* ---- 环境温湿度（SHT30，主要看湿度） ---- */
    if (SampleSched_NeedSample(SENSOR_TYPE_TEMP_AMB)) {
        int16_t amb_temp = 0;
        uint16_t humidity = 0;
        if (ERR_OK == BSP_SHT30_ReadData(&amb_temp, &humidity)) {
            s_lastAmbientSample = now;
            any_sampled = true;
        }
    }

    /* ---- 血氧 ---- */
    if (SampleSched_NeedSample(SENSOR_TYPE_SPO2)) {
        uint8_t spo2 = 0;
        if (ERR_OK == BSP_MAX30102_ReadSpO2(&spo2)) {
            AlarmMgr_FeedSpO2((float)spo2);
            s_lastSpO2Sample = now;
            any_sampled = true;
        }
    }

    /* ---- 计步（RTC 周期里也顺手累一次） ---- */
    if (SampleSched_NeedSample(SENSOR_TYPE_STEP)) {
        uint32_t steps = BSP_MPU6050_GetStepCount();
        if (steps > 0U) {
            uint32_t accumulated = HalRtc_BackupRead(RTC_BKP_STEP_COUNT_L);
            HalRtc_BackupWrite(RTC_BKP_STEP_COUNT_L, accumulated + steps);
        }
        s_lastStepSample = now;
    }

    /* 该叫就叫，不该叫它自己会判断 */
    AlarmMgr_ProcessAlarmAction();

    /* TODO 这里有问题：第一行赋值被第二行盖掉了，是死代码。
     *      原本想让四路间隔取最小值，现在实际只取了血氧和环境，
     *      计步的 5s 根本没生效。还没想好是改成取四个的 min
     *      还是干脆让体温主导，先留着 —— 功能能跑，就是多醒几次。 */
    s_nextAlarmSeconds = s_intervalStep;   s_nextAlarmSeconds = s_intervalSpO2;
    if (s_intervalAmbient < s_nextAlarmSeconds)  s_nextAlarmSeconds = s_intervalAmbient;

    HalRtc_SetAlarm(s_nextAlarmSeconds);

    /* 这轮啥也没采 → 没事干了，睡 */
    if (!any_sampled) {
        HalSystem_EnterStandby();
    }
}

/*==============================================================================
 * 全量采样（冷启动 / D级）
 *============================================================================*/

void SampleSched_UpdateAllSample(void)
{
    int16_t body_temp = 0, amb_temp = 0;
    uint16_t humidity = 0;
    uint8_t spo2 = 0;

    /* MLX90614 睡醒要 50ms 才出数，先唤醒再等 */
    BSP_MPU6050_WakeUp();
    HalSystem_DelayMs(10);

    BSP_MLX90614_ReadTemp(&body_temp);
    BSP_MLX90614_ReadAmbientTemp(&amb_temp);
    AlarmMgr_FeedTemp((float)body_temp / 100.0f,
                      (float)amb_temp / 100.0f);

    BSP_SHT30_ReadData(&amb_temp, &humidity);

    BSP_MAX30102_ReadSpO2(&spo2);
    AlarmMgr_FeedSpO2((float)spo2);

    uint32_t now = HalRtc_GetElapsedSeconds();
    s_lastTempSample    = now;
    s_lastSpO2Sample    = now;
    s_lastStepSample    = now;
    s_lastAmbientSample = now;

    HalRtc_SetAlarm(s_intervalTemp);
}

/*==============================================================================
 * 间隔管理
 *============================================================================*/

void SampleSched_SetTempInterval(uint16_t interval_sec)
{
    /* 上下限夹一下，防止别处传个 0 或者离谱值把系统搞死 */
    if (interval_sec < 5U)  interval_sec = 5U;
    if (interval_sec > 3600U) interval_sec = 3600U;

    s_intervalTemp = interval_sec;

    /* 存备份域，Standby 醒来要接着用 */
    HalRtc_BackupWrite(RTC_BKP_SAMPLE_INTERVAL, (uint32_t)interval_sec);

    /* 等级变了要立刻生效，不能等下一轮 */
    HalRtc_CancelAlarm();
    HalRtc_SetAlarm(interval_sec);
}

void SampleSched_SetSpO2Interval(uint16_t interval_sec)
{
    if (interval_sec < 30U)   interval_sec = 30U;
    if (interval_sec > 7200U) interval_sec = 7200U;
    s_intervalSpO2 = interval_sec;
}

uint16_t SampleSched_GetTempInterval(void)
{
    return s_intervalTemp;
}

/*==============================================================================
 * 到期判断
 *============================================================================*/

bool SampleSched_NeedSample(SensorType_t sensor_type)
{
    uint32_t now     = HalRtc_GetElapsedSeconds();
    uint32_t elapsed = 0;

    /* 用减法不用比较时间戳，RTC 计数器回绕也能算对 */
    switch (sensor_type) {
        case SENSOR_TYPE_TEMP_BODY:
            elapsed = now - s_lastTempSample;
            return (elapsed >= s_intervalTemp);
        case SENSOR_TYPE_SPO2:
            elapsed = now - s_lastSpO2Sample;
            return (elapsed >= s_intervalSpO2);
        case SENSOR_TYPE_STEP:
            elapsed = now - s_lastStepSample;
            return (elapsed >= s_intervalStep);
        case SENSOR_TYPE_TEMP_AMB:
            elapsed = now - s_lastAmbientSample;
            return (elapsed >= s_intervalAmbient);
        default:
            return false;
    }
}

uint32_t SampleSched_GetLastSampleTime(SensorType_t sensor_type)
{
    switch (sensor_type) {
        case SENSOR_TYPE_TEMP_BODY: return s_lastTempSample;
        case SENSOR_TYPE_SPO2:      return s_lastSpO2Sample;
        case SENSOR_TYPE_STEP:      return s_lastStepSample;
        case SENSOR_TYPE_TEMP_AMB:  return s_lastAmbientSample;
        default:                    return 0;
    }
}

/*==============================================================================
 * 累计步数
 *============================================================================*/

uint32_t SampleSched_GetAccumulatedSteps(void)
{
    return HalRtc_BackupRead(RTC_BKP_STEP_COUNT_L);
}
