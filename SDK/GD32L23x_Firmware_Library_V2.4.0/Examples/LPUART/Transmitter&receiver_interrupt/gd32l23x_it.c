/*!
    \file    gd32el23x_it.c
    \brief   interrupt service routines

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

#include "gd32l23x_it.h"

extern uint8_t transfersize;
extern uint8_t receivesize;
extern __IO uint8_t txcount;
extern __IO uint16_t rxcount;
extern uint8_t receiver_buffer[32];
extern uint8_t transmitter_buffer[];

/*!
    \brief      this function handles NMI exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void NMI_Handler(void)
{
    /* if NMI exception occurs, go to infinite loop */
    while(1){
    }
}

/*!
    \brief      this function handles HardFault exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void HardFault_Handler(void)
{
    /* if Hard Fault exception occurs, go to infinite loop */
    while(1){
    }
}

/*!
    \brief      this function handles SVC exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void SVC_Handler(void)
{
    /* if SVC exception occurs, go to infinite loop */
    while(1){
    }
}

/*!
    \brief      this function handles PendSV exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void PendSV_Handler(void)
{
    /* if PendSV exception occurs, go to infinite loop */
    while(1){
    }
}

/*!
    \brief      this function handles LPUART exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
#ifdef GD32L233
void LPUART_IRQHandler(void)
{
    if(RESET != lpuart_interrupt_flag_get(LPUART0, LPUART_INT_FLAG_RBNE)){
        /* receive data */
        receiver_buffer[rxcount++] = lpuart_data_receive(LPUART0);
        if(rxcount == receivesize){
            lpuart_interrupt_disable(LPUART0, LPUART_INT_RBNE);
        }
    }

    if(RESET != lpuart_interrupt_flag_get(LPUART0, LPUART_INT_FLAG_TBE)){
        /* transmit data */
        lpuart_data_transmit(LPUART0, transmitter_buffer[txcount++]);
        if(txcount == transfersize){
            lpuart_interrupt_disable(LPUART0, LPUART_INT_TBE);
        }
    }
}
#else
void LPUART0_IRQHandler(void)
{
    if(RESET != lpuart_interrupt_flag_get(LPUART0, LPUART_INT_FLAG_RBNE)){
        /* receive data */
        receiver_buffer[rxcount++] = lpuart_data_receive(LPUART0);
        if(rxcount == receivesize){
            lpuart_interrupt_disable(LPUART0, LPUART_INT_RBNE);
        }
    }

    if(RESET != lpuart_interrupt_flag_get(LPUART0, LPUART_INT_FLAG_TBE)){
        /* transmit data */
        lpuart_data_transmit(LPUART0, transmitter_buffer[txcount++]);
        if(txcount == transfersize){
            lpuart_interrupt_disable(LPUART0, LPUART_INT_TBE);
        }
    }
}
#endif /* GD32L233 */
