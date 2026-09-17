;/*!
;    \file    startup_gd32l235.S
;    \brief   start up file
;
;    \version 2026-02-02, V2.4.0, firmware for GD32L235
;*/
;
;/*
; * Copyright (c) 2009-2021 Arm Limited. All rights reserved.
; * Copyright (c) 2026, GigaDevice Semiconductor Inc.
; * SPDX-License-Identifier: Apache-2.0
; *
; * Licensed under the Apache License, Version 2.0 (the License); you may
; * not use this file except in compliance with the License.
; * You may obtain a copy of the License at
; *
; * www.apache.org/licenses/LICENSE-2.0
; *
; * Unless required by applicable law or agreed to in writing, software
; * distributed under the License is distributed on an AS IS BASIS, WITHOUT
; * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; * See the License for the specific language governing permissions and
; * limitations under the License.
; */
;/* This file refers the CMSIS standard, some adjustments are made according to GigaDevice chips */

  .syntax unified
  .cpu cortex-m23
  .fpu softvfp
  .thumb

.global  Default_Handler

/* necessary symbols defined in linker script to initialize data */
.word  _sidata
.word  _sdata
.word  _edata
.word  _sbss
.word  _ebss

  .section  .text.Reset_Handler
  .weak  Reset_Handler
  .type  Reset_Handler, %function

/* reset Handler */
Reset_Handler:
    ldr  r0, =0x1FFFF7E0
    ldr  r2, [r0]
    ldr  r0, = 0xFFFF0000
    ands  r2, r2, r0
    lsrs  r2, r2, #16
    lsls  r2, r2, #10
    ldr  r1, =0x20000000
    mov  r0, #0x00
Sram_Init:
    stm  r1!, {r0}
    subs  r2, r2, #4
    cmp  r2, #0x00
    bne  Sram_Init
    
  movs r1, #0
  b DataInit

CopyData:
    ldr  r3, =_sidata
    ldr  r3, [r3, r1]
    str  r3, [r0, r1]
    adds  r1, r1, #4

DataInit:
    ldr  r0, =_sdata
    ldr  r3, =_edata
    adds  r2, r0, r1
    cmp  r2, r3
    bcc  CopyData
    ldr  r2, =_sbss
    b  LoopFillZerobss

FillZerobss:
    movs  r3, #0
    str  r3, [r2]
    adds r2, r2, #4

LoopFillZerobss:
    ldr  r3, = _ebss
    cmp  r2, r3
    bcc  FillZerobss

/* Call SystemInit function */
    bl  SystemInit
/* Call static constructors */
    bl __libc_init_array
/*Call the main function */
    bl  main

    bx lr

.size  Reset_Handler, .-Reset_Handler

.section  .text.Default_Handler,"ax",%progbits

Default_Handler:
Infinite_Loop:
    b  Infinite_Loop
    .size  Default_Handler, .-Default_Handler

   .section  .vectors,"a",%progbits
   .global __gVectors

__gVectors:
                    .word _sp                                     /* Top of Stack */
                    .word Reset_Handler                           /* Reset Handler */
                    .word NMI_Handler                             /* NMI Handler */
                    .word HardFault_Handler                       /* Hard Fault Handler */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word SVC_Handler                             /* SVCall Handler */
                    .word 0                                       /* Reserved */
                    .word 0                                       /* Reserved */
                    .word PendSV_Handler                          /* PendSV Handler */
                    .word SysTick_Handler                         /* SysTick Handler */

                    /* External interrupts handler */
                    .word WWDGT_IRQHandler                        /* Vector Number 16,Window Watchdog Timer */
                    .word LVD_IRQHandler                          /* Vector Number 17,LVD through EXTI Line detect */
                    .word TAMPER_STAMP_IRQHandler                 /* Vector Number 18,RTC Tamper and TimeStamp through EXTI Line detect */
                    .word RTC_WKUP_IRQHandler                     /* Vector Number 19,RTC Wakeup from EXTI interrupt */
                    .word FMC_IRQHandler                          /* Vector Number 20,FMC global interrupt */
                    .word RCU_CTC_IRQHandler                      /* Vector Number 21,RCU or CTC global interrupt */
                    .word EXTI0_IRQHandler                        /* Vector Number 22,EXTI Line 0 */
                    .word EXTI1_IRQHandler                        /* Vector Number 23,EXTI Line 1 */
                    .word EXTI2_IRQHandler                        /* Vector Number 24,EXTI Line 2 */
                    .word EXTI3_IRQHandler                        /* Vector Number 25,EXTI Line 3 */
                    .word EXTI4_IRQHandler                        /* Vector Number 26,EXTI Line 4 */
                    .word DMA_Channel0_IRQHandler                 /* Vector Number 27,DMA Channel 0 */
                    .word DMA_Channel1_IRQHandler                 /* Vector Number 28,DMA Channel 1 */
                    .word DMA_Channel2_IRQHandler                 /* Vector Number 29,DMA Channel 2 */
                    .word DMA_Channel3_IRQHandler                 /* Vector Number 30,DMA Channel 3 */
                    .word DMA_Channel4_IRQHandler                 /* Vector Number 31,DMA Channel 4 */
                    .word DMA_Channel5_IRQHandler                 /* Vector Number 32,DMA Channel 5 */
                    .word DMA_Channel6_IRQHandler                 /* Vector Number 33,DMA Channel 6 */
                    .word ADC_IRQHandler                          /* Vector Number 34,ADC interrupt */
                    .word USBD_HP_CAN_TX_IRQHandler               /* Vector Number 35,USBD High Priority or CAN TX interrupt */
                    .word USBD_LP_CAN_RX0_IRQHandler              /* Vector Number 36,USBD Low Priority or CAN RX0 interrupt */
                    .word TIMER1_IRQHandler                       /* Vector Number 37,TIMER1 */
                    .word TIMER2_IRQHandler                       /* Vector Number 38,TIMER2 */
                    .word TIMER8_IRQHandler                       /* Vector Number 39,TIMER8 */
                    .word TIMER11_IRQHandler                      /* Vector Number 40,TIMER11 */
                    .word TIMER5_IRQHandler                       /* Vector Number 41,TIMER5 */
                    .word TIMER6_IRQHandler                       /* Vector Number 42,TIMER6 */
                    .word USART0_IRQHandler                       /* Vector Number 43,USART0 */
                    .word USART1_IRQHandler                       /* Vector Number 44,USART1 */
                    .word UART3_IRQHandler                        /* Vector Number 45,UART3 */
                    .word UART4_IRQHandler                        /* Vector Number 46,UART4 */
                    .word I2C0_EV_IRQHandler                      /* Vector Number 47,I2C0 Event */
                    .word I2C0_ER_IRQHandler                      /* Vector Number 48,I2C0 Error */
                    .word I2C1_EV_IRQHandler                      /* Vector Number 49,I2C1 Event */
                    .word I2C1_ER_IRQHandler                      /* Vector Number 50,I2C1 Error */
                    .word SPI0_IRQHandler                         /* Vector Number 51,SPI0 */
                    .word SPI1_IRQHandler                         /* Vector Number 52,SPI1 */
                    .word DAC_IRQHandler                          /* Vector Number 53,DAC */
                    .word 0                                       /* Vector Number 54,Reserved */
                    .word I2C2_EV_IRQHandler                      /* Vector Number 55,I2C2 Event */
                    .word I2C2_ER_IRQHandler                      /* Vector Number 56,I2C2 Error */
                    .word RTC_Alarm_IRQHandler                    /* Vector Number 57,RTC Alarm through EXTI Line detect */
                    .word USBD_WKUP_IRQHandler                    /* Vector Number 58,USBD wakeup through EXTI Line detect */
                    .word EXTI5_9_IRQHandler                      /* Vector Number 59,EXTI5 to EXTI9 */
                    .word TIMER0_TRG_CMT_UP_BRK_IRQHandler        /* Vector Number 60,TIMER0 trigger and Channel commutation or update or break interrupt */
                    .word TIMER0_Channel_IRQHandler               /* Vector Number 61,TIMER0 capture compare interrupt */
                    .word TIMER14_IRQHandler                      /* Vector Number 62,TIMER14 interrupt */
                    .word EXTI10_15_IRQHandler                    /* Vector Number 63,EXTI10 to EXT15 */
                    .word TIMER40_IRQHandler                      /* Vector Number 64,TIMER40 interrupt */
                    .word CAN_RX1_IRQHandler                      /* Vector Number 65,CAN RX1 interrupt */
                    .word CAN_EWMC_IRQHandler                     /* Vector Number 66,CAN EWMC interrupt */
                    .word 0                                       /* Vector Number 67,Reserved */
                    .word 0                                       /* Vector Number 68,Reserved */
                    .word 0                                       /* Vector Number 69,Reserved */
                    .word 0                                       /* Vector Number 70,Reserved */
                    .word DMAMUX_IRQHandler                       /* Vector Number 71,Reserved */
                    .word CMP0_IRQHandler                         /* Vector Number 72,Comparator 0 interrupt through EXTI Line detect */
                    .word CMP1_IRQHandler                         /* Vector Number 73,Comparator 1 interrupt through EXTI Line detect */
                    .word I2C0_WKUP_IRQHandler                    /* Vector Number 74,I2C0 Wakeup interrupt through EXTI Line detect */
                    .word I2C2_WKUP_IRQHandler                    /* Vector Number 75,I2C2 Wakeup interrupt through EXTI Line detect */
                    .word USART0_WKUP_IRQHandler                  /* Vector Number 76,USART0 Wakeup interrupt through EXTI Line detect */
                    .word LPUART0_IRQHandler                      /* Vector Number 77,LPUART0 global interrupt */
                    .word CAU_IRQHandler                          /* Vector Number 78,CAU */
                    .word TRNG_IRQHandler                         /* Vector Number 79,TRNG */
                    .word SLCD_IRQHandler                         /* Vector Number 80,SLCD */
                    .word USART1_WKUP_IRQHandler                  /* Vector Number 81,USART1 Wakeup interrupt through EXTI Line detect */
                    .word I2C1_WKUP_IRQHandler                    /* Vector Number 82,I2C1 Wakeup interrupt through EXTI Line detect */
                    .word LPUART0_WKUP_IRQHandler                 /* Vector Number 83,LPUART0 Wakeup interrupt through EXTI Line detect */
                    .word LPTIMER0_IRQHandler                     /* Vector Number 84,LPTIMER0 interrupt */
                    .word LPUART1_WKUP_IRQHandler                 /* Vector Number 85,LPUART1 Wakeup interrupt through EXTI Line detect */
                    .word LPTIMER1_IRQHandler                     /* Vector Number 86,LPTIMER1 interrupt */
                    .word LPUART1_IRQHandler                      /* Vector Number 87,LPUART1 global interrupt */

  .size   __gVectors, . - __gVectors

  .weak NMI_Handler
  .thumb_set NMI_Handler,Default_Handler

  .weak HardFault_Handler
  .thumb_set HardFault_Handler,Default_Handler

  .weak SVC_Handler
  .thumb_set SVC_Handler,Default_Handler

  .weak PendSV_Handler
  .thumb_set PendSV_Handler,Default_Handler

  .weak SysTick_Handler
  .thumb_set SysTick_Handler,Default_Handler

  .weak WWDGT_IRQHandler
  .thumb_set WWDGT_IRQHandler,Default_Handler

  .weak LVD_IRQHandler
  .thumb_set LVD_IRQHandler,Default_Handler

  .weak TAMPER_STAMP_IRQHandler
  .thumb_set TAMPER_STAMP_IRQHandler,Default_Handler

  .weak RTC_WKUP_IRQHandler
  .thumb_set RTC_WKUP_IRQHandler,Default_Handler

  .weak FMC_IRQHandler
  .thumb_set FMC_IRQHandler,Default_Handler

  .weak RCU_CTC_IRQHandler
  .thumb_set RCU_CTC_IRQHandler,Default_Handler

  .weak EXTI0_IRQHandler
  .thumb_set EXTI0_IRQHandler,Default_Handler

  .weak EXTI1_IRQHandler
  .thumb_set EXTI1_IRQHandler,Default_Handler

  .weak EXTI2_IRQHandler
  .thumb_set EXTI2_IRQHandler,Default_Handler

  .weak EXTI3_IRQHandler
  .thumb_set EXTI3_IRQHandler,Default_Handler

  .weak EXTI4_IRQHandler
  .thumb_set EXTI4_IRQHandler,Default_Handler

  .weak DMA_Channel0_IRQHandler
  .thumb_set DMA_Channel0_IRQHandler,Default_Handler

  .weak DMA_Channel1_IRQHandler
  .thumb_set DMA_Channel1_IRQHandler,Default_Handler

  .weak DMA_Channel2_IRQHandler
  .thumb_set DMA_Channel2_IRQHandler,Default_Handler

  .weak DMA_Channel3_IRQHandler
  .thumb_set DMA_Channel3_IRQHandler,Default_Handler

  .weak DMA_Channel4_IRQHandler
  .thumb_set DMA_Channel4_IRQHandler,Default_Handler

  .weak DMA_Channel5_IRQHandler
  .thumb_set DMA_Channel5_IRQHandler,Default_Handler

  .weak DMA_Channel6_IRQHandler
  .thumb_set DMA_Channel6_IRQHandler,Default_Handler

  .weak ADC_IRQHandler
  .thumb_set ADC_IRQHandler,Default_Handler

  .weak USBD_HP_CAN_TX_IRQHandler
  .thumb_set USBD_HP_CAN_TX_IRQHandler,Default_Handler

  .weak USBD_LP_CAN_RX0_IRQHandler
  .thumb_set USBD_LP_CAN_RX0_IRQHandler,Default_Handler

  .weak TIMER1_IRQHandler
  .thumb_set TIMER1_IRQHandler,Default_Handler

  .weak TIMER2_IRQHandler
  .thumb_set TIMER2_IRQHandler,Default_Handler

  .weak TIMER8_IRQHandler
  .thumb_set TIMER8_IRQHandler,Default_Handler

  .weak TIMER11_IRQHandler
  .thumb_set TIMER11_IRQHandler,Default_Handler

  .weak TIMER5_IRQHandler
  .thumb_set TIMER5_IRQHandler,Default_Handler

  .weak TIMER6_IRQHandler
  .thumb_set TIMER6_IRQHandler,Default_Handler

  .weak USART0_IRQHandler
  .thumb_set USART0_IRQHandler,Default_Handler

  .weak USART1_IRQHandler
  .thumb_set USART1_IRQHandler,Default_Handler

  .weak UART3_IRQHandler
  .thumb_set UART3_IRQHandler,Default_Handler

  .weak UART4_IRQHandler
  .thumb_set UART4_IRQHandler,Default_Handler

  .weak I2C0_EV_IRQHandler
  .thumb_set I2C0_EV_IRQHandler,Default_Handler

  .weak I2C0_ER_IRQHandler
  .thumb_set I2C0_ER_IRQHandler,Default_Handler

  .weak I2C1_EV_IRQHandler
  .thumb_set I2C1_EV_IRQHandler,Default_Handler

  .weak I2C1_ER_IRQHandler
  .thumb_set I2C1_ER_IRQHandler,Default_Handler

  .weak SPI0_IRQHandler
  .thumb_set SPI0_IRQHandler,Default_Handler

  .weak SPI1_IRQHandler
  .thumb_set SPI1_IRQHandler,Default_Handler

  .weak DAC_IRQHandler
  .thumb_set DAC_IRQHandler,Default_Handler

  .weak I2C2_EV_IRQHandler
  .thumb_set I2C2_EV_IRQHandler,Default_Handler

  .weak I2C2_ER_IRQHandler
  .thumb_set I2C2_ER_IRQHandler,Default_Handler

  .weak RTC_Alarm_IRQHandler
  .thumb_set RTC_Alarm_IRQHandler,Default_Handler

  .weak USBD_WKUP_IRQHandler
  .thumb_set USBD_WKUP_IRQHandler,Default_Handler

  .weak EXTI5_9_IRQHandler
  .thumb_set EXTI5_9_IRQHandler,Default_Handler

  .weak TIMER0_TRG_CMT_UP_BRK_IRQHandler
  .thumb_set TIMER0_TRG_CMT_UP_BRK_IRQHandler,Default_Handler

  .weak TIMER0_Channel_IRQHandler
  .thumb_set TIMER0_Channel_IRQHandler,Default_Handler

  .weak TIMER14_IRQHandler
  .thumb_set TIMER14_IRQHandler,Default_Handler

  .weak EXTI10_15_IRQHandler
  .thumb_set EXTI10_15_IRQHandler,Default_Handler

  .weak TIMER40_IRQHandler
  .thumb_set TIMER40_IRQHandler,Default_Handler

  .weak CAN_RX1_IRQHandler
  .thumb_set CAN_RX1_IRQHandler,Default_Handler

  .weak CAN_EWMC_IRQHandler
  .thumb_set CAN_EWMC_IRQHandler,Default_Handler

  .weak DMAMUX_IRQHandler
  .thumb_set DMAMUX_IRQHandler,Default_Handler

  .weak CMP0_IRQHandler
  .thumb_set CMP0_IRQHandler,Default_Handler

  .weak CMP1_IRQHandler
  .thumb_set CMP1_IRQHandler,Default_Handler

  .weak I2C0_WKUP_IRQHandler
  .thumb_set I2C0_WKUP_IRQHandler,Default_Handler

  .weak I2C2_WKUP_IRQHandler
  .thumb_set I2C2_WKUP_IRQHandler,Default_Handler

  .weak USART0_WKUP_IRQHandler
  .thumb_set USART0_WKUP_IRQHandler,Default_Handler

  .weak LPUART0_IRQHandler
  .thumb_set LPUART0_IRQHandler,Default_Handler

  .weak CAU_IRQHandler
  .thumb_set CAU_IRQHandler,Default_Handler

  .weak TRNG_IRQHandler
  .thumb_set TRNG_IRQHandler,Default_Handler

  .weak SLCD_IRQHandler
  .thumb_set SLCD_IRQHandler,Default_Handler

  .weak USART1_WKUP_IRQHandler
  .thumb_set USART1_WKUP_IRQHandler,Default_Handler

  .weak I2C1_WKUP_IRQHandler
  .thumb_set I2C1_WKUP_IRQHandler,Default_Handler

  .weak LPUART0_WKUP_IRQHandler
  .thumb_set LPUART0_WKUP_IRQHandler,Default_Handler

  .weak LPTIMER0_IRQHandler
  .thumb_set LPTIMER0_IRQHandler,Default_Handler

  .weak LPUART1_WKUP_IRQHandler
  .thumb_set LPUART1_WKUP_IRQHandler,Default_Handler

  .weak LPTIMER1_IRQHandler
  .thumb_set LPTIMER1_IRQHandler,Default_Handler

  .weak LPUART1_IRQHandler
  .thumb_set LPUART1_IRQHandler,Default_Handler

