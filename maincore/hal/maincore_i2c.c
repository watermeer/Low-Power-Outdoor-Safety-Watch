/* maincore_i2c.c — I2C1 主机（跟副核说话的唯一一条路）
 *
 * 副核是 I2C1 从机，地址 0x30。主核一问一答：GET_STATUS → RESP_STATUS，
 * 临了再发一帧 CMD_SLEEP 让它把电掐了。
 *
 * 时序全手动写寄存器（START/ADDR/TXE/BTF/NACK/STOP 一整套），
 * 不上中断也不上 DMA —— 一次就二十来个字节，为这点量搭个状态机不划算。
 *
 * 标志位不能死等：副核没就绪的时候标志永远不来，500ms 预算一次就耗光了，
 * 所以每个等待都带超时，整帧失败再重试。
 */

#include "maincore_i2c.h"
#include "maincore_system.h"
#include "protocol.h"
#include "stm32f10x.h"
#include <string.h>

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t MainCore_I2cInit(void)
{
    /* 1. 使能 GPIOB 和 I2C1 时钟 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    /* 2. 配置 PB6(SCL), PB7(SDA) — 复用开漏 */
    /* PB6 */
    GPIOB->CRL &= ~(0x0FU << (6 * 4));
    GPIOB->CRL |= (0x0BU << (6 * 4));   /* 复用开漏输出, 50MHz */
    /* PB7 */
    GPIOB->CRL &= ~(0x0FU << (7 * 4));
    GPIOB->CRL |= (0x0BU << (7 * 4));

    /* 3. 复位 I2C1 */
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST;

    /* 4. 配置 I2C1 时钟: 100KHz 标准模式 */
    /* I2C1 挂 APB1=36MHz, 100KHz → CCR = 36MHz/(2×100KHz) = 180 */
    I2C1->CR2 = 36U;                      /* FREQ = 36MHz，这格写错的后果是时序全偏 */
    I2C1->CCR = 180U;                     /* 标准模式 100KHz */
    I2C1->TRISE = 37U;                    /* 上升时间 = 1000ns / (1/36MHz) + 1 */

    /* 5. 使能 I2C1 */
    I2C1->CR1 |= I2C_CR1_PE;

    return ERR_OK;   /* 这里没判过硬件，反正配完也没法验，先当成功 */
}

/*==============================================================================
 * 底层 I2C 传输
 *============================================================================*/

/* ★ 设计关键点：标志轮询带软超时，绝不死等 */
/* 超时基准用的是 SysTick 的 uptime，没另开定时器 */
static ErrorCode_t i2c_wait_flag(uint32_t flag, FlagStatus expect, uint32_t timeout_ms)
{
    uint32_t start = MainCore_GetUptimeMs();
    while (MainCore_GetUptimeMs() - start < timeout_ms) {
        if ((I2C1->SR1 & flag) == (expect ? flag : 0U)) {
            return ERR_OK;
        }
    }
    return ERR_TIMEOUT;
}

/* ★ 设计关键点：失败重试 3 次，先补 STOP */
ErrorCode_t MainCore_I2cTransfer(const uint8_t *tx, uint8_t tx_len,
                                  uint8_t *rx, uint8_t *rx_len)
{
    uint8_t retries = 0;
    ErrorCode_t err;

    if (NULL == tx || 0U == tx_len) return ERR_NULL_POINTER;

    while (retries < MAINCORE_I2C_MAX_RETRIES) {
        /* ---- 阶段一：发送 ---- */
        I2C1->CR1 |= I2C_CR1_START;  /* 发送 START */
        err = i2c_wait_flag(I2C_SR1_SB, SET, MAINCORE_I2C_TIMEOUT_MS);
        /* 这条没补 STOP，跟下面 goto 那条不一样。SB 都没来，总线本来就不干净 */
        if (ERR_OK != err) { retries++; continue; }

        /* 发送从机地址 (7位) + W */
        I2C1->DR = (MAINCORE_I2C_SLAVE_ADDR << 1) | 0x00;
        err = i2c_wait_flag(I2C_SR1_ADDR, SET, MAINCORE_I2C_TIMEOUT_MS);
        if (ERR_OK != err) { I2C1->CR1 |= I2C_CR1_STOP; retries++; continue; }
        (void)I2C1->SR2;  /* 读 SR2 清除 ADDR 标志 */

        /* 发送数据 */
        for (uint8_t i = 0; i < tx_len; i++) {
            err = i2c_wait_flag(I2C_SR1_TXE, SET, MAINCORE_I2C_TIMEOUT_MS);
            if (ERR_OK != err) { I2C1->CR1 |= I2C_CR1_STOP; retries++; goto next_retry; }
            I2C1->DR = tx[i];
        }

        /* 等待 BTF (字节传输完成) */
        err = i2c_wait_flag(I2C_SR1_BTF, SET, MAINCORE_I2C_TIMEOUT_MS);
        if (ERR_OK != err) { I2C1->CR1 |= I2C_CR1_STOP; retries++; continue; }

        /* ---- 阶段二：接收 ---- */
        if (NULL != rx && NULL != rx_len && *rx_len > 0) {
            I2C1->CR1 |= I2C_CR1_START;  /* ReSTART */
            err = i2c_wait_flag(I2C_SR1_SB, SET, MAINCORE_I2C_TIMEOUT_MS);
            if (ERR_OK != err) { retries++; continue; }

            /* 发送从机地址 + R */
            I2C1->DR = (MAINCORE_I2C_SLAVE_ADDR << 1) | 0x01;
            err = i2c_wait_flag(I2C_SR1_ADDR, SET, MAINCORE_I2C_TIMEOUT_MS);
            if (ERR_OK != err) { I2C1->CR1 |= I2C_CR1_STOP; retries++; continue; }
            (void)I2C1->SR2;

            /* 最后一个字节前设 NACK + STOP */
            /* ★ 设计关键点：读最后字节 NACK 再 STOP */
            for (uint8_t i = 0; i < *rx_len; i++) {
                if (i == *rx_len - 1U) {
                    I2C1->CR1 &= ~I2C_CR1_ACK;   /* 最后一个字节不 ACK */
                    I2C1->CR1 |= I2C_CR1_STOP;   /* 自动 STOP */
                }
                err = i2c_wait_flag(I2C_SR1_RXNE, SET, MAINCORE_I2C_TIMEOUT_MS);
                if (ERR_OK != err) { retries++; goto next_retry; }
                rx[i] = (uint8_t)I2C1->DR;
            }

            /* 恢复 ACK */
            I2C1->CR1 |= I2C_CR1_ACK;
        } else {
            /* 无接收需求 → 直接 STOP */
            I2C1->CR1 |= I2C_CR1_STOP;
        }

        return ERR_OK;

next_retry:
        I2C1->CR1 |= I2C_CR1_STOP;
        MainCore_DelayMs(5);   /* 歇 5ms 再重来，连着怼从机它也缓不过来 */
        retries++;
    }

    return ERR_TIMEOUT;   /* 重试打光就认输，降不降级交给上层决定 */
}

ErrorCode_t MainCore_I2cSendOnly(const uint8_t *tx, uint8_t tx_len)
{
    /* 只发不收：rx 传 NULL，Transfer 里会自己走 STOP 那条分支 */
    return MainCore_I2cTransfer(tx, tx_len, NULL, NULL);
}

/*==============================================================================
 * 协议帧级通信
 *============================================================================*/

ErrorCode_t MainCore_I2cSendFrame(const CommFrame_t *tx_frame,
                                   CommFrame_t *rx_frame)
{
    uint8_t raw_tx[PROTOCOL_MAX_FRAME_LEN];
    uint8_t raw_rx[PROTOCOL_MAX_FRAME_LEN];
    uint8_t tx_len, rx_len;

    if (NULL == tx_frame || NULL == rx_frame) {
        return ERR_NULL_POINTER;
    }

    /* 两块 21 字节的栈缓冲，按最大帧长开死，不动态分配 */
    /* 序列化发送帧 */
    tx_len = 0;
    raw_tx[tx_len++] = (uint8_t)(tx_frame->header >> 8);
    raw_tx[tx_len++] = (uint8_t)(tx_frame->header & 0xFF);
    raw_tx[tx_len++] = tx_frame->cmd;
    raw_tx[tx_len++] = tx_frame->length;
    if (tx_frame->length > 0) {
        memcpy(&raw_tx[tx_len], tx_frame->payload, tx_frame->length);
        tx_len += tx_frame->length;
    }
    raw_tx[tx_len++] = tx_frame->checksum;

    /* 期望接收长度：最小帧 + 可能的最大载荷 */
    rx_len = PROTOCOL_MAX_FRAME_LEN;

    /* I2C 传输 */
    ErrorCode_t err = MainCore_I2cTransfer(raw_tx, tx_len, raw_rx, &rx_len);
    if (ERR_OK != err) return err;

    /* 反序列化接收帧 */
    return Protocol_UnpackFrame(raw_rx, rx_len, rx_frame);
}

/*==============================================================================
 * 上电读副核状态
 *============================================================================*/

ErrorCode_t MainCore_I2cReadSubStatus(PayloadStatus_t *out)
{
    CommFrame_t tx, rx;

    if (NULL == out) return ERR_NULL_POINTER;

    /* 打包 CMD_GET_STATUS（无载荷） */
    ErrorCode_t err = Protocol_PackFrame(CMD_GET_STATUS, NULL, 0, &tx);
    if (ERR_OK != err) return err;

    /* 发送并接收 */
    err = MainCore_I2cSendFrame(&tx, &rx);
    if (ERR_OK != err) return err;

    /* 校验响应 */
    if (CMD_RESP_STATUS != rx.cmd) {
        return ERR_UNKNOWN_CMD;
    }

    /* 解析载荷 */
    /* 按结构体大小直接拷，没比 rx.length —— payload 有 16 字节装得下。
     * 将来协议加字段，第一件事是回来加上长度判断 */
    memcpy(out, rx.payload, sizeof(PayloadStatus_t));
    return ERR_OK;
}

/*==============================================================================
 * 请求断电
 *
 * 发完这帧就没主核什么事了 —— 副核收到就拉 P-MOSFET，主核等着失电。
 *============================================================================*/

ErrorCode_t MainCore_I2cRequestSleep(void)
{
    CommFrame_t tx;

    ErrorCode_t err = Protocol_PackFrame(CMD_SLEEP, NULL, 0, &tx);
    if (ERR_OK != err) return err;

    /* CMD_SLEEP 是单向命令，不需要接收响应 */
    /* 这 5 个字节是手拼的，没走 I2cSendFrame（那个还等着收响应）。
     * 跟 SendFrame 里的序列化重了一小段，先这么放着 */
    uint8_t raw[PROTOCOL_MIN_FRAME_LEN];
    raw[0] = (uint8_t)(tx.header >> 8);
    raw[1] = (uint8_t)(tx.header & 0xFF);
    raw[2] = tx.cmd;
    raw[3] = tx.length;
    raw[4] = tx.checksum;

    return MainCore_I2cSendOnly(raw, PROTOCOL_MIN_FRAME_LEN);
}
