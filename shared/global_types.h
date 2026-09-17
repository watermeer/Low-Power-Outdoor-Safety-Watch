/* global_types.h — 全局类型：错误码 / 告警等级 / 告警类型
 *
 * 全项目公共类型，两个核都直接 include 这一份。
 * 一条规矩：只放纯类型，硬件相关的一个字都别进这个文件。
 *
 * 这里的枚举值是当协议字节发出去的（主核按 uint8_t 收），
 * 所以定下来就算协议的一部分，改一个值两边固件都得重烧。
 */

#ifndef GLOBAL_TYPES_H
#define GLOBAL_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 错误码
 *============================================================================*/

/* ★ 设计关键点：错误码统一走 ErrorCode_t
 *   主副核共用一套码，谁调谁校验返回值，省得两边各定一套对不上 */
typedef enum {
    ERR_OK           = 0x00,  /* 成功 */
    ERR_CHECKSUM     = 0x01,  /* 校验不过；帧头不对也借这个回 */
    ERR_UNKNOWN_CMD  = 0x02,  /* 不认识的指令 */
    ERR_LENGTH       = 0x03,  /* 长度不合法 */
    ERR_BUSY         = 0x04,  /* 忙着 */
    ERR_TIMEOUT      = 0x05,  /* 超时 */
    ERR_NOT_INIT     = 0x06,  /* 没初始化就调 */
    ERR_NULL_POINTER = 0x07,  /* 传了空指针 */
    ERR_HARDWARE     = 0x08,  /* 硬件异常 */
    ERR_NOT_FOUND    = 0x09,  /* 器件没应答 / ID 对不上 */
} ErrorCode_t;

/*==============================================================================
 * 告警等级 / 类型（核心业务状态）
 *============================================================================*/

/* ★ 设计关键点：枚举值直接当协议字节发，别插队
 *
 * 就四级，A→B→C→D 一级一级爬，跳级在状态机那边就被拦了 */
typedef enum {
    ALARM_LEVEL_A = 0,  /* 静默采样，正常 */
    ALARM_LEVEL_B = 1,  /* 趋势不对，加密采样盯着 */
    ALARM_LEVEL_C = 2,  /* 确认异常，副核自己震一下 */
    ALARM_LEVEL_D = 3,  /* 危险阈值，叫醒主核 */
} AlarmLevel_t;

/* 哪个指标出的问题 */
typedef enum {
    ALARM_TYPE_NONE      = 0x00,  /* 没告警 */
    ALARM_TYPE_TEMP_LOW  = 0x01,  /* 体温过低（失温） */
    ALARM_TYPE_TEMP_HIGH = 0x02,  /* 体温过高（中暑） */
    ALARM_TYPE_SPO2_LOW  = 0x03,  /* 血氧过低（高反） */
} AlarmType_t;

/*==============================================================================
 * 传感器编号
 *============================================================================*/

/* 编号两头共用：采样调度认它，协议里靠它指认目标传感器 */
typedef enum {
    SENSOR_TYPE_TEMP_BODY = 0x01,  /* MLX90614 红外体温 */
    SENSOR_TYPE_TEMP_AMB  = 0x02,  /* SHT30 环境温湿度 */
    SENSOR_TYPE_SPO2      = 0x03,  /* MAX30102 血氧 */
    SENSOR_TYPE_STEP      = 0x04,  /* MPU6050 计步 */
} SensorType_t;

/*==============================================================================
 * 副核运行模式
 *============================================================================*/

/* ★ 设计关键点：三档功耗换响应，深睡留 RTC+I2C
 *   主核叫门（I2C 地址匹配）才起来 */
typedef enum {
    SUB_MODE_NORMAL    = 0,  /* 全速 16MHz */
    SUB_MODE_LOW_POWER = 1,  /* 降频 2MHz */
    SUB_MODE_STANDBY   = 2,  /* Standby，就剩 RTC + I2C 唤醒 */
} SubCoreMode_t;

#ifdef __cplusplus
}
#endif

#endif /* GLOBAL_TYPES_H */
