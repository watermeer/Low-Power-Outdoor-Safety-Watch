/* kalman_filter.h — 一维卡尔曼
 *
 * 体温管线第 2 层（第 1 层是环境补偿，第 3 层进窗口）。
 * 纯算法，不碰硬件，也不依赖别的模块。
 *
 * 三个参数的含义和调法：
 *   Q  过程噪声 —— "体温能变多快"，越小越信模型
 *   R  测量噪声 —— "读数能有多抖"，越大越不信传感器
 *   P  估计协方差 —— "我现在有多确定"，会自动收敛
 *
 * Q/R 的比值决定平滑程度。我们是 0.01 : 2.0，也就是 1:200，
 * 属于"非常不信传感器"，因为 MLX90614 在穿戴场景下确实很不准（±1~2°C）。
 */

#ifndef KALMAN_FILTER_H
#define KALMAN_FILTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 滤波器状态
 *============================================================================*/

typedef struct {
    float x_est;   /* 当前最优估计 */
    float p_est;   /* 估计协方差，越小越确定 */
    float q;       /* 过程噪声 */
    float r;       /* 测量噪声 */
} Kalman1D_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @param   q         建议 KALMAN_TEMP_Q (0.01)
 * @param   r         建议 KALMAN_TEMP_R (2.0)
 * @param   initial_p 建议 KALMAN_TEMP_INIT_P (10.0)，给大点好一步收敛
 */
void Kalman1D_Init(Kalman1D_t *kf, float q, float r, float initial_p);

/**
 * @brief   喂一个测量值，拿回滤波后的估计
 * @param   measurement 传感器原始读数
 * @return  最优估计值
 * @note    每次调用都会推进一次"预测→校正"，别在循环里调着玩
 */
float Kalman1D_Update(Kalman1D_t *kf, float measurement);

#ifdef __cplusplus
}
#endif

#endif /* KALMAN_FILTER_H */
