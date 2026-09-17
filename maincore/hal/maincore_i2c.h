/* maincore_i2c.h — I2C 主机接口（对面是副核从机 0x30）
 *
 * 分了三层，用哪层拿哪层：
 *   Transfer / SendOnly  — 裸字节
 *   SendFrame            — 收发一整帧 CommFrame_t
 *   ReadSubStatus / RequestSleep — 主核真正要办的只有这两件事
 */

#ifndef MAINCORE_I2C_H
#define MAINCORE_I2C_H

#include "global_types.h"
#include "protocol.h"   /* 下面接口直接收发 CommFrame_t，必须带上 */
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 通信参数
 *
 * 超时 100ms × 重试 3 次，是配着 500ms 总预算算出来的，别随手加大。
 *============================================================================*/

/** @brief 副核 I2C 从机 7 位地址 */
#define MAINCORE_I2C_SLAVE_ADDR  0x30U

/** @brief 通信超时 (ms) */
#define MAINCORE_I2C_TIMEOUT_MS  100U

/** @brief 最大重试次数 */
#define MAINCORE_I2C_MAX_RETRIES 3U

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化 I2C1 为主机模式
 * @note    PB6(SCL), PB7(SDA), 100KHz 标准模式
 * @return  ERR_OK 成功; ERR_HARDWARE 外设故障
 */
ErrorCode_t MainCore_I2cInit(void);

/**
 * @brief   双向事务：先发一串，再收一串
 * @param   tx       发送缓冲区
 * @param   tx_len   发送长度
 * @param   rx       接收缓冲区（NULL = 只发）
 * @param   rx_len   期望接收长度（出参：实际长度）
 * @return  ERR_OK / ERR_TIMEOUT
 * @note    流程: START→[addr+W]→数据→ReSTART→[addr+R]→数据→STOP
 *          超时和重试都在内部处理掉了
 */
ErrorCode_t MainCore_I2cTransfer(const uint8_t *tx, uint8_t tx_len,
                                  uint8_t *rx, uint8_t *rx_len);

/**
 * @brief   只发不收（CMD_SLEEP 这类单向命令）
 * @param   tx     发送缓冲区
 * @param   tx_len 发送长度
 * @return  ERR_OK / ERR_TIMEOUT
 */
ErrorCode_t MainCore_I2cSendOnly(const uint8_t *tx, uint8_t tx_len);

/**
 * @brief   帧级收发：序列化 → 传输 → 反序列化 + 校验
 * @param   tx_frame 发送帧（已打包）
 * @param   rx_frame 接收帧（出参）
 * @return  ERR_OK / ERR_TIMEOUT / ERR_CHECKSUM
 */
ErrorCode_t MainCore_I2cSendFrame(const CommFrame_t *tx_frame,
                                   CommFrame_t *rx_frame);

/**
 * @brief   上电先干这个：读副核状态
 * @param   out 状态结构体（出参）
 * @return  ERR_OK / 通信失败码
 * @note    GET_STATUS 出去，RESP_STATUS 回来
 */
ErrorCode_t MainCore_I2cReadSubStatus(PayloadStatus_t *out);

/**
 * @brief   请副核断电
 * @return  ERR_OK / 通信失败码
 * @note    发 CMD_SLEEP，副核收到就拉掉主核的电
 */
ErrorCode_t MainCore_I2cRequestSleep(void);

#ifdef __cplusplus
}
#endif

#endif /* MAINCORE_I2C_H */
