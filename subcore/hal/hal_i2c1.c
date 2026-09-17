/* hal_i2c1.c — I2C1 从机（跟主核说话的那条线）
 *
 * 副核当从机，主核想聊就来寻址。中断里只搬字节 + STOP 时回调，
 * 业务一概不在这儿干 —— 在中断里磨蹭会跟 RTC 闹钟抢时间。
 *
 * 对照 GD32 官方库 V2.4.0 改过一轮，STM32 那套 API 名字在这儿基本都不对：
 *   i2c_address_config()               不是 i2c_slave_addressing()
 *   i2c_wakeup_from_deepsleep_enable() 不是 i2c_address_wakeup_enable()
 *   I2C1_EV_IRQn                       不是 I2C1_IRQn（后者根本不存在）
 *   标志查询要用 i2c_interrupt_flag_enum 那一套
 */

#include "hal_i2c1.h"
#include "hal_gpio.h"
#include "gd32l23x.h"
#include <string.h>

/*==============================================================================
 * 内部状态
 *============================================================================*/

static HalI2c1_RxCallback_t        s_rxCallback       = NULL;
static HalI2c1_TxRequestCallback_t s_txRequestCallback = NULL;

/* 收发缓冲按最大帧长开，收满了就丢，中断里不做长度检查 */
static uint8_t s_rxBuf[HAL_I2C1_RX_BUF_SIZE];
static uint8_t s_txBuf[HAL_I2C1_TX_BUF_SIZE];

/* 这几个全都是中断里改的，volatile 别手贱删 */
static volatile uint8_t s_rxIdx   = 0;
static volatile uint8_t s_txIdx   = 0;
static volatile uint8_t s_txLen   = 0;
static volatile bool    s_addressed = false;
static volatile bool    s_txActive  = false;

/*==============================================================================
 * 初始化 / 反初始化
 *============================================================================*/

/* ★ 设计关键点：使能地址匹配唤醒，主核一叫就醒 */
ErrorCode_t BSP_I2C1_SlaveInit(uint8_t slaveAddr)
{
    /* 1. 使能 GPIOB 和 I2C1 时钟 */
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_I2C1);

    /* 2. 配置 PB8=SCL, PB9=SDA — 复用开漏 */
    gpio_af_set(GPIOB, GPIO_AF_4, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_8 | GPIO_PIN_9);

    /* 3. 复位 I2C1 */
    i2c_deinit(I2C1);

    /* 4. 配置数字噪声滤波器（从机必须滤波防抖） */
    i2c_digital_noise_filter_config(I2C1, HAL_I2C1_DNF);
    i2c_analog_noise_filter_enable(I2C1);

    /* 5. 配置从机地址（7 位，传进来的 0x30 直接就是） */
    i2c_address_config(I2C1, (uint32_t)slaveAddr, I2C_ADDFORMAT_7BITS);

    /* 6. 使能 Deep-sleep 模式地址匹配唤醒 */
    i2c_wakeup_from_deepsleep_enable(I2C1);

    /* 7. 开启从机中断 (I2C1_EV_IRQn 用于事件中断) */
    nvic_irq_enable(I2C1_EV_IRQn, 1U);
    i2c_interrupt_enable(I2C1, I2C_INT_ADDM | I2C_INT_STPDET);

    /* 8. 使能 I2C1 */
    i2c_enable(I2C1);

    /* 9. 自检 */
    if (0U == (I2C_CTL0(I2C1) & I2C_CTL0_I2CEN)) {
        return ERR_HARDWARE;
    }

    return ERR_OK;
}

/* 先关中断再关外设，反了会漏一次中断。进 Standby 前调 */
ErrorCode_t HalI2c1_DeInit(void)
{
    i2c_interrupt_disable(I2C1, I2C_INT_ADDM | I2C_INT_STPDET);
    nvic_irq_disable(I2C1_EV_IRQn);
    i2c_disable(I2C1);

    if (0U != (I2C_CTL0(I2C1) & I2C_CTL0_I2CEN)) {
        return ERR_HARDWARE;
    }
    i2c_deinit(I2C1);
    return ERR_OK;
}

/*==============================================================================
 * 回调注册
 *============================================================================*/

/* 就是一句指针赋值。别在通信中途换回调 —— 中断里正用着 */
void HalI2c1_RegisterRxCallback(HalI2c1_RxCallback_t cb) { s_rxCallback = cb; }
void HalI2c1_RegisterTxCallback(HalI2c1_TxRequestCallback_t cb) { s_txRequestCallback = cb; }

/*==============================================================================
 * 状态查询
 *============================================================================*/

/* 是不是正被主机寻址（STOP 之后就清） */
bool HalI2c1_IsAddressed(void)  { return s_addressed; }

/* 注意：查的同时顺手把标志清了，名字上看不出来 ——
 * 连着调两次，第二次必然 false */
bool HalI2c1_IsWakeupSource(void)
{
    if (SET == i2c_interrupt_flag_get(I2C1, I2C_INT_FLAG_ADDSEND)) {
        i2c_interrupt_flag_clear(I2C1, I2C_INT_FLAG_ADDSEND);
        return true;
    }
    return false;
}

/*==============================================================================
 * 主动发送
 *============================================================================*/

/* ★ 设计关键点：不主动发，只预存，等主机来读 */
/**
 * @brief 预存要发给主核的数据
 * @note  这儿只 memcpy 进缓冲，真正上线是等主机下一次读事务。
 *        正在传的时候调会返回 ERR_BUSY，调用方自己看着办
 * @return ERR_OK 存下了; ERR_BUSY 发送中; ERR_NULL_POINTER 参数不对
 */
ErrorCode_t HalI2c1_SlaveSend(const uint8_t *data, uint8_t len)
{
    if (NULL == data || 0U == len || len > HAL_I2C1_TX_BUF_SIZE) {
        return ERR_NULL_POINTER;
    }
    if (s_txActive) return ERR_BUSY;

    memcpy(s_txBuf, data, len);
    s_txLen = len;
    s_txIdx = 0;
    return ERR_OK;
}

/*==============================================================================
 * I2C1 事件中断服务
 *
 * ★ 设计关键点：中断只搬字节，收齐（STOP）才回调
 *============================================================================*/

void HalI2c1_IRQHandler(void)
{
    /* ---- 地址匹配中断 ---- */
    if (SET == i2c_interrupt_flag_get(I2C1, I2C_INT_FLAG_ADDSEND)) {
        i2c_interrupt_flag_clear(I2C1, I2C_INT_FLAG_ADDSEND);
        s_addressed = true;
        s_rxIdx = 0;

        /* 判断主机读写方向: I2C_STAT_TR 位 (16) 读取传输方向
         * 方向直接问硬件，不用自己记上一次是读还是写 */
        if (0U != (I2C_STAT(I2C1) & I2C_STAT_TR)) {
            /* 主机要读 (TR=1: transmitter mode) → 准备发送数据 */
            s_txActive = true;
            s_txIdx = 0;

            /* 手上没数据就找上层要一次。回调里还得 memcpy 一发，
             * 中断上下文活儿有点多 —— 好在最多 32 字节 */
            if (0U == s_txLen && NULL != s_txRequestCallback) {
                const uint8_t *cbData = NULL;
                uint8_t cbLen = 0;
                s_txRequestCallback(&cbData, &cbLen);
                if (NULL != cbData && cbLen > 0 && cbLen <= HAL_I2C1_TX_BUF_SIZE) {
                    memcpy(s_txBuf, cbData, cbLen);
                    s_txLen = cbLen;
                }
            }
            /* 发送首个字节 */
            if (s_txIdx < s_txLen) {
                i2c_data_transmit(I2C1, (uint32_t)s_txBuf[s_txIdx++]);
            } else {
                i2c_data_transmit(I2C1, 0xFFU);   /* 没数据也得回一个，别晾着主机 */
            }

            /* 使能发送缓冲区空中断 */
            i2c_interrupt_enable(I2C1, I2C_INT_TI);
        } else {
            /* 主机要写 → 使能接收中断 */
            i2c_interrupt_enable(I2C1, I2C_INT_RBNE);
        }
    }

    /* ---- 接收数据寄存器非空中断 ---- */
    if (SET == i2c_interrupt_flag_get(I2C1, I2C_INT_FLAG_RBNE)) {
        uint8_t byte = (uint8_t)i2c_data_receive(I2C1);
        if (s_rxIdx < HAL_I2C1_RX_BUF_SIZE) {
            s_rxBuf[s_rxIdx++] = byte;   /* 满了就丢，不覆盖前面的 */
        }
    }

    /* ---- 发送缓冲区空中断 ---- */
    if (SET == i2c_interrupt_flag_get(I2C1, I2C_INT_FLAG_TI)) {
        if (s_txActive && s_txIdx < s_txLen) {
            i2c_data_transmit(I2C1, (uint32_t)s_txBuf[s_txIdx++]);
        } else {
            i2c_data_transmit(I2C1, 0xFFU);   /* 收尾再灌一个，然后把 TI 关了防刷屏 */
            i2c_interrupt_disable(I2C1, I2C_INT_TI);
        }
    }

    /* ---- STOP 检测中断 ---- */
    if (SET == i2c_interrupt_flag_get(I2C1, I2C_INT_FLAG_STPDET)) {
        i2c_interrupt_flag_clear(I2C1, I2C_INT_FLAG_STPDET);
        s_addressed = false;

        /* 通知上层。协议解析、回包全在回调里做，别搬进中断 */
        if (s_rxIdx > 0 && NULL != s_rxCallback) {
            s_rxCallback(s_rxBuf, s_rxIdx);
        }

        /* 禁用收发中断 */
        i2c_interrupt_disable(I2C1, I2C_INT_RBNE | I2C_INT_TI);

        s_rxIdx = 0;
        s_txActive = false;
    }
}
