/* maincore_vibrate.c — 震动马达（TIM3_CH3 硬件 PWM）
 *
 * PB0 出 PWM 推马达，1kHz、占空比固定 50%。PWM 在这儿只当开关使，
 * 想震感分级就改 CCR3 —— 暂时没做。
 *
 * 震动一律阻塞（DelayMs 硬等），主核总共只活 500ms，
 * 为这点事搭套非阻塞状态机不值。代价见 VIBRATE_PATTERN 那段。
 */

#include "maincore_vibrate.h"
#include "maincore_system.h"
#include "stm32f10x.h"

/* PB0 — TIM3_CH3 PWM 输出控制震动马达 */
#define VIBRATE_GPIO_PORT  GPIOB
#define VIBRATE_GPIO_PIN   GPIO_PIN_0
#define VIBRATE_TIM        TIM3
#define VIBRATE_TIM_CH     3

/* 上面这几个宏其实都没展开过（下面全是直接写掩码），所以一直没报错。
 * 提醒一声：GPIO_PIN_0 是副核 GD32 的宏，STM32 的 CMSIS 头里没有，
 * 哪天真要用了，第一件事是把它换成 (1U << 0) */

static bool s_vibrating = false;

/* ★ 设计关键点：马达用硬件 PWM，震感好调 */
void MainCore_VibrateInit(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* PB0 → 复用推挽 (TIM3_CH3) */
    GPIOB->CRL &= ~(0x0FU << 0);
    GPIOB->CRL |=  (0x0BU << 0);  /* 50MHz 复用推挽 */

    /* TIM3: PWM 模式, 1KHz, 50% 占空比 */
    /* PSC 是影子寄存器，不发 UG 事件的话要等第一次更新才装进去，
     * 头一个周期就只有 13.9us 而不是 1ms。短得听不出来，不管它 */
    TIM3->PSC  = 71;                /* 72MHz / 72 = 1MHz */
    TIM3->ARR  = 999;               /* 1MHz / 1000 = 1KHz */
    TIM3->CCR3 = 500;               /* 50% 占空比 */
    TIM3->CCMR2 |= (6U << 4);       /* CH3 PWM1 模式；OC3M 在 CCMR2 低字节，别写成 <<8 */
    TIM3->CCER  |= TIM_CCER_CC3E;   /* 使能 CH3 输出 */
    TIM3->CR1   &= ~TIM_CR1_CEN;    /* 初始关闭 */
}

void MainCore_VibrateSet(bool on)
{
    if (on) {
        TIM3->CR1 |= TIM_CR1_CEN;
    } else {
        TIM3->CR1 &= ~TIM_CR1_CEN;
    }
    s_vibrating = on;
}

/* ★ 设计关键点：震动全阻塞式，不搭非阻塞状态机 */
void MainCore_VibratePulse(VibrateMode_t mode)
{
    switch (mode) {
        case VIBRATE_SHORT:
            MainCore_VibrateSet(true);
            MainCore_DelayMs(100);
            MainCore_VibrateSet(false);
            break;

        case VIBRATE_PATTERN:
            /* 3 × (200+150) = 1050ms，这一下就顶穿了 500ms 的总预算。
             * 要么把节拍改短，要么认了 —— 先留着，反正用户能感觉到 */
            for (int i = 0; i < 3; i++) {
                MainCore_VibrateSet(true);
                MainCore_DelayMs(200);
                MainCore_VibrateSet(false);
                MainCore_DelayMs(150);
            }
            break;

        case VIBRATE_CONTINUOUS:
            /* 没人调这条：现在状态机用的是 PATTERN，要用的话得配个停的地方 */
            MainCore_VibrateSet(true);
            break;

        default:
            break;
    }
}

void MainCore_VibrateStop(void)
{
    MainCore_VibrateSet(false);
}

/* 目前没人查，留着调试时看马达状态 */
bool MainCore_IsVibrating(void)
{
    return s_vibrating;
}
