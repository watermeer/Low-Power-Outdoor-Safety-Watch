/* protocol.h — 双核通信帧格式 + CMD 指令集
 *
 * 纯软件，零硬件依赖。主副核都 -I shared 编这一份，是两边唯一的合同：
 * 副核不知道主核是 STM32，主核也不知道副核是 GD32，互相只认帧。
 *
 * 改这里等于改协议：test/test_protocol.c 里有一份 PC 端单元测试，
 * 动完帧格式或者 CMD 记得同步改，不然测试全绿也说明不了什么。
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include "global_types.h"
#include "global_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 帧格式常量
 *============================================================================*/

/* 帧头，找帧同步用的 */
#define PROTOCOL_HEADER      0xAA55

/* 版本号，先占个位，代码里还没人用 */
#define PROTOCOL_VERSION     0x01

/* 载荷上限 16 字节 */
#define PROTOCOL_MAX_PAYLOAD 16

/* 最小帧 5 字节：头2 + CMD1 + 长度1 + 校验1 */
#define PROTOCOL_MIN_FRAME_LEN  5

/* ★ 设计关键点：最大帧 21B，副核缓冲 32B 有余量
 *   想再加长载荷，先回去看 HAL_I2C1_TX_BUF_SIZE */
#define PROTOCOL_MAX_FRAME_LEN  (PROTOCOL_MIN_FRAME_LEN + PROTOCOL_MAX_PAYLOAD)

/*==============================================================================
 * CMD 指令集（主副核共享）
 *============================================================================*/

/* ★ 设计关键点：CMD 值是线序，定完不许插队 */
typedef enum {
    /* ---- 系统管理 (0x00 ~ 0x0F) ---- */
    /* 0x00 本来想说保留，后来还是被 RESP_OK 占了 */
    CMD_RESP_OK          = 0x00,  /* 通用成功应答 */
    CMD_PING             = 0x01,  /* 心跳，双向，无载荷 */
    CMD_GET_STATUS       = 0x02,  /* 主→副，拉状态 */
    CMD_RESP_STATUS      = 0x03,  /* 副→主，状态回包 */

    /* ---- 告警 (0x10 ~ 0x1F) ---- */
    CMD_TRIGGER_ALARM    = 0x10,  /* 副→主，D 级叫醒主核 */
    CMD_SLEEP            = 0x11,  /* 主→副，主核说看完了，可以断电 */

    /* ---- 数据查询 (0x20 ~ 0x2F) ---- */
    CMD_GET_TEMP_HISTORY = 0x20,  /* 主→副，要体温曲线 */
    CMD_RESP_TEMP_HISTORY = 0x21, /* 副→主，历史回包 */

    /* ---- 调试/配置 (0x30 ~ 0x3F) ---- */
    CMD_SET_SAMPLE_INTERVAL = 0x30, /* 主→副，改采样间隔 */

    /* ---- 错误（保留）---- */
    CMD_RESP_ERROR       = 0xFF,  /* 错误应答，双向 */
} ProtocolCmd_t;

/*==============================================================================
 * 帧结构体
 *============================================================================*/

/* 帧格式: Header(2B) | Cmd(1B) | Length(1B) | Payload(NB) | Checksum(1B)
 * 校验: 从帧头高字节一路异或到载荷末尾
 *
 * 这只是内存里的样子，别整块 memcpy 上 I2C —— 收发都走 Pack/Unpack，
 * 那边是手工逐字节拼的，不吃对齐和字节序的亏 */
typedef struct {
    uint16_t header;                  /* 帧头，固定 0xAA55 */
    uint8_t  cmd;                     /* 指令码 */
    uint8_t  length;                  /* 载荷长度 0~16 */
    uint8_t  payload[PROTOCOL_MAX_PAYLOAD]; /* 载荷区 */
    uint8_t  checksum;                /* 异或校验和 */
} CommFrame_t;

/*==============================================================================
 * 各指令的载荷结构体（按需取用）
 *============================================================================*/

/* CMD_RESP_STATUS (0x03) 的载荷 */
typedef struct {
    uint8_t  alarm_level;  /* 告警等级 AlarmLevel_t */
    uint8_t  alarm_type;   /* 告警类型 AlarmType_t */
    uint16_t battery_mv;   /* 电池 mV（TODO 电池 ADC 还没接，先写死） */
    uint32_t step_count;   /* 累计步数 */
} PayloadStatus_t;

/* CMD_TRIGGER_ALARM (0x10) 的载荷 */
typedef struct {
    uint8_t  alarm_level;  /* 告警等级 */
    uint8_t  alarm_type;   /* 告警类型 */
    uint16_t value;        /* 触发值：温度/血氧 ×10 */
} PayloadAlarm_t;

/* ★ 设计关键点：长度=TEMP_WINDOW_LEN
 *
 * CMD_RESP_TEMP_HISTORY (0x21) 的载荷：注意 count 后面编译器会垫 1 个填充字节，
 * 不管用 sizeof 还是手算 1 + count×2，把 (uint8_t *)&payload 直接扔上总线，
 * 都是从那个填充字节开始错位 */
typedef struct {
    uint8_t  count;        /* 实际几条 */
    int16_t  temp_data[TEMP_WINDOW_LEN]; /* 体温历史 (°C × 10) */
} PayloadTempHistory_t;

/* CMD_SET_SAMPLE_INTERVAL (0x30) 的载荷
 * sensor_type 指认改谁的间隔，一条命令管所有传感器，不用每个都开条 CMD */
typedef struct {
    uint8_t  sensor_type;  /* 传感器类型 SensorType_t */
    uint16_t interval_sec; /* 采样间隔（秒） */
} PayloadSetInterval_t;

/* CMD_RESP_ERROR (0xFF) 的载荷 */
typedef struct {
    uint8_t  error_code;   /* 错误码 ErrorCode_t */
} PayloadError_t;

/*==============================================================================
 * 打包 / 解包 API
 *============================================================================*/

/* ★ 设计关键点：跨板通信就压这一个字节：异或
 *   帧才 21 字节，I2C 每个字节还有 ACK 兜着，够用；CRC 软件查表要占 Flash，
 *   GD32L233 又没有硬件 CRC 单元 */
/**
 * @brief   算帧校验和（逐字节异或）
 * @param   frame 已填好除 checksum 外所有字段的帧
 * @return  8 位校验值
 */
uint8_t Protocol_CalcChecksum(const CommFrame_t *frame);

/**
 * @brief   打包一帧
 * @param   cmd     指令码
 * @param   payload 载荷指针（NULL = 没载荷）
 * @param   length  载荷长度（0 = 没载荷）
 * @param   out     输出帧
 * @return  ERR_OK / ERR_NULL_POINTER / ERR_LENGTH
 * @note    帧头、长度、校验和都自动填好，调用方只管 cmd 和载荷
 */
ErrorCode_t Protocol_PackFrame(ProtocolCmd_t cmd,
                               const uint8_t *payload,
                               uint8_t length,
                               CommFrame_t *out);

/**
 * @brief   解一帧出来，帧头、长度、校验和都顺手验掉
 * @param   raw     收到的原始字节
 * @param   raw_len 收到几个字节
 * @param   out     解析结果
 * @return  ERR_OK / ERR_CHECKSUM / ERR_LENGTH / ERR_NULL_POINTER
 * @note    副核是在 I2C 收帧回调里直接喂给它的，别在这里面加慢活
 */
ErrorCode_t Protocol_UnpackFrame(const uint8_t *raw,
                                 uint8_t raw_len,
                                 CommFrame_t *out);

/**
 * @brief   指令码转名字，串口打日志用
 * @param   cmd 指令码
 * @return  名字字符串，不认识就回 "CMD_UNKNOWN"
 */
const char *Protocol_CmdName(ProtocolCmd_t cmd);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
