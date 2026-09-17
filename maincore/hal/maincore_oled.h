/* maincore_oled.h — SSD1306 显示接口（SPI1, 128×64 单色）
 *
 * 正常 / 告警 / 关机，三个页面的入口都在这。
 * 提醒：字体引擎还是占位的，几个 Show 函数现在只把屏刷黑，看不到字。
 */

#ifndef MAINCORE_OLED_H
#define MAINCORE_OLED_H

#include "global_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 显示页面枚举
 *============================================================================*/

typedef enum {
    OLED_PAGE_NORMAL = 0,  /**< 正常显示：时间+步数+温度+血氧+电量 */
    OLED_PAGE_ALARM  = 1,  /**< 告警界面：全屏告警图标+数值+提示 */
    OLED_PAGE_SLEEP  = 2,  /**< 关机提示 */
} OledPage_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化 OLED
 * @note    SPI1 + 硬件复位 + SSD1306 配置序列
 *          引脚: PA0=CS, PA1=DC, PA4=RST, PA5=SCK, PA7=MOSI
 */
void MainCore_OledInit(void);

/**
 * @brief   清屏
 */
void MainCore_OledClear(void);

/**
 * @brief   切换显示页面
 * @param   page 目标页面
 * @note    当前是空实现，页面逻辑在状态机里
 */
void MainCore_OledSetPage(OledPage_t page);

/**
 * @brief   正常页面：时间/体温/血氧/步数/电量
 * @param   temp_x10   体温 (°C × 10)
 * @param   spo2       血氧 (%)
 * @param   steps      步数
 * @param   battery_mv 电池电压 (mV)
 * @param   alarm_level 当前告警等级
 */
void MainCore_OledShowNormal(int16_t temp_x10, uint8_t spo2,
                              uint32_t steps, uint16_t battery_mv,
                              AlarmLevel_t alarm_level);

/**
 * @brief   告警页面
 * @param   alarm_type  告警类型
 * @param   alarm_level 告警等级
 * @param   value       触发值 (°C×10 或 %×10)
 * @note    计划是：大字图标 + 类型 + 数值 + "按 KEY 确认"
 */
void MainCore_OledShowAlarm(AlarmType_t alarm_type,
                             AlarmLevel_t alarm_level,
                             uint16_t value);

/**
 * @brief   关机提示画面
 */
void MainCore_OledShowSleeping(void);

/**
 * @brief   关显示（只发 0xAE，电荷泵留着没关）
 */
void MainCore_OledSleep(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINCORE_OLED_H */
