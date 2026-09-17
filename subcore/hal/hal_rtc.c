/* hal_rtc.c — RTC 定时唤醒 + 备份域
 *
 * ★ 设计关键点：整个低功耗架构的节拍器
 *   副核 99% 的时间睡在 Standby（SRAM 全丢、只有 VBAT 域的 BKP 还活着），
 *   全靠 RTC 闹钟按时把它叫醒。所以"多久醒一次"就是"多耗多少电"，
 *   采样调度器动态改的就是这个闹钟。
 *
 * ★ 设计关键点：Standby 里只有 BKP 能存东西
 *   告警等级、采样间隔、累计步数都塞在这 5 个 32 位寄存器里，
 *   醒来先读它们还原状态。VBAT 也掉电就全清零 → 当成冷启动处理。
 *
 * 移植 GD32 库踩的坑（对着 V2.4.0 头文件一个个改过来的）：
 *   RTC 时钟源叫 IRC32K，不是 STM32 那种 LSI
 *   备份寄存器叫 RTC_BKP0~4，不是 BKP_DATA0~4
 *   中断向量叫 RTC_Alarm_IRQn(#41)，没有 RTC_IRQn 这个东西
 */

#include "hal_rtc.h"
#include "gd32l23x.h"

static volatile bool s_alarmSet    = false;

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t HalRtc_Init(void)
{
    /* 1. 备份域电源和时钟，不使能的话 BKP 根本写不进去 */
    rcu_periph_clock_enable(RCU_PMU);
    rcu_periph_clock_enable(RCU_BKP);
    pmu_backup_write_enable();

    /* 2. 内部 32K RC 起振 */
    rcu_osci_on(RCU_IRC32K);
    while (SET != rcu_flag_get(RCU_FLAG_IRC32KSTB)) {
        /* 等稳定 */
    }

    /* 3. 选 IRC32K 当 RTC 时钟源 */
    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();

    /* 4. 分频到 1Hz：(127+1)×(249+1) = 32000
     *    异步和同步两个因子是硬件固定的搭配，别随便改 */
    rtc_init_mode_enter();
    rtc_parameter_struct param = {0};
    param.factor_asyn    = 127U;
    param.factor_syn     = 249U;
    param.display_format = 0U;
    param.am_pm          = 0U;
    rtc_init(&param);
    rtc_init_mode_exit();

    /* 5. 先清标志再开中断，顺序反了会一进中断就触发 */
    rtc_flag_clear(RTC_FLAG_ALARM0);
    rtc_interrupt_enable(RTC_INT_ALARM0);
    nvic_irq_enable(RTC_Alarm_IRQn, 2U);

    return ERR_OK;
}

/*==============================================================================
 * 闹钟
 *============================================================================*/

/**
 * @brief 定一个 seconds 秒之后的闹钟
 * @note  RTC 闹钟是"匹配时刻"不是"倒计时"，所以得先把当前时间读出来，
 *        加上秒数换算成 时:分:秒 再写进去。
 *        跨天用 %24 兜住 —— 反正只要能匹配上就行，不关心是哪天。
 */
ErrorCode_t HalRtc_SetAlarm(uint32_t seconds)
{
    if (0U == seconds) return ERR_OK;

    rtc_parameter_struct now;
    rtc_current_time_get(&now);

    uint16_t total_sec = (uint16_t)(now.hour * 3600U + now.minute * 60U + now.second + seconds);
    uint8_t  h = (uint8_t)((total_sec / 3600U) % 24U);
    uint8_t  m = (uint8_t)((total_sec % 3600U) / 60U);
    uint8_t  s = (uint8_t)(total_sec % 60U);

    rtc_alarm_struct alarm = {0};
    alarm.alarm_mask   = 0x00000000U;  /* 时/分/秒全匹配 */
    alarm.alarm_hour   = h;
    alarm.alarm_minute = m;
    alarm.alarm_second = s;
    alarm.am_pm        = 0U;

    /* 写 RTC 寄存器前后都要等同步，不等的话有时候写不进去 */
    rtc_register_sync_wait();
    rtc_alarm_config(RTC_ALARM0, &alarm);
    rtc_alarm_enable(RTC_ALARM0);
    rtc_register_sync_wait();

    s_alarmSet = true;
    return ERR_OK;
}

void HalRtc_CancelAlarm(void)
{
    /* 改采样间隔时必须先取消再重设，不然老闹钟还会把人叫醒一次 */
    rtc_alarm_disable(RTC_ALARM0);
    rtc_register_sync_wait();
    s_alarmSet = false;
}

/**
 * @brief 取"今天已经过了多少秒"
 * @return 0 ~ 86399
 * @note  注意！过零点会回绕到 0。
 *        所有算时间差的地方都得考虑这个（窗口里那个 age 计算就有补偿），
 *        别直接做减法。
 */
uint32_t HalRtc_GetElapsedSeconds(void)
{
    rtc_parameter_struct now;
    rtc_current_time_get(&now);
    return (uint32_t)now.hour * 3600U + (uint32_t)now.minute * 60U + (uint32_t)now.second;
}

/*==============================================================================
 * 备份域 BKP0~BKP4
 *============================================================================*/

void HalRtc_BackupWrite(uint8_t index, uint32_t value)
{
    if (index > 4U) return;
    rtc_register_sync_wait();
    switch (index) {
        case 0:  RTC_BKP0 = value; break;
        case 1:  RTC_BKP1 = value; break;
        case 2:  RTC_BKP2 = value; break;
        case 3:  RTC_BKP3 = value; break;
        case 4:  RTC_BKP4 = value; break;
        default: break;
    }
}

uint32_t HalRtc_BackupRead(uint8_t index)
{
    if (index > 4U) return 0;
    rtc_register_sync_wait();
    switch (index) {
        case 0:  return RTC_BKP0;
        case 1:  return RTC_BKP1;
        case 2:  return RTC_BKP2;
        case 3:  return RTC_BKP3;
        case 4:  return RTC_BKP4;
        default: return 0;
    }
}

/*==============================================================================
 * 启动原因判断
 *============================================================================*/

/**
 * @brief 这次是冷启动还是被闹钟叫醒的？
 * @return true=冷启动（该全量采样），false=闹钟唤醒（接着上次状态跑）
 * @note  判据是 BKP3 里的魔数：VBAT 掉过电 → 魔数没了 → 冷启动
 */
bool HalRtc_IsColdBoot(void)
{
    uint32_t marker = HalRtc_BackupRead(RTC_BKP_RUN_MARKER);
    if (marker != RTC_RUN_MAGIC) return true;

    if (SET == rtc_flag_get(RTC_FLAG_ALARM0)) {
        rtc_flag_clear(RTC_FLAG_ALARM0);
        return false;
    }
    return true;
}

void HalRtc_MarkRunning(void)
{
    HalRtc_BackupWrite(RTC_BKP_RUN_MARKER, RTC_RUN_MAGIC);
}

bool HalRtc_IsWakeupFromAlarm(void)
{
    if (SET == rtc_flag_get(RTC_FLAG_ALARM0)) {
        rtc_flag_clear(RTC_FLAG_ALARM0);
        return true;
    }
    return false;
}

/*==============================================================================
 * 闹钟中断
 *============================================================================*/

/**
 * @brief 闹钟中断
 * @note  这里只清标志。真正的采样处理留给主循环 —— 中断里啥也别干，
 *        不然会跟 I2C 从机中断抢时间
 */
void HalRtc_IRQHandler(void)
{
    if (SET == rtc_flag_get(RTC_FLAG_ALARM0)) {
        rtc_flag_clear(RTC_FLAG_ALARM0);
        s_alarmSet = false;
    }
}
