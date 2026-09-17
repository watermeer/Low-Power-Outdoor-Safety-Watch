/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_sht30.c
 * @brief     driver sht30 源文件（温湿度传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_sht30.h"

/* ===== 常量 ===== */

#define SHT30_MEASUREMENT_BYTES     6U         /* 每次测量返回 6 字节 */
#define SHT30_CRC_POLYNOMIAL        0x31U      /* CRC-8: x^8 + x^5 + x^4 + 1 */
#define SHT30_CRC_INIT              0xFFU      /* CRC 初始值 */
#define SHT30_SOFT_RESET_DELAY_MS   2U         /* 软复位后等待时间 */

/* ===== 内部辅助函数 ===== */

/**
 * @brief     发送 16 位命令到 SHT30
 * @note      SHT30 是纯命令式传感器，没有寄存器概念。
 *            利用 iic_write 的 reg 参数发送命令高字节，
 *            利用 buf[0] 发送命令低字节。
 */
static uint8_t sht30_send_command(sht30_handle_t *handle, uint16_t command)
{
    uint8_t cmd_lo = (uint8_t)(command & 0xFFU);

    if ((NULL == handle) || (NULL == handle->iic_write)) {
        return 1;
    }

    /* reg=cmd_hi, buf[0]=cmd_lo，硬件上即为 I2C 写入 2 字节命令 */
    return handle->iic_write(handle->iic_addr, (uint8_t)(command >> 8), &cmd_lo, 1U);
}

/**
 * @brief     计算 CRC-8（Sensirion 多项式）
 * @param[in] data 数据指针
 * @param[in] len  数据长度
 * @return     CRC-8 校验值
 */
static uint8_t sht30_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = SHT30_CRC_INIT;
    uint8_t i, j;

    for (i = 0U; i < len; i++) {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1) ^ SHT30_CRC_POLYNOMIAL);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief     验证测量数据的 CRC
 * @return     0 验证通过，1 校验失败
 */
static uint8_t sht30_verify_crc(const uint8_t *raw, float *temperature, float *humidity)
{
    uint16_t raw_temp, raw_humi;

    /* 检查温度 CRC */
    if (raw[2] != sht30_crc8(&raw[0], 2U)) {
        return 1;
    }
    /* 检查湿度 CRC */
    if (raw[5] != sht30_crc8(&raw[3], 2U)) {
        return 1;
    }

    raw_temp = ((uint16_t)raw[0] << 8) | (uint16_t)raw[1];
    raw_humi = ((uint16_t)raw[3] << 8) | (uint16_t)raw[4];

    /* 数据手册公式 */
    if (NULL != temperature) {
        *temperature = -45.0f + 175.0f * ((float)raw_temp / 65535.0f);
    }
    if (NULL != humidity) {
        *humidity = 100.0f * ((float)raw_humi / 65535.0f);
    }

    return 0;
}

/* ===== 公开 API ===== */

uint8_t sht30_info(sht30_info_t *info)
{
    if (NULL == info) { return 2; }
    memset(info, 0, sizeof(sht30_info_t));
    strncpy(info->chip_name,         "SHT30", 31);
    strncpy(info->manufacturer_name, "Sensirion", 31);
    strncpy(info->interface,         "IIC", 7);
    info->supply_voltage_min_v = 2.4f;
    info->supply_voltage_max_v = 5.5f;
    info->max_current_ma       = 1.5f;
    info->temperature_min      = -40.0f;
    info->temperature_max      = 125.0f;
    info->driver_version       = 0x00010000U;
    return 0;
}

uint8_t sht30_set_addr_pin(sht30_handle_t *handle, sht30_address_t addr_pin)
{
    if (NULL == handle) { return 2; }
    handle->iic_addr = (uint8_t)addr_pin;
    return 0;
}

uint8_t sht30_init(sht30_handle_t *handle)
{
    if (NULL == handle) { return 2; }
    if (NULL == handle->iic_init || NULL == handle->iic_read || NULL == handle->iic_write) {
        return 3;
    }

    if (0U != handle->iic_init()) { return 1; }

    /* 软复位，确保传感器处于已知状态 */
    if (0U != sht30_soft_reset(handle)) {
        (void)handle->iic_deinit();
        return 4;
    }

    handle->repeatability = SHT30_REPEATABILITY_HIGH;
    handle->inited = 1U;
    return 0;
}

uint8_t sht30_deinit(sht30_handle_t *handle)
{
    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }
    handle->inited = 0U;
    if (NULL != handle->iic_deinit) { return handle->iic_deinit(); }
    return 0;
}

uint8_t sht30_set_repeatability(sht30_handle_t *handle, sht30_repeatability_t repeatability)
{
    if (NULL == handle) { return 2; }
    handle->repeatability = repeatability;
    return 0;
}

uint8_t sht30_soft_reset(sht30_handle_t *handle)
{
    if (NULL == handle) { return 2; }
    if (0U != sht30_send_command(handle, SHT30_CMD_SOFT_RESET)) { return 1; }
    handle->delay_ms(SHT30_SOFT_RESET_DELAY_MS);
    return 0;
}

uint8_t sht30_set_heater(sht30_handle_t *handle, sht30_bool_t enable)
{
    uint16_t cmd;

    if (NULL == handle) { return 2; }
    cmd = (SHT30_BOOL_TRUE == enable) ? SHT30_CMD_HEATER_ENABLE : SHT30_CMD_HEATER_DISABLE;
    return sht30_send_command(handle, cmd);
}

uint8_t sht30_read_status(sht30_handle_t *handle, uint16_t *status)
{
    uint8_t raw[3]; /* 2 字节数据 + 1 字节 CRC */

    if (NULL == handle || NULL == status) { return 2; }
    if (0U != sht30_send_command(handle, SHT30_CMD_READ_STATUS)) { return 1; }
    /* 读取状态寄存器：SHT30 直接读取，无需寄存器地址 */
    if (0U != handle->iic_read(handle->iic_addr, 0U, raw, 3U)) { return 1; }

    /* 验证 CRC */
    if (raw[2] != sht30_crc8(raw, 2U)) { return 1; }
    *status = ((uint16_t)raw[0] << 8) | (uint16_t)raw[1];
    return 0;
}

uint8_t sht30_read_temperature_humidity(sht30_handle_t *handle, float *temperature, float *humidity)
{
    uint8_t raw[SHT30_MEASUREMENT_BYTES];
    uint16_t cmd;

    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* 根据重复性选择单次测量命令（含时钟拉伸） */
    switch (handle->repeatability) {
    case SHT30_REPEATABILITY_LOW:
        cmd = SHT30_CMD_SINGLE_L_HOLD;  break;
    case SHT30_REPEATABILITY_MEDIUM:
        cmd = SHT30_CMD_SINGLE_M_HOLD;  break;
    case SHT30_REPEATABILITY_HIGH:
    default:
        cmd = SHT30_CMD_SINGLE_H_HOLD;  break;
    }

    /* 发送测量命令 */
    if (0U != sht30_send_command(handle, cmd)) { return 1; }

    /* SHT30 在时钟拉伸模式下会保持 SCL 低电平直到测量完成，
     * 因此可以在发送命令后立即读取，硬件会自动等待测量完成。 */
    if (0U != handle->iic_read(handle->iic_addr, 0U, raw, SHT30_MEASUREMENT_BYTES)) {
        return 1;
    }

    /* CRC 校验并转换数据 */
    return sht30_verify_crc(raw, temperature, humidity);
}

uint8_t sht30_start_periodic(sht30_handle_t *handle, sht30_mode_t mode, sht30_repeatability_t repeatability)
{
    uint16_t cmd;

    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }

    cmd = (uint16_t)(((uint16_t)mode << 8) | (uint16_t)repeatability);
    if (0U != sht30_send_command(handle, cmd)) { return 1; }

    handle->repeatability = repeatability;
    return 0;
}

uint8_t sht30_stop_periodic(sht30_handle_t *handle)
{
    if (NULL == handle) { return 2; }
    return sht30_send_command(handle, SHT30_CMD_BREAK);
}

uint8_t sht30_fetch_data(sht30_handle_t *handle, float *temperature, float *humidity)
{
    uint8_t raw[SHT30_MEASUREMENT_BYTES];

    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* 在周期模式下使用 FETCH_DATA 命令获取数据 */
    if (0U != sht30_send_command(handle, SHT30_CMD_FETCH_DATA)) { return 1; }
    if (0U != handle->iic_read(handle->iic_addr, 0U, raw, SHT30_MEASUREMENT_BYTES)) {
        return 1;
    }

    return sht30_verify_crc(raw, temperature, humidity);
}
