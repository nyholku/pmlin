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

#ifndef __PMLIN_HOST_H__
#define	__PMLIN_HOST_H__

#include <stdint.h>
#include <stdbool.h>
#include "pmlin.h"



typedef uint8_t PMLIN_error_t ; // return type from various functions aka error code, see below

// all possible error codes (PMLIN_error_t values)  returned by various functions
#define PMLIN_OK 0		// operation was succesful
#define PMLIN_CRC_ERROR 1 // CRC error in PMLIN_receive_message or PMLIN_send_message (echoed message was erronous)
#define PMLIN_TIMEOUT_ERROR 2 // Timeout ie not enough data received in PMLIN_receive_message
#define PMLIN_NO_ACK_ERROR 3 // The slave did not acknowledge a message in PMLIN_send_message
#define PMLIN_NO_RESP_ERROR 4 // No response was received from slave in PMLIN_send_message
#define PMLIN_NO_FREE_ID_ERROR 5 // No free ID could be found when renumbering slaves in PMLIN_auto_config
#define PMLIN_TYPE_CONFLICT_ERROR 6 // A slave responded with an unexpected type in PMLIN_check_config
#define PMLIN_NO_INITIALIZED_ERROR 7 // PMLIN master library has not been initalized with PMLIN_initialize_master

#define PMLIN_TYPE_CONFLICT_WARNING 128 // At least one slave had a conflicting type in PMLIN_auto_config
#define PMLIN_ID_RENUM_WARNING 129  // At least one slave was given a new ID in PMLIN_auto_config

#define PMLIN_TIMEOUT 1000000 // read message timeout value in micro seconds

// this structure holds  device mirroring info, i.e. automatic transfers
typedef struct PMLIN_mirror_def_t {
	volatile uint8_t m_device_id; // the device id
	volatile uint8_t m_message_type; // message type (type implicitly defines  transfer direction)
	volatile uint8_t *m_buffer; // pointer to buffer from which or to which message data is transferred
	volatile uint16_t m_tick_period; // how often the message is transferred, expressed in calls to PMLIN_mirror_tick()
	volatile uint16_t m_tick_phase; // a transfer takes place when m_tick_phase == m_ticker
	volatile uint16_t m_ticker; // down counter [0..m_tick_period[ decremented in PMLIN_mirror_tick()
} PMLIN_mirror_def_t;

// macro used to declare and define one message mirroring, used to to define an array of mirroring ops, see pmlin-mirror-demo.c
#define PMLIN_MIRROR_DEF(device_id, message_type, buffer, tick_period, tick_phase) ((PMLIN_mirror_def_t) { \
	.m_device_id = device_id, \
	.m_message_type = message_type, \
	.m_buffer = (volatile uint8_t *)buffer, \
	.m_tick_period = tick_period, \
	.m_tick_phase = tick_phase \
	})

// Purpose: send a message to a slave
//		This call blocks until the message has been sent
// Parameters:
// 		id (in)				Target slave id
//		type (in)			Message type
//		len (in)			Message payload pointer
//		data (out)			Pointer to message buffer which holds the message payload data to be sent
//	Returns:				Error code, see below and top of this header
//		PMLIN_NO_RESP_ERROR
//		PMLIN_TIMEOUT_ERROR
//		PMLIN_CRC_ERROR
//		PMLIN_OK
//

PMLIN_error_t PMLIN_send_message(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data);

// Purpose: receive a message from a slave
//		This call blocks until message has been received or a timeout occurs
// Parameters:
// 		id (in)				Target slave id
//		type (in)			Message type
//		len (in)			Message payload pointer
//		data (in)			Pointer to a buffer to receive the message payload data
//	Returns:				Error code, see below and top of this header
//		PMLIN_OK
//		PMLIN_NO_RESP_ERROR
//		PMLIN_TIMEOUT_ERROR
//		PMLIN_CRC_ERROR
//

PMLIN_error_t PMLIN_receive_message(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data);

// Purpose: send a command message to a slave
//		This call blocks until the message has been sent
// Parameters:
// 		id (in)				Target slave id
//		type (in)			Message type
//		data (in)			Pointer to message buffer which holds the message payload data to be sent
//		resp (out)			Pointer to a buffer to receive the command message response payload data
//							Both command message and response are PMLIN_CMD_MSG_LEN in length
//	Returns:				Error code, see below and top of this header
//		PMLIN_OK
//		PMLIN_NO_RESP_ERROR
//		PMLIN_TIMEOUT_ERROR
//		PMLIN_CRC_ERROR
//
PMLIN_error_t PMLIN_send_cmd_message(uint8_t id, volatile uint8_t *data, volatile uint8_t *resp);

// Purpose: Inform PMLIN master of all the expected slave devices
// Parameters:
//		devices[] (in)		An permanently allocated array of device declarations
//		num_devices (in) 	Size of the devices[] array

void PMLIN_define_devices(PMLIN_device_decl_t devices[], uint8_t num_devices);

// Purpose: Inform PMLIN master of all slaves and messages that are to be mirrored
// Parameters:
//		devices[] (in)		An permanently allocated array of device declarations
//		num_devices (in) 	Size of the devices[] array

void PMLIN_define_mirroring(PMLIN_mirror_def_t mirroring[], uint8_t num_mirroring);

// Purpose: Mirror data between the master and all slaves/messages whose turn it is
//		This call blocks until all the mirroring whose turn it is has been completed
// Parameters:
//		device_id (out)		Pointer (can be NULL) to device id which the PMLIN_mirror_tick sets
//							to the id of the first device that did NOT respond PMLIN_OK
// Returns:					Error code, see PMLIN_send_message and PMLIN_receive_message for possible values

PMLIN_error_t PMLIN_mirror_tick(uint8_t *device_id);

// Purpose: Attempts to renumber devices based on their type to correspond to the list passed to PMLIN_define_devices
//		This call blocks until the task is complete or fails
// Parameters:
//		renum (out)			Pointer to an array to receive the response / error code for each device, indexed by device id.
//							The array must be at least PMLIN_MAX_NUM_ID long.
//	Returns:				Error code or PMLIN_OK if nothing required re-assigning the device ids.
//							In addition what PMLIN_send_message and PMLIN_receive_message can return
//							other possible return values are
//							PMLIN_NO_FREE_ID_ERROR
//							PMLIN_TYPE_CONFLICT_ERROR
//							PMLIN_ID_RENUM_WARNING

PMLIN_error_t PMLIN_auto_config(PMLIN_error_t renum[]);

// Purpose: Checks that all the devices in the list passed to PMLIN_define_devices are present
//		and respond correctly to the probe and inquiry messages
//		This call blocks until the task is complete or fails
// Parameters:
//		renum (out)			Pointer (can be NULL) to an array that will receive the error code
//							for all the slaves. Indexed by device id. If not NULL the array must be
//							at least PMLIN_MAX_NUM_ID long.
//
// Returns:					Error code, see PMLIN_send_message and PMLIN_receive_message for possible values

PMLIN_error_t PMLIN_check_config(uint8_t *device_id);

// Purpose: Prints out to console in human readable form the list of devices passed to PMLIN_define_devices

void PMLIN_print_out_devices();

// Typedefs for function pointers to callbacks that the master client code needs to provide to PMLIN_initialize_master
typedef void (*PMLIN_write_fp)(uint8_t *buffer, uint16_t len); // send len bytes from buffer
typedef uint16_t (*PMLIN_read_fp)(uint8_t *buffer, uint16_t len, uint32_t timeout_us); // receive len bytes to buffer or until timeout_us micro seconds
typedef void (*PMLIN_send_break_fp)(); // send break
typedef void (*PMLIN_mutex_fp)(void*); // lock mutex/unlock mutex, block until successfull

// Purpose: Pass pointers to the callback and gives PMLIN master code chance to do its initializations
//		Initialisation includes finding, opening and configuring the serial port used by PMLIN master
// Parameters:
//		break_fp (in)		Pointer to function to send BREAK condition on the serial port
//		write_fp(in)		Pointer to function to send data to the serial port
//		read_fp	(in)		Pointer to function to receive data from the serial port
//		mutex (in)			Pointer to a mutex object that is compatible with the lock_fp and unlock_fp
//		lock_fp (in)		Pointer to function to lock a mutex
//		unlock_fp (in)		Pointer to function to unlock a mutex


void PMLIN_initialize_master( //
		PMLIN_send_break_fp break_fp, //
		PMLIN_write_fp write_fp, //
		PMLIN_read_fp read_fp, //
		void *mutex, //
		PMLIN_mutex_fp lock_fp, //
		PMLIN_mutex_fp unlock_fp //
		);

// Purpose: Given an error returns a pointer to human readable English language text string
// Parameters:
//		res (in)			The error code for which to return a human readable string
// Returns:					Pointer to a permanently allocated C-string

char* PMLIN_result_to_string(PMLIN_error_t res);

// Purpose: Turn on human readable serial data traffic logging to console
// Parameters:
//		debug_trafic (in)	If true (not zero) turns on the logging, 0 turns it off.

void PMLIN_set_debug_trafic(bool debug_trafic);

// Purpose: Attempts to assign a new id to a given slave
// Parameters:
//		old_id (in)			Target slave id before re-assigning it the new_id
//		new_id (in)			The new slave id for the old_id slave device
//	Returns:				Error code, anything that PMLIN_send_message and PMLIN_receive_message can return

PMLIN_error_t PMLIN_renum_id(uint8_t old_id, uint8_t new_id);

// Given a global array of PMLIN_device_decl_t this calls PMLIN_define_devices, used to make code more readable
#define PMLIN_DEFINE_DEVICES(device_array) PMLIN_define_devices(device_array,sizeof(device_array)/sizeof(device_array[0]))

// Given a global array of PMLIN_mirror_def_t this calls PMLIN_define_mirroring, used to make code more readable
#define PMLIN_DEFINE_MIRRORING(mirroring_array) PMLIN_define_mirroring(mirroring_array,sizeof(mirroring_array)/sizeof(mirroring_array[0]))

#endif
