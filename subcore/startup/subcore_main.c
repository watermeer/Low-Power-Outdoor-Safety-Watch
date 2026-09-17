/* subcore_main.c — 副核入口
 *
 * ★ 设计关键点：副核这辈子两件事，都在 main 里
 *   冷启动：全量初始化 → 全量采样 → 进主循环
 *   唤醒  ：最小恢复（只重开必要外设）→ 干一次活 → 接着睡
 *
 *   为什么两条路分开？因为 Standby 醒来等于重新上电，
 *   SRAM 全没了，但外设状态也没了。冷启动要配全，唤醒只要配"这次要用到的"，
 *   少配一个外设就少几十微安。
 *
 * 整个副核的常态是：睡着 → 被闹钟戳醒 → 采一次 → 判断要不要叫人 → 接着睡。
 * 所以这个文件的代码路径短得离谱，大部分时间根本不在跑。
 */

#include "global_types.h"
#include "global_config.h"
#include "protocol.h"
#include "hal_gpio.h"
#include "hal_i2c0.h"
#include "hal_i2c1.h"
#include "hal_rtc.h"
#include "hal_power.h"
#include "hal_system.h"
#include "bsp_sensors.h"
#include "sample_scheduler.h"
#include "alarm_manager.h"
#include "gd32l23x.h"   /* nvic_irq_enable 在 gd32l23x_misc.h 里 */
#include <stddef.h>

/*==============================================================================
 * 内部状态
 *============================================================================*/

static bool s_coldBoot = true;

/*==============================================================================
 * I2C1 收帧回调 —— 主核发过来的所有指令都进这里
 *============================================================================*/

static void on_i2c_rx_frame(const uint8_t *data, uint8_t len)
{
    CommFrame_t frame;

    /* 校验不过就直接回错误码，别硬着头皮往下解析 */
    if (ERR_OK != Protocol_UnpackFrame(data, len, &frame)) {
        PayloadError_t err = { .error_code = ERR_CHECKSUM };
        CommFrame_t resp;
        if (ERR_OK == Protocol_PackFrame(CMD_RESP_ERROR,
                                          (uint8_t *)&err, sizeof(err), &resp)) {
            HalI2c1_SlaveSend((uint8_t *)&resp,
                              PROTOCOL_MIN_FRAME_LEN + resp.length);
        }
        return;
    }

    switch (frame.cmd) {
        case CMD_PING: {
            /* 主核探活，不用带载荷 */
            CommFrame_t resp;
            Protocol_PackFrame(CMD_RESP_OK, NULL, 0, &resp);
            HalI2c1_SlaveSend((uint8_t *)&resp, PROTOCOL_MIN_FRAME_LEN);
            break;
        }

        case CMD_GET_STATUS: {
            /* 主核上电第一件事就是拉状态，靠这个决定显示什么 */
            AlarmMgr_Status_t status;
            AlarmMgr_GetStatus(&status);

            PayloadStatus_t payload = {
                .alarm_level = (uint8_t)status.current_level,
                .alarm_type  = (uint8_t)status.current_alarm_type,
                .battery_mv  = 3700,   /* TODO 电池 ADC 还没接，先写死 */
                .step_count  = BSP_MPU6050_GetStepCount()
            };

            CommFrame_t resp;
            Protocol_PackFrame(CMD_RESP_STATUS,
                               (uint8_t *)&payload, sizeof(payload), &resp);
            HalI2c1_SlaveSend((uint8_t *)&resp,
                              PROTOCOL_MIN_FRAME_LEN + resp.length);
            break;
        }

        case CMD_SLEEP: {
            /* 主核说"我看完了"，先回 ACK 再断电，顺序反了主核会以为通信失败 */
            CommFrame_t resp;
            Protocol_PackFrame(CMD_RESP_OK, NULL, 0, &resp);
            HalI2c1_SlaveSend((uint8_t *)&resp, PROTOCOL_MIN_FRAME_LEN);

            /* 用户已经看到了，等级从 A 重来 */
            AlarmMgr_ResetToLevelA();

            HalPower_MainCoreOff();
            break;
        }

        case CMD_GET_TEMP_HISTORY: {
            /* 主核想画体温曲线。载荷按实际条数发，别傻乎乎发满10个 */
            uint8_t count = 0;
            int16_t temp_data[TEMP_WINDOW_LEN];
            AlarmMgr_GetTempHistory(&count, temp_data);

            PayloadTempHistory_t payload;
            payload.count = count;
            for (uint8_t i = 0; i < count; i++) {
                payload.temp_data[i] = temp_data[i];
            }

            CommFrame_t resp;
            Protocol_PackFrame(CMD_RESP_TEMP_HISTORY,
                               (uint8_t *)&payload,
                               sizeof(uint8_t) + count * sizeof(int16_t),
                               &resp);
            HalI2c1_SlaveSend((uint8_t *)&resp,
                              PROTOCOL_MIN_FRAME_LEN + resp.length);
            break;
        }

        case CMD_SET_SAMPLE_INTERVAL: {
            /* 调试用：串口那边可以直接改采样间隔，省得每次重新烧 */
            if (frame.length >= 3) {
                uint8_t sensor_type = frame.payload[0];
                uint16_t interval = ((uint16_t)frame.payload[1] << 8)
                                    | frame.payload[2];
                if (SENSOR_TYPE_TEMP_BODY == sensor_type) {
                    SampleSched_SetTempInterval(interval);
                }
            }
            CommFrame_t resp;
            Protocol_PackFrame(CMD_RESP_OK, NULL, 0, &resp);
            HalI2c1_SlaveSend((uint8_t *)&resp, PROTOCOL_MIN_FRAME_LEN);
            break;
        }

        default:
            /* 未知指令不理它，回错误反而容易跟主核打架 */
            break;
    }
}

/**
 * @brief 主核要读数据时的回调
 * @note  实际数据是 HalI2c1_SlaveSend 提前塞好的，这里只在没预设时兜底
 */
static void on_i2c_tx_request(const uint8_t **data, uint8_t *len)
{
    (void)data;
    (void)len;
}

/*==============================================================================
 * 冷启动：全量初始化
 *============================================================================*/

static void cold_boot_init(void)
{
    HalSystem_ClockInit();                        /* 1. 16MHz */
    HalGpio_InitAll();                            /* 2. GPIO + LED + 调试串口 */
    HalPower_Init();                              /* 3. 默认断主核电、停震动 */
    HalRtc_Init();                                /* 4. RTC + 闹钟中断 */

    /* 注：RTC 用的是内部 IRC32K，不是 STM32 那套 LSI。
     * 之前注释一直写成 LSI，被自己误导过一次，改回来。 */
    BSP_Sensors_InitAll();                        /* 5. I2C0 + 四个传感器 */

    BSP_I2C1_SlaveInit(SUBCORE_I2C_SLAVE_ADDR);   /* 6. 双核通信从机 0x30 */
    HalI2c1_RegisterRxCallback(on_i2c_rx_frame);  /* 7. 挂回调 */
    HalI2c1_RegisterTxCallback(on_i2c_tx_request);

    SampleSched_Init();                           /* 8. 业务模块 */
    AlarmMgr_Init();

    HalRtc_MarkRunning();                         /* 9. 标记已运行，下次就不是冷启动了 */

    /* 10. 闪三下，说明初始化过了。黑屏的时候全靠这个灯判断死活 */
    for (int i = 0; i < 3; i++) {
        HalGpio_LedSet(true);
        HalSystem_DelayMs(100);
        HalGpio_LedSet(false);
        HalSystem_DelayMs(100);
    }
}

/*==============================================================================
 * Standby 唤醒：最小恢复
 *
 * 醒来时外设全是复位状态，得重开。但只开这次干活用得上的，
 * 没用到的一个都别碰 —— 每多使能一个时钟就多一份漏电。
 *============================================================================*/

static void standby_wake_init(void)
{
    HalSystem_ClockInit();
    /* 唤醒后先确保在正常频率上，Standby 前可能被降到 2MHz 了 */
    HalSystem_EnterNormalMode();

    HalGpio_InitAll();
    HalPower_Init();          /* 主核这时候应该是断的，重申一下状态 */
    HalI2c0_MasterInit();

    BSP_I2C1_SlaveInit(SUBCORE_I2C_SLAVE_ADDR);
    HalI2c1_RegisterRxCallback(on_i2c_rx_frame);
    HalI2c1_RegisterTxCallback(on_i2c_tx_request);

    /* RTC 本身没停（VBAT 域供电），但 NVIC 是复位状态，得重开中断。
     * 是 RTC_Alarm_IRQn(#41)，GD32L233 没有独立的 RTC_IRQn */
    nvic_irq_enable(RTC_Alarm_IRQn, 2U);

    /* 这两个会自己从备份域把等级/间隔捞回来，不用手动传 */
    SampleSched_Init();
    AlarmMgr_Init();
}

/*==============================================================================
 * main
 *============================================================================*/

int main(void)
{
    s_coldBoot = HalRtc_IsColdBoot();

    if (s_coldBoot) {
        cold_boot_init();

        /* 冷启动手里一条历史都没有，先全量采一遍把窗口垫上 */
        SampleSched_UpdateAllSample();
        AlarmMgr_ProcessAlarmAction();

        /* 这个 while 其实转不了几圈 —— Process 里没事干会直接进 Standby，
         * 而 Standby 是不返回的。留着是怕哪天 Standby 没进去，
         * 至少还能继续跑不至于死在这 */
        while (1) {
            SampleSched_Process(SAMPLE_EVENT_RTC);
        }
    } else {
        standby_wake_init();

        /* 判断这次是谁把我叫醒的，不同唤醒源干的活不一样 */
        SampleEvent_t event = SAMPLE_EVENT_RTC;

        if (HalI2c1_IsWakeupSource()) {
            event = SAMPLE_EVENT_I2C;     /* 主核来找我通信 */
        } else if (HalRtc_IsWakeupFromAlarm()) {
            event = SAMPLE_EVENT_RTC;     /* 到点采样 */
        } else if (BSP_MPU6050_GetMotionIntFlag()) {
            event = SAMPLE_EVENT_MOTION;  /* 用户动了，记步 */
        }

        SampleSched_Process(event);
    }

    return 0;   /* 走不到，但编译器喜欢有个 return */
}
