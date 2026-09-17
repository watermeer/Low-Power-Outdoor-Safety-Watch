/* sliding_window.h — 滑动窗口
 *
 * 环形缓冲，存最近 N 个采样点。纯软件模块，不碰硬件（只借 hal_rtc 取时间）。
 *
 * 两个斜率接口别用混：
 *   Window_CalcSlope         等权，x 轴是数据序号，只在采样间隔固定时才有意义
 *   Window_CalcSlopeWeighted 加权，x 轴是真时间(秒)，间隔变了也不影响
 * 正式判定一律用加权版，等权版留着调试看数。
 */

#ifndef SLIDING_WINDOW_H
#define SLIDING_WINDOW_H

#include "global_types.h"
#include "global_config.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================================
 * 数据结构
 *============================================================================*/

/**
 * @brief 环形缓冲
 * @note  data[] 按 TEMP_WINDOW_LEN 开死，SPO2 窗口也用同一个结构体，
 *        只是 capacity 传小一点。想省 RAM 就得拆成两个类型，暂时不值。
 */
typedef struct {
    int16_t  data[TEMP_WINDOW_LEN];        /* 数据本体（°C×10 或 %×10） */
    uint32_t timestamps[TEMP_WINDOW_LEN];  /* 采样时刻，加权算年龄用 */
    uint8_t  index;                        /* 下一个要写的坑 */
    uint8_t  count;                        /* 现有几条有效数据 */
    uint8_t  capacity;                     /* 容量，实际用几个 */
} SlidingWindow_t;

/*==============================================================================
 * API
 *============================================================================*/

/**
 * @brief   初始化
 * @param   capacity 通常传 TEMP_WINDOW_LEN(10)，超了会被截断
 */
void Window_Init(SlidingWindow_t *win, uint8_t capacity);

/**
 * @brief   压入一个点
 * @param   val °C×10 或 %×10
 * @note    满了就覆盖最旧的；时间戳自动记，调用方不用管
 */
void Window_Push(SlidingWindow_t *win, int16_t val);

/**
 * @brief   等权斜率（x 轴 = 数据序号）
 * @return  正=上升，负=下降；点数不够返回 0
 */
float Window_CalcSlope(const SlidingWindow_t *win);

/**
 * @brief   时间衰减加权斜率（x 轴 = 真时间，单位秒）
 * @param   tau_sec 衰减常数，实际查表按 1800 预生成，传别的值无效
 * @return  °C×10/秒，正=上升
 */
float Window_CalcSlopeWeighted(const SlidingWindow_t *win, uint16_t tau_sec);

/**
 * @brief   均值，空窗返回 0
 */
int16_t Window_GetAvg(const SlidingWindow_t *win);

/**
 * @brief   最小值，空窗返回 INT16_MAX
 */
int16_t Window_GetMin(const SlidingWindow_t *win);

/**
 * @brief   最大值，空窗返回 INT16_MIN
 */
int16_t Window_GetMax(const SlidingWindow_t *win);

/**
 * @brief   最新一个值，空窗返回 0
 */
int16_t Window_GetLatest(const SlidingWindow_t *win);

/**
 * @brief   有效数据条数
 */
uint8_t Window_GetCount(const SlidingWindow_t *win);

/**
 * @brief   满没满
 */
bool Window_IsFull(const SlidingWindow_t *win);

/**
 * @brief   清空
 * @note    数据清了但没清 timestamps，不过 count 归零后就没人读它了
 */
void Window_Clear(SlidingWindow_t *win);

/**
 * @brief   按偏移取历史值
 * @param   offset 0=最新，1=次新……越界返回 0
 */
int16_t Window_GetAt(const SlidingWindow_t *win, uint8_t offset);

#ifdef __cplusplus
}
#endif

#endif /* SLIDING_WINDOW_H */
