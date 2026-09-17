/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 * ... (许可证文本)
 *
 * @file      driver_max30102_interface.c
 * @brief     driver max30102 接口源文件（基于 GD32L23x I2C 固件库）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_max30102_interface.h"
#include "gd32l23x_i2c.h"
#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                            硬件配置宏                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief MAX30102 所使用的 I2C 外设实例
 * @note  根据实际硬件连接修改为 I2C0 或 I2C2
 */
#define MAX30102_I2C              I2C1

/**
 * @brief I2C 时序预分频器（0 ~ 15）
 */
#define MAX30102_I2C_PSC          3U

/**
 * @brief I2C SCL 低电平周期（400 kHz 快速模式）
 */
#define MAX30102_I2C_SCLL         0x11U

/**
 * @brief I2C SCL 高电平周期（400 kHz 快速模式）
 */
#define MAX30102_I2C_SCLH         0x11U

/**
 * @brief I2C 数据建立时间
 */
#define MAX30102_I2C_SCLDELY      0U

/**
 * @brief I2C 数据保持时间
 */
#define MAX30102_I2C_SDADELY      0U

/**
 * @brief I2C 数字噪声滤波器
 */
#define MAX30102_I2C_DNF          FILTER_DISABLE

/**
 * @brief I2C 标志轮询超时（毫秒）
 */
#define MAX30102_I2C_TIMEOUT_MS   50U

/* -------------------------------------------------------------------------- */
/*                            内部辅助函数                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief      将 8 位 I2C 地址转换为 7 位地址
 * @param[in]  addr 8 位 I2C 设备地址
 * @return     7 位地址
 */
static uint32_t max30102_addr_8bit_to_7bit(uint8_t addr)
{
    return (uint32_t)(addr >> 1);
}

/**
 * @brief      带超时的 I2C 标志位轮询
 * @param[in]  flag   目标标志位
 * @param[in]  expect 期待值（SET 或 RESET）
 * @return     0 成功，1 超时
 */
static uint8_t max30102_i2c_wait_flag(uint32_t flag, FlagStatus expect)
{
    uint32_t timeout = MAX30102_I2C_TIMEOUT_MS * 1000U;
    while (i2c_flag_get(MAX30102_I2C, flag) != expect) {
        if (0U == timeout--) { return 1; }
    }
    return 0;
}

/**
 * @brief      检测并清除 NACK
 * @return     0 无 NACK，1 有 NACK
 */
static uint8_t max30102_i2c_check_nack(void)
{
    if (SET == i2c_flag_get(MAX30102_I2C, I2C_FLAG_NACK)) {
        i2c_flag_clear(MAX30102_I2C, I2C_FLAG_NACK);
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
 */
uint8_t max30102_interface_iic_init(void)
{
    i2c_deinit(MAX30102_I2C);
    i2c_timing_config(MAX30102_I2C, MAX30102_I2C_PSC,
                      MAX30102_I2C_SCLDELY, MAX30102_I2C_SDADELY);
    i2c_master_clock_config(MAX30102_I2C, MAX30102_I2C_SCLH, MAX30102_I2C_SCLL);
    i2c_digital_noise_filter_config(MAX30102_I2C, MAX30102_I2C_DNF);
    i2c_analog_noise_filter_disable(MAX30102_I2C);
    i2c_enable(MAX30102_I2C);

    /* 自检：确认外设已使能 */
    if (0U == (I2C_CTL0(MAX30102_I2C) & I2C_CTL0_I2CEN)) {
        return 1;
    }
    return 0;
}

/**
 * @brief  IIC 总线反初始化
 * @return 0 成功，1 失败
 */
uint8_t max30102_interface_iic_deinit(void)
{
    i2c_disable(MAX30102_I2C);
    if (0U != (I2C_CTL0(MAX30102_I2C) & I2C_CTL0_I2CEN)) {
        return 1;
    }
    i2c_deinit(MAX30102_I2C);
    return 0;
}

/**
 * @brief  IIC 总线读取（两阶段：写寄存器地址 → 读数据）
 */
uint8_t max30102_interface_iic_read(uint8_t addr, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = max30102_addr_8bit_to_7bit(addr);

    if ((NULL == buf) || (0U == len)) { return 1; }

    /* 阶段一：写寄存器地址（无 STOP） */
    i2c_master_addressing(MAX30102_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MAX30102_I2C, 1U);
    i2c_automatic_end_disable(MAX30102_I2C);
    i2c_start_on_bus(MAX30102_I2C);

    if (0U != max30102_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MAX30102_I2C); return 1;
    }
    i2c_data_transmit(MAX30102_I2C, (uint32_t)reg);

    if (0U != max30102_i2c_check_nack()) {
        i2c_stop_on_bus(MAX30102_I2C); return 1;
    }
    if (0U != max30102_i2c_wait_flag(I2C_FLAG_TC, SET)) {
        i2c_stop_on_bus(MAX30102_I2C); return 1;
    }

    /* 阶段二：读取数据（自动 STOP） */
    i2c_master_addressing(MAX30102_I2C, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(MAX30102_I2C, (uint32_t)len);
    i2c_automatic_end_enable(MAX30102_I2C);
    i2c_start_on_bus(MAX30102_I2C);

    for (i = 0U; i < len; i++) {
        if (0U != max30102_i2c_wait_flag(I2C_FLAG_RBNE, SET)) { return 1; }
        buf[i] = (uint8_t)i2c_data_receive(MAX30102_I2C);
    }
    if (0U != max30102_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }

    return 0;
}

/**
 * @brief  IIC 总线写入（发送：START → [addr+W] → [reg] → [data] → STOP）
 */
uint8_t max30102_interface_iic_write(uint8_t addr, uint8_t reg,
                                      uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = max30102_addr_8bit_to_7bit(addr);

    if ((NULL == buf) || (0U == len)) { return 1; }

    i2c_master_addressing(MAX30102_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MAX30102_I2C, (uint32_t)(1U + len));
    i2c_automatic_end_enable(MAX30102_I2C);
    i2c_start_on_bus(MAX30102_I2C);

    if (0U != max30102_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MAX30102_I2C); return 1;
    }
    i2c_data_transmit(MAX30102_I2C, (uint32_t)reg);

    if (0U != max30102_i2c_check_nack()) {
        i2c_stop_on_bus(MAX30102_I2C); return 1;
    }

    for (i = 0U; i < len; i++) {
        if (0U != max30102_i2c_wait_flag(I2C_FLAG_TBE, SET)) { return 1; }
        i2c_data_transmit(MAX30102_I2C, (uint32_t)buf[i]);
        if (0U != max30102_i2c_check_nack()) { return 1; }
    }

    if (0U != max30102_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }
    return 0;
}

/**
 * @brief  毫秒延时
 */
void max30102_interface_delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0U; i < ms; i++) {
        for (j = 0U; j < 8000U; j++) { __NOP(); }
    }
}

/**
 * @brief  调试打印
 */
void max30102_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief  中断回调
 */
void max30102_interface_receive_callback(uint8_t type)
{
    switch (type) {
    case MAX30102_INTERRUPT_FIFO_FULL:
        max30102_interface_debug_print("max30102: irq fifo almost full.\n");
        break;
    case MAX30102_INTERRUPT_PPG_RDY:
        /* PPG 数据就绪，通常在此处理 FIFO 读取 */
        break;
    case MAX30102_INTERRUPT_ALC_OVF:
        max30102_interface_debug_print("max30102: irq ambient light overflow.\n");
        break;
    case MAX30102_INTERRUPT_DIE_TEMP:
        max30102_interface_debug_print("max30102: irq temperature ready.\n");
        break;
    default:
        max30102_interface_debug_print("max30102: irq unknown.\n");
        break;
    }
}
