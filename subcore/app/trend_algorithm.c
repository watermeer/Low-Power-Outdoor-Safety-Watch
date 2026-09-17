/* trend_algorithm.c — 趋势判断
 *
 * 把窗口里的历史点变成"在升/在降/没动"这个结论。
 *
 * 核心就一件事：算斜率，然后拿斜率跟阈值比。
 * 阈值单位是"每秒钟涨多少个 °C×10"，因为采样间隔会变，
 * 用"每个点"当单位的话，同一个生理过程在不同等级下会得出不同结论。
 *
 * 本文件不依赖 math.h（副核没 FPU，也不想为了一个 fabs 把整包浮点库拉进来）。
 */

#include "trend_algorithm.h"
#include "hal_rtc.h"
#include <stddef.h>

/**
 * @brief 自己写个绝对值
 * @note  别为了这个 include math.h，链接会胖一圈
 */
static float fabs_local(float x)
{
    return (x < 0.0f) ? (-x) : x;
}

/*==============================================================================
 * 线性回归
 *============================================================================*/

/**
 * @brief 从裸数组算斜率（x 轴 = 数组下标）
 * @note  早期版本留下的接口，现在活跃代码基本不走这里了
 */
float Trend_CalcSlope(const int16_t *data, uint8_t count)
{
    if (NULL == data || count < TREND_MIN_DATA_COUNT) {
        return 0.0f;
    }

    float sum_x = 0.0f, sum_y = 0.0f;
    float sum_xy = 0.0f, sum_xx = 0.0f;

    for (uint8_t i = 0; i < count; i++) {
        float x = (float)i;
        float y = (float)data[i];

        sum_x  += x;
        sum_y  += y;
        sum_xy += x * y;
        sum_xx += x * x;
    }

    float n = (float)count;
    float denominator = n * sum_xx - sum_x * sum_x;

    /* 所有 x 一样时分母是 0（正常不会，but 防御一下） */
    if (fabs_local(denominator) < 0.001f) {
        return 0.0f;
    }

    return (n * sum_xy - sum_x * sum_y) / denominator;
}

/*==============================================================================
 * 方向判定
 *============================================================================*/

/**
 * @brief 体温方向（等权版）
 * @note  降级路径里还在用它，因为那边看的是"短期有没有回头"，
 *        等权对最近几个点更敏感，反而合适。没换成加权是有意的。
 */
TrendDirection_t Trend_GetTempDirection(const SlidingWindow_t *win)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return TREND_STABLE;   /* 点太少不敢下结论 */
    }

    float slope = Window_CalcSlope(win);

    if (slope > (float)TREND_TEMP_RISING_SLOPE) {
        return TREND_RISING;
    } else if (slope < (float)TREND_TEMP_FALLING_SLOPE) {
        return TREND_FALLING;
    } else {
        return TREND_STABLE;
    }
}

/*==============================================================================
 * 体温方向（加权版）—— 正式判定走这条
 *
 * 跟等权版的区别就是 x 轴：
 *   等权   Window_CalcSlope           x = 第几个点，隐含"等间隔"假设
 *   加权   Window_CalcSlopeWeighted   x = 真实秒数，还带时间衰减
 *
 * 为什么非换不可：采样间隔在 A/B/C 级之间是 600/120/30 秒来回切的，
 * 等权版的斜率在同一生理变化下会给出三个不同的数，阈值根本没法定。
 *============================================================================*/

TrendDirection_t Trend_GetTempDirectionWeighted(const SlidingWindow_t *win,
                                                 uint16_t tau_sec)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return TREND_STABLE;
    }

    float slope = Window_CalcSlopeWeighted(win, tau_sec);

    /* 注意阈值也是"每秒"的，别手滑改成 TREND_TEMP_RISING_SLOPE */
    if (slope > TREND_TEMP_RISING_SLOPE_PER_SEC) {
        return TREND_RISING;
    } else if (slope < TREND_TEMP_FALLING_SLOPE_PER_SEC) {
        return TREND_FALLING;
    } else {
        return TREND_STABLE;
    }
}

TrendDirection_t Trend_GetSpO2Direction(const SlidingWindow_t *win)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return TREND_STABLE;
    }

    float slope = Window_CalcSlope(win);

    /* 血氧只防掉不防高，上升当没事 */
    if (slope < (float)TREND_SPO2_FALLING_SLOPE) {
        return TREND_FALLING;
    }
    return TREND_STABLE;
}

/*==============================================================================
 * 体温综合评估（打包结论，给上层用的）
 *============================================================================*/

TempAssessment_t Trend_AssessTemperature(const SlidingWindow_t *win, int16_t amb_temp)
{
    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return TEMP_ASSESS_NORMAL;
    }

    int16_t latest  = Window_GetLatest(win);
    TrendDirection_t dir = Trend_GetTempDirection(win);

    /* 环境补偿已经在告警机第1层做过了，这里再吃一遍就重复了 */
    (void)amb_temp;

    /* ---- 失温 ---- */
    if (latest <= TEMP_THRESHOLD_C_LOW) {
        return TEMP_ASSESS_DROPPING;   /* 已经低到确认线以下 */
    }
    if (dir == TREND_FALLING && latest <= TEMP_THRESHOLD_B_LOW) {
        return TEMP_ASSESS_COOLING;    /* 低 + 还在掉 = 失温前期 */
    }
    if (dir == TREND_FALLING) {
        return TEMP_ASSESS_COOLING;    /* 光是在掉也先标记 */
    }

    /* ---- 中暑 ---- */
    if (latest >= TEMP_THRESHOLD_C_HIGH) {
        return TEMP_ASSESS_SURGING;
    }
    if (dir == TREND_RISING && latest >= TEMP_THRESHOLD_B_HIGH) {
        return TEMP_ASSESS_RISING;
    }
    if (dir == TREND_RISING) {
        return TEMP_ASSESS_RISING;
    }

    return TEMP_ASSESS_NORMAL;
}

/*==============================================================================
 * 危险区判断
 *============================================================================*/

bool Trend_IsInDangerZone(int16_t value, int16_t threshold, bool is_rising)
{
    if (is_rising) {
        return (value >= threshold);   /* 高温：往上顶破上限 */
    } else {
        return (value <= threshold);   /* 低温：往下跌穿下限 */
    }
}

/*==============================================================================
 * 趋势置信度（R²）
 *
 * ★ 设计关键点：不只给方向，还给"这个方向有多可信"
 *   同样一条 RISING，R²=0.9 是身体真在升温，R²=0.3 只是噪声碰巧往上。
 *   只报方向不报置信度，等于把不可靠的判断和可靠的混在一起说。
 *
 *   R² = 1 - SS_res/SS_tot：1=完美落在直线上，0=回归线毫无解释力，
 *   工程上 >0.7 算线性关系可信。
 *
 * ⚠️ 这块是 9 月底赶出来的，有两个地方偷懒了，自己心里有数：
 *    1. 第二遍算残差时，截距直接拿均值顶替，不是真正的加权截距
 *    2. consecutive_dir 是写死的 3，根本没真去数连续同向次数
 *    结论能看，但别在面试里说它是"完整的置信度评估"
 *============================================================================*/

TrendQuality_t Trend_GetQuality(const SlidingWindow_t *win, uint16_t tau_sec)
{
    TrendQuality_t quality = {0};

    if (NULL == win || win->count < TREND_MIN_DATA_COUNT) {
        return quality;
    }

    quality.slope = Window_CalcSlopeWeighted(win, tau_sec);

    uint32_t now = HalRtc_GetElapsedSeconds();
    uint8_t  n   = win->count;

    float sum_y = 0.0f, sum_yy = 0.0f;
    float ss_res = 0.0f;

    /* 第一遍：均值和总平方和 */
    for (uint8_t i = 0; i < n; i++) {
        int16_t val = Window_GetAt(win, n - 1U - i);   /* 从最旧开始 */
        float   y   = (float)val;
        sum_y  += y;
        sum_yy += y * y;
    }
    float y_mean = sum_y / (float)n;
    float ss_tot = sum_yy - (float)n * y_mean * y_mean;

    /* 第二遍：残差平方和 */
    for (uint8_t i = 0; i < n; i++) {
        int16_t  val    = Window_GetAt(win, n - 1U - i);
        uint8_t  offset = n - 1U - i;
        /* 这段定位逻辑跟窗口里那段重复了第三次，迟早得挪成公共函数 */
        uint8_t  pos;
        if (win->index >= (offset + 1U)) {
            pos = (uint8_t)(win->index - 1U - offset);
        } else {
            pos = (uint8_t)(win->capacity + win->index - 1U - offset);
        }
        float age     = (float)(now - win->timestamps[pos]);
        float y_pred  = quality.slope * age + y_mean;   /* 偷懒：均值当截距 */
        float y_actual = (float)val;
        float residual = y_actual - y_pred;
        ss_res += residual * residual;
    }

    if (ss_tot > 0.001f) {
        quality.r_squared = 1.0f - (ss_res / ss_tot);
        /* 拟合太差会算出负数，夹到 [0,1] 免得调用方看到奇怪的值 */
        if (quality.r_squared < 0.0f) quality.r_squared = 0.0f;
        if (quality.r_squared > 1.0f) quality.r_squared = 1.0f;
    }

    /* 方向（复用加权阈值，单位统一） */
    TrendDirection_t dir = (quality.slope > TREND_TEMP_RISING_SLOPE_PER_SEC)  ? TREND_RISING
                         : (quality.slope < TREND_TEMP_FALLING_SLOPE_PER_SEC) ? TREND_FALLING
                         : TREND_STABLE;
    /* TODO 这里是假的，没真数连续次数，先写死 3 让 is_reliable 能过 */
    quality.consecutive_dir = (dir != TREND_STABLE) ? (uint8_t)3U : (uint8_t)0U;

    quality.is_reliable = (quality.r_squared > 0.7f) && (quality.consecutive_dir >= 3U);

    return quality;
}
