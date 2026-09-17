/* maincore_alarm.c — 主核 8 状态机（一次性执行）
 *
 * 上电 → 读副核状态 → 显示 → 震动/等按键 → 发 CMD_SLEEP → 等断电。
 * 没有 RTOS，没有常驻任务，主核的业务就这一个文件。
 *
 * 有件事别被下面的注释骗了：每个状态写的是最多待 3 秒（STATE_TIMEOUT_MS），
 * 但主循环顶上那个 500ms 兜底先到，所以实际上谁都活不过 500ms。
 * 两条线现在是打架的，先按 500ms 走。
 */

#include "maincore_alarm.h"
#include "maincore_system.h"
#include "maincore_i2c.h"
#include "maincore_oled.h"
#include "maincore_vibrate.h"
#include "stm32f10x.h"   /* __WFI 在 core_cm3.h 里，经由这个带进来 */
#include <string.h>      /* memset */

/*==============================================================================
 * 状态机上下文
 *============================================================================*/

static MainCoreState_t s_state = MC_STATE_INIT;
static PayloadStatus_t s_subStatus;   /* 副核状态快照 */
static PayloadAlarm_t   s_alarmInfo;  /* 告警信息 —— 没人填过它，欠的账 */
static bool             s_keyPressed = false;

/* 超时控制 */
static uint32_t s_stateEntryTime = 0;  /* 进入当前状态的时刻 */

#define STATE_TIMEOUT_MS  3000U  /* 单状态 3 秒 —— 现在够不着，500ms 兜底先到 */

/*==============================================================================
 * 内部函数
 *============================================================================*/

/* ★ 设计关键点：迁移全走 transition_to */
static void transition_to(MainCoreState_t new_state)
{
    s_state = new_state;
    s_stateEntryTime = MainCore_GetUptimeMs();
}

static bool state_timed_out(void)
{
    return (MainCore_GetUptimeMs() - s_stateEntryTime) > STATE_TIMEOUT_MS;
}

/*==============================================================================
 * 初始化
 *============================================================================*/

/* 真正的硬件初始化都在 main 里做完了，这里只管把状态机摆到起点 */
void MainCore_AlarmInit(void)
{
    s_keyPressed = false;
    memset(&s_subStatus, 0, sizeof(s_subStatus));
    memset(&s_alarmInfo, 0, sizeof(s_alarmInfo));

    transition_to(MC_STATE_READY);
}

/*==============================================================================
 * 主状态机处理
 *============================================================================*/

bool MainCore_AlarmProcess(void)
{
    /* 全局超时保护：总运行时间 > 500ms 强制退出 */
    /* ★ 设计关键点：500ms 兜底→SLEEPING */
    if (MainCore_GetUptimeMs() > MAINCORE_MAX_RUN_MS) {
        transition_to(MC_STATE_SLEEPING);
    }

    switch (s_state) {

        /* ---- INIT: 硬件初始化 ---- */
        case MC_STATE_INIT:
            /* 就走这一趟，下一轮去 READY */
            MainCore_AlarmInit();
            break;

        /* ---- READY: 准备查询副核 ---- */
        case MC_STATE_READY:
            transition_to(MC_STATE_QUERY);
            break;

        /* ---- QUERY: 查询副核状态 ---- */
        case MC_STATE_QUERY: {
            ErrorCode_t err = MainCore_I2cReadSubStatus(&s_subStatus);
            if (ERR_OK == err) {
                /* 根据告警等级决定后续状态 */
                if (s_subStatus.alarm_level >= ALARM_LEVEL_D) {
                    /* D 级告警 → 显示告警界面 */
                    transition_to(MC_STATE_ALARM);
                } else {
                    /* 正常或低等级 → 显示正常界面 */
                    transition_to(MC_STATE_NORMAL);
                }
            } else {
                /* 通信失败：原地重试，下一轮循环还进 QUERY，不往外抛 */
                /* ★ 设计关键点：读不到降级显示，别耗光 500ms */
                if (state_timed_out()) {
                    transition_to(MC_STATE_NORMAL);  /* 降级显示 */
                }
            }
            break;
        }

        /* ---- NORMAL: 显示正常界面 ---- */
        case MC_STATE_NORMAL:
            MainCore_OledSetPage(OLED_PAGE_NORMAL);
            /* 体温血氧是写死的：PayloadStatus_t 里根本没这俩字段，
             * 想要真值得走 CMD_GET_TEMP_HISTORY，回头补 */
            MainCore_OledShowNormal(
                /* temp */      360,  /* 占位: 36.0°C */
                /* spo2 */      98,   /* 占位: 98% */
                /* steps */     s_subStatus.step_count,
                /* battery */  s_subStatus.battery_mv,
                /* level */    (AlarmLevel_t)s_subStatus.alarm_level
            );

            /* 显示 3 秒后自动 SLEEP */
            /* 又是 3 秒：真跑起来 500ms 就被兜底拽走了，这个判断到不了 */
            if (state_timed_out()) {
                transition_to(MC_STATE_SLEEPING);
            }
            break;

        /* ---- ALARM: 全屏告警 ---- */
        case MC_STATE_ALARM: {
            /* 1. 显示告警界面 */
            /* 类型和等级从副核状态里取；数值只有 s_alarmInfo 有，
             * 可它从来没被填过 —— 所以告警页那个数字现在是 0 */
            MainCore_OledSetPage(OLED_PAGE_ALARM);
            MainCore_OledShowAlarm(
                (AlarmType_t)s_subStatus.alarm_type,
                (AlarmLevel_t)s_subStatus.alarm_level,
                s_alarmInfo.value
            );

            /* 2. 震动 */
            /* 这一下阻塞 1 秒多，比整个 500ms 预算还长（见 vibrate 那边） */
            MainCore_VibratePulse(VIBRATE_PATTERN);

            /* 3. 等待用户按键确认 */
            if (s_keyPressed) {
                s_keyPressed = false;
                MainCore_VibrateStop();
                transition_to(MC_STATE_CONFIRMED);
            }

            /* 4. 超时未确认 → 自动进入 SLEEP */
            if (state_timed_out()) {
                MainCore_VibrateStop();
                transition_to(MC_STATE_SLEEPING);
            }
            break;
        }

        /* ---- CONFIRMED: 用户已确认 ---- */
        case MC_STATE_CONFIRMED:
            MainCore_VibrateStop();
            MainCore_OledShowSleeping();
            MainCore_DelayMs(500);  /* 让确认画面停一下，这 500ms 也算在预算里 */
            transition_to(MC_STATE_SLEEPING);
            break;

        /* ---- SLEEPING: 请求断电 ---- */
        case MC_STATE_SLEEPING: {
            MainCore_OledShowSleeping();

            /* 发送 CMD_SLEEP */
            ErrorCode_t err = MainCore_I2cRequestSleep();
            if (ERR_OK == err) {
                transition_to(MC_STATE_OFF);
            }
            /* 无论成功与否，直接进入 OFF 状态 */
            /* 那个 if 是多余的，下面无条件又进一次 OFF。留着不碍事，回头清 */
            transition_to(MC_STATE_OFF);
            break;
        }

        /* ---- OFF: 即将断电 ---- */
        case MC_STATE_OFF:
            MainCore_OledSleep();
            MainCore_VibrateStop();

            /* 副核将在 < 100ms 内切断主核电源，等待断电 */
            /* ★ 设计关键点：等断电就 WFI 死等，不空转 */
            while (1) {
                __WFI();  /* 等待 P-MOSFET 关断 */
            }
            return false;  /* 实际不会执行到此处 */

        default:
            /* 状态值被写花了才落到这，退回 INIT 从头来 */
            transition_to(MC_STATE_INIT);
            break;
    }

    return true;
}

/*==============================================================================
 * 查询接口
 *============================================================================*/

/* 现在没人调，调试时看卡在哪一步用 */
MainCoreState_t MainCore_GetState(void)
{
    return s_state;
}

/* 只置位不清零，谁消费谁负责清（ALARM 状态里清的） */
void MainCore_OnKeyPress(bool pressed)
{
    if (pressed) {
        s_keyPressed = true;
        /* 按键是 EXTI1 中断里进来的，那边 PR1 位掩码踩过的坑记在 stm32f10x_it.c */
    }
}
