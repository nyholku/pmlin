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

#include "pmlin-slave.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "pmlin-slave-emufun.h"
#include "pmlin-slave-emulator.h"

char* get_time() {
	static char buffer[26];
	struct tm *tm_info;
	struct timeval tv;
	gettimeofday(&tv, NULL);
	tm_info = localtime(&tv.tv_sec);
	strftime(buffer, 26, "%H:%M:%S", tm_info);
	return buffer;
}

uint8_t demo_device_simu_function(uint8_t slave_action, uint8_t arg, volatile void *slave_data) {
	volatile demo_device_simulated_state_t *simstate = slave_data;
	if (slave_action == PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_SET_ID) {
		uint8_t id = arg;
		simstate->m_id = id;
		return 0;
	}
	if (slave_action == PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_INIT_TRANSFER) {
		uint8_t msg_type = arg;
		if (msg_type == DEMO_DEVICE_CONTROL_MSG_TYPE) {
			simstate->m_data_idx = 0;
			return PMLIN_INIT_RX_MSG;
		}
		if (msg_type == DEMO_DEVICE_STATUS_MSG_TYPE) {
			simstate->m_data_idx = 0;
			return PMLIN_INIT_TX_MSG;
		}
		return PMLIN_INIT_IGNORE_MSG;
	}
	if (slave_action == PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_STORE_DATA) {
		uint8_t data_in = arg;
		simstate->m_control_data_in[simstate->m_data_idx++] = data_in;
		return simstate->m_data_idx < DEMO_DEVICE_CONTROL_MSG_LENGTH;
	}
	if (slave_action == PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_FETCH_DATA) {
		if (simstate->m_data_idx < DEMO_DEVICE_STATUS_MSG_LENGTH)
			return simstate->m_control_data_out[simstate->m_data_idx++];
		 else
			return -1;
	}
	if (slave_action == PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_END_TRANSFER) {
		uint8_t msg_type = arg;
		if (msg_type == DEMO_DEVICE_CONTROL_MSG_TYPE) {
			bool set_output = (simstate->m_control_data_in[0] & 1) != 0;

			if (simstate->m_output != set_output) {
				simstate->m_output = set_output;
				printf("%s DEVICE id %d OUTPUT = %d\n", get_time(), simstate->m_id, simstate->m_output);
			}

			return 0;
		}
	}
	return 0;
}
