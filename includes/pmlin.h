/*
/Copyright 2023 Planmeca Oy

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

#ifndef __PMLIN_H__
#define	__PMLIN_H__

#include <stdint.h>

#define PMLIN_BAURDATE 38400

#define PMLIN_MAX_MESSAGE_TYPES 8

#define PMLIN_BROADCAST_ID 0
#define PMLIN_FIRST_DEVICE_ID 1
#define PMLIN_RESERVED_ID 31
#define PMLIN_MAX_NUM_ID 32

#define PMLIN_CRC_INIT_VAL 0xC5
#define PMLIN_ACK_CHAR 0x55
#define PMLIN_HEADER_LEN 2

#define PMLIN_MSG_TYPE_AND_ID_IDX 0
#define PMLIN_MSG_TYPE_MASK 0xE0
#define PMLIN_MSG_TYPE_BITPOS 5
#define PMLIN_MSG_ID_MASK 0x1F

#define PMLIN_MESSAGE_TYPE_CMD 7

#define PMLIN_CMD_MSG_LEN 5
#define PMLIN_CMD_MSG_CMD_IDX 0

#define PMLIN_CMD_MSG_CMD_PROBE 0
#define PMLIN_CMD_MSG_CMD_RENUM 1
#define PMLIN_CMD_MSG_CMD_INQUIRE 2

// for PMLIN_CMD_MSG_CMD_RENUM
#define PMLIN_CMD_MSG_RENUM_ID_IDX 1

// for PMLIN_CMD_MSG_CMD_INQUIRE
#define PMLIN_CMD_MSG_INDCTR_IDX 1
#define PMLIN_CMD_MSG_INDCTR_ENABLE_MASK 0x01
#define PMLIN_CMD_MSG_INDCTR_CTRL_MASK 0x02

#define PMLIN_CMD_RESP_LEN 5

// for accessing INQUIRY message response payload
#define PMLIN_CMD_RESP_DEV_TYPE_MSB_IDX 0
#define PMLIN_CMD_RESP_DEV_TYPE_LSB_IDX 1

#define PMLIN_CMD_RESP_FW_VER_MSB_IDX 2
#define PMLIN_CMD_RESP_FW_VER_LSB_IDX 3

#define PMLIN_CMD_RESP_HW_REV_IDX 4
#define PMLIN_CMD_RESP_HW_REV_MASK 0x7F

#define PMLIN_CMD_RESP_BUTTON_IDX 4
#define PMLIN_CMD_RESP_BUTTON_MASK 0x80

#define PMLIN_RESERVED_DEVICE_TYPE 0xFFFF
      
typedef volatile struct {
	uint8_t m_message_dir;
	uint32_t m_message_length;
} PMLIN_message_def_t;

typedef volatile struct {
	uint8_t m_id;
	uint16_t m_device_type;
	PMLIN_message_def_t m_messages[PMLIN_MAX_MESSAGE_TYPES];
} PMLIN_device_decl_t;

// Used to declare a message, for example usage see demo-device.h
#define PMLIN_MESSAGE_DEF(a,b,c) .m_messages[(a)] = ((PMLIN_message_def_t){ .m_message_dir = (b), .m_message_length=(c)})

// Used to declare a device, for example usage see demo-device.h
#define PMLIN_DECLARE_DEVICE(a,b,c,...) {.m_id=a, .m_device_type = b, c, __VA_ARGS__}

#define PMLIN_HOST_TO_SLAVE 0
#define PMLIN_SLAVE_TO_HOST 1

#endif /* PMLIN_MESSAGES */
