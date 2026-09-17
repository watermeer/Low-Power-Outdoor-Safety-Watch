/* bsp_sensors.h — 传感器总开关
 *
 * 业务层只 include 这一个就够了，四颗的头都在里面。
 * 顺序有讲究：开机先总线后器件，睡的时候反过来，先器件后总线。
 */

#ifndef BSP_SENSORS_H
#define BSP_SENSORS_H

#include "bsp_mpu6050.h"
#include "bsp_mlx90614.h"
#include "bsp_sht30.h"
#include "bsp_max30102.h"

/**
 * @brief   初始化全部传感器
 * @note    先 I2C0 总线，再逐个 init。四颗共用一根总线，
 *          只有体温失败会真的返回错误码，其余坏了等于没这颗
 * @return  ERR_OK 成功; 体温初始化失败的错误码
 */
ErrorCode_t BSP_Sensors_InitAll(void);

/**
 * @brief   休眠全部传感器
 * @note    进 Standby 之前调，四颗挨个睡，最后关总线
 */
void BSP_Sensors_SleepAll(void);

#endif /* BSP_SENSORS_H */
