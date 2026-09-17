/* hal_i2c0.c — I2C0 主机（传感器总线）
 *
 * PF6/PF7，400K 快速模式，上面挂一堆传感器。
 * 那几颗传感器的脾气各不相同，所以读写拆成三种模式，别硬套一种。
 *
 * 所有等待都带超时 —— 总线挂了宁可返回错误码，不能死等。
 */

#include "hal_i2c0.h"
#include "gd32l23x.h"
#include <stddef.h>   /* NULL —— 别指望 gd32l23x.h 会带进来 */

/*==============================================================================
 * 内部辅助函数
 *============================================================================*/

/* ★ 设计关键点：标志轮询都带超时，卡死也不阻塞 */
/**
 * @brief   带超时的标志位轮询
 * @return  0=成功, 1=超时
 */
static uint8_t i2c0_wait_flag(uint32_t flag, FlagStatus expect)
{
    /* 循环次数当时间使，跟主频绑死 —— 降频要重算 */
    uint32_t timeout = HAL_I2C0_TIMEOUT_MS * 2000U; /* ~50ms @ 16MHz */

    while (i2c_flag_get(I2C0, flag) != expect) {
        if (0U == timeout--) {
            return 1;
        }
    }
    return 0;
}

/**
 * @brief   查 NACK 并清掉
 * @return  0=OK, 1=NACK
 * @note    上层收到的是 ERR_BUSY，其实语义更像 ERR_NOT_FOUND。
 *          当初懒得挨个改调用方，先这么用着
 */
static uint8_t i2c0_check_nack(void)
{
    if (SET == i2c_flag_get(I2C0, I2C_FLAG_NACK)) {
        i2c_flag_clear(I2C0, I2C_FLAG_NACK);
        return 1;
    }
    return 0;
}

/*==============================================================================
 * 初始化 / 反初始化
 *============================================================================*/

ErrorCode_t HalI2c0_MasterInit(void)
{
    /* 1. 使能 GPIOF 和 I2C0 时钟 */
    rcu_periph_clock_enable(RCU_GPIOF);
    rcu_periph_clock_enable(RCU_I2C0);

    /* 2. 配置 PF6=SCL, PF7=SDA — 复用开漏 */
    gpio_af_set(GPIOF, GPIO_AF_4, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOF, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7);

    /* 3. 复位 I2C0 */
    i2c_deinit(I2C0);

    /* 4. 配置时序参数 */
    i2c_timing_config(I2C0, HAL_I2C0_PSC, HAL_I2C0_SCLDELY, HAL_I2C0_SDADELY);

    /* 5. 配置主模式时钟 (400KHz) */
    i2c_master_clock_config(I2C0, HAL_I2C0_SCLH, HAL_I2C0_SCLL);

    /* 6. 数字噪声滤波器（I2C0 这边是关的，只有从机 I2C1 那边才开） */
    i2c_digital_noise_filter_config(I2C0, HAL_I2C0_DNF);

    /* 7. 关闭模拟噪声滤波器（传感器总线走线短、干扰小） */
    i2c_analog_noise_filter_disable(I2C0);

    /* 8. 使能 I2C0 */
    i2c_enable(I2C0);

    /* 9. 自检 */
    if (0U == (I2C_CTL0(I2C0) & I2C_CTL0_I2CEN)) {
        return ERR_HARDWARE;
    }

    return ERR_OK;
}

/* TODO 只关了外设，PF6/PF7 还留在复用开漏上，没退回模拟输入。
 * 待机电流要是跟估算对不上，先来查这儿 */
ErrorCode_t HalI2c0_DeInit(void)
{
    i2c_disable(I2C0);

    if (0U != (I2C_CTL0(I2C0) & I2C_CTL0_I2CEN)) {
        return ERR_HARDWARE;
    }

    i2c_deinit(I2C0);
    return ERR_OK;
}

/*==============================================================================
 * 标准两阶段读取
 *
 * ★ 设计关键点：ReSTART 不发 STOP，否则复位
 *============================================================================*/

ErrorCode_t HalI2c0_MasterReadRegister(uint8_t addr_8bit,
                                       uint8_t reg,
                                       uint8_t *buf,
                                       uint16_t len)
{
    uint32_t addr_7bit;
    uint16_t i;

    if (NULL == buf || 0U == len) {
        return ERR_NULL_POINTER;
    }

    addr_7bit = HalI2c0_Addr8To7(addr_8bit);

    /* ---- 阶段一：写寄存器地址（不产生 STOP） ---- */
    i2c_master_addressing(I2C0, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(I2C0, 1U);
    i2c_automatic_end_disable(I2C0);

    i2c_start_on_bus(I2C0);

    if (0U != i2c0_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(I2C0);
        return ERR_TIMEOUT;
    }
    i2c_data_transmit(I2C0, (uint32_t)reg);

    if (0U != i2c0_check_nack()) {
        i2c_stop_on_bus(I2C0);
        return ERR_BUSY;
    }

    if (0U != i2c0_wait_flag(I2C_FLAG_TC, SET)) {
        i2c_stop_on_bus(I2C0);
        return ERR_TIMEOUT;
    }

    /* ---- 阶段二：读数据（自动 STOP） ---- */
    i2c_master_addressing(I2C0, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(I2C0, (uint32_t)len);
    i2c_automatic_end_enable(I2C0);

    i2c_start_on_bus(I2C0);

    for (i = 0U; i < len; i++) {
        if (0U != i2c0_wait_flag(I2C_FLAG_RBNE, SET)) {
            return ERR_TIMEOUT;   /* 这里没补 STOP，交给 BusReset 收拾 */
        }
        buf[i] = (uint8_t)i2c_data_receive(I2C0);
    }

    if (0U != i2c0_wait_flag(I2C_FLAG_TC, SET)) {
        return ERR_TIMEOUT;
    }

    return ERR_OK;
}

/*==============================================================================
 * 标准写入
 *============================================================================*/

ErrorCode_t HalI2c0_MasterWriteRegister(uint8_t addr_8bit,
                                        uint8_t reg,
                                        const uint8_t *buf,
                                        uint16_t len)
{
    uint32_t addr_7bit;
    uint16_t i;

    if (NULL == buf || 0U == len) {
        return ERR_NULL_POINTER;
    }

    addr_7bit = HalI2c0_Addr8To7(addr_8bit);

    i2c_master_addressing(I2C0, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(I2C0, (uint32_t)(1U + len));
    i2c_automatic_end_enable(I2C0);

    i2c_start_on_bus(I2C0);

    /* 写寄存器地址 */
    if (0U != i2c0_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(I2C0);
        return ERR_TIMEOUT;
    }
    i2c_data_transmit(I2C0, (uint32_t)reg);

    if (0U != i2c0_check_nack()) {
        i2c_stop_on_bus(I2C0);
        return ERR_BUSY;
    }

    /* 写数据 */
    for (i = 0U; i < len; i++) {
        if (0U != i2c0_wait_flag(I2C_FLAG_TBE, SET)) {
            return ERR_TIMEOUT;
        }
        i2c_data_transmit(I2C0, (uint32_t)buf[i]);

        if (0U != i2c0_check_nack()) {
            return ERR_BUSY;
        }
    }

    /* 等待 STOP */
    if (0U != i2c0_wait_flag(I2C_FLAG_TC, SET)) {
        return ERR_TIMEOUT;
    }

    return ERR_OK;
}

/*==============================================================================
 * 直接读取模式（SHT30 等命令式传感器）
 *
 * 没有寄存器地址，上来就是 START + 读地址 + 收数据
 *============================================================================*/

ErrorCode_t HalI2c0_MasterReadDirect(uint8_t addr_8bit,
                                     uint8_t *buf,
                                     uint16_t len)
{
    uint32_t addr_7bit;
    uint16_t i;

    if (NULL == buf || 0U == len) {
        return ERR_NULL_POINTER;
    }

    /* 命令式传感器：传入的是 8 位读地址（写地址 | 0x01） */
    addr_7bit = HalI2c0_Addr8To7(addr_8bit);

    i2c_master_addressing(I2C0, addr_7bit, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(I2C0, (uint32_t)len);
    i2c_automatic_end_enable(I2C0);

    i2c_start_on_bus(I2C0);

    for (i = 0U; i < len; i++) {
        if (0U != i2c0_wait_flag(I2C_FLAG_RBNE, SET)) {
            return ERR_TIMEOUT;
        }
        buf[i] = (uint8_t)i2c_data_receive(I2C0);
    }

    if (0U != i2c0_wait_flag(I2C_FLAG_TC, SET)) {
        return ERR_TIMEOUT;
    }

    return ERR_OK;
}

/*==============================================================================
 * 发送命令模式
 *
 * 只发不收。SHT30 的测量命令走这儿，发完等它转一会儿再 ReadDirect
 *============================================================================*/

ErrorCode_t HalI2c0_MasterSendCommand(uint8_t addr_8bit,
                                      uint8_t cmd_msb,
                                      uint8_t cmd_lsb)
{
    uint32_t addr_7bit;

    addr_7bit = HalI2c0_Addr8To7(addr_8bit);

    i2c_master_addressing(I2C0, addr_7bit, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(I2C0, 2U);
    i2c_automatic_end_enable(I2C0);

    i2c_start_on_bus(I2C0);

    /* 发送命令高字节 */
    if (0U != i2c0_wait_flag(I2C_FLAG_TBE, SET)) {
        i2c_stop_on_bus(I2C0);
        return ERR_TIMEOUT;
    }
    i2c_data_transmit(I2C0, (uint32_t)cmd_msb);

    if (0U != i2c0_check_nack()) {
        i2c_stop_on_bus(I2C0);
        return ERR_BUSY;
    }

    /* 发送命令低字节 */
    if (0U != i2c0_wait_flag(I2C_FLAG_TBE, SET)) {
        return ERR_TIMEOUT;
    }
    i2c_data_transmit(I2C0, (uint32_t)cmd_lsb);

    if (0U != i2c0_check_nack()) {
        return ERR_BUSY;
    }

    if (0U != i2c0_wait_flag(I2C_FLAG_TC, SET)) {
        return ERR_TIMEOUT;
    }

    return ERR_OK;
}

/*==============================================================================
 * 总线复位
 *============================================================================*/

/* ★ 设计关键点：9 拍 SCL，救回被拉死的从机 */
/**
 * @brief 总线复位
 * @note  1. 软件复位 I2C
 *        2. SCL/SDA 抢回来当 GPIO 使
 *        3. 打 9 个时钟脉冲（从机最多憋 8 位，第 9 个准松手）
 *        4. 补一个 STOP
 *        5. 重新初始化回 I2C
 *
 *        延时也是空循环，反正只在救火时调，不在乎这点时间
 */
void HalI2c0_BusReset(void)
{
    /* 1. 软件复位 */
    i2c_disable(I2C0);
    i2c_deinit(I2C0);

    /* 2. 将 SCL/SDA 临时切换为 GPIO */
    gpio_mode_set(GPIOF, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_output_options_set(GPIOF, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7);

    /* 3. 先拉高 SDA */
    gpio_bit_set(GPIOF, GPIO_PIN_7);

    /* 4. 打 9 个 SCL 脉冲。老注释说"每周期检查 SDA 是否释放"，
     *    实际没查，就是闷头打满 9 下 */
    for (int i = 0; i < 9; i++) {
        gpio_bit_reset(GPIOF, GPIO_PIN_6);  /* SCL LOW */
        /* 约 5us 延时（16MHz ≈ 80 cycles） */
        for (volatile int d = 0; d < 80; d++) { __NOP(); }
        gpio_bit_set(GPIOF, GPIO_PIN_6);    /* SCL HIGH */
        for (volatile int d = 0; d < 80; d++) { __NOP(); }
    }

    /* 5. 发送 STOP: SDA 在 SCL HIGH 期间从 LOW→HIGH */
    gpio_bit_reset(GPIOF, GPIO_PIN_7);  /* SDA LOW */
    for (volatile int d = 0; d < 80; d++) { __NOP(); }
    gpio_bit_set(GPIOF, GPIO_PIN_6);    /* SCL HIGH */
    for (volatile int d = 0; d < 80; d++) { __NOP(); }
    gpio_bit_set(GPIOF, GPIO_PIN_7);    /* SDA HIGH = STOP */

    /* 6. 重新初始化为 I2C */
    HalI2c0_MasterInit();
}
