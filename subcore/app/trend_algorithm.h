/* trend_algorithm.h — 趋势判断
 *
 * 输入窗口，输出方向。纯算法，不碰硬件。
 *
 * 两套阈值别搞混（踩过一次，A 级降到 B 级都没触发）：
 *   等权版  单位是 °C×10 / 每个点    → TREND_TEMP_RISING_SLOPE = 5
 *   加权版  单位是 °C×10 / 秒        → TREND_TEMP_RISING_SLOPE_PER_SEC = 0.008
 * 量纲不一样，绝对不能互相赋值。
 */

#ifndef TREND_ALGORITHM_H
#define TREND_ALGORITHM_H

#include "global_types.h"
#include "sliding_window.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 趋势方向
 *============================================================================*/

typedef enum {
    TREND_STABLE  = 0,  /* 斜率没超阈值，当没动 */
    TREND_RISING  = 1,
    TREND_FALLING = 2,
} TrendDirection_t;

/*==============================================================================
 * 体温评语（给主核 UI 直接用）
 *============================================================================*/

typedef enum {
    TEMP_ASSESS_NORMAL     = 0,  /* 正常 */
    TEMP_ASSESS_COOLING    = 1,  /* 在降温，提示保暖 */
    TEMP_ASSESS_RISING     = 2,  /* 在升温，提示散热 */
    TEMP_ASSESS_DROPPING   = 3,  /* 掉太快，失温风险 */
    TEMP_ASSESS_SURGING    = 4,  /* 升太快，中暑风险 */
} TempAssessment_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   裸数组算斜率
 * @param   data  按时间排，data[0] 最旧
 * @return  斜率，单位 °C×10/每点
 * @note    活跃代码基本不用了，留着给 PC 端测试调算法
 */
float Trend_CalcSlope(const int16_t *data, uint8_t count);

/**
 * @brief   体温方向（等权版）
 * @note    降级判断里还在用 —— 它盯最近几个点更紧，适合看"有没有回头"
 */
TrendDirection_t Trend_GetTempDirection(const SlidingWindow_t *win);

/**
 * @brief   体温方向（加权版）—— 正式判定用这个
 * @param   tau_sec 传 KALMAN_TAU_SEC(1800)
 * @note    采样间隔在 600/120/30s 之间切换，只有加权版斜率才有统一物理意义
 */
TrendDirection_t Trend_GetTempDirectionWeighted(const SlidingWindow_t *win,
                                                 uint16_t tau_sec);

/*==============================================================================
 * 趋势质量
 *============================================================================*/

/**
 * @brief 趋势质量
 * @note  consecutive_dir 和 is_reliable 目前是凑出来的（见 .c 里的 TODO），
 *        只有 r_squared 是真算的
 */
typedef struct {
    float   slope;            /* °C×10/秒 */
    float   r_squared;        /* 拟合优度 0~1，1=完美线性 */
    uint8_t consecutive_dir;  /* 连续同向次数（暂为占位） */
    bool    is_reliable;      /* R²>0.7 且连续同向≥3 */
} TrendQuality_t;

/**
 * @brief   评估趋势可信度
 * @note    光给方向不够 —— 两条都是 RISING，R²=0.9 和 R²=0.3 完全是两回事
 */
TrendQuality_t Trend_GetQuality(const SlidingWindow_t *win, uint16_t tau_sec);

/**
 * @brief   血氧方向
 * @note    只判下降，上升不算事
 */
TrendDirection_t Trend_GetSpO2Direction(const SlidingWindow_t *win);

/**
 * @brief   体温综合评语
 * @param   amb_temp (°C×100)，目前没用上（补偿在告警机里做过了）
 */
TempAssessment_t Trend_AssessTemperature(const SlidingWindow_t *win, int16_t amb_temp);

/**
 * @brief   越界判断的小工具
 * @param   is_rising true=看上限，false=看下限
 */
bool Trend_IsInDangerZone(int16_t value, int16_t threshold, bool is_rising);

#ifdef __cplusplus
}
#endif

#endif /* TREND_ALGORITHM_H */
