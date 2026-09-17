/*!
    \file    main.c
    \brief   LPUART transmit and receive interrupt

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
#include "gd32l23x_eval.h"

#define ARRAYNUM(arr_nanme)      (uint32_t)(sizeof(arr_nanme) / sizeof(*(arr_nanme)))
#define TRANSMIT_SIZE            (ARRAYNUM(transmitter_buffer) - 1)

uint8_t transmitter_buffer[] = "\n\rLPUART interrupt test\n\r";
uint8_t receiver_buffer[32];
uint8_t transfersize = TRANSMIT_SIZE;
uint8_t receivesize = 32;
__IO uint8_t txcount = 0;
__IO uint16_t rxcount = 0;

void com_lpuart_init(void);

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
#ifdef GD32L233
    /* LPUART interrupt configuration */
    nvic_irq_enable(LPUART_IRQn, 0);
#else
    /* LPUART interrupt configuration */
    nvic_irq_enable(LPUART0_IRQn, 0);
#endif /* GD32L233 */
    /* initialize the com */
    com_lpuart_init();

    /* enable LPUART TBE interrupt */
    lpuart_interrupt_enable(LPUART0, LPUART_INT_TBE);

    /* wait until LPUART send the transmitter_buffer */
    while(txcount < transfersize) {
    }

    while(RESET == lpuart_flag_get(LPUART0, LPUART_FLAG_TC)) {
    }

    lpuart_interrupt_enable(LPUART0, LPUART_INT_RBNE);

    /* wait until lpuart receive the receiver_buffer */
    while(rxcount < receivesize) {
    }
    if(rxcount == receivesize) {
        /* initilize USART */
        gd_eval_com_init(EVAL_COM);
        
        printf("\n\rLPUART receive successfully!\n\r");
    }

    while(1) {
    }
}

/*!
    \brief      initialize the LPUART configuration of the com
    \param[in]  none
    \param[out] none
    \retval     none
*/
void com_lpuart_init(void)
{
    /* enable COM GPIO clock */
    rcu_periph_clock_enable(RCU_GPIOA);
    /* enable LPUART clock */
#ifdef GD32L233
    rcu_periph_clock_enable(RCU_LPUART);
#else
    rcu_periph_clock_enable(RCU_LPUART0);
#endif /* GD32L233 */
    /* connect port to LPUART TX */
    gpio_af_set(GPIOA, GPIO_AF_8, GPIO_PIN_2);
    /* connect port to LPUART RX */
    gpio_af_set(GPIOA, GPIO_AF_8, GPIO_PIN_3);

    /* configure LPUART TX as alternate function push-pull */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_2);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_2);

    /* configure LPUART RX as alternate function push-pull */
    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_PULLUP, GPIO_PIN_3);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_3);

    /* LPUART configure */
    lpuart_deinit(LPUART0);
    lpuart_word_length_set(LPUART0, LPUART_WL_8BIT);
    lpuart_stop_bit_set(LPUART0, LPUART_STB_1BIT);
    lpuart_parity_config(LPUART0, LPUART_PM_NONE);
    lpuart_baudrate_set(LPUART0, 115200U);
    lpuart_receive_config(LPUART0, LPUART_RECEIVE_ENABLE);
    lpuart_transmit_config(LPUART0, LPUART_TRANSMIT_ENABLE);

    lpuart_enable(LPUART0);
}
