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

#include "pmlin-master.h"

#include "pmlin.h"
#include "aslac.h"
#include <string.h>
#include <stdio.h>

#define DEBUG_AUTOCONF

#ifdef DEBUG_AUTOCONF
#define ACD_PRINT(...) printf(__VA_ARGS__)
#else
#define ACD_PRINT(...)
#endif

#define BREAK_LEN 1
#define CRC_LEN 1
#define ACK_LEN 1

static void *g_PMLIN_mutex;

static bool g_PMLIN_initialized = false;
static PMLIN_send_break_fp PMLIN_send_break = NULL;
static PMLIN_write_fp PMLIN_write = NULL;
static PMLIN_read_fp PMLIN_read = NULL;
static PMLIN_mutex_fp PMLIN_lock_mutex = NULL;
static PMLIN_mutex_fp PMLIN_unlock_mutex = NULL;
static bool g_DEBUG_TRAFIC = 0;

static PMLIN_device_decl_t *g_PMLIN_id_to_device[PMLIN_MAX_NUM_ID];


static PMLIN_mirror_def_t *g_PMLIN_mirroring;
static uint8_t g_PMLIN_num_mirroring;

#define ACDPRINT(...) printf(__VA_ARGS__)

#define LOCK_MUTEX() do { \
	if (g_PMLIN_mutex) \
		PMLIN_lock_mutex(g_PMLIN_mutex); \
	} while(0)

#define UNLOCK_MUTEX() do { \
	if (g_PMLIN_mutex) \
		PMLIN_unlock_mutex(g_PMLIN_mutex); \
	} while(0)

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
				0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24, 0x15, 0x3b, 0x0a, 0x59, 0x68, 0xff, 0xce, 0x9d, 0xac  //
		};

uint8_t PMLIN_crc8(uint8_t crc, uint8_t data) {
	return g_PMLIN_crc8_table[crc ^ data];
}

void PMLIN_set_debug_trafic(bool debug_traffic) {
	g_DEBUG_TRAFIC = debug_traffic;
}
void PMLIN_initialize_master( //
		PMLIN_send_break_fp break_fp, //
		PMLIN_write_fp write_fp, //
		PMLIN_read_fp read_fp, //
		void *mutex, //
		PMLIN_mutex_fp lock_fp, //
		PMLIN_mutex_fp unlock_fp //
		) {
	PMLIN_send_break = break_fp;
	PMLIN_write = write_fp;
	PMLIN_read = read_fp;
	PMLIN_lock_mutex = lock_fp;
	PMLIN_unlock_mutex = unlock_fp;
	g_PMLIN_mutex = mutex;
	g_PMLIN_initialized = true;
}

PMLIN_error_t PMLIN_send_message(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data) {
	if (!g_PMLIN_initialized)
		return PMLIN_NO_INITIALIZED_ERROR;

	uint8_t buffer[1+PMLIN_HEADER_LEN+255+1];
	uint16_t sn = 0;
	uint8_t header = (type << PMLIN_MSG_TYPE_BITPOS) + id;
	buffer[sn++] = header;
	buffer[sn++] = PMLIN_crc8(PMLIN_CRC_INIT_VAL, header);
	uint8_t crc = PMLIN_CRC_INIT_VAL;
	for (uint16_t j = 0; j < len; j++) {
		uint8_t byte = data[j];
		crc = PMLIN_crc8(crc, byte);
		buffer[sn++] = byte;
	}
	buffer[sn++] = crc;
	LOCK_MUTEX();
	PMLIN_send_break();
	PMLIN_write(buffer, sn);
	uint16_t rn = BREAK_LEN + PMLIN_HEADER_LEN + len + CRC_LEN + ACK_LEN;
	uint16_t n = PMLIN_read(buffer, rn, PMLIN_TIMEOUT);
	UNLOCK_MUTEX();

	if (g_DEBUG_TRAFIC) {
		for (uint16_t i = 0; i < n; i++) {
			if (i < rn - 1)
				printf("(%02X) ", buffer[i]);
			else
				printf("[%02X] ", buffer[i]);
		}
		if (rn != n)
			printf("len!");
		else if (buffer[rn - 1] != PMLIN_ACK_CHAR)
			printf("ack!");
		else
			printf("ok");
		printf("\n");
	}
	if (sn + 1 == n)
		return PMLIN_NO_RESP_ERROR;
	else if (rn != n)
		return PMLIN_TIMEOUT_ERROR;
	else if (buffer[rn - 1] != PMLIN_ACK_CHAR)
		return PMLIN_NO_ACK_ERROR;
	else
		return PMLIN_OK;

}

PMLIN_error_t PMLIN_send_cmd_message(uint8_t id, volatile uint8_t *data, volatile uint8_t *resp) {
	if (!g_PMLIN_initialized)
		return PMLIN_NO_INITIALIZED_ERROR;
	uint8_t buffer[1 + PMLIN_HEADER_LEN + PMLIN_CMD_MSG_LEN + CRC_LEN + PMLIN_CMD_RESP_LEN + CRC_LEN];
	uint16_t sn = 0;
	uint8_t header = (PMLIN_MESSAGE_TYPE_CMD << PMLIN_MSG_TYPE_BITPOS) + id;
	buffer[sn++] = header;
	buffer[sn++] = PMLIN_crc8(PMLIN_CRC_INIT_VAL, header);
	uint8_t crc = PMLIN_CRC_INIT_VAL;
	for (uint16_t j = 0; j < PMLIN_CMD_MSG_LEN; j++) {
		uint8_t byte = data[j];
		crc = PMLIN_crc8(crc, byte);
		buffer[sn++] = byte;
	}
	buffer[sn++] = crc;
	LOCK_MUTEX();
	PMLIN_send_break();
	PMLIN_write(buffer, sn);
	uint16_t rn = sizeof(buffer) / sizeof(uint8_t);
	uint16_t n = PMLIN_read(buffer, rn, PMLIN_TIMEOUT);
	UNLOCK_MUTEX();
	crc = PMLIN_CRC_INIT_VAL;
	for (uint16_t i = 0; i < PMLIN_CMD_RESP_LEN + 1; i++) {
		uint8_t byte = buffer[BREAK_LEN + PMLIN_HEADER_LEN + PMLIN_CMD_MSG_LEN + CRC_LEN + i];
		crc = PMLIN_crc8(crc, byte);
	}
	for (uint16_t i = 0; i < PMLIN_CMD_RESP_LEN; i++)
		resp[i] = buffer[i + 1 + PMLIN_HEADER_LEN + PMLIN_CMD_MSG_LEN + CRC_LEN];

	if (g_DEBUG_TRAFIC) {
		for (uint16_t i = 0; i < n; i++) {
			if (i < BREAK_LEN + PMLIN_HEADER_LEN + PMLIN_CMD_MSG_LEN + CRC_LEN)
				printf("(%02X) ", buffer[i]);
			else
				printf("[%02X] ", buffer[i]);
		}
		if (rn != n)
			printf("len!");
		else if (crc)
			printf("crc!");
		else
			printf("ok");
		printf("\n");
	}
	if (sn + 1 == n)
		return PMLIN_NO_RESP_ERROR;
	else if (rn != n)
		return PMLIN_TIMEOUT_ERROR;
	else if (crc)
		return PMLIN_CRC_ERROR;
	else
		return PMLIN_OK;

}

PMLIN_error_t PMLIN_receive_message(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data) {
	if (!g_PMLIN_initialized)
		return PMLIN_NO_INITIALIZED_ERROR;
	uint8_t buffer[1+PMLIN_HEADER_LEN+255+1];
	uint16_t i = 0;
	uint8_t header = (type << PMLIN_MSG_TYPE_BITPOS) + id;

	buffer[i++] = header;
	buffer[i++] = PMLIN_crc8(PMLIN_CRC_INIT_VAL, header);

	uint16_t sn = i;
	LOCK_MUTEX();
	PMLIN_send_break();
	PMLIN_write(buffer, sn);

	uint16_t rn = BREAK_LEN + PMLIN_HEADER_LEN + len + CRC_LEN;
	uint16_t n = PMLIN_read(buffer, rn, PMLIN_TIMEOUT);
	UNLOCK_MUTEX();

	uint8_t crc = PMLIN_CRC_INIT_VAL;
	for (uint16_t i = 0; i < len + 1; i++) {
		uint8_t byte = buffer[BREAK_LEN + PMLIN_HEADER_LEN + i];
		crc = PMLIN_crc8(crc, byte);
	}

	if (g_DEBUG_TRAFIC) {
		for (uint16_t i = 0; i < n; i++) {
			if (i < BREAK_LEN + PMLIN_HEADER_LEN)
				printf("(%02X) ", buffer[i]);
			else
				printf("[%02X] ", buffer[i]);
		}
		if (rn != n)
			printf("len!");
		else if (crc)
			printf("crc!");
		else
			printf("ok");
		printf("\n");
	}

	memcpy((void* )data, (void* )&buffer[BREAK_LEN + PMLIN_HEADER_LEN], len);
	if (sn + 1 == n)
		return PMLIN_NO_RESP_ERROR;
	else if (rn != n)
		return PMLIN_TIMEOUT_ERROR;
	else if (crc)
		return PMLIN_CRC_ERROR;
	else
		return PMLIN_OK;
}

void PMLIN_define_devices(PMLIN_device_decl_t devices[], uint8_t num_devices) {
	for (uint8_t i = 0; i < PMLIN_MAX_NUM_ID; i++)
		g_PMLIN_id_to_device[i] = NULL;
	for (uint8_t i = 0; i < num_devices; i++) {
		g_PMLIN_id_to_device[devices[i].m_id] = &devices[i];
	}
}

void PMLIN_define_mirroring(PMLIN_mirror_def_t mirroring[], uint8_t num_mirroring) {
	g_PMLIN_mirroring = mirroring;
	g_PMLIN_num_mirroring = num_mirroring;
}

PMLIN_error_t PMLIN_mirror_tick(uint8_t *device_id_ptr) {
	for (uint8_t i = 0; i < g_PMLIN_num_mirroring; i++) {
		PMLIN_mirror_def_t *m = &g_PMLIN_mirroring[i];
		if (m->m_ticker)
			m->m_ticker--;
		else
			m->m_ticker = m->m_tick_period - 1;
		if (m->m_ticker == m->m_tick_phase) {
			uint8_t id = m->m_device_id;
			PMLIN_device_decl_t *d = g_PMLIN_id_to_device[id];
			if (d) {
				uint8_t mtype = m->m_message_type;
				PMLIN_error_t res;
				if (d->m_messages[mtype].m_message_dir == PMLIN_HOST_TO_SLAVE)
					res = PMLIN_send_message(id, mtype, d->m_messages[mtype].m_message_length, m->m_buffer);
				else
					res = PMLIN_receive_message(id, mtype, d->m_messages[mtype].m_message_length, m->m_buffer);
				if (res != PMLIN_OK) {
					if (device_id_ptr)
						*device_id_ptr = id;
					return res;
				}
			}
		}
	}
	return PMLIN_OK;
}

PMLIN_error_t PMLIN_renum_id(uint8_t old_id, uint8_t new_id) {
	uint16_t retry = 1000;
	PMLIN_error_t res = PMLIN_OK;
	while (retry) {
		uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
		uint8_t cmd_resp[PMLIN_CMD_MSG_LEN] = { 0 };
		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_RENUM;
		cmd_msg[PMLIN_CMD_MSG_RENUM_ID_IDX] = new_id;
		res = PMLIN_send_cmd_message(old_id, cmd_msg, cmd_resp);
		if (res == PMLIN_OK) {
			break;
		}
		retry--;
	}
	return res;
}

static PMLIN_error_t PMLIN_check_config_internal(uint8_t *device_id_ptr) {
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		if (id == PMLIN_RESERVED_ID)
			continue;
		if (!g_PMLIN_id_to_device[id])
			continue;
		if (device_id_ptr)
			*device_id_ptr = id;
		uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
		uint8_t cmd_resp[PMLIN_CMD_MSG_LEN] = { 0 };
		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_PROBE;
		PMLIN_error_t res = PMLIN_send_cmd_message(id, cmd_msg, cmd_resp);
		if (res != PMLIN_OK)
			return res;

		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_INQUIRE;
		res = PMLIN_send_cmd_message(id, cmd_msg, cmd_resp);
		if (res != PMLIN_OK)
			return res;
		uint16_t type = (cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_MSB_IDX] << 8) | cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_LSB_IDX];
		if (type != g_PMLIN_id_to_device[id]->m_device_type)
			return PMLIN_TYPE_CONFLICT_ERROR;

	}
	return PMLIN_OK;
}

PMLIN_error_t PMLIN_check_config(uint8_t *device_id_ptr) {
	LOCK_MUTEX();
	PMLIN_error_t res = PMLIN_check_config_internal(device_id_ptr);
	UNLOCK_MUTEX();
	return res;
}

static PMLIN_error_t PMLIN_auto_config_internal(PMLIN_error_t renum[]) {
	ACD_PRINT("PMLIN_autoconfig starting...\n");

	PMLIN_error_t ret = PMLIN_OK;
	PMLIN_error_t resp[PMLIN_MAX_NUM_ID];
	uint16_t type[PMLIN_MAX_NUM_ID];
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		renum[id] = PMLIN_OK;
		resp[id] = PMLIN_CRC_ERROR; // anything but OK or NO_RESP
		type[id] = PMLIN_RESERVED_DEVICE_TYPE;
	}

	renum:

//
// Scan for the devices
//

	ACD_PRINT("scanning devices\n");
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		if (id == PMLIN_RESERVED_ID)
			continue;
		if (resp[id] != PMLIN_OK && resp[id] != PMLIN_NO_RESP_ERROR) {
			uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
			uint8_t cmd_resp[PMLIN_CMD_MSG_LEN] = { 0 };

			ACD_PRINT(" probe device id = %d ", id);

			cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_PROBE;
			resp[id] = PMLIN_send_cmd_message(id, cmd_msg, cmd_resp);
			ACD_PRINT(",  send CMD_ENUM res: %s ", PMLIN_result_to_string(resp[id]));

			if (PMLIN_OK == resp[id]) {
				cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_INQUIRE;
				PMLIN_error_t res = PMLIN_send_cmd_message(id, cmd_msg, cmd_resp);
				ACD_PRINT(",  send CMD_INQUIRY, res: %s", PMLIN_result_to_string(res));

				if (PMLIN_OK == res) {
					type[id] = (cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_MSB_IDX] << 8) | cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_LSB_IDX];
					ACD_PRINT(",  device reported type %d", type[id]);

				}
			}
			ACD_PRINT("\n");
		}
	}

//
// Renumber one id conflict
//

	ACD_PRINT("renumber conflicting ids\n");

	uint8_t cnflct_id = 0;
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		if (id == PMLIN_RESERVED_ID)
			continue;
		ACD_PRINT(" check device id %d response %s\n", id, PMLIN_result_to_string(resp[id]));

		if (PMLIN_OK != resp[id] && PMLIN_NO_RESP_ERROR != resp[id]) {
			// we assume that if we get some response but not ok, then there is a conflict
			ACD_PRINT("  conflict detected id %d\n", id);

			cnflct_id = id;
			break;
		}
	}
	if (cnflct_id) {
		ACD_PRINT("find free id\n");

		uint8_t free_id = 0;
		for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
			if (id == PMLIN_RESERVED_ID)
				continue;
			if (PMLIN_NO_RESP_ERROR == resp[id]) {
				free_id = id;
				break;
			}
		}
		if (!free_id) {
			ACD_PRINT(" no free id found, returning with error code\n");

			return PMLIN_NO_FREE_ID_ERROR;
		} else {
			ACD_PRINT("try to renumber id %d to id %d\n", cnflct_id, free_id);

			PMLIN_error_t res = PMLIN_renum_id(cnflct_id, free_id);
			if (res != PMLIN_OK)
				return res;

			// mark both as renumbered so the caller knows that those may not be correct yet
			renum[cnflct_id] = PMLIN_ID_RENUM_WARNING;
			renum[free_id] = PMLIN_ID_RENUM_WARNING;
			// mark both for re-scanning
			resp[free_id] = PMLIN_CRC_ERROR;
			resp[cnflct_id] = PMLIN_CRC_ERROR;
			ret = PMLIN_ID_RENUM_WARNING;
			// restart
			goto renum;
		}
	}

//
// check for extra device ids
//
	uint8_t missing, extra;
	extra: //
	missing = 0;
	extra = 0;
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		if (id == PMLIN_RESERVED_ID)
			continue;
		PMLIN_device_decl_t *dev = g_PMLIN_id_to_device[id];
		if (dev != NULL && resp[id] == PMLIN_NO_RESP_ERROR)
			missing = id;
		if (dev == NULL && resp[id] == PMLIN_OK)
			extra = id;
		if (extra != 0 && missing != 0) {
			ACD_PRINT("try to renumber id %d to id %d\n", extra, missing);

			PMLIN_error_t res = PMLIN_renum_id(extra, missing);
			if (res != PMLIN_OK)
				return res;

			// mark both as renumbered so the caller knows that those may not be correct yet
			renum[extra] = PMLIN_ID_RENUM_WARNING;
			renum[missing] = PMLIN_ID_RENUM_WARNING;

			resp[missing] = PMLIN_OK;
			resp[extra] = PMLIN_NO_RESP_ERROR;

			type[missing] = type[extra];
			type[extra] = PMLIN_RESERVED_DEVICE_TYPE;
			ret = PMLIN_ID_RENUM_WARNING;
			// restart
			goto extra;
		}
	}

//
// Finally check for device type conflicts
//
	ACD_PRINT("check for type conflics\n");

	swap: ;
	uint16_t cnflct_id_1 = 0;
	for (uint8_t id = PMLIN_FIRST_DEVICE_ID; id < PMLIN_MAX_NUM_ID; id++) {
		if (id == PMLIN_RESERVED_ID)
			continue;
		if (PMLIN_OK == resp[id] && g_PMLIN_id_to_device[id]->m_device_type != type[id]) {
			ACD_PRINT(" type conflict %d was %d should have been %d\n", id, type[id], g_PMLIN_id_to_device[id]->m_device_type);

			cnflct_id_1 = id;
			break;
		}
	}

	if (cnflct_id_1) {
		ret = PMLIN_TYPE_CONFLICT_WARNING;
		// check if there is an other conflicting device of the same type so we could swap that to fix this
		uint8_t cnflct_id_2 = 0;
		uint16_t cnflct_id_1_type = g_PMLIN_id_to_device[cnflct_id_1]->m_device_type;
		for (uint8_t id = cnflct_id_1 + 1; id < PMLIN_RESERVED_ID; id++) {
			if (type[id] == cnflct_id_1_type) { // so we found a device that potentially could resolve this
				// if not in range or if it self is conflicting then we can use it
				if (type[id] != g_PMLIN_id_to_device[id]->m_device_type) {
					cnflct_id_2 = id;
					break;
				}
			}
		}

		if (!cnflct_id_2) {
			ACD_PRINT("non resolvable type conflict, return with error code\n");

			// fixme we should report for which device the conflict existed
			return PMLIN_TYPE_CONFLICT_ERROR;
		}

		ACD_PRINT(" swap id %d and id %d\n", cnflct_id_1, cnflct_id_2);

		// swap the conflicting ids
		uint8_t temp_id = 0;
		ACD_PRINT(" renum id %d => id %d\n", cnflct_id_1, temp_id);

		PMLIN_error_t res = PMLIN_renum_id(cnflct_id_2, temp_id);

		ACD_PRINT(" renum id %d => id %d\n", cnflct_id_1, cnflct_id_2);

		res = res == PMLIN_OK ? PMLIN_renum_id(cnflct_id_1, cnflct_id_2) : res;

		ACD_PRINT(" renum id %d and id %d\n", cnflct_id_1, cnflct_id_2);

		res = res == PMLIN_OK ? PMLIN_renum_id(temp_id, cnflct_id_1) : res;
		if (res != PMLIN_OK)
			return res;

		// and update the type info
		uint8_t tpy = type[cnflct_id_2];
		type[cnflct_id_2] = type[cnflct_id_1];
		type[cnflct_id_1] = tpy;

		goto swap;

	}

	return ret;
}

PMLIN_error_t PMLIN_auto_config(PMLIN_error_t renum[]) {
	LOCK_MUTEX();
	PMLIN_error_t res = PMLIN_auto_config_internal(renum);
	UNLOCK_MUTEX();
	return res;
}

void PMLIN_print_out_devices() {
	printf("PMLIN_print_out_devices\n");
	for (uint8_t i = 0; i <= PMLIN_MAX_NUM_ID; i++) {
		PMLIN_device_decl_t *p = g_PMLIN_id_to_device[i];
		if (!p)
			continue;
		printf("g_PMLIN_devices[%d]->m_device_type = %d\n", i, p->m_device_type);
		for (uint8_t j = 0; j < PMLIN_MAX_MESSAGE_TYPES; j++) {
			if (p->m_messages[j].m_message_length > 0) {
				printf("g_PMLIN_devices[%d]->m_messages[%d]\n", i, j);
				printf("	.m_message_dir    = %d\n", p->m_messages[j].m_message_dir);
				printf("	.m_message_length = %d\n", p->m_messages[j].m_message_length);
			}
		}
	}

	for (uint8_t i = 0; i < g_PMLIN_num_mirroring; i++) {
		PMLIN_mirror_def_t *p = &g_PMLIN_mirroring[i];
		printf("g_PMLIN_mirroring[%d]\n", i);
		printf("	.m_device_id    = %d\n", p->m_device_id);
		printf("	.m_message_type = %d\n", p->m_message_type);
		printf("	.m_tick_period  = %d\n", p->m_tick_period);
		printf("	.m_tick_phase   = %d\n", p->m_tick_phase);
		printf("	.m_ticker       = %d\n", p->m_ticker);
	}

}

char* PMLIN_result_to_string(PMLIN_error_t res) {
	switch (res) {
	case PMLIN_OK:
		return "PMLIN_OK";
	case PMLIN_CRC_ERROR:
		return "PMLIN_CRC_ERROR";
	case PMLIN_TIMEOUT_ERROR:
		return "PMLIN_TIMEOUT_ERROR";
	case PMLIN_NO_ACK_ERROR:
		return "PMLIN_NO_ACK_ERROR";
	case PMLIN_NO_RESP_ERROR:
		return "PMLIN_NO_RESP_ERROR";
	case PMLIN_NO_FREE_ID_ERROR:
		return "PMLIN_NO_FREE_ID_ERROR";
	case PMLIN_TYPE_CONFLICT_ERROR:
		return "PMLIN_TYPE_CONFLICT_ERROR";
	case PMLIN_TYPE_CONFLICT_WARNING:
		return "PMLIN_TYPE_CONFLICT_WARNING";
	case PMLIN_ID_RENUM_WARNING:
		return "PMLIN_ID_RENUM_WARNING";
	default:
		return "<UNKNOW ERRON RESULT CODE>";
	}
}
