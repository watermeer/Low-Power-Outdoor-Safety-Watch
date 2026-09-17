/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 * ... (许可证文本)
 *
 * @file      driver_mlx90614.h
 * @brief     driver mlx90614 头文件（红外非接触体温传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_MLX90614_H
#define DRIVER_MLX90614_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @addtogroup mlx90614_basic_driver
 * @{
 */

/**
 * @brief MLX90614 I2C 设备地址
 * @note  出厂默认地址为 0x5A（7 位），8 位写地址为 0xB4。
 *        可通过 EEPROM 编程修改。
 */
typedef enum
{
    MLX90614_ADDRESS_DEFAULT = 0xB4,       /**< 默认 8 位写地址（7 位 = 0x5A） */
} mlx90614_address_t;

/**
 * @brief MLX90614 温度数据源选择枚举
 */
typedef enum
{
    MLX90614_TEMP_AMBIENT   = 0x06,        /**< 环境温度（芯片自身温度） */
    MLX90614_TEMP_OBJECT1   = 0x07,        /**< 物体 1 温度（红外传感器视野内） */
    MLX90614_TEMP_OBJECT2   = 0x08,        /**< 物体 2 温度（双视野型号专用） */
} mlx90614_temp_source_t;

/**
 * @brief MLX90614 布尔枚举
 */
typedef enum
{
    MLX90614_BOOL_FALSE = 0x00,
    MLX90614_BOOL_TRUE  = 0x01,
} mlx90614_bool_t;

/**
 * @brief MLX90614 工作模式枚举
 */
typedef enum
{
    MLX90614_MODE_NORMAL     = 0x00,       /**< 正常模式（功耗约 2.5 mA） */
    MLX90614_MODE_SLEEP      = 0x01,       /**< 休眠模式（功耗约 2 μA，需 SCL 唤醒） */
} mlx90614_mode_t;

/**
 * @}
 */

/* EEPROM 寄存器地址 */
#define MLX90614_EEPROM_TOMAX        0x00U  /* 物体温度上限 */
#define MLX90614_EEPROM_TOMIN        0x01U  /* 物体温度下限 */
#define MLX90614_EEPROM_PWMCTRL      0x02U  /* PWM 控制 */
#define MLX90614_EEPROM_TARANGE      0x03U  /* 环境温度范围 */
#define MLX90614_EEPROM_EMISSIVITY   0x04U  /* 发射率（出厂 1.0 = 0xFFFF） */
#define MLX90614_EEPROM_CONFIG1      0x05U  /* 配置寄存器 1 */
#define MLX90614_EEPROM_SMBUS_ADDR   0x0EU  /* SMBus 地址（可修改） */
#define MLX90614_EEPROM_ID1          0x1CU  /* 器件 ID 高 16 位 */
#define MLX90614_EEPROM_ID2          0x1DU  /* 器件 ID 低 16 位 */
#define MLX90614_EEPROM_ID3          0x1EU  /* 器件 ID 低 16 位（续） */
#define MLX90614_EEPROM_ID4          0x1FU  /* 器件 ID 低 16 位（续） */

/* RAM 寄存器地址 */
#define MLX90614_RAM_AMBIENT_TEMP    0x06U  /* 环境温度 */
#define MLX90614_RAM_OBJECT1_TEMP    0x07U  /* 物体 1 红外温度 */
#define MLX90614_RAM_OBJECT2_TEMP    0x08U  /* 物体 2 红外温度 */
#define MLX90614_RAM_RAW_IR1         0x04U  /* 原始红外数据通道 1 */
#define MLX90614_RAM_RAW_IR2         0x05U  /* 原始红外数据通道 2 */

/* EEPROM 读取命令 */
#define MLX90614_CMD_EEPROM_BASE     0x20U  /* EEPROM 地址 = 0x20 | eeprom_addr */
#define MLX90614_CMD_SLEEP           0xFFU  /* 进入休眠模式 */
#define MLX90614_CMD_WAKEUP          0x00U  /* 唤醒（任意数据写入） */

/**
 * @brief MLX90614 操作句柄结构体
 */
typedef struct mlx90614_handle_s
{
    uint8_t iic_addr;                                                              /**< IIC 设备地址 */
    uint8_t (*iic_init)(void);                                                     /**< IIC 初始化 */
    uint8_t (*iic_deinit)(void);                                                   /**< IIC 反初始化 */
    uint8_t (*iic_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);    /**< IIC 读取 */
    uint8_t (*iic_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);   /**< IIC 写入 */
    void (*delay_ms)(uint32_t ms);                                                 /**< 毫秒延时 */
    void (*debug_print)(const char *const fmt, ...);                               /**< 调试打印 */
    uint8_t inited;                                                                /**< 初始化标志 */
} mlx90614_handle_t;

/**
 * @brief MLX90614 器件信息结构体
 */
typedef struct mlx90614_info_s
{
    char chip_name[32];
    char manufacturer_name[32];
    char interface[8];
    float supply_voltage_min_v;
    float supply_voltage_max_v;
    float max_current_ma;
    float temperature_min;
    float temperature_max;
    uint32_t driver_version;
} mlx90614_info_t;

/* ===== 链接宏 ===== */
#define DRIVER_MLX90614_LINK_INIT(HANDLE, STRUCTURE)         memset(HANDLE, 0, sizeof(STRUCTURE))
#define DRIVER_MLX90614_LINK_IIC_INIT(HANDLE, FUC)           (HANDLE)->iic_init = FUC
#define DRIVER_MLX90614_LINK_IIC_DEINIT(HANDLE, FUC)         (HANDLE)->iic_deinit = FUC
#define DRIVER_MLX90614_LINK_IIC_READ(HANDLE, FUC)           (HANDLE)->iic_read = FUC
#define DRIVER_MLX90614_LINK_IIC_WRITE(HANDLE, FUC)          (HANDLE)->iic_write = FUC
#define DRIVER_MLX90614_LINK_DELAY_MS(HANDLE, FUC)           (HANDLE)->delay_ms = FUC
#define DRIVER_MLX90614_LINK_DEBUG_PRINT(HANDLE, FUC)        (HANDLE)->debug_print = FUC

/* ===== API 函数声明 ===== */

uint8_t mlx90614_info(mlx90614_info_t *info);
uint8_t mlx90614_set_addr_pin(mlx90614_handle_t *handle, mlx90614_address_t addr_pin);
uint8_t mlx90614_init(mlx90614_handle_t *handle);
uint8_t mlx90614_deinit(mlx90614_handle_t *handle);

/**
 * @brief     读取温度值
 * @param[in]  *handle      指向操作句柄的指针
 * @param[in]  source       温度来源（环境 / 物体 1 / 物体 2）
 * @param[out] *degrees     温度值（摄氏度）
 * @return     状态码 - 0 成功，1 读取失败，2 句柄空，3 未初始化
 */
uint8_t mlx90614_read_temperature(mlx90614_handle_t *handle, mlx90614_temp_source_t source, float *degrees);

/**
 * @brief     同时读取环境和物体温度
 * @param[out] *ambient 环境温度（摄氏度）
 * @param[out] *object  物体温度（摄氏度）
 */
uint8_t mlx90614_read_both(mlx90614_handle_t *handle, float *ambient, float *object);

/**
 * @brief     从 EEPROM 中读取一个字（16 位）
 * @note      用于读取发射率、ID 等出厂参数
 */
uint8_t mlx90614_read_eeprom_word(mlx90614_handle_t *handle, uint8_t eeprom_addr, uint16_t *data);

/**
 * @brief     设置工作模式（正常 / 休眠）
 */
uint8_t mlx90614_set_mode(mlx90614_handle_t *handle, mlx90614_mode_t mode);

/**
 * @brief     获取器件 ID
 */
uint8_t mlx90614_get_id(mlx90614_handle_t *handle, uint64_t *id);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_MLX90614_H */
