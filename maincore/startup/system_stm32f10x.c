/* system_stm32f10x.c — CMSIS 系统初始化
 *
 * 这是启动文件要求"必须存在"的最小实现 —— startup 里有一句 bl SystemInit，
 * 少了它链接都过不去。
 *
 * 为什么不用 ST 官方那份 500 行的？
 *   官方那份是让你在编译期选 HSE_VALUE / PLL 倍频的，宏一大堆。
 *   但我们的时钟是自己在 MainCore_ClockInit() 里直接写寄存器配的
 *   （要精确控制 Flash 等待周期和 APB1 分频），两套会打架。
 *   所以这里只干最必要的事，时钟完全交给 MainCore_ClockInit。
 */

#include "stm32f10x.h"

/**
 * @brief 当前系统频率
 * @note  上电瞬间还是默认的 8MHz HSI，MainCore_ClockInit() 跑完才是 72MHz
 */
uint32_t SystemCoreClock = 8000000U;

/**
 * @brief 系统初始化 —— 比 main() 还早
 * @note  只把向量表摆到 Flash 起始，别的什么都不做。
 *        时钟故意留着不配，避免"官方配一遍、我再配一遍"来回切
 */
void SystemInit(void)
{
    /* 向量表定位到 0x08000000 */
    SCB->VTOR = FLASH_BASE | 0x0U;
}

/**
 * @brief 重算 SystemCoreClock
 * @note  主核只在启动时定一次频，运行期不切，所以直接给常数
 */
void SystemCoreClockUpdate(void)
{
    SystemCoreClock = 72000000U;
}
