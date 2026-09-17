/* bsp_mpu6050.h — MPU6050 六轴：计步 + 抬腕中断
 *
 * 上层只认这几个函数，别绕过这层直接去 include driver_mpu6050.h。
 * 句柄在 .c 里是 static 的，一个器件一个实例，没有多实例需求。
 */

#ifndef BSP_MPU6050_H
#define BSP_MPU6050_H

#include "global_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * API — 完整生命周期管理
 *============================================================================*/

/**
 * @brief   初始化（含 DMP 计步器）
 * @note    挂接口 → 查 WHO_AM_I → 配运动中断 → 开 DMP 计步
 * @return  ERR_OK 成功; ERR_NOT_FOUND ID 不对; ERR_HARDWARE 初始化失败
 */
ErrorCode_t BSP_MPU6050_Init(void);

/**
 * @brief   进休眠
 * @note    acc/gyro 全 standby，只留运动唤醒电路，1.25Hz 循环唤醒
 * @return  永远 ERR_OK —— 内部错误码被盖掉了，见 .c
 */
ErrorCode_t BSP_MPU6050_Sleep(void);

/**
 * @brief   唤醒
 * @note    退循环唤醒 + 恢复加速度计（陀螺仪一直没在用）
 * @return  ERR_OK 成功
 */
ErrorCode_t BSP_MPU6050_WakeUp(void);

/**
 * @brief   读 DMP 累计步数
 * @return  累计步数 —— 掉电清零，Standby 前记得先存备份域
 */
uint32_t BSP_MPU6050_GetStepCount(void);

/**
 * @brief   有没有人在动（抬腕检测）
 * @return  true 检测到运动; false 静止
 * @note    INT 脚 → EXTI 里叫 BSP_MPU6050_IRQHandler → 回调置位
 */
bool BSP_MPU6050_GetMotionIntFlag(void);

/**
 * @brief   清运动标志
 * @note    处理完再清，清早了会漏事件
 */
void BSP_MPU6050_ClearIntFlag(void);

/**
 * @brief   外部中断服务
 * @note    在 EXTI 向量里调: void EXTIx_IRQHandler(void) { BSP_MPU6050_IRQHandler(); }
 */
void BSP_MPU6050_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* BSP_MPU6050_H */
