/*!
    \file    slcd_seg.c
    \brief   source of the slcd segment driver

    \version 2026-02-02, V2.4.0, firmware for GD32L23x, add support for GD32L235
*/

/*
    Copyright (c) 2026, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#include "slcd_seg.h"

#ifdef GD32L233
/* digit SLCD DATA buffer */
uint8_t digit[4];

/* table of the digit code for SLCD */
__I uint32_t numbertable[10] = {
    /* 0     1     2     3     4 */
    0xDD, 0x88, 0x79, 0xE9, 0xAC,
    /* 5     6     7     8     9 */
    0xE5, 0xF5, 0x89, 0xFD, 0xED
};
#else
/* digit SLCD DATA buffer */
uint16_t digit[8];

/* table of the digit code for SLCD */
__I uint16_t numbertable[10] = {
    /* 0       1      2       3       4 */
    0xC993, 0x0880, 0xCA53, 0xCAC3, 0x0BC0,
    /* 5       6      7       8       9 */
    0xC3C3, 0xC3D3, 0xC880, 0xCBD3, 0xCBC3
};
#endif /* GD32L233 */

static void digit_to_code(uint8_t c);
static void slcd_gpio_config(void);
static void slcd_seg_digit_write(uint8_t ch, uint8_t position, slcd_display_enum type);

/*!
    \brief      convert digit to SLCD code
    \param[in]  the digit to write
    \param[out] none
    \retval     none
*/
#ifdef GD32L233
static void digit_to_code(uint8_t c)
{
    uint8_t ch = 0;

    /* the *c is a number */
    if(c < 10) {
        ch = numbertable[c];
    }

    digit[0] = (uint8_t)((ch) & 0x03);
    digit[1] = (uint8_t)((ch >> 2) & 0x03);
    digit[2] = (uint8_t)((ch >> 4) & 0x03);
    digit[3] = (uint8_t)((ch >> 6) & 0x03);
}
#else
static void digit_to_code(uint8_t c)
{
    uint16_t ch = 0;

    /* the *c is a number */
    if(c < 10) {
        ch = numbertable[c];
    }

    digit[0] = (uint16_t)((ch) & 0x03);
    digit[1] = (uint16_t)((ch >> 2) & 0x03);
    digit[2] = (uint16_t)((ch >> 4) & 0x03);
    digit[3] = (uint16_t)((ch >> 6) & 0x03);
    digit[4] = (uint16_t)((ch >> 8) & 0x03);
    digit[5] = (uint16_t)((ch >> 10) & 0x03);
    digit[6] = (uint16_t)((ch >> 12) & 0x03);
    digit[7] = (uint16_t)((ch >> 14) & 0x03);
}
#endif /* GD32L233 */
/*!
    \brief      init the GPIO port of SLCD peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void slcd_gpio_config(void)
{
    /* enable the clock of GPIO */
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
#ifdef GD32L233
    /* SLCD GPIO */
    /* configure GPIOA */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_af_set(GPIOA, GPIO_AF_3, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);

    /* configure GPIOB */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ,
                            GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_af_set(GPIOB, GPIO_AF_3, GPIO_PIN_1 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    /* configure GPIOC */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    gpio_af_set(GPIOC, GPIO_AF_3, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
#else
    /* configure SLCD COM */
    /* configure GPIOA: PA9-COM0, PA9-COM1, PA9-COM2 */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);

    /* configure GPIOB: PB9-COM3 */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_9);

    /* configure GPIOC: PC10-COM4, PC11-COM5, PC12-COM6 */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);

    /* configure GPIOD: PD2-COM7 */
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_2);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_2);

    gpio_af_set(GPIOA, GPIO_AF_3, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
    gpio_af_set(GPIOB, GPIO_AF_3, GPIO_PIN_9);
    gpio_af_set(GPIOC, GPIO_AF_3, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    gpio_af_set(GPIOD, GPIO_AF_3, GPIO_PIN_2);

    /* configure SLCD SEG */
    /* configure GPIOA */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_15);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_15);
    gpio_af_set(GPIOA, GPIO_AF_3, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_15);

    /* configure GPIOB */
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN,
                  GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 \
                  | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 \
                            | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
    gpio_af_set(GPIOB, GPIO_AF_3, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_11 \
                | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    /* configure GPIOC */
    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 \
                  | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 \
                            | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_af_set(GPIOC, GPIO_AF_3, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5 \
                | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);

    /* configure GPIOD */
    gpio_mode_set(GPIOD, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9);
    gpio_af_set(GPIOD, GPIO_AF_3, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_8 | GPIO_PIN_9);
#endif /* GD32L233 */
    /* configure pin of SLCD external voltage source */
    gpio_mode_set(GPIOD, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_6);
}

/*!
    \brief      configure SLCD peripheral
    \param[in]  none
    \param[out] none
    \retval     none
*/
void slcd_seg_configuration(void)
{
    volatile uint16_t i;

    /* configure the SLCD GPIO pins */
    slcd_gpio_config();

    /* enable the clock of SLCD */
    rcu_periph_clock_enable(RCU_SLCD);
    /* wait 2 RTC clock to write SLCD register */
    for(i = 0; i < 1000; i++);

    slcd_deinit();
    /* configure the prescaler and the divider of SLCD clock */
    slcd_clock_config(SLCD_PRESCALER_2, SLCD_DIVIDER_16);
    /* SLCD bias voltage select */
    slcd_bias_voltage_select(SLCD_BIAS_1_4);
#ifdef GD32L233
    /* select SLCD duty cycle */
    slcd_duty_select(SLCD_DUTY_1_4);
    /* configure SLCD dead time duration */
    slcd_dead_time_config(SLCD_DEADTIME_PERIOD_3);
    slcd_enhance_mode_enable();
#else
    /* select SLCD duty cycle */
    slcd_duty_select(SLCD_DUTY_1_8);
    /* configure SLCD dead time duration */
    slcd_dead_time_config(SLCD_DEADTIME_PERIOD_0);
#endif /* GD32L233 */
    /* select SLCD voltage source */
    slcd_voltage_source_select(SLCD_VOLTAGE_EXTERNAL);
    /* enable the permanent high drive */
    slcd_high_drive_config(ENABLE);
    /* wait for SLCD CFG register synchronization */
    while(!slcd_flag_get(SLCD_FLAG_SYN));
    /* enable SLCD interface */
    slcd_enable();
    /* wait for SLCD controller on flag */
    while(!slcd_flag_get(SLCD_FLAG_ON));
}

/*!
    \brief      write one digit to the SLCD DATA register
    \param[in]  ch: the digit to write
    \param[in]  position: position in the SLCD of the digit to write
    \param[out] none
    \retval     none
*/
void slcd_seg_digit_display(uint8_t ch, uint8_t position)
{
    /* wait the last SLCD DATA update request finished */
    while(slcd_flag_get(SLCD_FLAG_UPR));

    /* SLCD write a char */
    slcd_seg_digit_write(ch, position, INTEGER);

    /* request SLCD DATA update */
    slcd_data_update_request();
}

/*!
    \brief      write a integer(6 digits) to SLCD DATA register
    \param[in]  num: number to send to SLCD(0-999999)
    \param[out] none
    \retval     none
*/
void slcd_seg_number_display(uint32_t num)
{
    uint8_t i = 0x00, length, ch[6];

    ch[5] = num / 100000;
    ch[4] = (num % 100000) / 10000;
    ch[3] = (num % 10000) / 1000;
    ch[2] = (num % 1000) / 100;
    ch[1] = (num % 100) / 10;
    ch[0] = num % 10;

    if(ch[5]) {
        length = 6;
    } else if(ch[4]) {
        length = 5;
    } else if(ch[3]) {
        length = 4;
    } else if(ch[2]) {
        length = 3;
    } else if(ch[1]) {
        length = 2;
    } else {
        length = 1;
    }

    slcd_seg_digit_clear();
#ifdef GD32L233
    /* wait the last SLCD DATA update request finished */
    while(slcd_flag_get(SLCD_FLAG_UPR));
#endif /* GD32L233 */
    /* send the string character one by one to SLCD */
    while(i < length) {
        /* display one digit on SLCD */
        slcd_seg_digit_write(ch[i], 6 - i, INTEGER);
        /* increment the digit counter */
        i++;
    }
#ifdef GD32L233
    /* request SLCD DATA update */
    slcd_data_update_request();
#endif /* GD32L233 */
}

/*!
    \brief      write a float number(6 digits which has 2 decimal) to SLCD DATA register
    \param[in]  num: number to send to SLCD
    \param[out] none
    \retval     none
*/
void slcd_seg_floatnumber_display(float num)
{
    uint8_t i = 0x00, length, ch[6];
    uint32_t temp;

    temp = (uint32_t)(num * 100);
    ch[5] = temp / 100000;
    ch[4] = (temp % 100000) / 10000;
    ch[3] = (temp % 10000) / 1000;
    ch[2] = (temp % 1000) / 100;
    ch[1] = (temp % 100) / 10;
    ch[0] = temp % 10;

    if(ch[5]) {
        length = 6;
    } else if(ch[4]) {
        length = 5;
    } else if(ch[3]) {
        length = 4;
    } else {
        length = 3;
    }
    slcd_seg_digit_clear();
    /* wait the last SLCD DATA update request finished */
    while(slcd_flag_get(SLCD_FLAG_UPR));
    /* send the string character one by one to SLCD */
    while(i < length) {
        /* display one digit on SLCD */
        slcd_seg_digit_write(ch[i], 6 - i, FLOAT);
        /* increment the digit counter */
        i++;
    }
    /* request SLCD DATA update */
    slcd_data_update_request();
}

/*!
    \brief      write a digit to SLCD DATA register
    \param[in]  ch: the digit to write
    \param[in]  position: position in the SLCD of the digit to write,which can be 1..6
    \param[in]  type: the type of the data
    \param[out] none
    \retval     none
*/
#ifdef GD32L233
static void slcd_seg_digit_write(uint8_t ch, uint8_t position, slcd_display_enum type)
{
    /* convert ASCii to SLCD digit or char */
    digit_to_code(ch);

    switch(position) {
    case 6:
        /* clear the corresponding segments (COM0..COM3, SEG22..23) */
        SLCD_DATA0 &= (uint32_t)(0xFF3FFFFF);
        SLCD_DATA1 &= (uint32_t)(0xFF3FFFFF);
        SLCD_DATA2 &= (uint32_t)(0xFF3FFFFF);
        SLCD_DATA3 &= (uint32_t)(0xFF3FFFFF);

        /* write the colon of the time (COM0, SEG23) */
        if(type == TIME) {
            SLCD_DATA0 |= (uint32_t)((uint32_t)0x01 << 23);
        }
        /* write the corresponding segments (COM0..COM3, SEG22..23) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 22);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 22);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 22);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 22);
        break;

    case 5:
        /* clear the corresponding segments (COM0..COM3, SEG21..20) */
        SLCD_DATA0 &= (uint32_t)(0xFFCFFFFF);
        SLCD_DATA1 &= (uint32_t)(0xFFCFFFFF);
        SLCD_DATA2 &= (uint32_t)(0xFFCFFFFF);
        SLCD_DATA3 &= (uint32_t)(0xFFCFFFFF);

        /* write the corresponding segments (COM0..COM3, SEG21..20) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 20);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 20);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 20);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 20);
        break;

    case 4:
        /* clear the corresponding segments (COM0..COM3, SEG18..19) */
        SLCD_DATA0 &= (uint32_t)(0xFFF3FFFF);
        SLCD_DATA1 &= (uint32_t)(0xFFF3FFFF);
        SLCD_DATA2 &= (uint32_t)(0xFFF3FFFF);
        SLCD_DATA3 &= (uint32_t)(0xFFF3FFFF);

        /* write the point (COM0, SEG19) */
        if(type == FLOAT) {
            SLCD_DATA0 |= (uint32_t)(0x01 << 19);
        }
        /* write the corresponding segments (COM0..COM3, SEG18..19) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 18);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 18);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 18);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 18);
        break;

    case 3:
        /* clear the corresponding segments (COM0..COM3, SEG14..15) */
        SLCD_DATA0 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA1 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA2 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA3 &= (uint32_t)(0xFFFF3FFF);

        /* write the colon of the time (COM0, SEG15) */
        if(type == TIME) {
            SLCD_DATA0 |= (uint32_t)(0x01 << 15);
        }
        /* write the corresponding segments (COM0..COM3, SEG12..13) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 14);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 14);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 14);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 14);
        break;

    case 2:
        /* clear the corresponding segments (COM0..COM3, SEG12..13) */
        SLCD_DATA0 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA1 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA2 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA3 &= (uint32_t)(0xFFFFCFFF);

        /* write the corresponding segments (COM0..COM3, SEG12..13) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 12);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 12);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 12);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 12);
        break;


    case 1:
        /* clear the corresponding segments (COM0..COM3, SEG6/16) */
        SLCD_DATA0 &= (uint32_t)(0xFFFEFFBF);
        SLCD_DATA1 &= (uint32_t)(0xFFFEFFBF);
        SLCD_DATA2 &= (uint32_t)(0xFFFEFFBF);
        SLCD_DATA3 &= (uint32_t)(0xFFFEFFBF);

        /* write the corresponding segments (COM0..COM3, SEG6/16) */
        SLCD_DATA0 |= ((uint32_t)((digit[0] & 0x01) << 6)) | ((uint32_t)((digit[0] & 0x02) << 15));
        SLCD_DATA1 |= ((uint32_t)((digit[1] & 0x01) << 6)) | ((uint32_t)((digit[1] & 0x02) << 15));
        SLCD_DATA2 |= ((uint32_t)((digit[2] & 0x01) << 6)) | ((uint32_t)((digit[2] & 0x02) << 15));
        SLCD_DATA3 |= ((uint32_t)((digit[3] & 0x01) << 6)) | ((uint32_t)((digit[3] & 0x02) << 15));
        break;
    }
}
#else
static void slcd_seg_digit_write(uint8_t ch, uint8_t position, slcd_display_enum type)
{
    /* convert ASCii to SLCD digit or char */
    digit_to_code(ch);

    switch(position) {
    case 1:
        /* clear the corresponding segments (COM0..COM7, SEG12/13) */
        SLCD_DATA0 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA1 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA2 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA3 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA4 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA5 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA6 &= (uint32_t)(0xFFFFCFFF);
        SLCD_DATA7 &= (uint32_t)(0xFFFFCFFF);

        /* write the corresponding segments (COM0..COM7, SEG12/13) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 12);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 12);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 12);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 12);
        SLCD_DATA4 |= (uint32_t)(digit[4] << 12);
        SLCD_DATA5 |= (uint32_t)(digit[5] << 12);
        SLCD_DATA6 |= (uint32_t)(digit[6] << 12);
        SLCD_DATA7 |= (uint32_t)(digit[7] << 12);
        break;

    case 2:
        /* clear the corresponding segments (COM0..COM7, SEG14/15) */
        SLCD_DATA0 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA1 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA2 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA3 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA4 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA5 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA6 &= (uint32_t)(0xFFFF3FFF);
        SLCD_DATA7 &= (uint32_t)(0xFFFF3FFF);

        /* write the corresponding segments (COM0..COM7, SEG14/15) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 14);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 14);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 14);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 14);
        SLCD_DATA4 |= (uint32_t)(digit[4] << 14);
        SLCD_DATA5 |= (uint32_t)(digit[5] << 14);
        SLCD_DATA6 |= (uint32_t)(digit[6] << 14);
        SLCD_DATA7 |= (uint32_t)(digit[7] << 14);
        break;

    case 3:
        /* clear the corresponding segments (COM0..COM7, SEG30/31) */
        SLCD_DATA0 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA1 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA2 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA3 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA4 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA5 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA6 &= (uint32_t)(0x3FFFFFFF);
        SLCD_DATA7 &= (uint32_t)(0x3FFFFFFF);
        /* write the corresponding segments (COM0..COM7, SEG30/31) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 30);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 30);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 30);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 30);
        SLCD_DATA4 |= (uint32_t)(digit[4] << 30);
        SLCD_DATA5 |= (uint32_t)(digit[5] << 30);
        SLCD_DATA6 |= (uint32_t)(digit[6] << 30);
        SLCD_DATA7 |= (uint32_t)(digit[7] << 30);
        break;

    case 4:
        /* clear the corresponding segments (COM0..COM7, SEG24/25) */
        SLCD_DATA0 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA1 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA2 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA3 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA4 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA5 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA6 &= (uint32_t)(0xFCFFFFFF);
        SLCD_DATA7 &= (uint32_t)(0xFCFFFFFF);

        /* write the corresponding segments (COM0..COM7, SEG24/25) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 24);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 24);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 24);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 24);
        SLCD_DATA4 |= (uint32_t)(digit[4] << 24);
        SLCD_DATA5 |= (uint32_t)(digit[5] << 24);
        SLCD_DATA6 |= (uint32_t)(digit[6] << 24);
        SLCD_DATA7 |= (uint32_t)(digit[7] << 24);
        break;

    case 5:
        /* clear the corresponding segments (COM0..COM7, SEG26/27) */
        SLCD_DATA0 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA1 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA2 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA3 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA4 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA5 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA6 &= (uint32_t)(0xF3FFFFFF);
        SLCD_DATA7 &= (uint32_t)(0xF3FFFFFF);

        /* write the corresponding segments (COM0..COM7, SEG26/27) */
        SLCD_DATA0 |= (uint32_t)(digit[0] << 26);
        SLCD_DATA1 |= (uint32_t)(digit[1] << 26);
        SLCD_DATA2 |= (uint32_t)(digit[2] << 26);
        SLCD_DATA3 |= (uint32_t)(digit[3] << 26);
        SLCD_DATA4 |= (uint32_t)(digit[4] << 26);
        SLCD_DATA5 |= (uint32_t)(digit[5] << 26);
        SLCD_DATA6 |= (uint32_t)(digit[6] << 26);
        SLCD_DATA7 |= (uint32_t)(digit[7] << 26);
        break;

    case 6:
        /* clear the corresponding segments (COM0..COM7, SEG17/28) */
        SLCD_DATA0 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA1 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA2 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA3 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA4 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA5 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA6 &= (uint32_t)(0xEFFDFFFF);
        SLCD_DATA7 &= (uint32_t)(0xEFFDFFFF);
        /* write the point (COM4, SEG6) */
        if(type == FLOAT) {
            SLCD_DATA3 |= (uint32_t)(0x01 << 6);
        }
        /* write the corresponding segments (COM0..COM7, SEG17/28) */
        SLCD_DATA0 |= ((uint32_t)((digit[0] & 0x01) << 17)) | ((uint32_t)((digit[0] & 0x02) << 27));
        SLCD_DATA1 |= ((uint32_t)((digit[1] & 0x01) << 17)) | ((uint32_t)((digit[1] & 0x02) << 27));
        SLCD_DATA2 |= ((uint32_t)((digit[2] & 0x01) << 17)) | ((uint32_t)((digit[2] & 0x02) << 27));
        SLCD_DATA3 |= ((uint32_t)((digit[3] & 0x01) << 17)) | ((uint32_t)((digit[3] & 0x02) << 27));
        SLCD_DATA4 |= ((uint32_t)((digit[4] & 0x01) << 17)) | ((uint32_t)((digit[4] & 0x02) << 27));
        SLCD_DATA5 |= ((uint32_t)((digit[5] & 0x01) << 17)) | ((uint32_t)((digit[5] & 0x02) << 27));
        SLCD_DATA6 |= ((uint32_t)((digit[6] & 0x01) << 17)) | ((uint32_t)((digit[6] & 0x02) << 27));
        SLCD_DATA7 |= ((uint32_t)((digit[7] & 0x01) << 17)) | ((uint32_t)((digit[7] & 0x02) << 27));
        break;

    default:
        break;
    }
}
#endif /* GD32L233 */
/*!
    \brief      clear data in the SLCD DATA register
    \param[in]  position: position in the SLCD of the digit to write,which can be 1..6
    \param[out] none
    \retval     none
*/
void slcd_seg_digit_clear(void)
{
#ifdef GD32L233
    /* clear the corresponding segments (COM0..COM3, SEG23..SEG18/SEG16..SEG12/SEG6) */
    SLCD_DATA0 &= (uint32_t)(0xFF828FBF);
    SLCD_DATA1 &= (uint32_t)(0xFF020FBF);
    SLCD_DATA2 &= (uint32_t)(0xFF020FBF);
    SLCD_DATA3 &= (uint32_t)(0xFF020FBF);
#else
    /* clear the corresponding segments (COM0..COM7, SEG31/30/28..24/17/15..12/6) */
    SLCD_DATA0 &= (uint32_t)(0x20FD0FFF);
    SLCD_DATA1 &= (uint32_t)(0x20FD0FFF);
    SLCD_DATA2 &= (uint32_t)(0x20FD0FFF);
    SLCD_DATA3 &= (uint32_t)(0x20FD0FFF);
    SLCD_DATA4 &= (uint32_t)(0x20FD0FBF);
    SLCD_DATA5 &= (uint32_t)(0x20FD0FFF);
    SLCD_DATA6 &= (uint32_t)(0xFFFFCFFF);
    SLCD_DATA7 &= (uint32_t)(0x20FD0FFF);
#endif /* GD32L233 */
}

/*!
    \brief      clear all the SLCD DATA register
    \param[in]  none
    \param[out] none
    \retval     none
*/
void slcd_seg_clear_all(void)
{
#ifdef GD32L233
    SLCD_DATA0 = 0x00000000;
    SLCD_DATA1 = 0x00000000;
    SLCD_DATA2 = 0x00000000;
    SLCD_DATA3 = 0x00000000;
#else
    SLCD_DATA0 = 0x00000000;
    SLCD_DATA1 = 0x00000000;
    SLCD_DATA2 = 0x00000000;
    SLCD_DATA3 = 0x00000000;
    SLCD_DATA4 = 0x00000000;
    SLCD_DATA5 = 0x00000000;
    SLCD_DATA6 = 0x00000000;
    SLCD_DATA7 = 0x00000000;
#endif /* GD32L233 */
}

#ifdef GD32L235
/*!
    \brief      displays the percentage on the SLCD screen
    \param[in]  position: position in the SLCD screen, which can be 0..3
    \param[out] none
    \retval     none
*/
void slcd_seg_percent_display(uint8_t position)
{
    switch(position) {
    case 0:
        /* clear the corresponding segments (COM0..COM7, SEG11/SEG10/SEG6) */
        SLCD_DATA0 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA1 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA2 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA3 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA4 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA5 &= (uint32_t)(0xFFFFF3BF);
        SLCD_DATA6 &= (uint32_t)(0xFFFFF3BF);
        SLCD_DATA7 &= (uint32_t)(0xFFFFF3BF);
        /* write the corresponding segments (COM0..COM7, SEG11) */
        SLCD_DATA7 |= (uint32_t)1 << 11;
        break;

    case 1:
        /* clear the corresponding segments (COM0..COM4/COM7, SEG11) */
        SLCD_DATA0 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA1 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA2 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA3 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA4 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA6 &= (uint32_t)(0xFFFFF7FF);
        SLCD_DATA7 &= (uint32_t)(0xFFFFF7FF);

        /* write the corresponding segments (COM0..COM4, SEG11) */
        SLCD_DATA0 |= (uint32_t)1 << 11;
        SLCD_DATA1 |= (uint32_t)1 << 11;
        SLCD_DATA2 |= (uint32_t)1 << 11;
        SLCD_DATA3 |= (uint32_t)1 << 11;
        SLCD_DATA4 |= (uint32_t)1 << 11;
        SLCD_DATA6 |= (uint32_t)1 << 11;
        break;

    case 2:
        /* clear the corresponding segments (COM2..COM6, SEG11/SEG10) */
        SLCD_DATA2 &= (uint32_t)(0xFFFFFBFF);
        SLCD_DATA3 &= (uint32_t)(0xFFFFFBFF);
        SLCD_DATA4 &= (uint32_t)(0xFFFFFBFF);
        SLCD_DATA5 &= (uint32_t)(0xFFFFF3FF);
        SLCD_DATA6 &= (uint32_t)(0xFFFFF7FF);

        /* write the corresponding segments (COM2..COM6, SEG11/SEG10) */

        SLCD_DATA2 |= (uint32_t)1 << 10;
        SLCD_DATA3 |= (uint32_t)1 << 10;
        SLCD_DATA4 |= (uint32_t)1 << 10;
        SLCD_DATA5 |= (uint32_t)3 << 10;
        SLCD_DATA6 |= (uint32_t)1 << 10;
        break;

    case 3:
        /* clear the corresponding segments (COM0/COM2/COM5..COM7, SEG10/SEG6) */
        SLCD_DATA0 &= (uint32_t)(0xFFFFFBFF);
        SLCD_DATA1 &= (uint32_t)(0xFFFFFBFF);
        SLCD_DATA5 &= (uint32_t)(0xFFFFFFBF);
        SLCD_DATA6 &= (uint32_t)(0xFFFFFBBF);
        SLCD_DATA7 &= (uint32_t)(0xFFFFFFBF);

        /* write the corresponding segments (COM0/COM2/COM5..COM7, SEG10/SEG6) */
        SLCD_DATA0 |= (uint32_t)1 << 10;
        SLCD_DATA1 |= (uint32_t)1 << 10;
        SLCD_DATA5 |= (uint32_t)1 << 6;
        SLCD_DATA6 |= (uint32_t)1 << 6;
        SLCD_DATA7 |= 0x00000440;
        break;
    default:
        break;
    }
}

/*!
    \brief      slcd signal display
    \param[in]  none
    \param[out] none
    \retval     none
*/
void slcd_seg_signal_display(void)
{
    uint16_t num_temp  = 0;

    /* clear the corresponding segments (COM0..COM7, SEG0..SEG5/SEG7..SEG9/SEG16/SEG18..SEG23/SEG29) */
    SLCD_DATA0 &= (uint32_t)(0xDF03FC40);
    SLCD_DATA1 &= (uint32_t)(0xDF0EFC40);
    SLCD_DATA2 &= (uint32_t)(0xDF0EFC40);
    SLCD_DATA3 &= (uint32_t)(0xDF02FC40);
    SLCD_DATA4 &= (uint32_t)(0xDF02FC40);
    SLCD_DATA5 &= (uint32_t)(0xDF02FC40);
    SLCD_DATA6 &= (uint32_t)(0xDF02FC40);
    SLCD_DATA7 &= (uint32_t)(0xDF02FC40);
    /* write the corresponding segments (COM0..COM7, SEG29/SEG21..SEG18/SEG16/SEG9..SEG07/SEG3..SEG0) */
    SLCD_DATA0 |= (uint32_t)0x2038038F;
    SLCD_DATA1 |= (uint32_t)0x2039038F;
    SLCD_DATA2 |= (uint32_t)0x2039038F;
    SLCD_DATA3 |= (uint32_t)0x203D038F;
    SLCD_DATA4 |= (uint32_t)0x203D038F;
    SLCD_DATA5 |= (uint32_t)0x203D038F;
    SLCD_DATA6 |= (uint32_t)0x203C038F;
    SLCD_DATA7 |= (uint32_t)0x2039038F;

    /* write the corresponding segments (COM0..COM7, SEG5/SEG23) */
    num_temp = numbertable[1];
    SLCD_DATA0 |= ((uint32_t)((num_temp >> 0) & 0x0001) << 5) | ((uint32_t)((num_temp >> 0) & 0x0002) << 22);
    SLCD_DATA1 |= ((uint32_t)((num_temp >> 2) & 0x0001) << 5) | ((uint32_t)((num_temp >> 2) & 0x0002) << 22);
    SLCD_DATA2 |= ((uint32_t)((num_temp >> 4) & 0x0001) << 5) | ((uint32_t)((num_temp >> 4) & 0x0002) << 22);
    SLCD_DATA3 |= ((uint32_t)((num_temp >> 6) & 0x0001) << 5) | ((uint32_t)((num_temp >> 6) & 0x0002) << 22);
    SLCD_DATA4 |= ((uint32_t)((num_temp >> 8) & 0x0001) << 5) | ((uint32_t)((num_temp >> 8) & 0x0002) << 22);
    SLCD_DATA5 |= ((uint32_t)((num_temp >> 10) & 0x0001) << 5) | ((uint32_t)((num_temp >> 10) & 0x0002) << 22);
    SLCD_DATA6 |= ((uint32_t)((num_temp >> 12) & 0x0001) << 5) | ((uint32_t)((num_temp >> 12) & 0x0002) << 22);
    SLCD_DATA7 |= ((uint32_t)((num_temp >> 14) & 0x0001) << 5) | ((uint32_t)((num_temp >> 14) & 0x0002) << 22);
    /* write the corresponding segments (COM0..COM7, SEG22/SEG4) */
    num_temp = numbertable[8];
    SLCD_DATA0 |= ((uint32_t)((num_temp >> 1) & 0x0001) << 4) | ((uint32_t)((num_temp >> 0) & 0x0001) << 22);
    SLCD_DATA1 |= ((uint32_t)((num_temp >> 3) & 0x0001) << 4) | ((uint32_t)((num_temp >> 2) & 0x0001) << 22);
    SLCD_DATA2 |= ((uint32_t)((num_temp >> 5) & 0x0001) << 4) | ((uint32_t)((num_temp >> 4) & 0x0001) << 22);
    SLCD_DATA3 |= ((uint32_t)((num_temp >> 7) & 0x0001) << 4) | ((uint32_t)((num_temp >> 6) & 0x0001) << 22);
    SLCD_DATA4 |= ((uint32_t)((num_temp >> 9) & 0x0001) << 4) | ((uint32_t)((num_temp >> 8) & 0x0001) << 22);
    SLCD_DATA5 |= ((uint32_t)((num_temp >> 11) & 0x0001) << 4) | ((uint32_t)((num_temp >> 10) & 0x0001) << 22);
    SLCD_DATA6 |= ((uint32_t)((num_temp >> 13) & 0x0001) << 4) | ((uint32_t)((num_temp >> 12) & 0x0001) << 22);
    SLCD_DATA7 |= ((uint32_t)((num_temp >> 15) & 0x0001) << 4) | ((uint32_t)((num_temp >> 14) & 0x0001) << 22);
}

/*!
    \brief      slcd display
    \param[in]  position: position in the SLCD screen, which can be 0..3
    \param[in]  num: number of SLCD disaplay(0-999999)
    \param[out] none
    \retval     none
*/
void slcd_seg_display(uint8_t position, uint32_t num)
{
    /* wait the last SLCD DATA update request finished */
    while(slcd_flag_get(SLCD_FLAG_UPR));
    /* write a integer(6 digits) to SLCD DATA register */
    slcd_seg_number_display(num);
    /* displays the percentage on the SLCD screen */
    slcd_seg_percent_display(position);
    slcd_seg_signal_display();
    /* request SLCD DATA update */
    slcd_data_update_request();
}
#endif /* GD32L235 */
