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

#include "pmlin-command-line-demo.h"

#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <string.h>
#include <readline/readline.h>

#include "pmlin.h"
#include "pmlin-master.h"
#include "demo-device.h"
#include "pmlin-master-demo.h"
#include "pmlin-slave-emufun.h"
#include "pmlin-slave-emulator.h"

int STDIN = 0;
int STDOUT = 1;

uint8_t g_target_id = 1;
uint8_t g_prev_target_id = 1;
uint16_t g_ignore_count = 0;

bool check_error(PMLIN_error_t res) {
	if (res != PMLIN_OK) {
		printf("PMLIN returned %s\n", PMLIN_result_to_string(res));
		return 1;
	}
	return 0;
}

void receice_message_with_retry(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data) {
	for (uint16_t i = 0; i < 10; i++) {
		if (PMLIN_OK == PMLIN_receive_message(id, type, len, data))
			return;
	}
	printf("TIMEOUT!\n");
}

void send_message_with_retry(uint8_t id, uint8_t type, uint8_t len, volatile uint8_t *data) {
	for (uint16_t i = 0; i < 10; i++) {
		if (PMLIN_OK == PMLIN_send_message(id, type, len, data))
			return;
	}
	printf("TIMEOUT!\n");
}

void prompt() {
	printf("> ");
	fflush(stdout);
}

static void handle_cmd_from_console(char cmd) {
	if (cmd >= '0' && cmd <= '9') {
		g_prev_target_id = g_target_id;
		g_target_id = cmd - '0';
		printf("target ID set to %d\n", g_target_id);
		return;
	}
	switch (cmd) {
	case 't': {
		static uint8_t buffer[DEMO_DEVICE_CONTROL_MSG_LENGTH] = { 1 }; // 'static' to allow toggling by retaining buffer state!
		DEMO_DEVICE_control_msg_t *msg = (void*) &buffer[0];
		msg->m_output = !msg->m_output;
		printf("Turn %s (intensity %1.1f %%) device id %d\n", buffer[0] & 1 ? "ON" : "OFF", buffer[1] / 2.0, g_target_id);
		send_message_with_retry(g_target_id, DEMO_DEVICE_CONTROL_MSG_TYPE, DEMO_DEVICE_CONTROL_MSG_LENGTH, buffer);
		break;
	}
	case 's': {
		static uint8_t buffer[DEMO_DEVICE_STATUS_MSG_LENGTH];
		memset(&buffer, 0, sizeof(buffer));
		DEMO_DEVICE_status_msg_t *msg = (void*) &buffer[0];
		printf("Status read from device %d\n", g_target_id);
		receice_message_with_retry(g_target_id, DEMO_DEVICE_STATUS_MSG_TYPE, DEMO_DEVICE_STATUS_MSG_LENGTH, buffer);
		printf("INPUT = %d\n",msg->m_input);
		break;
	}
	case 'i': {
		printf("Inquire device id %d\n", g_target_id);
		uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
		uint8_t cmd_resp[PMLIN_CMD_MSG_LEN];
		memset(&cmd_resp, 0, sizeof(cmd_resp));
		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_INQUIRE;
		if (check_error(PMLIN_send_cmd_message(g_target_id, cmd_msg, cmd_resp)))
			return;
		uint16_t dev_type = (cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_MSB_IDX] << 8) + cmd_resp[PMLIN_CMD_RESP_DEV_TYPE_LSB_IDX];
		uint16_t firmware_ver = (cmd_resp[PMLIN_CMD_RESP_FW_VER_MSB_IDX] << 8) + cmd_resp[PMLIN_CMD_RESP_FW_VER_LSB_IDX];
		uint16_t hardware_rev = cmd_resp[PMLIN_CMD_RESP_HW_REV_IDX] & PMLIN_CMD_RESP_HW_REV_MASK;
		uint8_t button = cmd_resp[PMLIN_CMD_RESP_BUTTON_IDX] & PMLIN_CMD_RESP_BUTTON_MASK;
		printf("dev type %d firmware ver %d.%d.%d hardware rev %d button=%s\n", dev_type, (firmware_ver>>8)&0xF,(firmware_ver>>4)&0xF,(firmware_ver>>0)&0xF,hardware_rev,button?"ON":"OFF");
		break;
	}
	case 'p': {
		printf("Probe device id %d\n", g_target_id);
		uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
		uint8_t cmd_resp[PMLIN_CMD_MSG_LEN];
		memset(&cmd_resp, 0, sizeof(cmd_resp));
		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_PROBE;
		if (check_error(PMLIN_send_cmd_message(g_target_id, cmd_msg, cmd_resp)))
			return;
		break;
	}
	case 'n': {
		printf("Renum device id %d to id %d\n", g_prev_target_id, g_target_id);
		uint8_t cmd_msg[PMLIN_CMD_MSG_LEN] = { 0 };
		uint8_t cmd_resp[PMLIN_CMD_MSG_LEN];
		memset(&cmd_resp, 0, sizeof(cmd_resp));
		cmd_msg[PMLIN_CMD_MSG_CMD_IDX] = PMLIN_CMD_MSG_CMD_RENUM;
		cmd_msg[PMLIN_CMD_MSG_RENUM_ID_IDX] = g_target_id;
		if (check_error(PMLIN_send_cmd_message(g_prev_target_id, cmd_msg, cmd_resp)))
			return;
		break;
	}
	case 'x': {
		uint8_t buffer[DEMO_DEVICE_CONTROL_MSG_LENGTH];
		memset(&buffer, 0, sizeof(buffer));
		check_error(PMLIN_receive_message(g_target_id, DEMO_DEVICE_CONTROL_MSG_TYPE, sizeof(buffer), buffer));
		return;
		break;
	}

	default:
		if (cmd >= ' ')
			printf("no such command '%c'\n", cmd);
		else
			printf("no such command '^%c'\n", cmd + '@');
		break;
	}

	printf("Done.\n\n");
	prompt();
}

void PMLIN_command_line_interface(int com) {
	if (com > 0)
		// get rid of stuff that has been echoed to us
		tcflush(com, TCIFLUSH); // .. then get rid of the echoes and what ever...

	struct termios newtio, oldtio;
	tcgetattr(STDIN, &oldtio); // we do not want to change the console setting forever, so keep the old
	tcgetattr(STDIN, &newtio); // configure the console to return as soon as a key is pressed
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 1;
	tcsetattr(STDIN, TCSANOW, &newtio);

	printf("\nWelcome to PMLIN command line interface, press CTRL-C to exit.\n");
	prompt();

	fd_set readfs;
	int maxfd;
	int res;
	char buf[2];
	if (com > 0)
		maxfd = com + 1;
	else
		maxfd = STDIN + 1;

	while (1) {
		// wait for something from console or serial port
		FD_SET(STDIN, &readfs);
		if (com > 0)
			FD_SET(com, &readfs);
		struct timeval tout;
		tout.tv_usec = 1000;
		tout.tv_sec = 0;
		select(maxfd, &readfs, NULL, NULL, &tout);

		// copy console stuff to the serial port
		if (FD_ISSET(STDIN, &readfs)) {
			unsigned char chr;
			res = read(STDIN, &chr, 1);
			if (chr == 3)
				break; // CTRL-C terminates
			handle_cmd_from_console(chr);
		}

		if (com > 0 && FD_ISSET(com, &readfs)) {
			unsigned char chr;
			res = read(com, &chr, 1);
			if (res > 0) {
				if (g_ignore_count > 0) {
					g_ignore_count--;
					printf("[%02X] ", chr);
					fflush(stdout);
				} else {
					write(STDOUT, &chr, res);
					fflush(stdout);
				}
			}
		}
	}

	tcsetattr(STDIN, TCSANOW, &oldtio); // restore console settings

}

void command_line_demo(bool emu) {
	printf("Following (keyboard keys) commands are available:\n");
	printf(" 0..9 : set target device id\n");
	printf(" t    : toggle target output on/off\n");
	printf(" s    : read target status \n");
	printf(" i    : inquire target device type, fw and hw versions\n");
	printf(" p    : probe the target device\n");
	printf(" n    : renumber prev target id to current target id\n");
	PMLIN_command_line_interface(emu ? 0 : g_pmlin_seril_port_fd);
}
