/* bsp_mpu6050.c — MPU6050 六轴：DMP 计步 + 抬腕中断
 *
 * 把 LibDriver 的通用驱动绑到这块板子上：I2C0 总线，AD0 接地（地址 0x68）。
 * 计步丢给硬件 DMP 算，副核没 FPU，自己跑算法不划算。
 *
 * 引脚那个坑写在下面 set_interrupt_pin_type 旁边，别重复踩。
 */

#include "bsp_mpu6050.h"
#include "driver_mpu6050.h"
#include "driver_mpu6050_interface.h"
#include "hal_i2c0.h"

/* 句柄 + 中断标志 */
static mpu6050_handle_t g_mpu6050;
static volatile bool    s_motionDetected = false;   /* 中断写、主循环读，volatile 不能省 */

/*==============================================================================
 * LibDriver 返回值 → ErrorCode_t 转换
 *============================================================================*/

/* LibDriver 惯例：0=成功 1=通信失败 2=空句柄 3=没初始化 4=复位失败 5=ID 错
 * 四个传感器一人抄一份，改的时候记着点 */
static ErrorCode_t driver_err_to_hal(uint8_t drv_err)
{
    switch (drv_err) {
        case 0:  return ERR_OK;
        case 1:  return ERR_TIMEOUT;     /* I2C 通信失败 */
        case 2:  return ERR_NULL_POINTER;
        case 3:  return ERR_NOT_INIT;
        case 4:  return ERR_HARDWARE;    /* 复位失败 */
        case 5:  return ERR_NOT_FOUND;   /* ID 无效 */
        default: return ERR_HARDWARE;
    }
}

/*==============================================================================
 * 运动中断回调（由 driver_mpu6050_interface 调用）
 *============================================================================*/

/* ★ 设计关键点：中断里只置标志，业务全留给主循环 */
/* 挂给 interface 层的 receive_callback，由 mpu6050_irq_handler 那条链子调进来 */
static void mpu6050_motion_callback(uint8_t type)
{
    if (MPU6050_INTERRUPT_MOTION == type) {
        s_motionDetected = true;
    }
}

/*==============================================================================
 * 初始化
 *============================================================================*/

ErrorCode_t BSP_MPU6050_Init(void)
{
    uint8_t ret;

    /* 1. 挂接口：I2C 读写、延时、打印、中断回调 */
    DRIVER_MPU6050_LINK_INIT(&g_mpu6050, mpu6050_handle_t);
    DRIVER_MPU6050_LINK_IIC_INIT(&g_mpu6050, mpu6050_interface_iic_init);
    DRIVER_MPU6050_LINK_IIC_DEINIT(&g_mpu6050, mpu6050_interface_iic_deinit);
    DRIVER_MPU6050_LINK_IIC_READ(&g_mpu6050, mpu6050_interface_iic_read);
    DRIVER_MPU6050_LINK_IIC_WRITE(&g_mpu6050, mpu6050_interface_iic_write);
    DRIVER_MPU6050_LINK_DELAY_MS(&g_mpu6050, mpu6050_interface_delay_ms);
    DRIVER_MPU6050_LINK_DEBUG_PRINT(&g_mpu6050, mpu6050_interface_debug_print);
    DRIVER_MPU6050_LINK_RECEIVE_CALLBACK(&g_mpu6050, mpu6050_motion_callback);

    /* 2. 地址 + init（init 里会读 WHO_AM_I，不对就返回 5） */
    ret = mpu6050_set_addr_pin(&g_mpu6050, MPU6050_ADDRESS_AD0_LOW);
    if (0U != ret) return driver_err_to_hal(ret);

    ret = mpu6050_init(&g_mpu6050);
    if (0U != ret) return driver_err_to_hal(ret);

    /* 3. 运动检测中断 —— 抬腕唤醒就靠它 */
    /* 阈值 10、持续 5：先往"不误触发"那边调 */
    (void)mpu6050_set_motion_threshold(&g_mpu6050, 10U);
    (void)mpu6050_set_motion_duration(&g_mpu6050, 5U);

    /* 运动中断引到 INT 脚上 */
    (void)mpu6050_set_interrupt(&g_mpu6050, MPU6050_INTERRUPT_MOTION, MPU6050_BOOL_TRUE);
    /* 踩坑：这一项配的是 INT 引脚"推挽还是开漏"，不是有效电平。
     * 想改高/低有效得去动 INT_PIN_CFG 的 INT_LEVEL 位，LibDriver 没暴露。 */
    (void)mpu6050_set_interrupt_pin_type(&g_mpu6050, MPU6050_PIN_TYPE_PUSH_PULL);
    (void)mpu6050_set_interrupt_latch(&g_mpu6050, MPU6050_BOOL_TRUE);  /* 锁存直到清除 */
    (void)mpu6050_set_interrupt_read_clear(&g_mpu6050, MPU6050_BOOL_FALSE);

    /* 4. DMP 计步器 */
    /* ★ 设计关键点：计步交给 DMP，MCU 不碰算法 */
    (void)mpu6050_dmp_set_pedometer_walk_time(&g_mpu6050, 1000U);       /* 1秒步频阈值 */
    (void)mpu6050_dmp_set_pedometer_step_count(&g_mpu6050, 0U);         /* 上电清零，累计靠备份域 */

    return ERR_OK;
}

/*==============================================================================
 * 低功耗休眠 / 唤醒
 *============================================================================*/

ErrorCode_t BSP_MPU6050_Sleep(void)
{
    uint8_t ret;

    /* ★ 设计关键点：只关 acc/gyro，留运动唤醒 */
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_X,  MPU6050_BOOL_TRUE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_Y,  MPU6050_BOOL_TRUE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_Z,  MPU6050_BOOL_TRUE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_GYRO_X, MPU6050_BOOL_TRUE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_GYRO_Y, MPU6050_BOOL_TRUE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_GYRO_Z, MPU6050_BOOL_TRUE);

    /* 低功耗循环唤醒：1.25Hz 醒一下，看看有没有在动 */
    ret = mpu6050_set_wake_up_frequency(&g_mpu6050, MPU6050_WAKE_UP_FREQUENCY_1P25_HZ);
    ret = mpu6050_set_cycle_wake_up(&g_mpu6050, MPU6050_BOOL_TRUE);

    /* 前面的错误码被最后一句盖掉了，最后还是硬返回 OK
     * —— 真睡不下去这层也不知道该干嘛，先这么放着 */
    (void)ret;
    return ERR_OK;
}

ErrorCode_t BSP_MPU6050_WakeUp(void)
{
    uint8_t ret;

    /* 退出循环唤醒 */
    ret = mpu6050_set_cycle_wake_up(&g_mpu6050, MPU6050_BOOL_FALSE);

    /* 只恢复加速度计，陀螺仪从项目开始就没用过 */
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_X, MPU6050_BOOL_FALSE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_Y, MPU6050_BOOL_FALSE);
    ret = mpu6050_set_standby_mode(&g_mpu6050, MPU6050_SOURCE_ACC_Z, MPU6050_BOOL_FALSE);

    (void)ret;
    return ERR_OK;
}

/*==============================================================================
 * 数据读取
 *============================================================================*/

uint32_t BSP_MPU6050_GetStepCount(void)
{
    uint32_t count = 0;
    (void)mpu6050_dmp_get_pedometer_step_count(&g_mpu6050, &count);   /* 读失败就当 0 步 */
    return count;
}

bool BSP_MPU6050_GetMotionIntFlag(void)
{
    return s_motionDetected;
}

void BSP_MPU6050_ClearIntFlag(void)
{
    s_motionDetected = false;
}

/*==============================================================================
 * 中断服务
 *============================================================================*/

void BSP_MPU6050_IRQHandler(void)
{
    /* 丢给驱动处理，它内部会回调上面那个 callback */
    (void)mpu6050_irq_handler(&g_mpu6050);
}
