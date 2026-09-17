/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_sht30_interface.h
 * @brief     driver sht30 接口头文件
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_SHT30_INTERFACE_H
#define DRIVER_SHT30_INTERFACE_H

#include "driver_sht30.h"

#ifdef __cplusplus
extern "C"{
#endif

uint8_t sht30_interface_iic_init(void);
uint8_t sht30_interface_iic_deinit(void);
uint8_t sht30_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t sht30_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
void    sht30_interface_delay_ms(uint32_t ms);
void    sht30_interface_debug_print(const char *const fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SHT30_INTERFACE_H */
