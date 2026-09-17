/* maincore_main.c — 主核入口
 *
 * ★ 设计关键点：主核是一次性执行的，跑完就死
 *   它不是常驻跑 while(1) 的普通单片机程序。整个生命周期是：
 *     副核放给它电 → 它从复位向量启动 → 初始化 → 读副核状态 → 显示 + 震动
 *     → 发 CMD_SLEEP → 副核确认后切电 → 它自己失电"关机"
 *
 *   为什么这么设计？因为它身上挂着 OLED，是整套系统里最耗电的部分。
 *   与其让它进 Stop 模式慢慢漏电，不如干脆断电。省下来的电全给副核撑着待机。
 *   代价就是每次唤醒都得从零启动，所以硬压了 500ms 的上限（MAINCORE_MAX_RUN_TIME_MS）。
 *
 * 备注：SysTick_Handler 和 EXTI1_IRQHandler 都在 stm32f10x_it.c 里。
 *      这两个以前在这儿也写了一份，链接直接报 multiple definition，删掉了。
 */

#include "maincore_system.h"
#include "maincore_i2c.h"
#include "maincore_oled.h"
#include "maincore_vibrate.h"
#include "maincore_alarm.h"
#include "stm32f10x.h"

int main(void)
{
    /* ── 阶段1: 硬件初始化，预算 50ms ── */

    MainCore_ClockInit();      /* 72MHz */
    MainCore_SysTickInit();    /* 1ms 滴答，后面所有延时都靠它 */
    MainCore_GpioInit();

    /* I2C 起不来说明副核那边有问题，直接去请求断电，别在这耗着 */
    ErrorCode_t err = MainCore_I2cInit();
    if (ERR_OK != err) {
        goto request_sleep;
    }

    MainCore_OledInit();
    MainCore_VibrateInit();
    MainCore_AlarmInit();

    /* ── 阶段2: 状态机，预算 450ms ──
     * 状态机自己知道什么时候该结束（显示完了、或者用户按键确认了），
     * 返回值就是"还要不要继续转" */
    while (1) {
        bool running = MainCore_AlarmProcess();

        if (!running) {
            break;
        }

        /* 兜底：状态机再怎么写飞，也不能超过 500ms。
         * 这个不只是体验问题 —— 超时了副核会认为主核挂了 */
        if (MainCore_GetUptimeMs() > MAINCORE_MAX_RUN_MS) {
            break;
        }
    }

request_sleep:
    /* ── 阶段3: 收尾 ──
     * I2C 初始化失败也会跳到这里，所以这几句必须能重复调用不炸 */
    MainCore_I2cRequestSleep();
    MainCore_OledSleep();
    MainCore_VibrateStop();

    /* 在这儿等副核把电切了。
     * WFI 不是为了省电（马上就要断电了），是为了别让 CPU 空转乱跳 */
    while (1) {
        __WFI();
    }

    return 0;   /* 到不了这儿 */
}
