/**
 * Copyright (c) 2015 - present LibDriver All rights reserved
 *
 * The MIT License (MIT)
 * ... (许可证文本)
 *
 * @file      driver_sht30.h
 * @brief     driver sht30 头文件（温湿度传感器）
 * @version   1.0.0
 * @author    LibDriver
 * @date      2026-07-07
 */

#ifndef DRIVER_SHT30_H
#define DRIVER_SHT30_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @addtogroup sht30_basic_driver
 * @{
 */

/**
 * @brief SHT30 I2C 设备地址枚举
 */
typedef enum
{
    SHT30_ADDRESS_ADDR_LOW  = 0x88,        /**< ADDR 引脚接低电平 */
    SHT30_ADDRESS_ADDR_HIGH = 0x8A,        /**< ADDR 引脚接高电平 */
} sht30_address_t;

/**
 * @brief SHT30 测量重复性枚举
 */
typedef enum
{
    SHT30_REPEATABILITY_HIGH   = 0x00,     /**< 高重复性，测量时间最长 */
    SHT30_REPEATABILITY_MEDIUM = 0x01,     /**< 中等重复性 */
    SHT30_REPEATABILITY_LOW    = 0x02,     /**< 低重复性，测量时间最短 */
} sht30_repeatability_t;

/**
 * @brief SHT30 测量模式枚举
 */
typedef enum
{
    SHT30_MODE_SINGLE_SHOT      = 0x00,    /**< 单次测量模式 */
    SHT30_MODE_PERIODIC_0_5_MPS = 0x20,    /**< 周期测量 0.5 mps */
    SHT30_MODE_PERIODIC_1_MPS   = 0x21,    /**< 周期测量 1 mps */
    SHT30_MODE_PERIODIC_2_MPS   = 0x22,    /**< 周期测量 2 mps */
    SHT30_MODE_PERIODIC_4_MPS   = 0x23,    /**< 周期测量 4 mps */
    SHT30_MODE_PERIODIC_10_MPS  = 0x27,    /**< 周期测量 10 mps */
} sht30_mode_t;

/**
 * @brief SHT30 布尔枚举
 */
typedef enum
{
    SHT30_BOOL_FALSE = 0x00,
    SHT30_BOOL_TRUE  = 0x01,
} sht30_bool_t;

/**
 * @}
 */

/**
 * @brief SHT30 命令码（16 位，数据手册 §4.5）
 */
#define SHT30_CMD_FETCH_DATA             0xE000U   /* 周期模式下读取测量结果 */
#define SHT30_CMD_BREAK                  0x3093U   /* 停止周期测量模式 */
#define SHT30_CMD_SOFT_RESET             0x30A2U   /* 软复位 */
#define SHT30_CMD_HEATER_ENABLE          0x306DU   /* 使能加热器 */
#define SHT30_CMD_HEATER_DISABLE         0x3066U   /* 禁用加热器 */
#define SHT30_CMD_READ_STATUS            0xF32DU   /* 读取状态寄存器 */
#define SHT30_CMD_CLEAR_STATUS           0x3041U   /* 清除状态寄存器 */

/* 单次测量命令（时钟拉伸使能） */
#define SHT30_CMD_SINGLE_H_HOLD          0x2C06U   /* 高重复性 + 时钟拉伸 */
#define SHT30_CMD_SINGLE_M_HOLD          0x2C0DU   /* 中重复性 + 时钟拉伸 */
#define SHT30_CMD_SINGLE_L_HOLD          0x2C10U   /* 低重复性 + 时钟拉伸 */

/* 单次测量命令（时钟拉伸禁用，需手动延时等待） */
#define SHT30_CMD_SINGLE_H_NOHOLD        0x2400U   /* 高重复性 */
#define SHT30_CMD_SINGLE_M_NOHOLD        0x240BU   /* 中重复性 */
#define SHT30_CMD_SINGLE_L_NOHOLD        0x2416U   /* 低重复性 */

/**
 * @brief SHT30 操作句柄结构体
 */
typedef struct sht30_handle_s
{
    uint8_t iic_addr;                                                              /**< IIC 设备地址 */
    uint8_t (*iic_init)(void);                                                     /**< IIC 初始化函数指针 */
    uint8_t (*iic_deinit)(void);                                                   /**< IIC 反初始化函数指针 */
    uint8_t (*iic_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);    /**< IIC 读取函数指针（注：SHT30 reg 参数忽略，为直接读取） */
    uint8_t (*iic_write)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);   /**< IIC 写入函数指针 */
    void (*delay_ms)(uint32_t ms);                                                 /**< 毫秒延时函数指针 */
    void (*debug_print)(const char *const fmt, ...);                               /**< 调试打印函数指针 */
    uint8_t inited;                                                                /**< 初始化标志 */
    sht30_repeatability_t repeatability;                                           /**< 当前重复性设置 */
} sht30_handle_t;

/**
 * @brief SHT30 器件信息结构体
 */
typedef struct sht30_info_s
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
} sht30_info_t;

/* ===== 链接宏 ===== */
#define DRIVER_SHT30_LINK_INIT(HANDLE, STRUCTURE)         memset(HANDLE, 0, sizeof(STRUCTURE))
#define DRIVER_SHT30_LINK_IIC_INIT(HANDLE, FUC)           (HANDLE)->iic_init = FUC
#define DRIVER_SHT30_LINK_IIC_DEINIT(HANDLE, FUC)         (HANDLE)->iic_deinit = FUC
#define DRIVER_SHT30_LINK_IIC_READ(HANDLE, FUC)           (HANDLE)->iic_read = FUC
#define DRIVER_SHT30_LINK_IIC_WRITE(HANDLE, FUC)          (HANDLE)->iic_write = FUC
#define DRIVER_SHT30_LINK_DELAY_MS(HANDLE, FUC)           (HANDLE)->delay_ms = FUC
#define DRIVER_SHT30_LINK_DEBUG_PRINT(HANDLE, FUC)        (HANDLE)->debug_print = FUC

/* ===== API 声明 ===== */

uint8_t sht30_info(sht30_info_t *info);
uint8_t sht30_set_addr_pin(sht30_handle_t *handle, sht30_address_t addr_pin);
uint8_t sht30_init(sht30_handle_t *handle);
uint8_t sht30_deinit(sht30_handle_t *handle);

/**
 * @brief     执行单次测量并读取温湿度值
 * @param[in]  *handle      指向操作句柄的指针
 * @param[out] *temperature 温度值（摄氏度）
 * @param[out] *humidity    相对湿度（百分比）
 * @return     状态码 - 0 成功，1 失败，2 句柄空，3 未初始化
 */
uint8_t sht30_read_temperature_humidity(sht30_handle_t *handle, float *temperature, float *humidity);

/**
 * @brief     设置测量重复性
 * @param[in]  *handle        指向操作句柄的指针
 * @param[in]  repeatability  重复性等级
 */
uint8_t sht30_set_repeatability(sht30_handle_t *handle, sht30_repeatability_t repeatability);

/**
 * @brief     使能/禁用加热器
 */
uint8_t sht30_set_heater(sht30_handle_t *handle, sht30_bool_t enable);

/**
 * @brief     执行软复位
 */
uint8_t sht30_soft_reset(sht30_handle_t *handle);

/**
 * @brief     读取状态寄存器
 * @param[out] *status 指向 16 位状态寄存器的指针
 */
uint8_t sht30_read_status(sht30_handle_t *handle, uint16_t *status);

/**
 * @brief     启动周期测量模式
 */
uint8_t sht30_start_periodic(sht30_handle_t *handle, sht30_mode_t mode, sht30_repeatability_t repeatability);

/**
 * @brief     在周期模式下读取最新数据
 */
uint8_t sht30_fetch_data(sht30_handle_t *handle, float *temperature, float *humidity);

/**
 * @brief     停止周期测量模式
 */
uint8_t sht30_stop_periodic(sht30_handle_t *handle);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_SHT30_H */
