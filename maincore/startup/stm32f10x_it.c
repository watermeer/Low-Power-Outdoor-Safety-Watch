/* stm32f10x_it.c — 主核中断向量
 *
 * 就两个有用的中断：SysTick 计时 + 按键。
 * 主核只管跑 500ms 然后自杀，中断越少越好。
 */

#include "stm32f10x.h"
#include "maincore_system.h"
#include "maincore_alarm.h"

/*==============================================================================
 * SysTick
 *============================================================================*/

void SysTick_Handler(void)
{
    /* 转给 HAL。之前这里直接 extern 了 core 里的 static 变量 s_uptimeMs，
     * 那个 static 是文件作用域的，链接根本找不到，白报一堆错。
     * 以后统一走函数接口，别再跨文件摸人家变量 */
    MainCore_SysTickHandler();
}

/*==============================================================================
 * EXTI1 —— 按键（PB1），用户按一下表示"我看到告警了"
 *============================================================================*/

void EXTI1_IRQHandler(void)
{
    /* ⚠️ 这里踩过一个特别隐蔽的坑：
     *   原来写的是  if (SET == (EXTI->PR & EXTI_PR_PR1))
     *   EXTI_PR_PR1 是 0x02（位掩码），SET 是 0x01，
     *   两者永远不可能相等 —— 按键按烂了也进不来。
     *   编译器 -Wextra 报了 tautological-compare 才发现的。
     *   正确写法就是判断非零。 */
    if (0U != (EXTI->PR & EXTI_PR_PR1)) {
        EXTI->PR = EXTI_PR_PR1;      /* 写 1 清标志，写 0 没用 */
        MainCore_OnKeyPress(true);
    }
}

/*==============================================================================
 * 异常
 *============================================================================*/

void NMI_Handler(void)
{
    while (1) { __NOP(); }
}

/**
 * @brief 硬件错误
 * @note  不像副核那样死循环，这里直接软复位。
 *        主核本来就是个短命任务，复位重来一遍比卡死强 ——
 *        卡死的话副核等不到 CMD_SLEEP，会一直给它供电。
 */
void HardFault_Handler(void)
{
    MainCore_SoftReset();
}

void MemManage_Handler(void)
{
    while (1) { __NOP(); }
}

void BusFault_Handler(void)
{
    while (1) { __NOP(); }
}

void UsageFault_Handler(void)
{
    while (1) { __NOP(); }
}
