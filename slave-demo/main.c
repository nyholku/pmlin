/*
Copyright 2023 Planmeca Oy 

Author Kustaa Nyholm (kustaa.nyholm@planmeca.com)

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
   this list of conditions and the following disclaimer in the documentation 
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors 
   may be used to endorse or promote products derived from this software 
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. 

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY 
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF 
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <xc.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "pmlin.h"
#include "pmlin-slave.h"
#include "demo-device.h"
#include "stdbool.h"

#define CLK_FREQ 3333333.33333
#define USART0_BAUD_RATE(BAUD_RATE) ((float)((CLK_FREQ) * 64 / (16 * (float)(BAUD_RATE)) + 0.5))
#define TICK_IN_MICROSECONDS 500 // Note:TIMER0_PERIOD needs to fit in 16 bit so take care when changing 
#define TIMER0_PERIOD ((uint16_t)((TICK_IN_MICROSECONDS/1000000.0)/(1.0/CLK_FREQ)+0.5))  

uint8_t EEMEM g_device_ID = 1; // defaults for virgin devices

volatile uint16_t g_led_blink_cntr = 0;

inline void set_led(char set) {
    VPORTB.DIR |= 0x10;
    if (set)
        VPORTB.OUT &= ~0x10;
    else
        VPORTB.OUT |= 0x10;
}

/* Interrupt service routine for Receive Complete */
ISR(USART0_RXC_vect) {
    if (USART0.STATUS & USART_RXCIF_bm) {
        uint8_t byte_received = USART0.RXDATAL;
        bool break_detected = (USART0.STATUS & USART_BDF_bm) != 0;
        USART0.STATUS = USART_BDF_bm;
        PMLIN_UART_data_received_interrupt_handler(byte_received, break_detected);
    }
}

/* Interrupt service routine for Data Register Empty */
ISR(USART0_DRE_vect) {
    uint8_t data_to_send = PMLIN_UART_data_register_empty_interrupt_handler();
    USART0.TXDATAL = data_to_send;
}

void blink_led() {
    set_led(g_led_blink_cntr > 100000L/TICK_IN_MICROSECONDS);
    if (g_led_blink_cntr > 0) 
        g_led_blink_cntr--;
    else
        g_led_blink_cntr = 500000L / TICK_IN_MICROSECONDS; 

}

/* Interrupt service routine for TCA0 timer which polls the PMLIN k*/

ISR(TCA0_OVF_vect) {
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_OVF_bm;
    PMLIN_TIMER_interrupt_handler();
    blink_led();
}

void setup_USART0() {
    VPORTB.DIR |= 0x04; // PB2 TxD as output

    USART0.BAUD = (uint16_t) USART0_BAUD_RATE(PMLIN_BAURDATE); /* set baud rate register */

    USART0.CTRLA = 0 << USART_ABEIE_bp /* Auto-baud Error Interrupt Enable: disabled */
            | 0 << USART_DREIE_bp /* Data Register Empty Interrupt Enable: disabled */
            | 0 << USART_LBME_bp /* Loop-back Mode Enable: disabled */
            | USART_RS485_OFF_gc /* RS485 Mode disabled */
            | 1 << USART_RXCIE_bp /* Receive Complete Interrupt Enable: enabled */
            | 0 << USART_RXSIE_bp /* Receiver Start Frame Interrupt Enable: disabled */
            | 0 << USART_TXCIE_bp; /* Transmit Complete Interrupt Enable: disabled */

    USART0.CTRLB = 0 << USART_MPCM_bp /* Multi-processor Communication Mode: disabled */
            | 0 << USART_ODME_bp /* Open Drain Mode Enable: disabled */
            | 1 << USART_RXEN_bp /* Receiver enable: enabled */
            | USART_RXMODE_NORMAL_gc /* Normal mode */
            | 0 << USART_SFDEN_bp /* Start Frame Detection Enable: disabled */
            | 1 << USART_TXEN_bp; /* Transmitter Enable: enabled */
}

void USART_0_write(const uint8_t data) {
    while (!(USART0.STATUS & USART_DREIF_bm)) {
        /* wait */;
    }
}

ISR(AC2_AC_vect) {
    AC2.STATUS = 0x01;
    VPORTA.OUT &= ~0x10;
}

void setup_system_tick_TIMER0() {

    TCA0.SINGLE.INTCTRL = 0 << TCA_SINGLE_CMP0_bp
            | 0 << TCA_SINGLE_CMP1_bp
            | 0 << TCA_SINGLE_CMP2_bp
            | 1 << TCA_SINGLE_OVF_bp;

    TCA0.SINGLE.PER = TIMER0_PERIOD;

    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1_gc
            | 1 << TCA_SINGLE_ENABLE_bp;
}

void PMLIN_store_id(uint8_t id) {
    eeprom_write_byte(&g_device_ID, id);
}

uint8_t PMLIN_init_transfer(uint8_t msg_type) {
    return PMLIN_INIT_IGNORE_MSG;
}

void PMLIN_end_transfer(uint8_t msg_type) {
    //if (ASLAC_CONTROL_MSG_TYPE == msg_type) {
       // ASLAC_control_msg_t* msg = ((void*) &g_PMLIN_buffer);
    //}
}

int16_t PMLIN_get_byte_to_transmit_to_host() {
    if (g_PMLIN_trf_idx < g_PMLIN_trf_len)
        return g_PMLIN_buffer[g_PMLIN_trf_idx++];
    else
        return -1;
}

void PMLIN_UART_enable_data_register_empty_interrupt(bool enable_interrupt) {
    if (enable_interrupt)
        USART0.CTRLA |= USART_DREIE_bm;
    else
        USART0.CTRLA &= ~USART_DREIE_bm;
}

uint16_t PMLIN_random() {
    return (TCA0.SINGLE.CNT^(TCA0.SINGLE.CNTL << 8));
}

bool PMLIN_handle_byte_received_from_host(uint8_t data_in) {
    g_PMLIN_buffer[g_PMLIN_trf_idx++] = data_in;

    return g_PMLIN_trf_idx < g_PMLIN_trf_len;
}

bool PMLIN_handle_indicator_button(bool enable_indicator, bool set_indicator) {
    return 0;
}

int main(void) {

    setup_USART0();

    setup_system_tick_TIMER0();

    PMLIN_initialize(eeprom_read_byte(&g_device_ID), DEMO_DEVICE_DEVICE_TYPE, DEMO_DEVICE_FIRMWARE_VERSION,DEMO_DEVICE_HARDWARE_REVISION);

    PMLIN_set_timer_period(TICK_IN_MICROSECONDS);

    ei();

    while (1) {
    }
}


