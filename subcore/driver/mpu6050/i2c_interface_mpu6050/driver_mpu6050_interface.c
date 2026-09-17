/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * @file      driver_mpu6050_interface.c
 * @brief     driver mpu6050 接口源文件（基于 GD32L23x I2C 固件库）
 * @version   1.0.0
 * @author    Shifeng Li
 * @date      2022-06-30
 *
 * <h3>history</h3>
 * <table>
 * <tr><th>Date        <th>Version  <th>Author      <th>Description
 * <tr><td>2022/06/30  <td>1.0      <td>Shifeng Li  <td>first upload
 * </table>
 */

#include "driver_mpu6050_interface.h"
#include "gd32l23x_i2c.h"
#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                            硬件配置宏                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief MPU6050 所使用的 I2C 外设实例
 * @note  根据实际硬件连接修改为 I2C0 或 I2C2
 */
#define MPU6050_I2C              I2C1

/**
 * @brief I2C 时序预分频器（0 ~ 15）
 * @note  决定 tI2CCLK = (PSC + 1) × tPCLK1
 *        需要根据系统的 PCLK1 频率进行调整。
 *        示例：PCLK1 = 64 MHz, PSC = 3 → tI2CCLK = 250 ns
 */
#define MPU6050_I2C_PSC          3U

/**
 * @brief I2C 主模式下 SCL 低电平周期（0 ~ 255）
 * @note  与 SCLH 共同决定 SCL 时钟频率。
 *        标准模式 (100 kHz): SCLH ≈ SCLL ≈ 0x4D
 *        快速模式 (400 kHz): SCLH ≈ SCLL ≈ 0x11
 */
#define MPU6050_I2C_SCLL         0x11U

/**
 * @brief I2C 主模式下 SCL 高电平周期（0 ~ 255）
 */
#define MPU6050_I2C_SCLH         0x11U

/**
 * @brief I2C 数据建立时间（0 ~ 15）
 */
#define MPU6050_I2C_SCLDELY      0U

/**
 * @brief I2C 数据保持时间（0 ~ 15）
 */
#define MPU6050_I2C_SDADELY      0U

/**
 * @brief I2C 数字噪声滤波器长度
 *        可选值：FILTER_DISABLE 或 FILTER_LENGTH_1 ~ FILTER_LENGTH_15
 */
#define MPU6050_I2C_DNF          FILTER_DISABLE

/**
 * @brief I2C 标志位轮询超时时间（毫秒）
 */
#define MPU6050_I2C_TIMEOUT_MS   50U

/* -------------------------------------------------------------------------- */
/*                            内部辅助函数                                    */
/* -------------------------------------------------------------------------- */

/**
 * @brief      将 LibDriver 的 8 位 I2C 地址转换为 GD32 所需的 7 位地址
 * @param[in]  addr 8 位 IIC 设备地址（例如 0xD0 → 0x68）
 * @return     供 GD32 I2C API 使用的 7 位地址
 * @note       LibDriver 传入的是完整的 8 位写地址（AD0 引脚为低 → 0xD0），
 *             GD32 的 i2c_master_addressing 期望的是 7 位地址。
 */
static uint32_t mpu6050_addr_8bit_to_7bit(uint8_t addr)
{
    return (uint32_t)(addr >> 1);
}

/**
 * @brief      带超时的 I2C 标志位轮询
 * @param[in]  flag   要轮询的 I2C 标志位（例如 I2C_FLAG_TBE）
 * @param[in]  expect 期望的标志状态（SET 或 RESET）
 * @return     状态码
 *             - 0 成功（标志位在超时前达到预期状态）
 *             - 1 超时失败
 */
static uint8_t mpu6050_i2c_wait_flag(uint32_t flag, FlagStatus expect)
{
    uint32_t timeout = MPU6050_I2C_TIMEOUT_MS * 1000U;

    while (i2c_flag_get(MPU6050_I2C, flag) != expect) {
        if (0U == timeout--) {
            return 1;   /* 超时，通信失败 */
        }
    }
    return 0;
}

/**
 * @brief      检测并清除 NACK 标志
 * @return     状态码
 *             - 0 未检测到 NACK
 *             - 1 检测到 NACK（从机无应答）
 */
static uint8_t mpu6050_i2c_check_nack(void)
{
    if (SET == i2c_flag_get(MPU6050_I2C, I2C_FLAG_NACK)) {
        i2c_flag_clear(MPU6050_I2C, I2C_FLAG_NACK);
        return 1;   /* 从机无应答 */
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/*                          接口函数实现                                      */
/* -------------------------------------------------------------------------- */

/**
 * @brief  IIC 总线初始化
 * @return 状态码
 *         - 0 成功
 *         - 1 IIC 初始化失败
 * @note   以 400 kHz 快速模式配置 I2C 外设，用于 MPU6050 通信。
 *         I2C 的 GPIO 引脚（I2C1 对应 PB6/PB7，其他实例请查阅数据手册）
 *         需要在板级初始化代码中单独配置：
 *         1. rcu_periph_clock_enable 使能对应 GPIO 时钟
 *         2. gpio_init 配置为复用开漏输出
 */
uint8_t mpu6050_interface_iic_init(void)
{
    /* 复位 I2C 外设至默认状态 */
    i2c_deinit(MPU6050_I2C);

    /* 配置时序参数：预分频器、数据建立/保持时间 */
    i2c_timing_config(MPU6050_I2C, MPU6050_I2C_PSC,
                      MPU6050_I2C_SCLDELY, MPU6050_I2C_SDADELY);

    /* 配置主模式 SCL 高低电平周期（决定时钟频率） */
    i2c_master_clock_config(MPU6050_I2C, MPU6050_I2C_SCLH, MPU6050_I2C_SCLL);

    /* 配置数字噪声滤波器 */
    i2c_digital_noise_filter_config(MPU6050_I2C, MPU6050_I2C_DNF);

    /* 关闭模拟噪声滤波器（在噪声较大的环境中可改为使能） */
    i2c_analog_noise_filter_disable(MPU6050_I2C);

    /* 使能 I2C 外设 */
    i2c_enable(MPU6050_I2C);

    /* 自检：读取 CTL0 寄存器的 I2CEN 位，确认外设已成功使能 */
    if (0U == (I2C_CTL0(MPU6050_I2C) & I2C_CTL0_I2CEN)) {
        return 1;   /* 外设使能失败，返回接口注释规定的错误码 1 */
    }

    return 0;
}

/**
 * @brief  IIC 总线反初始化
 * @return 状态码
 *         - 0 成功
 *         - 1 IIC 反初始化失败
 * @note   禁用 I2C 外设并复位其寄存器。
 */
uint8_t mpu6050_interface_iic_deinit(void)
{
    /* 禁用 I2C 外设 */
    i2c_disable(MPU6050_I2C);

    /* 自检：确认 I2CEN 位已清零，外设已成功禁用 */
    if (0U != (I2C_CTL0(MPU6050_I2C) & I2C_CTL0_I2CEN)) {
        return 1;   /* 外设禁用失败，返回接口注释规定的错误码 1 */
    }

    /* 复位 I2C 寄存器至默认值 */
    i2c_deinit(MPU6050_I2C);

    return 0;
}


/**
 * @brief      IIC 总线读取
 * @param[in]  addr IIC 设备写地址（8 位，例如 AD0 为低时传入 0xD0）
 * @param[in]  reg  MPU6050 内部寄存器地址
 * @param[out] *buf 数据缓冲区指针
 * @param[in]  len  要读取的字节数
 * @return     状态码
 *             - 0 成功
 *             - 1 读取失败
 * @note       执行两阶段 I2C 事务：
 *             1. 主机发送模式写寄存器地址（不产生 STOP）
 *             2. 主机接收模式读取 len 字节（硬件自动产生 STOP）
 */
uint8_t mpu6050_interface_iic_read(uint8_t addr, uint8_t reg,
                                    uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = mpu6050_addr_8bit_to_7bit(addr);

    /* 参数有效性检查：缓冲区为空或长度为 0 时返回失败 */
    if ((NULL == buf) || (0U == len)) {
        return 1;
    }

    /* ===== 阶段一：主机发送模式，写入寄存器地址（不产生 STOP） ===== */

    /* 配置为 7 位地址 + 主机发送模式，1 字节，禁用自动 STOP */
    i2c_master_addressing(MPU6050_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MPU6050_I2C, 1U);
    i2c_automatic_end_disable(MPU6050_I2C);

    /* 发送 START 条件 */
    i2c_start_on_bus(MPU6050_I2C);

    /* 等待发送缓冲区为空，然后写入寄存器地址 */
    if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MPU6050_I2C);
        return 1;   /* TBE 超时，总线异常 */
    }
    i2c_data_transmit(MPU6050_I2C, (uint32_t)reg);

    /* 检查从机是否应答（NACK 表示从机地址或寄存器地址无效） */
    if (0U != mpu6050_i2c_check_nack()) {
        i2c_stop_on_bus(MPU6050_I2C);
        return 1;   /* 从机无应答 */
    }

    /* 等待寄存器地址字节发送完成（TC 置位） */
    if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TC, SET)) {
        i2c_stop_on_bus(MPU6050_I2C);
        return 1;   /* TC 超时，发送未完成 */
    }

    /* ===== 阶段二：主机接收模式，读取数据（硬件自动 STOP） ===== */

    /* 切换为 7 位地址 + 主机接收模式，len 字节，使能自动 STOP */
    i2c_master_addressing(MPU6050_I2C, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(MPU6050_I2C, (uint32_t)len);
    i2c_automatic_end_enable(MPU6050_I2C);

    /* 发送重复 START 条件 */
    i2c_start_on_bus(MPU6050_I2C);

    /* 逐字节读取数据 */
    for (i = 0U; i < len; i++) {
        if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_RBNE, SET)) {
            return 1;   /* RBNE 超时，数据接收失败 */
        }
        buf[i] = (uint8_t)i2c_data_receive(MPU6050_I2C);
    }

    /* 等待 AUTOEND 完成（硬件在最后一个字节后自动发送 STOP） */
    if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TC, SET)) {
        return 1;   /* TC 超时，STOP 未完成 */
    }

    return 0;
}

/**
 * @brief     IIC 总线写入
 * @param[in] addr IIC 设备写地址（8 位，例如 AD0 为低时传入 0xD0）
 * @param[in] reg  MPU6050 内部寄存器地址
 * @param[in] *buf 数据缓冲区指针
 * @param[in] len  要写入的字节数
 * @return    状态码
 *            - 0 成功
 *            - 1 写入失败
 * @note      发送序列：START → [addr+W] → [reg] → [buf[0]…buf[len-1]] → STOP
 */
uint8_t mpu6050_interface_iic_write(uint8_t addr, uint8_t reg,
                                     uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint32_t addr_7bit = mpu6050_addr_8bit_to_7bit(addr);

    /* 参数有效性检查：缓冲区为空或长度为 0 时返回失败 */
    if ((NULL == buf) || (0U == len)) {
        return 1;
    }

    /* 配置为 7 位地址 + 主机发送模式，(1 寄存器地址 + len 数据) 字节，使能自动 STOP */
    i2c_master_addressing(MPU6050_I2C, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(MPU6050_I2C, (uint32_t)(1U + len));
    i2c_automatic_end_enable(MPU6050_I2C);

    /* 发送 START 条件 */
    i2c_start_on_bus(MPU6050_I2C);

    /* 等待发送缓冲区为空，然后写入寄存器地址 */
    if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(MPU6050_I2C);
        return 1;   /* TBE 超时，总线异常 */
    }
    i2c_data_transmit(MPU6050_I2C, (uint32_t)reg);

    /* 检查从机是否应答寄存器地址 */
    if (0U != mpu6050_i2c_check_nack()) {
        i2c_stop_on_bus(MPU6050_I2C);
        return 1;   /* 从机无应答 */
    }

    /* 逐字节发送数据 */
    for (i = 0U; i < len; i++) {
        if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TBE, SET)) {
            return 1;   /* TBE 超时，数据发送中断 */
        }
        i2c_data_transmit(MPU6050_I2C, (uint32_t)buf[i]);

        /* 每发送一个字节后检查从机应答 */
        if (0U != mpu6050_i2c_check_nack()) {
            return 1;   /* 从机在数据传输过程中无应答 */
        }
    }

    /* 等待传输完成（硬件在最后一个字节后自动发送 STOP） */
    if (0U != mpu6050_i2c_wait_flag(I2C_FLAG_TC, SET)) {
        return 1;   /* TC 超时，STOP 未完成 */
    }

    return 0;
}

/**
 * @brief     毫秒级延时
 * @param[in] ms 延时时间（毫秒）
 * @note      使用简单的软件延时循环实现。
 *            如需更精确的延时，可替换为：
 *            - RTOS 延时（例如 vTaskDelay）
 *            - 硬件定时器（例如 SysTick）延时
 *            内层循环次数 8000 是在典型 Cortex-M 主频下的估算值，
 *            需根据实际核心时钟频率进行调整。
 */
void mpu6050_interface_delay_ms(uint32_t ms)
{
    uint32_t i, j;

    for (i = 0U; i < ms; i++) {
        /* 约 1 ms 的粗略延时（基于典型 Cortex-M 主频估算） */
        for (j = 0U; j < 8000U; j++) {
            __NOP();
        }
    }
}

/**
 * @brief     调试信息格式化打印
 * @param[in] fmt 格式化字符串
 * @note      重定向至标准 printf。
 *            对于没有重定向 UART printf 的嵌入式目标平台，
 *            可替换为自定义调试输出（UART 直接发送、SWO、SEGGER RTT 等）。
 */
void mpu6050_interface_debug_print(const char *const fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

/**
 * @brief     中断接收回调
 * @param[in] type 中断类型（参见 mpu6050_interrupt_t 枚举）
 * @note      由 mpu6050_irq_handler 在检测到中断时调用。
 *            需要将此函数与 MPU6050 INT 引脚对应的 EXTI / GPIO 中断处理函数
 *            进行对接。典型用法：
 *            1. 在 EXTI 中断服务函数中调用 mpu6050_irq_handler
 *            2. mpu6050_irq_handler 内部读取中断状态寄存器
 *            3. 根据中断类型回调此函数
 */
void mpu6050_interface_receive_callback(uint8_t type)
{
    switch (type)
    {
        /* 运动检测中断 */
        case MPU6050_INTERRUPT_MOTION :
        {
            mpu6050_interface_debug_print("mpu6050: irq motion.\n");

            break;
        }
        /* FIFO 溢出中断 */
        case MPU6050_INTERRUPT_FIFO_OVERFLOW :
        {
            mpu6050_interface_debug_print("mpu6050: irq fifo overflow.\n");

            break;
        }
        /* I2C 主模式中断 */
        case MPU6050_INTERRUPT_I2C_MAST :
        {
            mpu6050_interface_debug_print("mpu6050: irq i2c master.\n");

            break;
        }
        /* DMP 中断 */
        case MPU6050_INTERRUPT_DMP :
        {
            mpu6050_interface_debug_print("mpu6050: irq dmp\n");

            break;
        }
        /* 数据就绪中断 */
        case MPU6050_INTERRUPT_DATA_READY :
        {
            mpu6050_interface_debug_print("mpu6050: irq data ready\n");

            break;
        }
        /* 未知中断类型 */
        default :
        {
            mpu6050_interface_debug_print("mpu6050: irq unknown code.\n");

            break;
        }
    }
}

/**
 * @brief     DMP 敲击检测回调
 * @param[in] count     敲击次数
 * @param[in] direction 敲击方向（参见 mpu6050_dmp_tap_t 枚举）
 * @note      当 DMP 检测到敲击手势时被调用。
 */
void mpu6050_interface_dmp_tap_callback(uint8_t count, uint8_t direction)
{
    switch (direction)
    {
        /* X 轴正方向敲击 */
        case MPU6050_DMP_TAP_X_UP :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq x up with %d.\n", count);

            break;
        }
        /* X 轴负方向敲击 */
        case MPU6050_DMP_TAP_X_DOWN :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq x down with %d.\n", count);

            break;
        }
        /* Y 轴正方向敲击 */
        case MPU6050_DMP_TAP_Y_UP :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq y up with %d.\n", count);

            break;
        }
        /* Y 轴负方向敲击 */
        case MPU6050_DMP_TAP_Y_DOWN :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq y down with %d.\n", count);

            break;
        }
        /* Z 轴正方向敲击 */
        case MPU6050_DMP_TAP_Z_UP :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq z up with %d.\n", count);

            break;
        }
        /* Z 轴负方向敲击 */
        case MPU6050_DMP_TAP_Z_DOWN :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq z down with %d.\n", count);

            break;
        }
        /* 未知敲击方向 */
        default :
        {
            mpu6050_interface_debug_print("mpu6050: tap irq unknown code.\n");

            break;
        }
    }
}

/**
 * @brief     DMP 方向检测回调
 * @param[in] orientation 设备方向（参见 mpu6050_dmp_orient_t 枚举）
 * @note      当 DMP 检测到设备方向发生变化时被调用。
 */
void mpu6050_interface_dmp_orient_callback(uint8_t orientation)
{
    switch (orientation)
    {
        /* 竖屏正向 */
        case MPU6050_DMP_ORIENT_PORTRAIT :
        {
            mpu6050_interface_debug_print("mpu6050: orient irq portrait.\n");

            break;
        }
        /* 横屏正向 */
        case MPU6050_DMP_ORIENT_LANDSCAPE :
        {
            mpu6050_interface_debug_print("mpu6050: orient irq landscape.\n");

            break;
        }
        /* 竖屏倒置 */
        case MPU6050_DMP_ORIENT_REVERSE_PORTRAIT :
        {
            mpu6050_interface_debug_print("mpu6050: orient irq reverse portrait.\n");

            break;
        }
        /* 横屏倒置 */
        case MPU6050_DMP_ORIENT_REVERSE_LANDSCAPE :
        {
            mpu6050_interface_debug_print("mpu6050: orient irq reverse landscape.\n");

            break;
        }
        /* 未知方向 */
        default :
        {
            mpu6050_interface_debug_print("mpu6050: orient irq unknown code.\n");

            break;
        }
    }
}
