/* protocol.c — 校验和 / 打包 / 解包
 *
 * 就三个纯字节函数，一行硬件代码没有，PC 上也能直接编（test/ 就是这么测的）。
 *
 * 有件事记着：解包是从 I2C 中断里被调的（STOP 中断 → 回调 → 进来这里），
 * 所以这几个函数里别加打印、延时、大循环。
 */

#include "protocol.h"
#include <string.h>

/*==============================================================================
 * 校验和
 *============================================================================*/

/* ★ 设计关键点：防御性钳位，传 NULL 不崩，不越界读
 *   （异或 vs CRC 那段取舍写在 protocol.h 了，不重复） */
/**
 * @brief   算校验和（逐字节异或）
 * @param   frame 待计算的帧（checksum 字段自己不参与）
 * @return  8 位校验值
 * @note    范围: header[1] ^ header[0] ^ cmd ^ length ^ payload[0..length-1]
 */
uint8_t Protocol_CalcChecksum(const CommFrame_t *frame)
{
    if (frame == NULL) {
        return 0;   /* 传空也不崩，回 0 让它校验不过去重试 */
    }

    uint8_t sum = 0;
    uint8_t i;

    /* 帧头两字节，高字节先来 */
    sum = (uint8_t)((frame->header >> 8) & 0xFF);
    sum ^= (uint8_t)(frame->header & 0xFF);
    /* 指令码 + 长度 */
    sum ^= frame->cmd;
    sum ^= frame->length;

    /* 载荷按 length 算，但不能越界读 —— 参数写花了也不许崩 */
    uint8_t len = frame->length;
    if (len > PROTOCOL_MAX_PAYLOAD) {
        len = PROTOCOL_MAX_PAYLOAD;
    }
    for (i = 0; i < len; i++) {
        sum ^= frame->payload[i];
    }

    return sum;
}

/*==============================================================================
 * 打包
 *
 * ★ 设计关键点：手工逐字节拼，结构体不上线，两核对齐不同
 *============================================================================*/

ErrorCode_t Protocol_PackFrame(ProtocolCmd_t cmd,
                               const uint8_t *payload,
                               uint8_t length,
                               CommFrame_t *out)
{
    /* 参数先过一遍 */
    if (out == NULL) {
        return ERR_NULL_POINTER;
    }
    if (length > PROTOCOL_MAX_PAYLOAD) {
        return ERR_LENGTH;
    }
    if (length > 0 && payload == NULL) {
        return ERR_NULL_POINTER;
    }

    /* 整帧先清 0，载荷没填满的那截也留着 0 */
    memset(out, 0, sizeof(CommFrame_t));

    /* 帧头 + 指令码 + 长度 */
    out->header = PROTOCOL_HEADER;
    out->cmd = (uint8_t)cmd;
    out->length = length;

    /* 声明多少就拷多少，多一个字节都不带 */
    if (length > 0 && payload != NULL) {
        memcpy(out->payload, payload, length);
    }

    /* 校验和最后算，得等前面都填好 */
    out->checksum = Protocol_CalcChecksum(out);

    return ERR_OK;
}

/*==============================================================================
 * 解包
 *
 * ★ 设计关键点：快速失败，指针→最小长度→帧头→校验和
 *============================================================================*/

ErrorCode_t Protocol_UnpackFrame(const uint8_t *raw,
                                 uint8_t raw_len,
                                 CommFrame_t *out)
{
    /* 参数先过一遍 */
    if (raw == NULL || out == NULL) {
        return ERR_NULL_POINTER;
    }

    /* 连最小帧都不到，不可能是帧 */
    if (raw_len < PROTOCOL_MIN_FRAME_LEN) {
        return ERR_LENGTH;
    }

    memset(out, 0, sizeof(CommFrame_t));

    /* 手工拼，别图省事 (uint16_t *)raw 强转 —— 未对齐访问在 M23 上会崩 */
    uint16_t header = ((uint16_t)raw[0] << 8) | raw[1];
    if (header != PROTOCOL_HEADER) {
        return ERR_CHECKSUM;  /* 帧头不对也回校验错，反正调用方都是重试 */
    }
    out->header = header;

    /* 指令码 + 载荷长度 */
    out->cmd = raw[2];

    out->length = raw[3];
    if (out->length > PROTOCOL_MAX_PAYLOAD) {
        return ERR_LENGTH;
    }

    /* 收到的比声明的少 → I2C 传到一半断了就长这样 */
    if (raw_len < (PROTOCOL_MIN_FRAME_LEN + out->length)) {
        return ERR_LENGTH;
    }

    if (out->length > 0) {
        memcpy(out->payload, &raw[4], out->length);
    }

    /* 报文里的校验和 vs 本地重算的，对不上就是帧坏了 */
    uint8_t received_checksum = raw[4 + out->length];
    out->checksum = received_checksum;

    uint8_t calc_checksum = Protocol_CalcChecksum(out);
    if (calc_checksum != received_checksum) {
        return ERR_CHECKSUM;
    }

    return ERR_OK;
}

/*==============================================================================
 * 调试辅助
 *============================================================================*/

/* 串口抓包全靠它，加了新 CMD 记得回来补一条 */
const char *Protocol_CmdName(ProtocolCmd_t cmd)
{
    switch (cmd) {
        case CMD_RESP_OK:            return "RESP_OK";
        case CMD_PING:               return "PING";
        case CMD_GET_STATUS:         return "GET_STATUS";
        case CMD_RESP_STATUS:        return "RESP_STATUS";
        case CMD_TRIGGER_ALARM:      return "TRIGGER_ALARM";
        case CMD_SLEEP:              return "SLEEP";
        case CMD_GET_TEMP_HISTORY:   return "GET_TEMP_HISTORY";
        case CMD_RESP_TEMP_HISTORY:  return "RESP_TEMP_HISTORY";
        case CMD_SET_SAMPLE_INTERVAL:return "SET_SAMPLE_INTERVAL";
        case CMD_RESP_ERROR:         return "RESP_ERROR";
        default:                     return "CMD_UNKNOWN";
    }
}
