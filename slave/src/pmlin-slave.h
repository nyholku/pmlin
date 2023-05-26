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

#ifndef __PMLIN_SLAVE_H__
#define	__PMLIN_SLAVE_H__

#include <stdint.h>
#include <stdbool.h>
#include "pmlin.h"

// slave client code can (but does not need to) use following
#define PMLIN_BUFFER_SIZE 8
extern volatile uint8_t g_PMLIN_buffer[PMLIN_BUFFER_SIZE];
extern volatile uint8_t g_PMLIN_trf_idx;
extern volatile uint8_t g_PMLIN_trf_len;

// Implement this, PMLIN code calls this to allow the client code to prepare for data transfer based on the message_type.
// This function needs to set up buffers and pointers/indexes that PMLIN_handle_byte_received_from_host and
// PMLIN_get_byte_to_transmit_to_host need to perform their duties.
// For messages to be sent this function typically copies the data to a transmit buffer.

uint8_t PMLIN_init_transfer(uint8_t type);

// PMLIN_init_transfer needs to return one of following
#define PMLIN_INIT_IGNORE_MSG 0
#define PMLIN_INIT_RX_MSG 1
#define PMLIN_INIT_TX_MSG 2

// Implement this, PMLIN code calls this when the transfer is complete to allow the client to process the received message.
// Parameter message_type contains the type of the received message completed message. This is the same message_type
// that was passed to PMLIN_init_transfer, repeated here for convenience.
// For messages received all message processing typically happens within in this function call and for tramitted
// messages this is typically a no-operation.
void PMLIN_end_transfer(uint8_t message_type);

// Implement this, PMLIN code calls this from within the data received interrupt to deliver the payload to the client code
// this should return true if this was not the last byte of the message
bool PMLIN_handle_byte_received_from_host(uint8_t received_byte) ;

// Implement this, PMLIN code calls this to get next byte to send, return -1 when there are no more bytes to send
int16_t PMLIN_get_byte_to_transmit_to_host();

// Implement this, PMLIN code calls this to control the transmit data register empty UART interrupt
void PMLIN_UART_enable_data_register_empty_interrupt(bool enable_interrupt);


// Implement this, PMLIN code calls this get a random number in the range 0..65535
uint16_t PMLIN_random();

// Implement this, PMLIN code calls this to update indicator and read button status
// enabled_indicator should enable/disable the indicator and set_indicator should set or clear the indicator
// return value is true if the button is pressed
bool PMLIN_handle_indicator_button(bool enable_indicator, bool set_indicator);

// Implement this, PMLIN code calls this to store the slave_id into EEPROM when the slave is renumbered
void PMLIN_store_id(uint8_t slave_id);

// Call this to initialize PMLIN code and set the slave id, firmware_version in BCD
void PMLIN_initialize(uint8_t slave_id, uint16_t device_type, uint16_t firmware_version,int8_t hardware_revision);



// Call following interrupt handlers from the client code hardware interrupt handler


// This needs to be called when the DRE interrupt has been enabled and the UART is ready to receive next char
uint8_t PMLIN_UART_data_register_empty_interrupt_handler();

// This needs to be called when a new character has been received
// note that this must not be called for the break char but if the break has been detected then the 'break_detected'
// needs to be true when the first character of the header is delivered to PMLIN in the 'data_in'.
void PMLIN_UART_data_received_interrupt_handler(uint8_t data_in, bool break_detected);

// This needs to be called from a regular system tick interrupt
void PMLIN_TIMER_interrupt_handler();

// Call this to tell PMLIN code what is the timer interrupt period
void PMLIN_set_timer_period(uint16_t period_in_usec);

#endif	/* PMLIN_SLAVE_H */
