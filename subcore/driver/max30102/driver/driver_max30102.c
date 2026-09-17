/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 * ... (许可证文本同 header)
 *
 * @file      driver_max30102.c
 * @brief     driver max30102 源文件（心率/血氧传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_max30102.h"

/* 下面那一堆位定义用了 BIT()，但这份驱动是从 LibDriver 抠出来的，
 * 原本依赖各家 MCU 库自带的 BIT 宏。这里自带一份，省得为了一个宏
 * 把 gd32l23x.h 拖进驱动层（驱动层要保持跟平台无关）。 */
#ifndef BIT
#define BIT(x)  ((uint32_t)((uint32_t)0x01U << (x)))
#endif

/* ===== 寄存器地址定义（MAX30102 数据手册） ===== */

#define MAX30102_REG_INT_STATUS_1        0x00U   /* 中断状态寄存器 1 */
#define MAX30102_REG_INT_STATUS_2        0x01U   /* 中断状态寄存器 2 */
#define MAX30102_REG_INT_ENABLE_1        0x02U   /* 中断使能寄存器 1 */
#define MAX30102_REG_INT_ENABLE_2        0x03U   /* 中断使能寄存器 2 */
#define MAX30102_REG_FIFO_WR_PTR         0x04U   /* FIFO 写指针 */
#define MAX30102_REG_FIFO_OVF_COUNTER    0x05U   /* FIFO 溢出计数器 */
#define MAX30102_REG_FIFO_RD_PTR         0x06U   /* FIFO 读指针 */
#define MAX30102_REG_FIFO_DATA           0x07U   /* FIFO 数据寄存器（连续读取） */
#define MAX30102_REG_FIFO_CONFIG         0x08U   /* FIFO 配置 */
#define MAX30102_REG_MODE_CONFIG         0x09U   /* 模式配置 */
#define MAX30102_REG_SPO2_CONFIG         0x0AU   /* SpO2 配置 */
#define MAX30102_REG_LED1_PULSE_AMP      0x0CU   /* LED1（红光）脉冲幅度 [7:0] */
#define MAX30102_REG_LED2_PULSE_AMP      0x0DU   /* LED2（红外）脉冲幅度 [7:0] */
#define MAX30102_REG_LED_MODE_SLOT1      0x11U   /* 多 LED 模式控制 slot1 */
#define MAX30102_REG_LED_MODE_SLOT2      0x12U   /* 多 LED 模式控制 slot2 */
#define MAX30102_REG_DIE_TEMP_INT        0x1FU   /* 芯片温度整数部分 */
#define MAX30102_REG_DIE_TEMP_FRAC       0x20U   /* 芯片温度小数部分 */
#define MAX30102_REG_DIE_TEMP_CONFIG     0x21U   /* 芯片温度配置 */
#define MAX30102_REG_REVISION_ID         0xFEU   /* 修订版本 ID */
#define MAX30102_REG_PART_ID             0xFFU   /* 器件 ID（MAX30102 = 0x15） */

/* ===== 寄存器位定义 ===== */

/* 中断状态 1 */
#define MAX30102_INT1_A_FULL             BIT(7)  /* FIFO 几乎满 */
#define MAX30102_INT1_PPG_RDY            BIT(6)  /* PPG 数据就绪 */
#define MAX30102_INT1_ALC_OVF            BIT(5)  /* 环境光消除溢出 */

/* 中断状态 2 */
#define MAX30102_INT2_DIE_TEMP_RDY       BIT(1)  /* 芯片温度就绪 */
#define MAX30102_INT2_PWR_RDY            BIT(0)  /* 上电就绪 */

/* 中断使能 1 */
#define MAX30102_INT_EN1_A_FULL_EN       BIT(7)  /* 使能 A_FULL 中断 */
#define MAX30102_INT_EN1_PPG_RDY_EN      BIT(6)  /* 使能 PPG_RDY 中断 */
#define MAX30102_INT_EN1_ALC_OVF_EN      BIT(5)  /* 使能 ALC_OVF 中断 */

/* 中断使能 2 */
#define MAX30102_INT_EN2_DIE_TEMP_RDY_EN BIT(1)  /* 使能温度就绪中断 */

/* FIFO 配置 */
#define MAX30102_FIFO_CFG_SMP_AVE_POS    5U      /* 采样平均位偏移 */
#define MAX30102_FIFO_CFG_SMP_AVE_MSK    0xE0U   /* 采样平均位掩码 */
#define MAX30102_FIFO_CFG_ROLL_OVER_EN   BIT(4)  /* FIFO 翻转模式 */
#define MAX30102_FIFO_CFG_A_FULL_POS     0U      /* 几乎满阈值位偏移 */
#define MAX30102_FIFO_CFG_A_FULL_MSK     0x0FU   /* 几乎满阈值位掩码 */

/* 模式配置 */
#define MAX30102_MODE_SHDN               BIT(7)  /* 关机模式 */
#define MAX30102_MODE_RESET              BIT(6)  /* 复位 */
#define MAX30102_MODE_MODE_POS           0U      /* 模式位偏移 */
#define MAX30102_MODE_MODE_MSK           0x07U   /* 模式位掩码 */

/* SpO2 配置 */
#define MAX30102_SPO2_ADC_RGE_POS        5U      /* ADC 量程位偏移 */
#define MAX30102_SPO2_ADC_RGE_MSK        0x60U   /* ADC 量程位掩码 */
#define MAX30102_SPO2_SR_POS             2U      /* 采样率位偏移 */
#define MAX30102_SPO2_SR_MSK             0x1CU   /* 采样率位掩码 */
#define MAX30102_SPO2_LED_PW_POS         0U      /* LED 脉宽位偏移 */
#define MAX30102_SPO2_LED_PW_MSK         0x03U   /* LED 脉宽位掩码 */

/* FIFO 数据每个样本占用的字节数 */
#define MAX30102_FIFO_SAMPLE_BYTES       6U      /* 每样本 6 字节（3 字节 IR + 3 字节 Red） */
#define MAX30102_FIFO_DEPTH             32U      /* FIFO 深度 32 个样本 */

/* 器件 ID 常量 */
#define MAX30102_PART_ID_VAL             0x15U   /* MAX30102 的 Part ID */
#define MAX30102_REVISION_ID_MASK        0x0FU   /* 修订版本掩码 */

/* ===== 辅助函数：写/读寄存器 ===== */

/**
 * @brief     通过句柄函数指针写入传感器寄存器
 */
static uint8_t max30102_write_reg(max30102_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if ((NULL == handle) || (NULL == handle->iic_write)) {
        return 1;
    }
    return handle->iic_write(handle->iic_addr, reg, buf, len);
}

/**
 * @brief     通过句柄函数指针读取传感器寄存器
 */
static uint8_t max30102_read_reg(max30102_handle_t *handle, uint8_t reg, uint8_t *buf, uint16_t len)
{
    if ((NULL == handle) || (NULL == handle->iic_read)) {
        return 1;
    }
    return handle->iic_read(handle->iic_addr, reg, buf, len);
}

/**
 * @brief     写入单个寄存器值
 */
static uint8_t max30102_write_reg8(max30102_handle_t *handle, uint8_t reg, uint8_t value)
{
    return max30102_write_reg(handle, reg, &value, 1U);
}

/**
 * @brief     读取单个寄存器值
 */
static uint8_t max30102_read_reg8(max30102_handle_t *handle, uint8_t reg, uint8_t *value)
{
    return max30102_read_reg(handle, reg, value, 1U);
}

/* ===== 公开 API 实现 ===== */

/**
 * @brief     获取器件信息
 */
uint8_t max30102_info(max30102_info_t *info)
{
    if (NULL == info) {
        return 2;
    }

    memset(info, 0, sizeof(max30102_info_t));
    strncpy(info->chip_name,        "MAX30102", 31);
    strncpy(info->manufacturer_name, "Maxim Integrated", 31);
    strncpy(info->interface,        "IIC", 7);
    info->supply_voltage_min_v = 1.8f;
    info->supply_voltage_max_v = 3.3f;
    info->max_current_ma       = 50.0f;
    info->temperature_min      = -40.0f;
    info->temperature_max      = 85.0f;
    info->driver_version       = 0x00010000U;

    return 0;
}

/**
 * @brief     设置芯片地址引脚
 */
uint8_t max30102_set_addr_pin(max30102_handle_t *handle, max30102_address_t addr_pin)
{
    if (NULL == handle) {
        return 2;
    }
    handle->iic_addr = (uint8_t)addr_pin;
    return 0;
}

/**
 * @brief     初始化芯片
 */
uint8_t max30102_init(max30102_handle_t *handle)
{
    uint8_t part_id;

    if (NULL == handle) {
        return 2;
    }
    if (NULL == handle->iic_init || NULL == handle->iic_read || NULL == handle->iic_write) {
        return 3;
    }

    /* 调用 IIC 总线初始化 */
    if (0U != handle->iic_init()) {
        return 1;
    }

    /* 检查器件 ID（确保 I2C 通信正常） */
    if (0U != max30102_read_reg8(handle, MAX30102_REG_PART_ID, &part_id)) {
        (void)handle->iic_deinit();
        return 5;
    }
    if (MAX30102_PART_ID_VAL != part_id) {
        (void)handle->iic_deinit();
        return 5;
    }

    /* 复位芯片 */
    if (0U != max30102_reset(handle)) {
        (void)handle->iic_deinit();
        return 4;
    }

    /* 等待复位完成（手册要求至少 1ms） */
    handle->delay_ms(10);

    /* 开启温度传感器 */
    (void)max30102_write_reg8(handle, MAX30102_REG_DIE_TEMP_CONFIG, 0x01U);

    handle->inited = 1U;
    return 0;
}

/**
 * @brief     关闭芯片
 */
uint8_t max30102_deinit(max30102_handle_t *handle)
{
    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    /* 进入关机模式 */
    (void)max30102_set_shutdown(handle, MAX30102_BOOL_TRUE);

    handle->inited = 0U;

    if (NULL != handle->iic_deinit) {
        return handle->iic_deinit();
    }
    return 0;
}

/**
 * @brief     复位芯片
 */
uint8_t max30102_reset(max30102_handle_t *handle)
{
    uint8_t reg_val;
    uint16_t timeout = 1000U;

    if (NULL == handle) {
        return 2;
    }

    /* 设置 RESET 位（写入后硬件自动清零） */
    if (0U != max30102_write_reg8(handle, MAX30102_REG_MODE_CONFIG, MAX30102_MODE_RESET)) {
        return 1;
    }

    /* 等待 RESET 位被硬件清零（表示复位完成） */
    do {
        handle->delay_ms(1);
        if (0U != max30102_read_reg8(handle, MAX30102_REG_MODE_CONFIG, &reg_val)) {
            return 1;
        }
        if (0U == timeout--) {
            return 1;   /* 复位超时 */
        }
    } while (0U != (reg_val & MAX30102_MODE_RESET));

    return 0;
}

/**
 * @brief     设置传感器工作模式
 */
uint8_t max30102_set_mode(max30102_handle_t *handle, max30102_mode_t mode)
{
    uint8_t reg_val;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    /* 先读取当前 MODE_CONFIG 寄存器值，保留 SHDN/RESET 位 */
    if (0U != max30102_read_reg8(handle, MAX30102_REG_MODE_CONFIG, &reg_val)) {
        return 1;
    }

    reg_val &= ~MAX30102_MODE_MODE_MSK;
    reg_val |= (uint8_t)mode;

    return max30102_write_reg8(handle, MAX30102_REG_MODE_CONFIG, reg_val);
}

/**
 * @brief     获取传感器工作模式
 */
uint8_t max30102_get_mode(max30102_handle_t *handle, max30102_mode_t *mode)
{
    uint8_t reg_val;

    if (NULL == handle || NULL == mode) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    if (0U != max30102_read_reg8(handle, MAX30102_REG_MODE_CONFIG, &reg_val)) {
        return 1;
    }

    *mode = (max30102_mode_t)(reg_val & MAX30102_MODE_MODE_MSK);
    return 0;
}

/**
 * @brief     设置 SpO2 配置
 */
uint8_t max30102_set_spo2_config(max30102_handle_t *handle, max30102_adc_range_t adc_range,
                                  max30102_sample_rate_t sample_rate, max30102_pulse_width_t pulse_width)
{
    uint8_t reg_val;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    reg_val  = ((uint8_t)adc_range   << MAX30102_SPO2_ADC_RGE_POS) & MAX30102_SPO2_ADC_RGE_MSK;
    reg_val |= ((uint8_t)sample_rate << MAX30102_SPO2_SR_POS)      & MAX30102_SPO2_SR_MSK;
    reg_val |= ((uint8_t)pulse_width << MAX30102_SPO2_LED_PW_POS)  & MAX30102_SPO2_LED_PW_MSK;

    return max30102_write_reg8(handle, MAX30102_REG_SPO2_CONFIG, reg_val);
}

/**
 * @brief     设置 LED 脉冲幅度
 */
uint8_t max30102_set_led_pulse_amplitude(max30102_handle_t *handle, max30102_led_t led, uint8_t amplitude)
{
    uint8_t reg;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    reg = (MAX30102_LED_RED == led) ? MAX30102_REG_LED1_PULSE_AMP : MAX30102_REG_LED2_PULSE_AMP;
    return max30102_write_reg8(handle, reg, amplitude);
}

/**
 * @brief     配置 FIFO
 */
uint8_t max30102_set_fifo_config(max30102_handle_t *handle, max30102_sample_avg_t sample_avg,
                                  max30102_bool_t roll_over, uint8_t almost_full)
{
    uint8_t reg_val;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    reg_val  = ((uint8_t)sample_avg << MAX30102_FIFO_CFG_SMP_AVE_POS) & MAX30102_FIFO_CFG_SMP_AVE_MSK;

    if (MAX30102_BOOL_TRUE == roll_over) {
        reg_val |= MAX30102_FIFO_CFG_ROLL_OVER_EN;
    }

    reg_val |= (almost_full << MAX30102_FIFO_CFG_A_FULL_POS) & MAX30102_FIFO_CFG_A_FULL_MSK;

    return max30102_write_reg8(handle, MAX30102_REG_FIFO_CONFIG, reg_val);
}

/**
 * @brief     读取芯片温度
 */
uint8_t max30102_read_temperature(max30102_handle_t *handle, float *degrees)
{
    uint8_t temp_int, temp_frac;

    if (NULL == handle || NULL == degrees) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    /* 触发一次温度测量：写入 TEMP_EN = 1 */
    if (0U != max30102_write_reg8(handle, MAX30102_REG_DIE_TEMP_CONFIG, 0x01U)) {
        return 1;
    }

    /* 等待温度测量完成 */
    handle->delay_ms(30);

    /* 读取温度整数部分 */
    if (0U != max30102_read_reg8(handle, MAX30102_REG_DIE_TEMP_INT, &temp_int)) {
        return 1;
    }

    /* 读取温度小数部分（高 4 位有效） */
    if (0U != max30102_read_reg8(handle, MAX30102_REG_DIE_TEMP_FRAC, &temp_frac)) {
        return 1;
    }

    /* 数据手册公式：T = T_int + (T_frac / 16.0) */
    *degrees = (float)((int8_t)temp_int) + ((float)(temp_frac & 0x0FU) / 16.0f);

    return 0;
}

/**
 * @brief     获取 FIFO 写指针
 */
uint8_t max30102_get_fifo_write_pointer(max30102_handle_t *handle, uint8_t *wr_ptr)
{
    if (NULL == handle || NULL == wr_ptr) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    return max30102_read_reg8(handle, MAX30102_REG_FIFO_WR_PTR, wr_ptr);
}

/**
 * @brief     获取 FIFO 读指针
 */
uint8_t max30102_get_fifo_read_pointer(max30102_handle_t *handle, uint8_t *rd_ptr)
{
    if (NULL == handle || NULL == rd_ptr) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    return max30102_read_reg8(handle, MAX30102_REG_FIFO_RD_PTR, rd_ptr);
}

/**
 * @brief     设置 FIFO 读写指针
 */
uint8_t max30102_set_fifo_pointers(max30102_handle_t *handle, uint8_t wr_ptr, uint8_t rd_ptr)
{
    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    if (0U != max30102_write_reg8(handle, MAX30102_REG_FIFO_WR_PTR, wr_ptr)) {
        return 1;
    }

    return max30102_write_reg8(handle, MAX30102_REG_FIFO_RD_PTR, rd_ptr);
}

/**
 * @brief     读取 FIFO 中的采样数据
 * @note      每个样本 = IR[17:0] + RED[17:0]，共 6 字节
 *            字节序列：[IR[15:8], IR[7:0], IR[17:16]<<4 | 4'b0,
 *                      RED[15:8], RED[7:0], RED[17:16]<<4 | 4'b0]
 *_            由于 FIFO_DATA 寄存器支持连续读取，一次性读取 sample_count * 6 字节即可。
 */
uint8_t max30102_read_fifo(max30102_handle_t *handle, max30102_fifo_data_t *buf,
                           uint16_t sample_count, uint16_t *read_count)
{
    uint8_t raw[192]; /* 最大 32 样本 × 6 字节 = 192 */
    uint16_t i, bytes_to_read, samples_to_read;
    uint32_t ir_raw, red_raw;

    if (NULL == handle || NULL == buf || NULL == read_count) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    *read_count = 0U;

    if (0U == sample_count) {
        return 1;
    }

    /* 限制单次读取不超过 FIFO 深度 */
    if (sample_count > MAX30102_FIFO_DEPTH) {
        sample_count = MAX30102_FIFO_DEPTH;
    }

    bytes_to_read = sample_count * MAX30102_FIFO_SAMPLE_BYTES;
    if (bytes_to_read > sizeof(raw)) {
        bytes_to_read = sizeof(raw);
    }
    samples_to_read = bytes_to_read / MAX30102_FIFO_SAMPLE_BYTES;

    /* 一次性连续读取 FIFO_DATA 寄存器（硬件自动递增内部指针） */
    if (0U != max30102_read_reg(handle, MAX30102_REG_FIFO_DATA, raw, bytes_to_read)) {
        return 1;
    }

    /* 解析样本数据 */
    for (i = 0U; i < samples_to_read; i++) {
        uint8_t *p = &raw[i * MAX30102_FIFO_SAMPLE_BYTES];

        /* IR 通道：3 字节 → 18 位 */
        ir_raw  = ((uint32_t)p[0] << 16) & 0x00030000UL;
        ir_raw |= ((uint32_t)p[1] << 8);
        ir_raw |=  (uint32_t)p[2];
        ir_raw &= 0x0003FFFFUL;

        /* RED 通道：3 字节 → 18 位 */
        red_raw  = ((uint32_t)p[3] << 16) & 0x00030000UL;
        red_raw |= ((uint32_t)p[4] << 8);
        red_raw |=  (uint32_t)p[5];
        red_raw &= 0x0003FFFFUL;

        buf[i].ir_value  = ir_raw;
        buf[i].red_value = red_raw;
    }

    *read_count = samples_to_read;
    return 0;
}

/**
 * @brief     使能或禁用中断
 */
uint8_t max30102_set_interrupt(max30102_handle_t *handle, max30102_interrupt_t interrupt,
                                max30102_bool_t enable)
{
    uint8_t reg_addr, bit_pos, reg_val;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    /* 中断 1 寄存器（A_FULL, PPG_RDY, ALC_OVF）和中断 2 寄存器（DIE_TEMP_RDY） */
    switch (interrupt) {
    case MAX30102_INTERRUPT_FIFO_FULL:
        reg_addr = MAX30102_REG_INT_ENABLE_1;
        bit_pos  = 7U;
        break;
    case MAX30102_INTERRUPT_PPG_RDY:
        reg_addr = MAX30102_REG_INT_ENABLE_1;
        bit_pos  = 6U;
        break;
    case MAX30102_INTERRUPT_ALC_OVF:
        reg_addr = MAX30102_REG_INT_ENABLE_1;
        bit_pos  = 5U;
        break;
    case MAX30102_INTERRUPT_DIE_TEMP:
        reg_addr = MAX30102_REG_INT_ENABLE_2;
        bit_pos  = 1U;
        break;
    default:
        return 1;   /* 不支持的中断类型 */
    }

    if (0U != max30102_read_reg8(handle, reg_addr, &reg_val)) {
        return 1;
    }

    if (MAX30102_BOOL_TRUE == enable) {
        reg_val |= (uint8_t)(1U << bit_pos);
    } else {
        reg_val &= (uint8_t)(~(1U << bit_pos));
    }

    return max30102_write_reg8(handle, reg_addr, reg_val);
}

/**
 * @brief     获取中断状态
 */
uint8_t max30102_get_interrupt_status(max30102_handle_t *handle, uint8_t *status1, uint8_t *status2)
{
    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    if (NULL != status1) {
        if (0U != max30102_read_reg8(handle, MAX30102_REG_INT_STATUS_1, status1)) {
            return 1;
        }
    }
    if (NULL != status2) {
        if (0U != max30102_read_reg8(handle, MAX30102_REG_INT_STATUS_2, status2)) {
            return 1;
        }
    }

    return 0;
}

/**
 * @brief     进入或退出关机模式
 */
uint8_t max30102_set_shutdown(max30102_handle_t *handle, max30102_bool_t shutdown)
{
    uint8_t reg_val;

    if (NULL == handle) {
        return 2;
    }

    if (0U != max30102_read_reg8(handle, MAX30102_REG_MODE_CONFIG, &reg_val)) {
        return 1;
    }

    if (MAX30102_BOOL_TRUE == shutdown) {
        reg_val |= MAX30102_MODE_SHDN;
    } else {
        reg_val &= ~MAX30102_MODE_SHDN;
    }

    return max30102_write_reg8(handle, MAX30102_REG_MODE_CONFIG, reg_val);
}

/**
 * @brief     IRQ 处理函数
 */
uint8_t max30102_irq_handler(max30102_handle_t *handle)
{
    uint8_t status1, status2;

    if (NULL == handle) {
        return 2;
    }
    if (0U == handle->inited) {
        return 3;
    }

    /* 读取中断状态 */
    if (0U != max30102_get_interrupt_status(handle, &status1, &status2)) {
        return 1;
    }

    /* 回调通知上层应用 */
    if (NULL != handle->receive_callback) {
        if (0U != (status1 & MAX30102_INT1_PPG_RDY)) {
            handle->receive_callback(MAX30102_INTERRUPT_PPG_RDY);
        }
        if (0U != (status1 & MAX30102_INT1_A_FULL)) {
            handle->receive_callback(MAX30102_INTERRUPT_FIFO_FULL);
        }
        if (0U != (status1 & MAX30102_INT1_ALC_OVF)) {
            handle->receive_callback(MAX30102_INTERRUPT_ALC_OVF);
        }
        if (0U != (status2 & MAX30102_INT2_DIE_TEMP_RDY)) {
            handle->receive_callback(MAX30102_INTERRUPT_DIE_TEMP);
        }
    }

    return 0;
}
