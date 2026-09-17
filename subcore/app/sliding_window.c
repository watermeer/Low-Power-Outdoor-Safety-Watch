/* sliding_window.c — 滑动窗口（环形缓冲）
 *
 * 整个趋势判定的地基。存最近 10 个点，新的挤掉旧的。
 *
 * 9月做了一次定点优化，因为加权斜率里那个 expf() 太贵了：
 *   Cortex-M23 没 FPU，软件 expf 一次要几千周期，10 个点就是 3 万多，
 *   采一次样光算权重就耗掉一大截电。改成 Q15 查表后降到 1500 周期左右。
 *   详见下面 WEIGHT_LUT 那段。
 *
 * 注意：本文件不 include <math.h> 了，别手贱加回去。
 */

#include "sliding_window.h"
#include "hal_rtc.h"
#include <string.h>
#include <limits.h>

/*==============================================================================
 * 初始化
 *============================================================================*/

void Window_Init(SlidingWindow_t *win, uint8_t capacity)
{
    if (NULL == win) return;

    memset(win, 0, sizeof(SlidingWindow_t));
    /* 容量不能超过编译期开好的数组大小，宁可截断也别越界 */
    if (capacity > TEMP_WINDOW_LEN) {
        capacity = TEMP_WINDOW_LEN;
    }
    win->capacity = capacity;
}

/*==============================================================================
 * 数据操作
 *============================================================================*/

void Window_Push(SlidingWindow_t *win, int16_t val)
{
    if (NULL == win || 0U == win->capacity) return;

    win->data[win->index]       = val;
    /* 时间戳顺手记，别让调用方操心 —— 加权斜率要靠它算数据年龄 */
    win->timestamps[win->index] = HalRtc_GetElapsedSeconds();
    win->index = (win->index + 1U) % win->capacity;

    if (win->count < win->capacity) {
        win->count++;
    }
}

int16_t Window_GetLatest(const SlidingWindow_t *win)
{
    if (NULL == win || 0U == win->count) return 0;

    /* index 指向"下一个要写的坑"，所以最新数据在它前面一格 */
    uint8_t pos = (win->index == 0U)
                  ? (uint8_t)(win->capacity - 1U)
                  : (uint8_t)(win->index - 1U);
    return win->data[pos];
}

int16_t Window_GetAt(const SlidingWindow_t *win, uint8_t offset)
{
    if (NULL == win || offset >= win->count) return 0;

    /* 逻辑偏移转物理下标：offset=0 最新，offset=count-1 最旧 */
    uint8_t pos;
    if (win->index >= (offset + 1U)) {
        pos = win->index - 1U - offset;
    } else {
        /* 绕回去了，补一圈容量 */
        pos = (uint8_t)(win->capacity + win->index - 1U - offset);
    }
    return win->data[pos];
}

void Window_Clear(SlidingWindow_t *win)
{
    if (NULL == win) return;
    memset(win->data, 0, sizeof(win->data));
    win->index = 0;
    win->count = 0;
}

/*==============================================================================
 * 统计计算
 *============================================================================*/

/**
 * @brief 等权线性回归斜率
 * @note  只用来做粗略判断 / 调试看数，正式判定走加权版
 *        斜率的 x 轴是"数据序号"不是时间，采样间隔一变就没意义了
 */
float Window_CalcSlope(const SlidingWindow_t *win)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return 0.0f;
    }

    /* 最小二乘: slope = (n·Σxy - Σx·Σy) / (n·Σxx - Σx·Σx) */
    float sum_x = 0.0f, sum_y = 0.0f;
    float sum_xy = 0.0f, sum_xx = 0.0f;
    uint8_t n = win->count;

    for (uint8_t i = 0; i < n; i++) {
        /* i=0 取最旧的，这样 x 递增方向和数据的时间方向一致 */
        int16_t val = Window_GetAt(win, (uint8_t)(n - 1U - i));
        float x = (float)i;
        float y = (float)val;

        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float denom = (float)n * sum_xx - sum_x * sum_x;
    if (denom < 0.001f && denom > -0.001f) {
        return 0.0f;   /* 所有点挤在一起，分母≈0，直接装作没趋势 */
    }

    return ((float)n * sum_xy - sum_x * sum_y) / denom;
}

int16_t Window_GetAvg(const SlidingWindow_t *win)
{
    if (NULL == win || 0U == win->count) return 0;

    /* 求和顺序不影响结果，所以直接遍历物理数组，省得绕环形 */
    int32_t sum = 0;
    for (uint8_t i = 0; i < win->count; i++) {
        sum += win->data[i];
    }
    return (int16_t)(sum / win->count);
}

int16_t Window_GetMin(const SlidingWindow_t *win)
{
    if (NULL == win || 0U == win->count) return INT16_MAX;

    int16_t min_val = INT16_MAX;
    for (uint8_t i = 0; i < win->count; i++) {
        if (win->data[i] < min_val) {
            min_val = win->data[i];
        }
    }
    return min_val;
}

int16_t Window_GetMax(const SlidingWindow_t *win)
{
    if (NULL == win || 0U == win->count) return INT16_MIN;

    int16_t max_val = INT16_MIN;
    for (uint8_t i = 0; i < win->count; i++) {
        if (win->data[i] > max_val) {
            max_val = win->data[i];
        }
    }
    return max_val;
}

uint8_t Window_GetCount(const SlidingWindow_t *win)
{
    if (NULL == win) return 0;
    return win->count;
}

bool Window_IsFull(const SlidingWindow_t *win)
{
    if (NULL == win) return false;
    return (win->count >= win->capacity);
}

/*==============================================================================
 * 时间衰减加权最小二乘
 *
 * ★ 设计关键点：数据年龄加权：w=exp(-age/τ)
 *   τ 取 1800s（30分钟）—— 人体调温系统的生理响应时间尺度。
 *   age=τ 时权重剩 37%，age=2τ 剩 14%，老数据自动边缘化。
 *
 *   这一手同时解决三个麻烦，性价比很高：
 *     1. 采样间隔会变（600/120/30s），加权后斜率单位统一成 °C×10/秒
 *     2. 环境突变后旧数据自动降权，不用手动清窗口
 *     3. 新旧数据混在一起时，天然是新的说话更算数
 *
 * ★ 设计关键点：无 FPU 定点优化，副核最花心思
 *   1. expf() → Q15 查表：121 项 LUT，60s 一档，最多差半档可接受
 *   2. 累加全用 int64 定点：软件 64 位乘加照样比软件 float 快得多
 *   3. 只有最后一次除法用 float，全函数唯一一处浮点
 *   实测 10 点窗口：38000 周期 → 1500 周期，快 25 倍。
 *============================================================================*/

/* 权重表: exp(-age/1800) × 32768，age 从 0 到 7200s，每 60s 一档
 *
 * 几个用来验算的锚点：
 *   age=0     → 32767  (exp(0)=1.000)
 *   age=1800  → 12055  (exp(-1)=0.368) ← τ，一个时间常数
 *   age=3600  →  4435  (exp(-2)=0.135)
 *   age=7200  →   600  (exp(-4)=0.018) ← 超过 4τ，相当于不要了
 */
#define WEIGHT_LUT_STEP_S  60U
#define WEIGHT_LUT_BUCKETS 121U

static const uint16_t s_weightLut[WEIGHT_LUT_BUCKETS] = {
    32767, 31694, 30655, 29650, 28678, 27738, 26828, 25949, 25098, 24275,
    23479, 22710, 21965, 21245, 20548, 19875, 19223, 18593, 17983, 17394,
    16824, 16272, 15739, 15223, 14724, 14241, 13774, 13322, 12886, 12463,
    12055, 11659, 11277, 10908, 10550, 10204,  9870,  9546,  9233,  8930,
     8638,  8354,  8080,  7816,  7559,  7312,  7072,  6840,  6616,  6399,
     6189,  5986,  5790,  5600,  5417,  5239,  5067,  4901,  4740,  4585,
     4435,  4289,  4149,  4013,  3881,  3754,  3631,  3512,  3397,  3285,
     3178,  3073,  2973,  2875,  2781,  2690,  2602,  2516,  2434,  2354,
     2277,  2202,  2130,  2060,  1993,  1927,  1864,  1803,  1744,  1687,
     1631,  1578,  1526,  1476,  1428,  1381,  1336,  1292,  1250,  1209,
     1169,  1131,  1094,  1058,  1023,   990,   957,   926,   895,   866,
      838,   810,   784,   758,   733,   709,   686,   663,   642,   621,
      600
};

/**
 * @brief 查表取权重（Q15）
 * @param age_sec 数据年龄（秒）
 * @return 权重×32768
 * @note  只有一次 clamp，分支好预测；超表的直接取最后一档
 */
static inline uint16_t weight_lookup(uint32_t age_sec)
{
    uint16_t bucket = (uint16_t)(age_sec / WEIGHT_LUT_STEP_S);
    if (bucket >= WEIGHT_LUT_BUCKETS) {
        bucket = (uint16_t)(WEIGHT_LUT_BUCKETS - 1U);
    }
    return s_weightLut[bucket];
}

/* 逻辑偏移 → 物理下标。和 Window_GetAt 里那段一样，
 * 抽出来是因为加权斜率也要用，之前复制了一份被 review 出来了 */
static uint8_t get_physical_pos(const SlidingWindow_t *win, uint8_t offset)
{
    if (win->index >= (offset + 1U)) {
        return (uint8_t)(win->index - 1U - offset);
    } else {
        return (uint8_t)(win->capacity + win->index - 1U - offset);
    }
}

/**
 * @brief 带时间衰减权重的线性回归斜率
 *
 * 坐标取法要注意符号：
 *   x = -age，最新点 x≈0，最旧点 x 是最负的
 *   体温上升 → 时间越晚 y 越大 → 在负 x 轴上表现为 y 随 x 增大而增大
 *   → 正斜率。跟等权版 Window_CalcSlope 的符号约定对齐，调用方不用改。
 *
 * 定点量纲核算（怕溢出，先算清楚）：
 *   w_lut          ∈ [600, 32767]        Q15
 *   sum_w          ∈ int32              最大 10×32767 ≈ 3.3e5
 *   sum_wx/wy      ∈ int64              最大 ≈ 2.4e9
 *   sum_wxy        ∈ int64              最大 ≈ 1e12
 *   sum_wxx        ∈ int64              最大 ≈ 1.7e13
 *   分子            ∈ int64              最大 ≈ 3e17，离 9.22e18 还早
 *   分子分母的 32768 因子会约掉，最后结果单位就是 °C×10/秒
 *
 * @param win     窗口
 * @param tau_sec 时间衰减常数，仅用于语义说明；
 *                LUT 是按 KALMAN_TAU_SEC=1800 预生成的，传别的值不会变
 * @return 斜率（°C×10/秒），正=上升
 */
float Window_CalcSlopeWeighted(const SlidingWindow_t *win, uint16_t tau_sec)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT || 0U == tau_sec) {
        return 0.0f;
    }

    uint32_t now = HalRtc_GetElapsedSeconds();
    uint8_t  n   = win->count;

    int32_t sum_w   = 0;
    int64_t sum_wx  = 0;
    int64_t sum_wy  = 0;
    int64_t sum_wxy = 0;
    int64_t sum_wxx = 0;

    for (uint8_t i = 0; i < n; i++) {
        /* i=0 是最旧的，i=n-1 最新 */
        uint8_t  offset = (uint8_t)(n - 1U - i);
        int16_t  val    = Window_GetAt(win, offset);
        uint8_t  pos    = get_physical_pos(win, offset);
        uint32_t ts     = win->timestamps[pos];

        /* 跨天回绕：RTC 秒数到 86400 归零，直接相减会得到个巨大的数 */
        uint32_t age_u32;
        if (now >= ts) {
            age_u32 = now - ts;
        } else {
            age_u32 = (86400U - ts) + now;
        }

        uint16_t w_lut = weight_lookup(age_u32);   /* 这里以前是 expf() */

        int32_t x = -(int32_t)age_u32;   /* 负的年龄，最新点≈0 */
        int32_t y = (int32_t)val;        /* °C×10 */

        sum_w   += (int32_t)w_lut;
        sum_wx  += (int64_t)w_lut * x;
        sum_wy  += (int64_t)w_lut * y;
        sum_wxy += (int64_t)w_lut * x * y;
        sum_wxx += (int64_t)w_lut * x * x;
    }

    int64_t s_w   = (int64_t)sum_w;
    int64_t denom = s_w * sum_wxx - sum_wx * sum_wx;
    if (denom > -1000LL && denom < 1000LL) {
        return 0.0f;   /* 定点版的除零保护，1000 约等于 float 那边的 0.001 */
    }

    int64_t numer = s_w * sum_wxy - sum_wx * sum_wy;

    /* 全函数唯一一次浮点运算，前面所有累加都是整数 */
    return (float)numer / (float)denom;
}
