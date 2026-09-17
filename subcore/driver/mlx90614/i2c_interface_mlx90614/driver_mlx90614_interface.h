/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_mlx90614_interface.h
 * @brief     driver mlx90614 接口头文件
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_MLX90614_INTERFACE_H
#define DRIVER_MLX90614_INTERFACE_H

#include "driver_mlx90614.h"

#ifdef __cplusplus
extern "C"{
#endif

uint8_t mlx90614_interface_iic_init(void);
uint8_t mlx90614_interface_iic_deinit(void);
uint8_t mlx90614_interface_iic_read(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
uint8_t mlx90614_interface_iic_write(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
void    mlx90614_interface_delay_ms(uint32_t ms);
void    mlx90614_interface_debug_print(const char *const fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_MLX90614_INTERFACE_H */
