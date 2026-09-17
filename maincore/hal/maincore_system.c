/* maincore_system.c — 时钟 / GPIO / SysTick
 *
 * 全项目唯一动 RCC 的地方。没上 ST 标准外设库，直接怼寄存器 ——
 * 库一层层包下来太占 Flash，C8 就 64KB，固件已经吃掉七成多。
 *
 * 主核这 500ms 是一次性买卖：上电、干活、失电。所以这里不管低功耗模式，
 * 只要时钟准、滴答准。别指望在这加什么休眠策略。
 */

#include "maincore_system.h"
#include "stm32f10x.h"  /* 只借 CMSIS 的寄存器定义，没用标准库 */

static volatile uint32_t s_uptimeMs = 0;

/*==============================================================================
 * 时钟初始化: HSE 8MHz → PLL ×9 → 72MHz
 *============================================================================*/

/* ★ 设计关键点：绕开外设库直配寄存器省 Flash */
void MainCore_ClockInit(void)
{
    /* 1. 使能 HSE */
    RCC->CR |= RCC_CR_HSEON;
    while (0U == (RCC->CR & RCC_CR_HSERDY)) {
        /* 等待 HSE 稳定 */
        /* 没加超时，晶振不起振就卡死在这。手头板子没遇到过，先不动 */
    }

    /* 2. 配置 Flash 等待周期（72MHz 需 2 个等待周期） */
    /* 必须赶在切 PLL 之前设，顺序反了会跑飞 */
    FLASH->ACR |= FLASH_ACR_LATENCY_2;

    /* 3. 配置 PLL: HSE × 9 = 72MHz */
    /* PLLSRC=HSE, PLLMUL=9, PLLXTPRE=/1 */
    RCC->CFGR &= ~RCC_CFGR_PLLSRC;
    RCC->CFGR |= RCC_CFGR_PLLMULL9;

    /* 4. 使能 PLL */
    RCC->CR |= RCC_CR_PLLON;
    while (0U == (RCC->CR & RCC_CR_PLLRDY)) {
        /* 等待 PLL 锁定 */
    }

    /* 5. 配置 AHB/APB 预分频 */
    /* AHB = 72MHz, APB1 = 36MHz, APB2 = 72MHz */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1;     /* AHB ÷ 1 */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;    /* APB1 ÷ 2 (36MHz, max) */
    RCC->CFGR |= RCC_CFGR_PPRE2_DIV1;    /* APB2 ÷ 1 (72MHz) */

    /* 6. 切换系统时钟到 PLL */
    RCC->CFGR |= RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {
        /* 等待切换完成 */
    }
}

/*==============================================================================
 * GPIO: 仅初始化必要引脚
 *
 * 叫 GpioInit，其实只开了三个端口的时钟，引脚全由各模块自己配。
 * 名字起大了，先不改 —— 改了 main 里那行也得跟着动。
 *============================================================================*/

void MainCore_GpioInit(void)
{
    /* 使能 GPIO 时钟（仅使用到的端口） */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN;

    /* I2C1 引脚: PB6(SCL), PB7(SDA) — 复用开漏，50MHz */
    /* 实际配置由 I2C 模块负责，此处仅确保时钟已使能 */

    /* OLED 引脚:
     *   PA0 = CS    (推挽输出)
     *   PA1 = DC    (推挽输出)
     *   PA5 = SCK   (SPI1 复用推挽)
     *   PA7 = MOSI  (SPI1 复用推挽)
     *   PA4 = RST   (推挽输出)
     */

    /* 震动马达: PB0 — 推挽输出 */
    /* 由 maincore_vibrate 模块配置 */

    /* 按键: PB1 — 上拉输入（用户确认告警） */

    /* 所有未用引脚默认模拟输入（STM32F103 复位默认值即为浮空输入，
     * 由各模块初始化时显式配置） */
}

/*==============================================================================
 * SysTick: 1ms 滴答
 *
 * ★ 设计关键点：主核唯一时间源，超时都靠它
 *============================================================================*/

void MainCore_SysTickInit(void)
{
    /* SysTick 重装载值 = 72MHz / 1000 = 72000 */
    SysTick->LOAD = MAINCORE_SYSCLK_HZ / 1000U - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |  /* 使用核心时钟 */
                    SysTick_CTRL_TICKINT_Msk   |  /* 使能中断 */
                    SysTick_CTRL_ENABLE_Msk;      /* 使能 SysTick */

    s_uptimeMs = 0;
}

/* ★ 设计关键点：延时 WFI 睡到下一滴答，不烧电 */
void MainCore_DelayMs(uint32_t ms)
{
    uint32_t start = s_uptimeMs;
    /* 无符号减法，回绕了结果也是对的，不用管溢出 */
    while ((s_uptimeMs - start) < ms) {
        __WFI();  /* 等待中断，降低功耗 */
    }
}

uint32_t MainCore_GetUptimeMs(void)
{
    return s_uptimeMs;
}

/*==============================================================================
 * 软复位
 *============================================================================*/

void MainCore_SoftReset(void)
{
    /* 通过 SCB AIRCR 触发系统复位 */
    /* 0x5FA 是解锁键值，写错了不报错，只是不复位 */
    /* 目前只有 HardFault 会走到这儿，正常流程用不上 */
    SCB->AIRCR = (0x5FAU << SCB_AIRCR_VECTKEY_Pos) | SCB_AIRCR_SYSRESETREQ_Msk;
    __DSB();
    while (1) { __WFI(); }  /* 等待复位 */
}

/*==============================================================================
 * SysTick 中断服务
 *============================================================================*/

void MainCore_SysTickHandler(void)
{
    s_uptimeMs++;  /* 中断里就干这一件事 */
}
