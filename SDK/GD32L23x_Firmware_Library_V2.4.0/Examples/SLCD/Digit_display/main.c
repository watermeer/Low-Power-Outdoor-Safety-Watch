/*!
    \file    main.c
    \brief   SLCD digit display

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

#include "gd32l23x.h"
#include <stdio.h>
#include "systick.h"
#include "slcd_seg.h"

void rcu_configuration(void);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    uint32_t num = 0;
#ifdef GD32L235
    uint8_t percent = 0;
#endif /* GD32L235 */
    /* configure system clocks */
    rcu_configuration();
    /* configure systick: 1ms delay */
    systick_config();
    /* configure slcd interface */
    slcd_seg_configuration();
    /* clear the SLCD data register */
    slcd_seg_clear_all();
    while(1) {
        if(num > 999999) {
            num = 0;
        }
#ifdef GD32L235
        if(percent > 3) {
            percent = 0;
        }
        slcd_seg_display(percent++, num++);
#else
        slcd_seg_number_display(num++);
#endif /* GD32L235 */
        delay_1ms(1000);
    }
}

/*!
    \brief      configure the different system clocks
    \param[in]  none
    \param[out] none
    \retval     none
*/
void rcu_configuration(void)
{
    /* enable PMU clock */
    rcu_periph_clock_enable(RCU_PMU);
    /* enable PMU backup domain write */
    pmu_backup_write_enable();

    /* enable IRC32K */
    rcu_osci_on(RCU_IRC32K);
    /* wait for IRC40K stabilization flags */
    rcu_osci_stab_wait(RCU_IRC32K);
    /* configure the RTC clock source selection */
    rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);
}
