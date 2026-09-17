/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_mlx90614_interface.c
 * @brief     driver mlx90614 接口源文件（基于 GD32L23x I2C 固件库）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_mlx90614_interface.h"
#include "gd32l23x_i2c.h"
#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                            硬件配置宏                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief MLX90614 所使用的 I2C 外设实例
 * @note  根据实际硬件连接修改。MLX90614 是 SMBus 兼容设备，
 *        可与其它 I2C 传感器共用同一条总线。
 */
#define MLX90614_I2C              I2C1

/**
 * @brief I2C 时序预分频器（0 ~ 15）
 */
#define MLX90614_I2C_PSC          3U

/**
 * @brief I2C SCL 低电平周期（400 kHz 快速模式）
 * @note  MLX90614 支持标准模式（100 kHz）和快速模式（400 kHz）下的 SMBus
 */
#define MLX90614_I2C_SCLL         0x11U

/**
 * @brief I2C SCL 高电平周期
 */
#define MLX90614_I2C_SCLH         0x11U

/**
 * @brief I2C 数据建立时间
 */
#define MLX90614_I2C_SCLDELY      0U

/**
 * @brief I2C 数据保持时间
 */
#define MLX90614_I2C_SDADELY      0U

/**
 * @brief I2C 数字噪声滤波器
 */
#define MLX90614_I2C_DNF          FILTER_DISABLE

/**
 * @brief I2C 标志轮询超时（毫秒）
 */
#define MLX90614_I2C_TIMEOUT_MS   50U

/* -------------------------------------------------------------------------- */
/*                            内部辅助函数                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief      将 8 位 I2C 地址转换为 7 位地址
 */
static uint32_t mlx90614_addr_8bit_to_7bit(uint8_t addr)
{
    return (uint32_t)(addr >> 1);
}

/**
 * @brief      带超时的 I2C 标志位轮询
 */
static uint8_t mlx90614_i2c_wait_flag(uint32_t flag, FlagStatus expect)
{
    uint32_t timeout = MLX90614_I2C_TIMEOUT_MS * 1000U;
    while (i2c_flag_get(MLX90614_I2C, flag) != expect) {
        if (0U == timeout--) { return 1; }
    }
    return 0;
}

/**
 * @brief      检测并清除 NACK
 */
static uint8_t mlx90614_i2c_check_nack(void)
{
    if (SET == i2c_flag_get(MLX90614_I2C, I2C_FLAG_NACK)) {
        i2c_flag_clear(MLX90614_I2C, I2C_FLAG_NACK);
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
uint8_t mlx90614_interface_iic_init(void)
{
    i2c_deinit(MLX90614_I2C);
    i2c_timing_config(MLX90614_I2C, MLX90614_I2C_PSC,
                      MLX90614_I2C_SCLDELY, MLX90614_I2C_SDADELY);
    i2c_master_clock_config(MLX90614_I2C, MLX90614_I2C_SCLH, MLX90614_I2C_SCLL);
    i2c_digital_noise_filter_config(MLX90614_I2C, MLX90614_I2C_DNF);
    i2c_analog_noise_filter_disable(MLX90614_I2C);
    i2c_enable(MLX90614_I2C);

    /* 自检 */
    if (0U == (I2C_CTL0(MLX90614_I2C) & I2C_CTL0_I2CEN)) { return 1; }
    return 0;
}

/**
 * @brief  IIC 总线反初始化
 * @return 0 成功，1 失败
 */
uint8_t mlx90614_interface_iic_deinit(void)
{
    i2c_disable(MLX90614_I2C);
    if (0U != (I2C_CTL0(MLX90614_I2C) & I2C_CTL0_I2CEN)) { return 1; }
    i2c_deinit(MLX90614_I2C);
    return 0;
}

/**
 * @brief  IIC 总线读取（标准 SMBus read word 兼容模式）
 * @note   MLX90614 使用 SMBus read word 协议：
 *         START → [addr+W] → [reg] → RESTART → [addr+R] → [DataL] → [DataH] → [PEC] → STOP
 *         这与标准两阶段 I2C 读完全一致。
 */
uint8_t mlx90614_interface_iic_read(uint8_t addr, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = mlx90614_addr_8bit_to_7bit(addr);

    if ((NULL == buf) || (0U == len)) { return 1; }

    /* 阶段一：主机发送 → 写寄存器地址（SMBus command），不产生 STOP */
    i2c_master_addressing(MLX90614_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MLX90614_I2C, 1U);
    i2c_automatic_end_disable(MLX90614_I2C);
    i2c_start_on_bus(MLX90614_I2C);

    if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MLX90614_I2C); return 1;
    }
    i2c_data_transmit(MLX90614_I2C, (uint32_t)reg);

    if (0U != mlx90614_i2c_check_nack()) {
        i2c_stop_on_bus(MLX90614_I2C); return 1;
    }
    if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TC, SET)) {
        i2c_stop_on_bus(MLX90614_I2C); return 1;
    }

    /* 阶段二：主机接收 → 读取数据 + PEC，硬件自动 STOP */
    i2c_master_addressing(MLX90614_I2C, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(MLX90614_I2C, (uint32_t)len);
    i2c_automatic_end_enable(MLX90614_I2C);
    i2c_start_on_bus(MLX90614_I2C);

    for (i = 0U; i < len; i++) {
        if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_RBNE, SET)) { return 1; }
        buf[i] = (uint8_t)i2c_data_receive(MLX90614_I2C);
    }
    if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }

    return 0;
}

/**
 * @brief  IIC 总线写入（标准模式）
 * @note   MLX90614 SMBus write：START → [addr+W] → [cmd] → [data] → STOP
 *         用于休眠/唤醒命令及 EEPROM 写入（需注意 EEPROM 写入的延时要求）。
 */
uint8_t mlx90614_interface_iic_write(uint8_t addr, uint8_t reg,
                                      uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = mlx90614_addr_8bit_to_7bit(addr);

    if ((NULL == buf) || (0U == len)) { return 1; }

    i2c_master_addressing(MLX90614_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MLX90614_I2C, (uint32_t)(1U + len));
    i2c_automatic_end_enable(MLX90614_I2C);
    i2c_start_on_bus(MLX90614_I2C);

    if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MLX90614_I2C); return 1;
    }
    i2c_data_transmit(MLX90614_I2C, (uint32_t)reg);

    if (0U != mlx90614_i2c_check_nack()) {
        i2c_stop_on_bus(MLX90614_I2C); return 1;
    }

    for (i = 0U; i < len; i++) {
        if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TBE, SET)) { return 1; }
        i2c_data_transmit(MLX90614_I2C, (uint32_t)buf[i]);
        if (0U != mlx90614_i2c_check_nack()) { return 1; }
    }

    if (0U != mlx90614_i2c_wait_flag(I2C_FLAG_TC, SET)) { return 1; }
    return 0;
}

/**
 * @brief  毫秒延时
 */
void mlx90614_interface_delay_ms(uint32_t ms)
{
    uint32_t i, j;
    for (i = 0U; i < ms; i++) {
        for (j = 0U; j < 8000U; j++) { __NOP(); }
    }
}

/**
 * @brief  调试打印
 */
void mlx90614_interface_debug_print(const char *const fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}
