/* kalman_filter.c — 一维卡尔曼
 *
 * ★ 设计关键点：拿"体温不变"这个物理先验去压传感器噪声
 *   状态方程假设 x(k+1) = x(k) + w，看着像偷懒，其实是对的 ——
 *   人体调温是分钟级的慢过程，30 秒里生理变化远小于 MLX90614 的噪声。
 *   Q 取 0.01、R 取 2.0，Q<<R 意味着"我更相信体温没变，不太相信读数"，
 *   卡尔曼增益自然压得很小，输出就平了。
 *
 * 实现上是全项目最便宜的一环：一次 Update 就 6 次浮点 + 1 次除法，
 * 30 秒才跑一次，M23 软浮点也毫无压力，所以没做定点优化。
 */

#include "kalman_filter.h"
#include <stddef.h>

/**
 * @brief 初始化
 *
 * x_est 直接给 0 —— 意思是"我完全不知道用户现在几度"。
 * 靠 initial_p=10 这个大的初始协方差，第一次 Update 就能一步收敛到实测值，
 * 省掉了预热期，也不用把滤波器状态存进 BKP。
 */
void Kalman1D_Init(Kalman1D_t *kf, float q, float r, float initial_p)
{
    if (NULL == kf) return;

    kf->x_est = 0.0f;
    kf->p_est = initial_p;
    kf->q     = q;
    kf->r     = r;
}

/**
 * @brief 一步更新（预测 + 校正）
 *
 * 有意思的地方是它自带自适应，不用手动调参：
 *   刚上电 P 大   → K≈1 → 几乎全信测量 → 快速跟上
 *   稳态 P 小     → K≈0.01 → 基本忽略单点 → 输出很平
 *   连续偏离模型  → P 被 Q 一点点喂大 → K 回升 → 重新开始跟
 * 所以传感器坏了再恢复、或者用户真的发烧了，滤波器都能自己转过来。
 */
float Kalman1D_Update(Kalman1D_t *kf, float measurement)
{
    if (NULL == kf) return measurement;

    /* 预测：状态不动（体温恒定假设），确定性稍微变差一点 */
    float x_pred = kf->x_est;
    float p_pred = kf->p_est + kf->q;

    /* 校正：按"谁更可信"加权融合 */
    float K = p_pred / (p_pred + kf->r);
    kf->x_est = x_pred + K * (measurement - x_pred);
    kf->p_est = (1.0f - K) * p_pred;

    return kf->x_est;
}
