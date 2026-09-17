/* hal_system.c — 时钟 + 低功耗模式
 *
 * ★ 设计关键点：IRC16M 直驱 16M，不开 PLL
 *   板上没焊外部晶振，也不想为那点主频去等 PLL 锁定。
 *   平常跑 16MHz 够用（采样是秒级的活），嫌费电就 AHB 八分频降到 2MHz。
 *   降频只要改一个分频寄存器，比切 PLL 快得多，也更省切换功耗。
 *
 * ⚠️ 血泪教训：这个文件的所有延时/SysTick 重装值都跟 16MHz 绑死。
 *    之前 system_gd32l23x.c 里默认选的是 64MHz PLL+HXTAL，
 *    结果上电就卡在 SystemInit 死等晶振起振 —— 板子上根本没那颗晶振。
 *    改成 __SYSTEM_CLOCK_16M_IRC16M 才活过来。改主频记得两边一起改！
 */

#include "hal_system.h"
#include "hal_rtc.h"
#include "hal_power.h"
#include "gd32l23x.h"

/*==============================================================================
 * 内部状态
 *============================================================================*/

static SubCoreMode_t s_currentMode = SUB_MODE_NORMAL;
static volatile uint32_t s_tickMs  = 0;

/*==============================================================================
 * 时钟初始化
 *============================================================================*/

ErrorCode_t HalSystem_ClockInit(void)
{
    /* 1. IRC16M 起振 */
    rcu_osci_on(RCU_IRC16M);
    while (SET != rcu_flag_get(RCU_FLAG_IRC16MSTB)) {
        /* 等稳定 */
    }

    /* 2. 直接拿 IRC16M 当系统时钟，不走 PLL */
    rcu_system_clock_source_config(RCU_CKSYSSRC_IRC16M);
    while (RCU_SCSS_IRC16M != rcu_system_clock_source_get()) {
        /* 等切完 */
    }

    /* 3. 分频。APB1 挂 I2C，不要超过它的上限 */
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);      /* AHB = 16MHz */
    rcu_apb1_clock_config(RCU_APB1_CKAHB_DIV2);    /* APB1 = 8MHz */
    rcu_apb2_clock_config(RCU_APB2_CKAHB_DIV1);    /* APB2 = 16MHz */

    /* 4. SysTick 1ms */
    SysTick_Config(16000000U / 1000U);

    s_currentMode = SUB_MODE_NORMAL;

    return ERR_OK;
}

/*==============================================================================
 * 时钟模式切换
 *
 * 注意这两处 LOAD 是写死的常数，不是按 SystemCoreClock 算的 ——
 * 因为 SystemCoreClock 在 Standby 唤醒后的值不一定可信。
 * 代价就是主频一改，这里就得手动跟着改。
 *============================================================================*/

void HalSystem_EnterLowPowerMode(void)
{
    if (SUB_MODE_LOW_POWER == s_currentMode) return;

    /* 16MHz ÷ 8 = 2MHz */
    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV8);

    /* 分频变了，SysTick 得重装，否则 1ms 会变 8ms */
    SysTick->LOAD = 2000U - 1U;
    SysTick->VAL  = 0U;

    s_currentMode = SUB_MODE_LOW_POWER;
}

void HalSystem_EnterNormalMode(void)
{
    if (SUB_MODE_NORMAL == s_currentMode) return;

    rcu_ahb_clock_config(RCU_AHB_CKSYS_DIV1);

    SysTick->LOAD = 16000U - 1U;
    SysTick->VAL  = 0U;

    s_currentMode = SUB_MODE_NORMAL;
}

SubCoreMode_t HalSystem_GetMode(void)
{
    return s_currentMode;
}

/*==============================================================================
 * Standby 深度休眠
 *============================================================================*/

/**
 * @brief 进 Standby
 *
 * ★ 设计关键点：最省电状态，SRAM 全丢只留 VBAT
 *   所以进之前必须把关键状态写进备份域，醒来再从复位向量重跑一遍。
 *   顺序不能乱：先存状态 → 再断主核电 → 清中断 → 最后才 WFI。
 *
 * 唤醒源有两个：RTC 闹钟（定时采样）和 I2C1 地址匹配（主核要找我们）。
 * 副作用：这个函数正常情况不会返回 —— 醒了是重新上电，不是从这里继续。
 */
void HalSystem_EnterStandby(void)
{
    /* 1. 打个运行标记，下次醒来靠它区分冷启动 */
    HalRtc_MarkRunning();

    /* 2. 主核必须断电，不然它漏的电比我们省的还多 */
    HalPower_MainCoreOff();

    /* 3. 降频，切模式本身也费电 */
    HalSystem_EnterLowPowerMode();

    /* 4. 关外设时钟（GPIO 留着，唤醒引脚要用） */
    rcu_periph_clock_disable(RCU_I2C0);
    rcu_periph_clock_disable(RCU_USART1);

    /* 5. 清挂起中断。不清的话 WFI 会被立刻弹回来，等于没睡 */
    NVIC_ClearPendingIRQ(RTC_Alarm_IRQn);
    NVIC_ClearPendingIRQ(I2C1_EV_IRQn);

    /* 6. 备份域写保护放开 */
    pmu_backup_write_enable();

    /* 7. 使能唤醒脚。I2C1 的从机地址匹配是硬件自动的，不用额外配 */
    pmu_wakeup_pin_enable(PMU_WAKEUP_PIN0);

    /* 8. 睡 */
    s_currentMode = SUB_MODE_STANDBY;
    pmu_to_standbymode();

    /* 正常情况下走不到这。真走到这儿说明 Standby 没进去，是个 bug */
}

/*==============================================================================
 * 启动原因
 *============================================================================*/

bool HalSystem_IsStandbyWakeup(void)
{
    return !HalRtc_IsColdBoot();
}

/*==============================================================================
 * 延时
 *============================================================================*/

/**
 * @brief 毫秒延时
 * @note  用 SysTick 计数，中间 WFI 让 CPU 歇着 —— 空转太费电
 */
void HalSystem_DelayMs(uint32_t ms)
{
    uint32_t start = s_tickMs;
    while ((s_tickMs - start) < ms) {
        __WFI();
    }
}

/**
 * @brief 微秒延时
 * @note  ⚠️ 纯空循环凑的，精度很一般（编译器开了 -Os，循环次数不好预测）。
 *        目前只有 I2C 时序和传感器上电等待在用，这些地方 ±20% 都能忍。
 *        哪天要精确延时就得挂个定时器，别指望这个函数。
 */
void HalSystem_DelayUs(uint32_t us)
{
    uint32_t loops_per_us;
    if (SUB_MODE_LOW_POWER == s_currentMode) {
        loops_per_us = 2U;   /* 2MHz */
    } else {
        loops_per_us = 16U;  /* 16MHz */
    }

    for (uint32_t i = 0; i < us; i++) {
        for (volatile uint32_t j = 0; j < loops_per_us; j++) {
            __NOP();
        }
    }
}

uint32_t HalSystem_GetTickMs(void)
{
    return s_tickMs;
}

/*==============================================================================
 * SysTick 中断
 *============================================================================*/

void HalSystem_SysTickHandler(void)
{
    s_tickMs++;
}
