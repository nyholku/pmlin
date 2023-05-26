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

#include "pmlin-master-demo.h"

#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/queue.h>
#include <pthread.h>
#include <errno.h>

#include "pmlin-master.h"
#include "pmlin-command-line-demo.h"
#include "pmlin-mirror-demo.h"
#include "pmlin-autoconfig-demo.h"
#include "pmlin.h"
#include "demo-device.h"
#include "pmlin-slave-emufun.h"
#include "pmlin-slave-emulator.h"

#define SERIAL_PORT_NAME "/dev/cu.usbserial-FTC7LESI"

volatile int g_pmlin_seril_port_fd;

int pmlin_init_serial_port() {
	char *port_name = SERIAL_PORT_NAME;
	int com = open(port_name, O_RDWR | O_NOCTTY | O_NDELAY);
	if (com < 0) {
		perror(port_name);
		exit(errno);
	}

	fcntl(com, F_SETFL, 0);

	struct termios opts;

	tcgetattr(com, &opts);

	opts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);

	opts.c_cflag |= (CLOCAL | CREAD);
	opts.c_cflag &= ~PARENB;
	opts.c_cflag |= CSTOPB; // two stop bits
	opts.c_cflag &= ~CSIZE;
	opts.c_cflag |= CS8;

	opts.c_oflag &= ~OPOST;

	opts.c_iflag &= ~INPCK;
	opts.c_iflag &= ~(IXON | IXOFF | IXANY);
	opts.c_cc[ VMIN] = 0;
	opts.c_cc[ VTIME] = 10; //0.1 sec

	cfsetispeed(&opts, PMLIN_BAURDATE);
	cfsetospeed(&opts, PMLIN_BAURDATE);

	if (tcsetattr(com, TCSANOW, &opts) != 0) {
		perror(port_name);
		abort();
	}

	tcflush(com, TCIOFLUSH); // just in case some crap is the buffers
	return com;
}

void pmlin_write(uint8_t *buffer, uint16_t len) {
	write(g_pmlin_seril_port_fd, (const void*) buffer, len);
	tcdrain(g_pmlin_seril_port_fd);
}

uint16_t pmlin_read(uint8_t *buffer, uint16_t bytes_to_read, uint32_t timeout_ms) {
	fd_set fdset;
	for (uint16_t i = 0; i < bytes_to_read; i++) {
		struct timeval tout;
		tout.tv_usec = timeout_ms % 1000000;
		tout.tv_sec = timeout_ms / 1000000;
		FD_ZERO(&fdset);
		FD_SET(g_pmlin_seril_port_fd, &fdset);
		if (select(g_pmlin_seril_port_fd + 1, &fdset, NULL, NULL, &tout) < 0)
			return i;
		if (FD_ISSET(g_pmlin_seril_port_fd, &fdset)) {
			int n = read(g_pmlin_seril_port_fd, buffer + i, 1);
			if (n < 1)
				return i;
		} else
			// timeout
			return i;

	}
	return bytes_to_read;
}

void pmlin_send_break() {
	usleep(1000);
	tcflush(g_pmlin_seril_port_fd, TCIOFLUSH); // get rid of any extra crap
	usleep(1000);

	struct termios opts;

	tcgetattr(g_pmlin_seril_port_fd, &opts);

	// note! this relies on the non guaranteed fact that baudrate constant is actually the baudrate integer
	cfsetispeed(&opts, PMLIN_BAURDATE);
	cfsetospeed(&opts, PMLIN_BAURDATE);

	if (tcsetattr(g_pmlin_seril_port_fd, TCSADRAIN, &opts) != 0) {
		perror("abort()"__FILE__ "__LINE__");
		abort();
	}

	cfsetispeed(&opts, PMLIN_BAURDATE / 2);
	cfsetospeed(&opts, PMLIN_BAURDATE / 2);
	tcsetattr(g_pmlin_seril_port_fd, TCSADRAIN, &opts); // wait for tx queue empty and then set baudrate

	tcdrain(g_pmlin_seril_port_fd); // wait for chars to be sent (just in case)

	// send break
	char break_char = 0;
	write(g_pmlin_seril_port_fd, &break_char, 1);
	tcdrain(g_pmlin_seril_port_fd); // wait for the break char to be sent (does not realy work, hence next delay)
	usleep(4*1000000 / (PMLIN_BAURDATE/2/10)); // 2 msec delay
	cfsetispeed(&opts, PMLIN_BAURDATE);
	cfsetospeed(&opts, PMLIN_BAURDATE);
	tcsetattr(g_pmlin_seril_port_fd, TCSADRAIN, &opts); // wait for tx queue empty and then set baudrate

}

int main(int argc, char *argv[]) {
	uint16_t i;
	bool emu = false;

	for (i = 1; i < argc; i++) {
		if (strcmp("-e", argv[i]) == 0)
			emu = true;
		if (strcmp("-t", argv[i]) == 0)
			PMLIN_set_debug_trafic(true);
	}
	if (argc <= 1) {
		printf("usage: pmlin-demo [-t] [-s] demo \n");
		printf(" where demo is an integer as follows:\n");
		printf("  0 : command_line_demo (CLI/REPL)\n");
		printf("  1 : mirror_demo\n");
		printf("  2 : autoconfig_demo\n");
		printf(" options:\n");
		printf("  -t display PMLIN serial traffic\n");
		printf("  -e emulate slaves (no hardware required)\n");
		return 0;
	}

	if (emu) {
		demo_device_simulated_state_t demo_device_simulated_state[3] = { 0 };
		pmlin_emulated_slave_descriptor_t slaves[] = {	//
				PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &demo_device_simulated_state[0], DEMO_DEVICE_DEVICE_DECL(1)),	//
				PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &demo_device_simulated_state[0], DEMO_DEVICE_DEVICE_DECL(2)),	//
				PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &demo_device_simulated_state[0], DEMO_DEVICE_DEVICE_DECL(3)),	//
				};	//

		pmlin_start_emulated_slaves(&slaves, sizeof(slaves) / sizeof(slaves[0]));
		pmlin_start_emulated_master();
	} else {
		g_pmlin_seril_port_fd = pmlin_init_serial_port();
		PMLIN_initialize_master(pmlin_send_break, pmlin_write, pmlin_read, NULL, NULL, NULL);
	}

	uint8_t demo = atoi(argv[argc-1]);
	switch (demo) {
	case 0:
		command_line_demo(emu);
		break;
	case 1:
		mirror_demo(emu);
		break;
	case 2:
		autoconfig_demo(emu);
		break;
	}
	if (emu)
		pmlin_kill_emulated_slaves();

	return 0;
}

// to compile && run:
//gcc src/pmlin-demo.c ../PMLIN/crc8.c -o pmlin-demo && ./pmlin-demo
