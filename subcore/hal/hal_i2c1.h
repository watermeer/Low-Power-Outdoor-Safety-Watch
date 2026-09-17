/* hal_i2c1.h — I2C1 从机（双核通信）
 *
 * PB8/PB9，从机地址 0x30。
 * 最要紧的是地址匹配能把副核从 Standby 里拽出来 —— 主核一寻址就醒，
 * 不用等 RTC 那一拍，用户按了键马上有反应。
 */

#ifndef HAL_I2C1_H
#define HAL_I2C1_H

#include "global_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * I2C1 从机配置宏
 *============================================================================*/

/* 这个宏没人引用 —— 实际地址在 global_config 的 SUBCORE_I2C_SLAVE_ADDR，
 * 也是 0x30。两处别改岔了 */
/** @brief I2C1 从机 7 位地址（双核通信专用） */
#define HAL_I2C1_SLAVE_ADDR  0x30U

/** @brief I2C1 数字滤波器长度（从机侧建议开启以抑制毛刺） */
#define HAL_I2C1_DNF         FILTER_LENGTH_2

/* 32 是跟着协议最大帧长定的，改协议记得同步这个数 */
/** @brief I2C1 接收缓冲区大小（与 CommFrame_t 最大帧长一致） */
#define HAL_I2C1_RX_BUF_SIZE  32U
#define HAL_I2C1_TX_BUF_SIZE  32U

/* 没用上 —— 从机全程中断驱动，压根没有轮询的地方，留着凑数 */
/** @brief I2C1 标志轮询超时（ms） */
#define HAL_I2C1_TIMEOUT_MS   100U

/*==============================================================================
 * 回调函数类型
 *============================================================================*/

/* ★ 设计关键点：HAL 不碰协议，解包归 main */
/**
 * @brief   收完一帧的回调（在 STOP 中断里被调，别写耗时的活）
 * @param   data  收到的数据
 * @param   len   收了多少字节
 */
typedef void (*HalI2c1_RxCallback_t)(const uint8_t *data, uint8_t len);

/**
 * @brief   主机要读数据时来问这个回调
 * @param   data  出参：指向要发数据的指针，回调里填
 * @param   len   出参：发多少字节
 * @note    只在手上没预存数据时才被叫到 —— 正常路径是 SlaveSend 先存好
 */
typedef void (*HalI2c1_TxRequestCallback_t)(const uint8_t **data, uint8_t *len);

/*==============================================================================
 * API
 *============================================================================*/

/* ★ 设计关键点：Standby 下也能地址匹配唤醒 */
/**
 * @brief   初始化 I2C1 从机
 * @note    配地址 + 开唤醒 + 开中断。引脚 PB8(SCL)/PB9(SDA)，换板子改这儿
 *          末尾查 I2CEN 自检
 * @return  ERR_OK 成功; ERR_HARDWARE 没使能上
 */
ErrorCode_t BSP_I2C1_SlaveInit(uint8_t slaveAddr);

/**
 * @brief   反初始化 I2C1
 * @note    先关中断再关外设，进 Standby 前调
 * @return  ERR_OK 成功; ERR_HARDWARE 没关掉
 */
ErrorCode_t HalI2c1_DeInit(void);

/**
 * @brief   注册接收回调
 * @param   cb 函数指针，传 NULL 就是取消注册
 */
void HalI2c1_RegisterRxCallback(HalI2c1_RxCallback_t cb);

/**
 * @brief   注册发送请求回调
 * @param   cb 函数指针，传 NULL 就是取消注册
 */
void HalI2c1_RegisterTxCallback(HalI2c1_TxRequestCallback_t cb);

/**
 * @brief   现在是不是正被主机寻址
 * @return  true=通信中, false=空闲
 */
bool HalI2c1_IsAddressed(void);

/**
 * @brief   这次唤醒是不是地址匹配弄醒的
 * @return  true=主核在找我们
 * @note    退出 Standby 后调。有副作用：顺手把地址匹配标志清了，
 *          所以一次唤醒只该问一次
 */
bool HalI2c1_IsWakeupSource(void);

/**
 * @brief   预存要发给主核的数据
 * @param   data 数据缓冲区
 * @param   len  长度（≤ HAL_I2C1_TX_BUF_SIZE）
 * @return  ERR_OK 成功; ERR_BUSY 上一笔还没发完
 * @note    不主动发，只是存着 —— 等主机的下一次读事务把它拉走
 */
ErrorCode_t HalI2c1_SlaveSend(const uint8_t *data, uint8_t len);

/**
 * @brief   I2C1 中断服务
 * @note    由中断向量转发：void I2C1_EV_IRQHandler(void) { HalI2c1_IRQHandler(); }
 *          是 I2C1_EV，不叫 I2C1_IRQHandler（那是 ST 的写法）
 */
void HalI2c1_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C1_H */
