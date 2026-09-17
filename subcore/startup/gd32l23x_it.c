/* gd32l23x_it.c — 副核中断向量
 *
 * 全部就干一件事：把中断转给对应模块，然后立刻返回。
 * 中断里不许做耗时操作 —— I2C 从机中断随时会来，占着 CPU 会丢字节。
 *
 * 移植踩的坑（ST 的代码看多了，GD32 这边名字全不一样）：
 *   I2C1_EV_IRQHandler   不是 I2C1_IRQHandler（那是 ST 的叫法）
 *   RTC_Alarm_IRQHandler 不是 RTC_IRQHandler，GD32L233 没有独立的 RTC 中断
 *   MPU6050 接在 PB5~PB9 那组，所以是 EXTI5_9
 */

#include "gd32l23x.h"
#include "hal_i2c1.h"
#include "hal_rtc.h"
#include "hal_system.h"
#include "bsp_mpu6050.h"

void SysTick_Handler(void)      { HalSystem_SysTickHandler(); }

void RTC_Alarm_IRQHandler(void) { HalRtc_IRQHandler(); }

void I2C1_EV_IRQHandler(void)   { HalI2c1_IRQHandler(); }

/* ---- EXTI5_9 是共享向量，进来得自己判断是哪个脚触发的 ---- */
void EXTI5_9_IRQHandler(void)
{
    if (SET == exti_interrupt_flag_get(EXTI_5)) {
        exti_interrupt_flag_clear(EXTI_5);
        BSP_MPU6050_IRQHandler();
    }
}

/*==============================================================================
 * 异常处理
 *
 * ⚠️ 这几个都是死循环。主核那边 HardFault 是走软复位重来的，
 *    副核这边目前只能原地卡住 —— 问题是一卡住就再也不会进 Standby，
 *    电流从几微安涨到几毫安，一块电池一晚上就没了。
 *    得改成复位，或者至少把主核的电断掉。还没顾上做。
 *============================================================================*/

void NMI_Handler(void)          { while (1) { __NOP(); } }
void HardFault_Handler(void)    { while (1) { __NOP(); } }
void MemManage_Handler(void)    { while (1) { __NOP(); } }
void BusFault_Handler(void)     { while (1) { __NOP(); } }
void UsageFault_Handler(void)   { while (1) { __NOP(); } }
