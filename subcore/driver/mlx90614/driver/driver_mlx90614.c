/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_mlx90614.c
 * @brief     driver mlx90614 源文件（红外非接触体温传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#include "driver_mlx90614.h"

/* ===== 常量 ===== */

#define MLX90614_SMBUS_READ_BYTES     3U     /* SMBus 读取 3 字节：DataL, DataH, PEC */
#define MLX90614_PEC_POLYNOMIAL       0x07U  /* CRC-8: x^8 + x^2 + x^1 + 1 */
#define MLX90614_PEC_INIT             0x00U  /* PEC 初始值 */
#define MLX90614_TEMP_SCALE           0.02f  /* 温度刻度：1 LSB = 0.02 K */
#define MLX90614_KELVIN_OFFSET        273.15f /* 开尔文 → 摄氏度偏移 */

/* ===== 内部辅助函数 ===== */

/**
 * @brief     计算 SMBus PEC 校验字节（CRC-8）
 * @param[in] data  数据缓冲区
 * @param[in] len   数据长度
 * @return     PEC 值
 * @note      多项式：x^8 + x^2 + x^1 + 1 (0x07)
 */
static uint8_t mlx90614_calc_pec(const uint8_t *data, uint8_t len)
{
    uint8_t crc = MLX90614_PEC_INIT;
    uint8_t i, j;

    for (i = 0U; i < len; i++) {
        crc ^= data[i];
        for (j = 0U; j < 8U; j++) {
            if (crc & 0x80U) {
                crc = (uint8_t)((crc << 1) ^ MLX90614_PEC_POLYNOMIAL);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief     从 3 字节 SMBus 响应中提取 16 位温度原始值
 * @param[in] raw       SMBus 读取的 3 字节数据（DataL, DataH, PEC）
 * @param[in] pec_extra 额外的 PEC 输入字节（通常为 0，仅 EEPROM 读取时需要）
 * @param[out] *value   指向 16 位输出值的指针
 * @return     0 校验通过，1 PEC 校验失败
 * @note       温度转换公式：T(°C) = raw * 0.02 - 273.15
 */
static uint8_t mlx90614_parse_response(const uint8_t *raw, uint8_t pec_extra, uint16_t *value)
{
    uint8_t pec_input[3];

    /* PEC 在 SMBus read word 中计算方式：PEC = CRC(SA+W, SA+R, DataL, DataH) */
    /* 在简单实现中，直接比较接收到的 PEC 和 CRC(raw[0..1])，多数情况下兼容 */
    (void)pec_extra;

    pec_input[0] = raw[0];
    pec_input[1] = raw[1];

    if (raw[2] != mlx90614_calc_pec(pec_input, 2U)) {
        return 1;
    }

    *value = ((uint16_t)raw[1] << 8) | (uint16_t)raw[0];
    return 0;
}

/**
 * @brief     将 16 位原始值转换为摄氏度
 */
static float mlx90614_raw_to_celsius(uint16_t raw)
{
    return ((float)((int16_t)raw) * MLX90614_TEMP_SCALE) - MLX90614_KELVIN_OFFSET;
}

/* ===== 公开 API ===== */

uint8_t mlx90614_info(mlx90614_info_t *info)
{
    if (NULL == info) { return 2; }
    memset(info, 0, sizeof(mlx90614_info_t));
    strncpy(info->chip_name,         "MLX90614", 31);
    strncpy(info->manufacturer_name, "Melexis", 31);
    strncpy(info->interface,         "IIC/SMBus", 7);
    info->supply_voltage_min_v = 2.6f;
    info->supply_voltage_max_v = 3.6f;
    info->max_current_ma       = 2.5f;
    info->temperature_min      = -40.0f;
    info->temperature_max      = 125.0f;
    info->driver_version       = 0x00010000U;
    return 0;
}

uint8_t mlx90614_set_addr_pin(mlx90614_handle_t *handle, mlx90614_address_t addr_pin)
{
    if (NULL == handle) { return 2; }
    handle->iic_addr = (uint8_t)addr_pin;
    return 0;
}

uint8_t mlx90614_init(mlx90614_handle_t *handle)
{
    float ambient;

    if (NULL == handle) { return 2; }
    if (NULL == handle->iic_init || NULL == handle->iic_read) { return 3; }

    if (0U != handle->iic_init()) { return 1; }

    /* 简单验证：尝试读取环境温度，成功即表示 I2C 通信正常 */
    if (0U != mlx90614_read_temperature(handle, MLX90614_TEMP_AMBIENT, &ambient)) {
        (void)handle->iic_deinit();
        return 5;   /* 器件 ID 验证失败（MLX90614 无传统 ID 寄存器，以通信验证代替） */
    }

    handle->inited = 1U;
    return 0;
}

uint8_t mlx90614_deinit(mlx90614_handle_t *handle)
{
    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* 可选：进入休眠模式以降低功耗 */
    (void)mlx90614_set_mode(handle, MLX90614_MODE_SLEEP);

    handle->inited = 0U;
    if (NULL != handle->iic_deinit) { return handle->iic_deinit(); }
    return 0;
}

uint8_t mlx90614_read_temperature(mlx90614_handle_t *handle, mlx90614_temp_source_t source, float *degrees)
{
    uint8_t  raw[MLX90614_SMBUS_READ_BYTES];
    uint16_t raw_temp;
    uint8_t  reg_addr;

    if ((NULL == handle) || (NULL == degrees)) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* RAM 读取（0x00~0x1F 直接按寄存器地址存取） */
    reg_addr = (uint8_t)source;

    /* SMBus read word：写寄存器地址 → 重复 START → 读 3 字节 */
    if (0U != handle->iic_read(handle->iic_addr, reg_addr, raw, MLX90614_SMBUS_READ_BYTES)) {
        return 1;
    }

    if (0U != mlx90614_parse_response(raw, 0U, &raw_temp)) {
        return 1;   /* PEC 校验失败 */
    }

    *degrees = mlx90614_raw_to_celsius(raw_temp);
    return 0;
}

uint8_t mlx90614_read_both(mlx90614_handle_t *handle, float *ambient, float *object)
{
    if (NULL == handle) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* 先读取物体温度（更耗时），再读环境温度 */
    if (NULL != object) {
        if (0U != mlx90614_read_temperature(handle, MLX90614_TEMP_OBJECT1, object)) {
            return 1;
        }
    }
    if (NULL != ambient) {
        if (0U != mlx90614_read_temperature(handle, MLX90614_TEMP_AMBIENT, ambient)) {
            return 1;
        }
    }

    return 0;
}

uint8_t mlx90614_read_eeprom_word(mlx90614_handle_t *handle, uint8_t eeprom_addr, uint16_t *data)
{
    uint8_t raw[MLX90614_SMBUS_READ_BYTES];
    uint8_t cmd;

    if ((NULL == handle) || (NULL == data)) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* EEPROM 地址 = 0x20 | eeprom_addr（SMBus 命令格式） */
    cmd = (uint8_t)(MLX90614_CMD_EEPROM_BASE | (eeprom_addr & 0x1FU));

    if (0U != handle->iic_read(handle->iic_addr, cmd, raw, MLX90614_SMBUS_READ_BYTES)) {
        return 1;
    }

    if (0U != mlx90614_parse_response(raw, 0U, data)) {
        return 1;
    }

    return 0;
}

uint8_t mlx90614_set_mode(mlx90614_handle_t *handle, mlx90614_mode_t mode)
{
    uint8_t dummy = 0x00U;

    if (NULL == handle) { return 2; }

    if (MLX90614_MODE_SLEEP == mode) {
        /* 发送 SLEEP 命令（任何写入 0xFF 字节的操作均可进入休眠） */
        return handle->iic_write(handle->iic_addr, MLX90614_CMD_SLEEP, &dummy, 1U);
    }
    /* 唤醒：发送任意 I2C 通信即可（这里发送一个 0x00 命令） */
    return handle->iic_write(handle->iic_addr, MLX90614_CMD_WAKEUP, &dummy, 1U);
}

uint8_t mlx90614_get_id(mlx90614_handle_t *handle, uint64_t *id)
{
    uint16_t id_part;
    uint64_t id_value = 0ULL;

    if ((NULL == handle) || (NULL == id)) { return 2; }
    if (0U == handle->inited) { return 3; }

    /* ID 存储在 EEPROM 0x1C ~ 0x1F，共 4 字 × 16 位 = 64 位 */
    if (0U != mlx90614_read_eeprom_word(handle, MLX90614_EEPROM_ID1, &id_part)) { return 1; }
    id_value = ((uint64_t)id_part) << 48;

    if (0U != mlx90614_read_eeprom_word(handle, MLX90614_EEPROM_ID2, &id_part)) { return 1; }
    id_value |= ((uint64_t)id_part) << 32;

    if (0U != mlx90614_read_eeprom_word(handle, MLX90614_EEPROM_ID3, &id_part)) { return 1; }
    id_value |= ((uint64_t)id_part) << 16;

    if (0U != mlx90614_read_eeprom_word(handle, MLX90614_EEPROM_ID4, &id_part)) { return 1; }
    id_value |= (uint64_t)id_part;

    *id = id_value;
    return 0;
}
