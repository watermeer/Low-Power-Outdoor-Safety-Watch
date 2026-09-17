/*!
    \file    readme.txt
    \brief   description of LPTIMER decoder mode example

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

  This example is based on the GD32L233R - EVAL - V1.0 / GD32L235R - EVAL - V1.0 board, it 
provides a description of LPTIMER decoder mode 0 example.

  LPTIMERN is used in decoder mode 0, it will count with two quadrature signals which input
of LPTIMERN_IN0 (PA6) and LPTIMERN_IN1 (PC2) pins.

  LPTIMERN frequency is set to internal clock IRC32K, the prescaler is 1, so the LPTIMERN
counter clock is 32KHz.

  LPTIMERN configuration:
  generate 1 PWM signal with the duty cycle:
  LPTIMERCLK = IRC32K / 1 = 32KHz.
  LPTIMERN_O duty cycle = (50 / 100) * 100  = 50 %.

  Connect the PA6 and PC2 with quadrature external signals.
  Connect the LPTIMERN_O pin (PA4) to an oscilloscope to monitor the PWM waveform.
