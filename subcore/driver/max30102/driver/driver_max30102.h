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
 * @file      driver_max30102.h
 * @brief     driver max30102 头文件（心率/血氧传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_MAX30102_H
#define DRIVER_MAX30102_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @addtogroup max30102_basic_driver
 * @{
 */

/**
 * @brief MAX30102 I2C 设备地址枚举
 */
typedef enum
{
    MAX30102_ADDRESS_AD0_LOW  = 0xAE,        /**< AD0 引脚接低电平时的 8 位写地址 */
    MAX30102_ADDRESS_AD0_HIGH = 0xAF,        /**< AD0 引脚接高电平时的 8 位写地址 */
} max30102_address_t;

/**
 * @brief MAX30102 工作模式枚举
 */
typedef enum
{
    MAX30102_MODE_HEART_RATE   = 0x02,        /**< 仅心率模式（仅红光 LED） */
    MAX30102_MODE_SPO2         = 0x03,        /**< 血氧模式（红光 + 红外 LED） */
    MAX30102_MODE_MULTI_LED    = 0x07,        /**< 多 LED 模式 */
} max30102_mode_t;

/**
 * @brief MAX30102 ADC 分辨率枚举
 */
typedef enum
{
    MAX30102_ADC_RANGE_2048  = 0x00,          /**< ±2048 nA */
    MAX30102_ADC_RANGE_4096  = 0x01,          /**< ±4096 nA */
    MAX30102_ADC_RANGE_8192  = 0x02,          /**< ±8192 nA */
    MAX30102_ADC_RANGE_16384 = 0x03,          /**< ±16384 nA */
} max30102_adc_range_t;

/**
 * @brief MAX30102 采样率枚举
 */
typedef enum
{
    MAX30102_SAMPLE_RATE_50_HZ   = 0x00,      /**< 每秒 50 个样本 */
    MAX30102_SAMPLE_RATE_100_HZ  = 0x01,      /**< 每秒 100 个样本 */
    MAX30102_SAMPLE_RATE_200_HZ  = 0x02,      /**< 每秒 200 个样本 */
    MAX30102_SAMPLE_RATE_400_HZ  = 0x03,      /**< 每秒 400 个样本 */
    MAX30102_SAMPLE_RATE_800_HZ  = 0x04,      /**< 每秒 800 个样本 */
    MAX30102_SAMPLE_RATE_1000_HZ = 0x05,      /**< 每秒 1000 个样本 */
    MAX30102_SAMPLE_RATE_1600_HZ = 0x06,      /**< 每秒 1600 个样本 */
    MAX30102_SAMPLE_RATE_3200_HZ = 0x07,      /**< 每秒 3200 个样本 */
} max30102_sample_rate_t;

/**
 * @brief MAX30102 LED 脉冲宽度枚举
 */
typedef enum
{
    MAX30102_PULSE_WIDTH_69_US   = 0x00,      /**< 脉宽 69 μs（15 位分辨率） */
    MAX30102_PULSE_WIDTH_118_US  = 0x01,      /**< 脉宽 118 μs（16 位分辨率） */
    MAX30102_PULSE_WIDTH_215_US  = 0x02,      /**< 脉宽 215 μs（17 位分辨率） */
    MAX30102_PULSE_WIDTH_411_US  = 0x03,      /**< 脉宽 411 μs（18 位分辨率） */
} max30102_pulse_width_t;

/**
 * @brief MAX30102 FIFO 采样平均次数枚举
 */
typedef enum
{
    MAX30102_SAMPLE_AVG_1   = 0x00,           /**< 不进行平均（每个样本单独存入 FIFO） */
    MAX30102_SAMPLE_AVG_2   = 0x01,           /**< 2 个样本平均 */
    MAX30102_SAMPLE_AVG_4   = 0x02,           /**< 4 个样本平均 */
    MAX30102_SAMPLE_AVG_8   = 0x03,           /**< 8 个样本平均 */
    MAX30102_SAMPLE_AVG_16  = 0x04,           /**< 16 个样本平均 */
    MAX30102_SAMPLE_AVG_32  = 0x05,           /**< 32 个样本平均 */
} max30102_sample_avg_t;

/**
 * @brief MAX30102 中断类型枚举
 */
typedef enum
{
    MAX30102_INTERRUPT_FIFO_FULL  = 0x07,      /**< FIFO 几乎满中断 */
    MAX30102_INTERRUPT_PPG_RDY    = 0x06,      /**< 新 PPG 数据就绪 */
    MAX30102_INTERRUPT_ALC_OVF    = 0x05,      /**< 环境光消除溢出 */
    MAX30102_INTERRUPT_PROX_INT   = 0x04,      /**< 接近检测中断（MAX30105） */
    MAX30102_INTERRUPT_DIE_TEMP   = 0x02,      /**< 芯片温度就绪中断 */
    MAX30102_INTERRUPT_PWR_RDY    = 0x00,      /**< 上电就绪中断 */
} max30102_interrupt_t;

/**
 * @brief MAX30102 LED 通道选择枚举
 */
typedef enum
{
    MAX30102_LED_RED  = 0x00,                  /**< 红光 LED（LED1） */
    MAX30102_LED_IR   = 0x01,                  /**< 红外 LED（LED2） */
} max30102_led_t;

/**
 * @brief MAX30102 布尔值枚举
 */
typedef enum
{
    MAX30102_BOOL_FALSE = 0x00,                /**< 关闭功能 */
    MAX30102_BOOL_TRUE  = 0x01,                /**< 启用功能 */
} max30102_bool_t;

/**
 * @}
 */

/**
 * @brief MAX30102 操作句柄结构体
 */
typedef struct max30102_handle_s
{
    uint8_t iic_addr;                                                              /**< IIC 设备地址 */
    uint8_t (*iic_init)(void);                                                     /**< IIC 初始化函数指针 */
    uint8_t (*iic_deinit)(void);                                                   /**< IIC 反初始化函数指针 */
    uint8_t (*iic_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);    /**< IIC 读取函数指针 */
    uint8_t (*iic_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);   /**< IIC 写入函数指针 */
    void (*delay_ms)(uint32_t ms);                                                 /**< 毫秒延时函数指针 */
    void (*debug_print)(const char *const fmt, ...);                               /**< 调试打印函数指针 */
    void (*receive_callback)(uint8_t type);                                        /**< 中断回调函数指针 */
    uint8_t inited;                                                                /**< 初始化标志 */
    uint16_t fifo_sample_count;                                                    /**< FIFO 采样计数 */
} max30102_handle_t;

/**
 * @brief MAX30102 器件信息结构体
 */
typedef struct max30102_info_s
{
    char chip_name[32];                /**< 芯片名称 */
    char manufacturer_name[32];        /**< 制造商名称 */
    char interface[8];                 /**< 通信接口名称 */
    float supply_voltage_min_v;        /**< 最低供电电压 */
    float supply_voltage_max_v;        /**< 最高供电电压 */
    float max_current_ma;              /**< 最大工作电流 */
    float temperature_min;             /**< 最低工作温度 */
    float temperature_max;             /**< 最高工作温度 */
    uint32_t driver_version;           /**< 驱动版本号 */
} max30102_info_t;

/**
 * @brief MAX30102 单次采样数据结构体（一个红光样本 + 一个红外样本）
 */
typedef struct max30102_fifo_data_s
{
    uint32_t ir_value;                 /**< 红外通道采样值 */
    uint32_t red_value;                /**< 红光通道采样值 */
} max30102_fifo_data_t;

/* ===== 链接宏 ===== */

#define DRIVER_MAX30102_LINK_INIT(HANDLE, STRUCTURE)         memset(HANDLE, 0, sizeof(STRUCTURE))
#define DRIVER_MAX30102_LINK_IIC_INIT(HANDLE, FUC)           (HANDLE)->iic_init = FUC
#define DRIVER_MAX30102_LINK_IIC_DEINIT(HANDLE, FUC)         (HANDLE)->iic_deinit = FUC
#define DRIVER_MAX30102_LINK_IIC_READ(HANDLE, FUC)           (HANDLE)->iic_read = FUC
#define DRIVER_MAX30102_LINK_IIC_WRITE(HANDLE, FUC)          (HANDLE)->iic_write = FUC
#define DRIVER_MAX30102_LINK_DELAY_MS(HANDLE, FUC)           (HANDLE)->delay_ms = FUC
#define DRIVER_MAX30102_LINK_DEBUG_PRINT(HANDLE, FUC)        (HANDLE)->debug_print = FUC
#define DRIVER_MAX30102_LINK_RECEIVE_CALLBACK(HANDLE, FUC)   (HANDLE)->receive_callback = FUC

/* ===== API 函数声明 ===== */

/**
 * @brief     获取器件信息
 * @param[out] *info 指向器件信息结构体的指针
 * @return     状态码 - 0 成功
 */
uint8_t max30102_info(max30102_info_t *info);

/**
 * @brief     设置芯片地址引脚
 * @param[in]  *handle   指向操作句柄的指针
 * @param[in]  addr_pin  芯片地址引脚配置
 * @return     状态码
 *             - 0 成功
 *             - 2 句柄为空
 */
uint8_t max30102_set_addr_pin(max30102_handle_t *handle, max30102_address_t addr_pin);

/**
 * @brief     初始化芯片
 * @param[in]  *handle 指向操作句柄的指针
 * @return     状态码
 *             - 0 成功
 *             - 1 IIC 初始化失败
 *             - 2 句柄为空
 *             - 3 链接函数为空
 *             - 4 复位失败
 *             - 5 器件 ID 无效
 */
uint8_t max30102_init(max30102_handle_t *handle);

/**
 * @brief     关闭芯片
 * @param[in]  *handle 指向操作句柄的指针
 * @return     状态码
 *             - 0 成功
 *             - 1 IIC 反初始化失败
 *             - 2 句柄为空
 *             - 3 句柄未初始化
 */
uint8_t max30102_deinit(max30102_handle_t *handle);

/**
 * @brief     读取芯片温度
 * @param[in]  *handle   指向操作句柄的指针
 * @param[out] *degrees  指向温度值（摄氏度）缓冲区的指针
 * @return     状态码
 *             - 0 成功
 *             - 1 读取失败
 *             - 2 句柄为空
 *             - 3 句柄未初始化
 */
uint8_t max30102_read_temperature(max30102_handle_t *handle, float *degrees);

/**
 * @brief     读取 FIFO 中的采样数据
 * @param[in]  *handle       指向操作句柄的指针
 * @param[out] *buf          指向 FIFO 数据缓冲区的指针
 * @param[in]  sample_count  要读取的样本数量
 * @param[out] *read_count   实际读取到的样本数量
 * @return     状态码
 *             - 0 成功
 *             - 1 读取失败
 *             - 2 句柄为空
 *             - 3 句柄未初始化
 */
uint8_t max30102_read_fifo(max30102_handle_t *handle, max30102_fifo_data_t *buf,
                           uint16_t sample_count, uint16_t *read_count);

/**
 * @brief     获取 FIFO 中已写入的数据指针位置
 * @param[in]  *handle  指向操作句柄的指针
 * @param[out] *wr_ptr  指向写指针缓冲区的指针
 * @return     状态码
 *             - 0 成功
 *             - 1 读取失败
 *             - 2 句柄为空
 *             - 3 句柄未初始化
 */
uint8_t max30102_get_fifo_write_pointer(max30102_handle_t *handle, uint8_t *wr_ptr);

/**
 * @brief     获取 FIFO 中已读取的数据指针位置
 * @param[in]  *handle  指向操作句柄的指针
 * @param[out] *rd_ptr  指向读指针缓冲区的指针
 * @return     状态码
 */
uint8_t max30102_get_fifo_read_pointer(max30102_handle_t *handle, uint8_t *rd_ptr);

/**
 * @brief     设置 FIFO 读写指针（用于 FIFO 数据对齐）
 * @param[in]  *handle  指向操作句柄的指针
 * @param[in]  wr_ptr   FIFO 写指针
 * @param[in]  rd_ptr   FIFO 读指针
 * @return     状态码
 */
uint8_t max30102_set_fifo_pointers(max30102_handle_t *handle, uint8_t wr_ptr, uint8_t rd_ptr);

/**
 * @brief     设置传感器工作模式
 * @param[in]  *handle 指向操作句柄的指针
 * @param[in]  mode    工作模式
 * @return     状态码
 */
uint8_t max30102_set_mode(max30102_handle_t *handle, max30102_mode_t mode);

/**
 * @brief     获取传感器工作模式
 * @param[in]  *handle 指向操作句柄的指针
 * @param[out] *mode   指向模式缓冲区的指针
 * @return     状态码
 */
uint8_t max30102_get_mode(max30102_handle_t *handle, max30102_mode_t *mode);

/**
 * @brief     设置 SpO2 配置（ADC 量程 + 采样率 + 脉宽）
 * @param[in]  *handle      指向操作句柄的指针
 * @param[in]  adc_range    ADC 量程
 * @param[in]  sample_rate  采样率
 * @param[in]  pulse_width  LED 脉冲宽度
 * @return     状态码
 */
uint8_t max30102_set_spo2_config(max30102_handle_t *handle, max30102_adc_range_t adc_range,
                                  max30102_sample_rate_t sample_rate, max30102_pulse_width_t pulse_width);

/**
 * @brief     设置 LED 脉冲幅度（电流）
 * @param[in]  *handle     指向操作句柄的指针
 * @param[in]  led         要配置的 LED 通道
 * @param[in]  amplitude   脉冲幅度（0x00 ~ 0xFF，约 0.2 mA/步进）
 * @return     状态码
 */
uint8_t max30102_set_led_pulse_amplitude(max30102_handle_t *handle, max30102_led_t led, uint8_t amplitude);

/**
 * @brief     配置 FIFO
 * @param[in]  *handle     指向操作句柄的指针
 * @param[in]  sample_avg  采样平均次数
 * @param[in]  roll_over   是否启用 FIFO 翻转覆盖（TRUE = 新数据覆盖旧数据）
 * @param[in]  almost_full FIFO 几乎满阈值（0 ~ 15，触发 A_FULL 中断的剩余空间）
 * @return     状态码
 */
uint8_t max30102_set_fifo_config(max30102_handle_t *handle, max30102_sample_avg_t sample_avg,
                                  max30102_bool_t roll_over, uint8_t almost_full);

/**
 * @brief     使能或禁用一个中断
 * @param[in]  *handle     指向操作句柄的指针
 * @param[in]  interrupt   中断类型
 * @param[in]  enable      使能状态
 * @return     状态码
 */
uint8_t max30102_set_interrupt(max30102_handle_t *handle, max30102_interrupt_t interrupt,
                                max30102_bool_t enable);

/**
 * @brief     获取中断状态寄存器
 * @param[in]  *handle 指向操作句柄的指针
 * @param[out] *status1 指向中断状态寄存器 1 的指针
 * @param[out] *status2 指向中断状态寄存器 2 的指针
 * @return     状态码
 */
uint8_t max30102_get_interrupt_status(max30102_handle_t *handle, uint8_t *status1, uint8_t *status2);

/**
 * @brief     复位芯片
 * @param[in]  *handle 指向操作句柄的指针
 * @return     状态码
 */
uint8_t max30102_reset(max30102_handle_t *handle);

/**
 * @brief     进入或退出关机模式
 * @param[in]  *handle  指向操作句柄的指针
 * @param[in]  shutdown 是否关机
 * @return     状态码
 */
uint8_t max30102_set_shutdown(max30102_handle_t *handle, max30102_bool_t shutdown);

/**
 * @brief     IRQ 处理函数（需在主 EXTI 中断服务函数中调用）
 * @param[in] *handle 指向操作句柄的指针
 * @return    状态码
 */
uint8_t max30102_irq_handler(max30102_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_MAX30102_H */
