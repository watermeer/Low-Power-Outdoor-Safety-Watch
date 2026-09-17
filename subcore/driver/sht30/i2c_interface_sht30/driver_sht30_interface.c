/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_sht30_interface.c
 * @brief     driver sht30 接口源文件（基于 GD32L23x I2C 固件库）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_sht30_interface.h"
#include "gd32l23x_i2c.h"
#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                            硬件配置宏                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief SHT30 所使用的 I2C 外设实例
 * @note  通常与 MPU6050 共用同一条 I2C 总线
 */
#define SHT30_I2C              I2C1

/**
 * @brief I2C 时序预分频器（0 ~ 15）
 */
#define SHT30_I2C_PSC          3U

/**
 * @brief I2C SCL 低电平周期（400 kHz 快速模式）
 */
#define SHT30_I2C_SCLL         0x11U

/**
 * @brief I2C SCL 高电平周期（400 kHz 快速模式）
 */
#define SHT30_I2C_SCLH         0x11U

/**
 * @brief I2C 数据建立时间
 */
#define SHT30_I2C_SCLDELY      0U

/**
 * @brief I2C 数据保持时间
 */
#define SHT30_I2C_SDADELY      0U

/**
 * @brief I2C 数字噪声滤波器
 */
#define SHT30_I2C_DNF          FILTER_DISABLE

/**
 * @brief I2C 标志轮询超时（毫秒）
 */
#define SHT30_I2C_TIMEOUT_MS   50U

/* -------------------------------------------------------------------------- */
/*                            内部辅助函数                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief      将 8 位 I2C 地址转换为 7 位地址
 */
static uint32_t sht30_addr_8bit_to_7bit(uint8_t addr)
{
    return (uint32_t)(addr >> 1);
}

/**
 * @brief      带超时的 I2C 标志位轮询
 */
static uint8_t sht30_i2c_wait_flag(uint32_t flag, FlagStatus expect)
{
    uint32_t timeout = SHT30_I2C_TIMEOUT_MS * 1000U;
    while (i2c_flag_get(SHT30_I2C, flag) != expect) {
        if (0U == timeout--) { return 1; }
    }
    return 0;
}

/**
 * @brief      检测并清除 NACK
 */
static uint8_t sht30_i2c_check_nack(void)
{
    if (SET == i2c_flag_get(SHT30_I2C, I2C_FLAG_NACK)) {
        i2c_flag_clear(SHT30_I2C, I2C_FLAG_NACK);
        return 1;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/*                          接口函数实现                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief  IIC 总线初始化
 * @return 0 成功，1 失败
 * @note   如果多个传感器共用同一 I2C 总线，此函数仅需调用一次，
 *         后续传感器直接跳过 I2C 初始化即可（共享已配置的外设）。
 */
uint8_t sht30_interface_iic_init(void)
{
    i2c_deinit(SHT30_I2C);
    i2c_timing_config(SHT30_I2C, SHT30_I2C_PSC,
                      SHT30_I2C_SCLDELY, SHT30_I2C_SDADELY);
    i2c_master_clock_config(SHT30_I2C, SHT30_I2C_SCLH, SHT30_I2C_SCLL);
    i2c_digital_noise_filter_config(SHT30_I2C, SHT30_I2C_DNF);
    i2c_analog_noise_filter_disable(SHT30_I2C);
    i2c_enable(SHT30_I2C);

    /* 自检 */
    if (0U == (I2C_CTL0(SHT30_I2C) & I2C_CTL0_I2CEN)) { return 1; }
    return 0;
}

/**
 * @brief  IIC 总线反初始化
 * @return 0 成功，1 失败
 */
uint8_t sht30_interface_iic_deinit(void)
{
    i2c_disable(SHT30_I2C);
    if (0U != (I2C_CTL0(SHT30_I2C) & I2C_CTL0_I2CEN)) { return 1; }
    i2c_deinit(SHT30_I2C);
    return 0;
}

/**
 * @brief  IIC 总线读取（直接读取模式，无寄存器地址阶段）
 * @note   SHT30 是命令式传感器，数据读取时不需要先写寄存器地址。
 *         此实现为：START → [addr+R] → [读 len 字节] → STOP
 *         其中 reg 参数在此模式下被忽略（保留以兼容 LibDriver 句柄接口）。
 */
uint8_t sht30_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = sht30_addr_8bit_to_7bit(addr);

    (void)reg;   /* SHT30 直接读取不使用寄存器地址，保留参数以兼容接口 */

    if ((NULL == buf) || (0U == len)) { return 1; }

    /* 直接主机接收模式：无寄存器地址阶段，START 后直接读取 */
    i2c_master_addressing(SHT30_I2C, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(SHT30_I2C, (uint32_t)len);
    i2c_automatic_end_enable(SHT30_I2C);
    i2c_start_on_bus(SHT30_I2C);

    for (i = 0U; i < len; i++) {
        if (0U != sht30_i2c_wait_flag(I2C_FLAG_RBNE, SET)) { return 1; }
        buf[i] = (uint8_t)i2c_data_receive(SHT30_I2C);
    }
    if (0U != sht30_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }

    return 0;
}

/**
 * @brief  IIC 总线写入（标准模式：寄存器地址 + 数据）
 * @note   SHT30 用此函数发送 16 位命令：
 *         调用 write(addr, cmd_msb, &cmd_lsb, 1)
 *         → I2C 上发送：START → [addr+W] → [cmd_msb] → [cmd_lsb] → STOP
 */
uint8_t sht30_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = sht30_addr_8bit_to_7bit(addr);

    if ((NULL == buf) || (0U == len)) { return 1; }

    i2c_master_addressing(SHT30_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(SHT30_I2C, (uint32_t)(1U + len));
    i2c_automatic_end_enable(SHT30_I2C);
    i2c_start_on_bus(SHT30_I2C);

    /* 发送寄存器地址（对 SHT30 而言是命令高字节） */
    if (0U != sht30_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(SHT30_I2C); return 1;
    }
    i2c_data_transmit(SHT30_I2C, (uint32_t)reg);

    if (0U != sht30_i2c_check_nack()) {
        i2c_stop_on_bus(SHT30_I2C); return 1;
    }

    /* 发送数据（对 SHT30 而言是命令低字节） */
    for (i = 0U; i < len; i++) {
        if (0U != sht30_i2c_wait_flag(I2C_FLAG_TBE, SET)) { return 1; }
        i2c_data_transmit(SHT30_I2C, (uint32_t)buf[i]);
        if (0U != sht30_i2c_check_nack()) { return 1; }
    }

    if (0U != sht30_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }
    return 0;
}

/**
 * @brief  毫秒延时
 */
void sht30_interface_delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0U; i < ms; i++) {
        for (j = 0U; j < 8000U; j++) { __NOP(); }
    }
}

/**
 * @brief  调试打印
 */
void sht30_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
