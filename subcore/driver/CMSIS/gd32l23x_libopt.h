/*!
    \file    gd32l23x_libopt.h
    \brief   副核外设库裁剪开关 — 只开本项目用得到的那几个

    \note    这个文件名字是 GD32 官方约定，gd32l23x.h 末尾会强 include 它，
             所以必须放在 gd32l23x.h 同目录（subcore/driver/CMSIS/）。

    \note    ⚠️ 别照抄 SDK 里的原版：那份把 ADC/CAN/CRC/DAC/DMA... 全include了，
             但那些 .c 我们根本没搬进 subcore/driver/stdperiph/Source/，
             一 include 就报找不到头文件。

             这里只列"已移植的 8 个"：exti / gpio / i2c / misc / pmu / rcu / rtc / usart
             以后要用新外设（比如 ADC 读电池），先搬 .c 和 .h，再回来加一行。
*/

#ifndef GD32L23X_LIBOPT_H
#define GD32L23X_LIBOPT_H

/* ── 已移植的外设（对应 subcore/driver/stdperiph/） ── */
#include "gd32l23x_exti.h"      /* 外部中断：MPU6050 运动/抬腕唤醒 */
#include "gd32l23x_gpio.h"      /* GPIO 配置 */
#include "gd32l23x_i2c.h"       /* I2C0 传感器总线 + I2C1 双核从机 */
#include "gd32l23x_misc.h"      /* NVIC 优先级分组 */
#include "gd32l23x_pmu.h"       /* 电源管理：Standby 进入 + 唤醒源 */
#include "gd32l23x_rcu.h"       /* 复位与时钟单元：IRC16M 选型 */
#include "gd32l23x_rtc.h"       /* RTC 闹钟定时唤醒 + 备份域 BKP */
#include "gd32l23x_usart.h"     /* 串口调试 */

#endif /* GD32L23X_LIBOPT_H */
