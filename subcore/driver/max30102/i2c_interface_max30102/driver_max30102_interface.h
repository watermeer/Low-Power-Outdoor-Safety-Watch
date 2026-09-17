/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 * ... (许可证文本同 header)
 *
 * @file      driver_max30102_interface.h
 * @brief     driver max30102 接口头文件
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_MAX30102_INTERFACE_H
#define DRIVER_MAX30102_INTERFACE_H

#include "driver_max30102.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @defgroup max30102_interface_driver max30102 接口驱动
 * @{
 */

uint8_t max30102_interface_iic_init(void);
uint8_t max30102_interface_iic_deinit(void);
uint8_t max30102_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t max30102_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
void    max30102_interface_delay_ms(uint32_t ms);
void    max30102_interface_debug_print(const char *const fmt, ...);
void    max30102_interface_receive_callback(uint8_t type);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_MAX30102_INTERFACE_H */
