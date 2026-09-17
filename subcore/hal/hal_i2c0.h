/* hal_i2c0.h — I2C0 主机（传感器总线）
 *
 * PF6/PF7，400K。挂的是 MLX90614 体温、MAX30102 血氧、
 * SHT30 环境、MPU6050 计步，副核这边只当主机。
 *
 * 三种读写模式是按传感器分的，别看着像就乱用：
 *   寄存器式（MPU6050/MAX30102/MLX90614）→ MasterReadRegister
 *   命令式（SHT30）→ MasterSendCommand + MasterReadDirect
 */

#ifndef HAL_I2C0_H
#define HAL_I2C0_H

#include "global_types.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * I2C0 物理配置宏
 *============================================================================*/

/** @brief I2C0 时钟预分频器 (PSC+1 倍频) */
#define HAL_I2C0_PSC      3U

/** @brief SCL 低电平周期 (400KHz 快速模式) */
#define HAL_I2C0_SCLL     0x11U

/** @brief SCL 高电平周期 */
#define HAL_I2C0_SCLH     0x11U

/** @brief 数据建立时间 */
#define HAL_I2C0_SCLDELY  0U

/** @brief 数据保持时间 */
#define HAL_I2C0_SDADELY  0U

/* 主机这边滤波全关（走线短），从机 I2C1 那边是开的 —— 两边策略
 * 不一样是有意的，不是漏配 */
/** @brief 数字噪声滤波器长度 */
#define HAL_I2C0_DNF      FILTER_DISABLE

/** @brief 标志位轮询超时（ms） */
#define HAL_I2C0_TIMEOUT_MS  50U

/*==============================================================================
 * API
 *============================================================================*/

/* ★ 设计关键点：按传感器脾气分三种传输模式 */
/**
 * @brief   初始化 I2C0 主机
 * @note    400KHz 快速模式；PF6(SCL)/PF7(SDA)，换板子改这儿
 *          末尾读 I2CEN 位自检
 * @return  ERR_OK 成功; ERR_HARDWARE 外设没使能上
 */
ErrorCode_t HalI2c0_MasterInit(void);

/**
 * @brief   反初始化 I2C0
 * @note    进 Standby 前调，省电
 * @return  ERR_OK 成功; ERR_HARDWARE 没关掉
 */
ErrorCode_t HalI2c0_DeInit(void);

/**
 * @brief   标准两阶段读（MPU6050, MAX30102, MLX90614）
 * @param   addr_8bit 8 位设备写地址（MPU6050 AD0=LOW → 0xD0）
 * @param   reg       器件内部寄存器地址
 * @param   buf       收数据的缓冲区
 * @param   len       读几个字节
 * @return  ERR_OK 成功; ERR_TIMEOUT / ERR_BUSY / ERR_NULL_POINTER
 *
 * @note    时序: START→[addr+W]→[reg]→ReSTART→[addr+R]→[data]→STOP
 */
ErrorCode_t HalI2c0_MasterReadRegister(uint8_t addr_8bit,
                                       uint8_t reg,
                                       uint8_t *buf,
                                       uint16_t len);

/**
 * @brief   标准写（所有寄存器式传感器通用）
 * @param   addr_8bit 8 位设备写地址
 * @param   reg       器件内部寄存器地址
 * @param   buf       要写的数据
 * @param   len       写几个字节
 * @return  ERR_OK 成功; ERR_TIMEOUT / ERR_BUSY / ERR_NULL_POINTER
 *
 * @note    时序: START→[addr+W]→[reg]→[buf[0]…buf[len-1]]→STOP
 */
ErrorCode_t HalI2c0_MasterWriteRegister(uint8_t addr_8bit,
                                        uint8_t reg,
                                        const uint8_t *buf,
                                        uint16_t len);

/**
 * @brief   直接读（SHT30 这类命令式传感器，没有寄存器地址）
 * @param   addr_8bit 8 位设备读地址（SHT30 ADDR=LOW → 0x89 = 0x88+1）
 * @param   buf       收数据的缓冲区
 * @param   len       读几个字节
 * @return  ERR_OK 成功; ERR_TIMEOUT / ERR_NULL_POINTER
 *
 * @note    时序: START→[addr+R]→[data]→STOP
 *           调它之前得先 SendCommand 把测量下下去
 */
ErrorCode_t HalI2c0_MasterReadDirect(uint8_t addr_8bit,
                                     uint8_t *buf,
                                     uint16_t len);

/**
 * @brief   发命令（SHT30 这类）
 * @param   addr_8bit 8 位设备写地址
 * @param   cmd_msb   命令高字节
 * @param   cmd_lsb   命令低字节
 * @return  ERR_OK 成功; ERR_TIMEOUT 失败
 *
 * @note    时序: START→[addr+W]→[cmd_msb]→[cmd_lsb]→STOP
 *           固定两个字节 —— SHT30 的命令都是 16 位的，凑合够用
 */
ErrorCode_t HalI2c0_MasterSendCommand(uint8_t addr_8bit,
                                      uint8_t cmd_msb,
                                      uint8_t cmd_lsb);

/* ★ 设计关键点：超时 + 总线复位，卡死能爬起来 */
/**
 * @brief   总线复位
 * @note    9 个 SCL 周期 + STOP，把卡死的总线放出来。
 *          连续几次通信失败就先调它
 */
void HalI2c0_BusReset(void);

/**
 * @brief   8 位设备地址转 7 位（各家的传感器手册给法不统一，干脆统一在入口转）
 * @param   addr_8bit 8位地址（如 0xD0）
 * @return  7位地址（如 0x68）
 */
static inline uint32_t HalI2c0_Addr8To7(uint8_t addr_8bit)
{
    return (uint32_t)(addr_8bit >> 1);
}

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C0_H */
