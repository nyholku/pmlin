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

#include "pmlin.h"
#include "pmlin-slave.h"
#include <stddef.h>

#define MY_DEBUG_SLOT 400

volatile int16_t g_PMLIN_my_id = 1;
volatile uint8_t g_PMLIN_trf_idx = 0;
volatile uint8_t g_PMLIN_trf_len = 0;
volatile uint8_t g_PMLIN_verf_idx = 0;
volatile uint8_t g_PMLIN_crc = 0;
volatile uint16_t g_PMLIN_timer = 0; // counts in micro seconds
volatile uint16_t g_PMLIN_timer_period = 1000; // in micro seconds
volatile uint16_t g_PMLIN_renum_to_id = 0;

volatile uint8_t g_PMLIN_buffer[PMLIN_BUFFER_SIZE];

#define PMLIN_STATE_WAIT_BREAK 0
#define PMLIN_STATE_RX_HEADER 1
#define PMLIN_STATE_RX_MSG 2
#define PMLIN_STATE_TX_MSG 3
#define PMLIN_STATE_RX_CTRL_MSG 4
#define PMLIN_STATE_TX_CTRL_RESP 5
#define PMLIN_STATE_WAIT_RENUM_TIMER 6
#define PMLIN_STATE_TX_RENUM_CONF 7
#define PMLIN_STATE_TX_ACK 8
#define PMLIN_STATE_CHECK_RX_MSG_CRC 9
#define PMLIN_STATE_CHECK_RX_CTRL_MSG_CRC 10

volatile uint8_t g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;

static uint16_t g_PMLIN_device_type;
static uint16_t g_PMLIN_firmware_version;
static uint8_t g_PMLIN_hardware_revision;

static unsigned char const g_PMLIN_crc8_table[256] = { //
		0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97, 0xb9, 0x88, 0xdb, 0xea, 0x7d, 0x4c, 0x1f, 0x2e, //
			0x43, 0x72, 0x21, 0x10, 0x87, 0xb6, 0xe5, 0xd4, 0xfa, 0xcb, 0x98, 0xa9, 0x3e, 0x0f, 0x5c, 0x6d, //
			0x86, 0xb7, 0xe4, 0xd5, 0x42, 0x73, 0x20, 0x11, 0x3f, 0x0e, 0x5d, 0x6c, 0xfb, 0xca, 0x99, 0xa8, //
			0xc5, 0xf4, 0xa7, 0x96, 0x01, 0x30, 0x63, 0x52, 0x7c, 0x4d, 0x1e, 0x2f, 0xb8, 0x89, 0xda, 0xeb, //
			0x3d, 0x0c, 0x5f, 0x6e, 0xf9, 0xc8, 0x9b, 0xaa, 0x84, 0xb5, 0xe6, 0xd7, 0x40, 0x71, 0x22, 0x13, //
			0x7e, 0x4f, 0x1c, 0x2d, 0xba, 0x8b, 0xd8, 0xe9, 0xc7, 0xf6, 0xa5, 0x94, 0x03, 0x32, 0x61, 0x50, //
			0xbb, 0x8a, 0xd9, 0xe8, 0x7f, 0x4e, 0x1d, 0x2c, 0x02, 0x33, 0x60, 0x51, 0xc6, 0xf7, 0xa4, 0x95, //
			0xf8, 0xc9, 0x9a, 0xab, 0x3c, 0x0d, 0x5e, 0x6f, 0x41, 0x70, 0x23, 0x12, 0x85, 0xb4, 0xe7, 0xd6, //
			0x7a, 0x4b, 0x18, 0x29, 0xbe, 0x8f, 0xdc, 0xed, 0xc3, 0xf2, 0xa1, 0x90, 0x07, 0x36, 0x65, 0x54, //
			0x39, 0x08, 0x5b, 0x6a, 0xfd, 0xcc, 0x9f, 0xae, 0x80, 0xb1, 0xe2, 0xd3, 0x44, 0x75, 0x26, 0x17, //
			0xfc, 0xcd, 0x9e, 0xaf, 0x38, 0x09, 0x5a, 0x6b, 0x45, 0x74, 0x27, 0x16, 0x81, 0xb0, 0xe3, 0xd2, //
			0xbf, 0x8e, 0xdd, 0xec, 0x7b, 0x4a, 0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xc2, 0xf3, 0xa0, 0x91, //
			0x47, 0x76, 0x25, 0x14, 0x83, 0xb2, 0xe1, 0xd0, 0xfe, 0xcf, 0x9c, 0xad, 0x3a, 0x0b, 0x58, 0x69, //
			0x04, 0x35, 0x66, 0x57, 0xc0, 0xf1, 0xa2, 0x93, 0xbd, 0x8c, 0xdf, 0xee, 0x79, 0x48, 0x1b, 0x2a, //
			0xc1, 0xf0, 0xa3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49, 0x1a, 0x2b, 0xbc, 0x8d, 0xde, 0xef, //
			0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24, 0x15, 0x3b, 0x0a, 0x59, 0x68, 0xff, 0xce, 0x9d, 0xac //
		};

static uint8_t PMLIN_crc8(uint8_t crc, uint8_t data) {
	return g_PMLIN_crc8_table[crc ^ data];
}

volatile uint8_t g_PMLIN_msg_type;
volatile uint8_t g_PMLIN_msg_id;

static void PMLIN_handle_id() {
	g_PMLIN_trf_idx = 0;
	g_PMLIN_crc = PMLIN_CRC_INIT_VAL;
	g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
	if (g_PMLIN_my_id == g_PMLIN_msg_id) {
		if (PMLIN_MESSAGE_TYPE_CMD == g_PMLIN_msg_type) {
			g_PMLIN_state = PMLIN_STATE_RX_CTRL_MSG;
			g_PMLIN_trf_len = PMLIN_CMD_MSG_LEN;
		} else {
			switch (PMLIN_init_transfer(g_PMLIN_msg_type)) {
			case PMLIN_INIT_RX_MSG:
				g_PMLIN_state = PMLIN_STATE_RX_MSG;
				break;
			case PMLIN_INIT_TX_MSG:
				g_PMLIN_state = PMLIN_STATE_TX_MSG;
				PMLIN_UART_enable_data_register_empty_interrupt(1);
				break;
			default:
				break;
			}
		}
	}
}

static void fill_buffer_with_random_data(uint8_t len) {
	while (len > 0)
		g_PMLIN_buffer[--len] = PMLIN_random();
}

static void PMLIN_handle_message() {
	g_PMLIN_trf_idx = 0;
	g_PMLIN_crc = PMLIN_CRC_INIT_VAL;

	switch (g_PMLIN_state) {
	case PMLIN_STATE_CHECK_RX_MSG_CRC:
		g_PMLIN_state = PMLIN_STATE_TX_ACK;
		PMLIN_UART_enable_data_register_empty_interrupt(1);
		PMLIN_end_transfer(g_PMLIN_msg_type);
		break;
	case PMLIN_STATE_CHECK_RX_CTRL_MSG_CRC: {
		uint8_t cmd = g_PMLIN_buffer[PMLIN_CMD_MSG_CMD_IDX];
		if (PMLIN_CMD_MSG_CMD_PROBE == cmd) {
			g_PMLIN_trf_len = PMLIN_CMD_RESP_LEN;
			fill_buffer_with_random_data(PMLIN_CMD_RESP_LEN);
			g_PMLIN_state = PMLIN_STATE_TX_CTRL_RESP;
			PMLIN_UART_enable_data_register_empty_interrupt(1);
			break;
		}
		if (PMLIN_CMD_MSG_CMD_RENUM == cmd) {
			g_PMLIN_renum_to_id = g_PMLIN_buffer[PMLIN_CMD_MSG_RENUM_ID_IDX];
			fill_buffer_with_random_data(PMLIN_CMD_RESP_LEN);
			g_PMLIN_state = PMLIN_STATE_WAIT_RENUM_TIMER;
			// random wait time is random [0..31] * 2.0 * UART char time in microseconds
			g_PMLIN_timer = (PMLIN_random() & 0x1F) * (uint16_t) (2.0 * 10 * 1000000.0 / PMLIN_BAUDRATE);
			break;
		}
		if (PMLIN_CMD_MSG_CMD_INQUIRE == cmd) {
            bool button = PMLIN_handle_indicator_button(//
                    PMLIN_CMD_MSG_INDCTR_ENABLE_MASK & g_PMLIN_buffer[PMLIN_CMD_MSG_INDCTR_IDX], //
                    PMLIN_CMD_MSG_INDCTR_CTRL_MASK & g_PMLIN_buffer[PMLIN_CMD_MSG_INDCTR_IDX]); //

			g_PMLIN_buffer[PMLIN_CMD_RESP_DEV_TYPE_MSB_IDX] = g_PMLIN_device_type >> 8;
			g_PMLIN_buffer[PMLIN_CMD_RESP_DEV_TYPE_LSB_IDX] = g_PMLIN_device_type & 0xFF;
			g_PMLIN_buffer[PMLIN_CMD_RESP_FW_VER_MSB_IDX] = g_PMLIN_firmware_version >> 8;
			g_PMLIN_buffer[PMLIN_CMD_RESP_FW_VER_LSB_IDX] = g_PMLIN_firmware_version & 0xFF;
            if (button)
                g_PMLIN_buffer[PMLIN_CMD_RESP_BUTTON_IDX] |= PMLIN_CMD_RESP_BUTTON_MASK;
            else
                g_PMLIN_buffer[PMLIN_CMD_RESP_BUTTON_IDX] &= ~PMLIN_CMD_RESP_BUTTON_MASK;

			g_PMLIN_state = PMLIN_STATE_TX_CTRL_RESP;
			PMLIN_UART_enable_data_register_empty_interrupt(1);
			break;
		}
		g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
		break;
	}
	default:
		g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
		break;
	}

}

void PMLIN_initialize(uint8_t id, uint16_t device_type, uint16_t firmware_version,int8_t hardware_revision) {
	if (id < 1 || id > 31)
		id = 1;
	g_PMLIN_my_id = id;
	g_PMLIN_device_type = device_type;
	g_PMLIN_firmware_version = firmware_version;
	g_PMLIN_hardware_revision = hardware_revision;
}

void PMLIN_set_timer_period(uint16_t period_in_usec) {
	g_PMLIN_timer_period = period_in_usec;
}

void PMLIN_UART_data_received_interrupt_handler(uint8_t data_in, bool break_detected) {
	if (break_detected) {
		g_PMLIN_state = PMLIN_STATE_RX_HEADER;
		g_PMLIN_crc = PMLIN_CRC_INIT_VAL;
		g_PMLIN_trf_idx = 0;
	}

	switch (g_PMLIN_state) {
	case PMLIN_STATE_RX_HEADER:
		g_PMLIN_crc = PMLIN_crc8(g_PMLIN_crc, data_in);
		g_PMLIN_buffer[g_PMLIN_trf_idx++] = data_in;
		if (g_PMLIN_trf_idx >= PMLIN_HEADER_LEN) {
			if (g_PMLIN_crc != 0) {
				g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
				break;
			}
			g_PMLIN_msg_type = (g_PMLIN_buffer[PMLIN_MSG_TYPE_AND_ID_IDX] & PMLIN_MSG_TYPE_MASK) >> PMLIN_MSG_TYPE_BITPOS;
			g_PMLIN_msg_id = (g_PMLIN_buffer[PMLIN_MSG_TYPE_AND_ID_IDX] & PMLIN_MSG_ID_MASK);
			PMLIN_handle_id();
		}
		break;
	case PMLIN_STATE_RX_MSG:
		g_PMLIN_crc = PMLIN_crc8(g_PMLIN_crc, data_in);
		if (!PMLIN_handle_byte_received_from_host(data_in))
			g_PMLIN_state = PMLIN_STATE_CHECK_RX_MSG_CRC;
		break;
	case PMLIN_STATE_RX_CTRL_MSG:
		g_PMLIN_crc = PMLIN_crc8(g_PMLIN_crc, data_in);
		g_PMLIN_buffer[g_PMLIN_trf_idx++] = data_in;
		if (g_PMLIN_trf_idx >= g_PMLIN_trf_len)
			g_PMLIN_state = PMLIN_STATE_CHECK_RX_CTRL_MSG_CRC;
		break;
	case PMLIN_STATE_CHECK_RX_CTRL_MSG_CRC: // fall through
	case PMLIN_STATE_CHECK_RX_MSG_CRC:
		g_PMLIN_crc = PMLIN_crc8(g_PMLIN_crc, data_in);
		if (g_PMLIN_crc != 0) {
			g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
			break;
		}
		PMLIN_handle_message();
		break;
	case PMLIN_STATE_WAIT_RENUM_TIMER:
		// we received a character while we were waiting our turn, so someone beat us to it
		g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
		break;
	case PMLIN_STATE_TX_RENUM_CONF:
		if (g_PMLIN_verf_idx < g_PMLIN_trf_len) {
			if (data_in != g_PMLIN_buffer[g_PMLIN_verf_idx++]) { // failed to get my data back
				g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
				break;
			}
		} else { // last byte = crc != 0x00 => fail)
			if (data_in != g_PMLIN_crc) {
				g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
				break;
			}
			g_PMLIN_my_id = g_PMLIN_renum_to_id;
			PMLIN_store_id(g_PMLIN_my_id);
			g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
		}

		break;
	default:
		break;
	}
}

uint8_t PMLIN_UART_data_register_empty_interrupt_handler() {
	if (PMLIN_STATE_TX_ACK == g_PMLIN_state) {
		g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
		PMLIN_UART_enable_data_register_empty_interrupt(0);
		return PMLIN_ACK_CHAR;
	}
	int16_t data;
	if (g_PMLIN_state == PMLIN_STATE_TX_MSG)
		data = PMLIN_get_byte_to_transmit_to_host();
	else {
		if (g_PMLIN_trf_idx < g_PMLIN_trf_len)
			data = g_PMLIN_buffer[g_PMLIN_trf_idx++];
		else
			data = -1;
	}

	if (data >= 0)
		g_PMLIN_crc = PMLIN_crc8(g_PMLIN_crc, data);
	else {
		data = g_PMLIN_crc;
		PMLIN_UART_enable_data_register_empty_interrupt(0);
		switch (g_PMLIN_state) {
		case PMLIN_STATE_TX_RENUM_CONF:
			// either break char or verification brings us out of this state
			break;
		case PMLIN_STATE_TX_MSG:
			PMLIN_end_transfer(g_PMLIN_msg_type);
			g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
			break;
		case PMLIN_STATE_TX_CTRL_RESP:
			g_PMLIN_state = PMLIN_STATE_WAIT_BREAK;
			break;
		default:
			break;
		}
	}
	return data;
}

void PMLIN_TIMER_interrupt_handler() {
	if (g_PMLIN_state == PMLIN_STATE_WAIT_RENUM_TIMER) {
		if (g_PMLIN_timer >= g_PMLIN_timer_period)
			g_PMLIN_timer -= g_PMLIN_timer_period;
		else {
			g_PMLIN_timer = 0;
			g_PMLIN_crc = PMLIN_CRC_INIT_VAL;
			g_PMLIN_trf_idx = 0;
			g_PMLIN_trf_len = PMLIN_CMD_RESP_LEN;
			g_PMLIN_verf_idx = 0;
			g_PMLIN_state = PMLIN_STATE_TX_RENUM_CONF;

			PMLIN_UART_enable_data_register_empty_interrupt(1);
		}
	}
}
