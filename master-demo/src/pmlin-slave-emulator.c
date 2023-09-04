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
#include "pmlin-master.h"
#include "pmlin-slave.h"
#include "demo-device.h"

#include <stddef.h>
#include <stdbool.h>
#include <sys/wait.h> /* wait */
#include <stdio.h>
#include <stdlib.h>   /* exit functions */
#include <unistd.h>   /* read, write, pipe, _exit */
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <termios.h>
#include <pthread.h>

#include "demo-device.h"
#include "pmlin-slave-emulator.h"

#define TIMER_PERIOD_uS 1000

#define MY_MASTER_TO_SLAVE_LEN 3
#define MY_SLAVE_TO_MASTER_LEN 3

uint8_t g_slave_no;

bool g_send_break;

typedef struct {
	volatile int m_read;
	volatile int m_write;
	volatile int m_data_len;
	volatile uint8_t m_data[2];
} pipe_t;

pid_t *g_PIDs;

pipe_t *volatile g_to_slave_pipes;
pipe_t *volatile g_from_slave_pipes;
pipe_t g_to_master_pipe;
pipe_t g_from_master_pipe;

volatile bool g_data_register_empty_interrupt_enabled;

uint8_t g_slave_count = 0;
pmlin_emulated_slave_descriptor_t (*g_slave_descriptors)[] = NULL;

// this macro help calling slave simulation function
#define CALL_SLAVE_FUN(action, arg) \
	 ((*g_slave_descriptors)[g_slave_no].m_slave_fun) ? \
		(*g_slave_descriptors)[g_slave_no].m_slave_fun( \
				action,\
				arg, \
				(*g_slave_descriptors)[g_slave_no].m_slave_data)\
	: 0 ;




// I know, gettimeofday() is not monotonic but for this application it does not matter
static uint64_t get_time_stamp_usec() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * (uint64_t) 1000000 + tv.tv_usec;
}

static void create_pipe(pipe_t *p) {
	if (pipe((int*) p) < 0)
		report_and_exit("pipe()");
	p->m_data_len = 0;
	if (fcntl(p->m_read, F_SETFL, fcntl(p->m_read, F_GETFL) | O_NONBLOCK) < 0)
		report_and_exit("fnctl()");
}

// the simulated serial communication data in pipes moves as a pair of byte,
// one for break status and one for the actual emulated transmitted data byte.
// poll_pipe returns true only when two bytes have been received so we stay in sync
// we cannot use blocking reads and hence we need to do polling because we want to simulate
// the fact that each slave may take more or less random time to respond
static bool poll_pipe(pipe_t *p) {
	if (1 == read(p->m_read, (void*) &p->m_data[p->m_data_len], 1)) {
		p->m_data_len++;
		if (p->m_data_len >= sizeof(p->m_data) / sizeof(p->m_data[0])) {
			p->m_data_len = 0;
			return 1;
		}
	}
	return 0;
}

static void write_pipe(pipe_t *p, uint8_t data, bool brk) {
	uint8_t buf[] = { data, brk };
	if (2 != write(p->m_write, &buf, 2))
		report_and_exit("write_pipe");
}

static void* slave_transmit_thread(void *arguments) {
	struct timespec sleep = { 0, 1000000000 / (PMLIN_BAUDRATE / 10) };
	while (1) {
		nanosleep(&sleep, NULL);
		if (g_data_register_empty_interrupt_enabled) {
			write_pipe(&g_from_slave_pipes[*(int*) arguments], PMLIN_UART_data_register_empty_interrupt_handler(), 0);
		}
	}
}

static void* partyline_thread(void *arguments) {
	struct timespec sleep = { 0, 1000000000 / (PMLIN_BAUDRATE / 10) };
	while (1) {
		nanosleep(&sleep, NULL);

		bool gotit = false;
		uint8_t data = 0xFF;
		bool brk = false;
		for (uint8_t i = 0; i < g_slave_count; i++) {
			uint8_t buf;
			if (poll_pipe(&g_from_slave_pipes[i])) {
				data = data & g_from_slave_pipes[i].m_data[0];
				brk = brk | g_from_slave_pipes[i].m_data[1];
				gotit = true;
			}
		}
		if (poll_pipe(&g_from_master_pipe)) {
			data = data & g_from_master_pipe.m_data[0];
			brk = brk | g_from_master_pipe.m_data[1];
			gotit = true;
		}
		if (gotit) {
			if (brk)
				write_pipe(&g_to_master_pipe, 0, 1);
			write_pipe(&g_to_master_pipe, data, 0);
			for (uint8_t i = 0; i < g_slave_count; i++) {
				write_pipe(&g_to_slave_pipes[i], data, brk);
			}
		}
	}
}

void pmlin_master_send_break() {
	g_send_break = 1;
	// get rid of any left over garbage
	while (poll_pipe(&g_to_master_pipe) || g_to_master_pipe.m_data_len > 0)
		;
}

void pmlin_master_write(uint8_t *buffer, uint16_t len) {
	for (uint16_t i = 0; i < len; i++) {
		write_pipe(&g_from_master_pipe, buffer[i], g_send_break);
		g_send_break = 0;
	}
}

uint16_t pmlin_master_read(uint8_t *buffer, uint16_t bytes_to_read, uint32_t timeout_us) {
	uint64_t t0 = get_time_stamp_usec();
	uint16_t n = 0;
	struct timespec sleep = { 0, 1000000000 / (PMLIN_BAUDRATE / 10) };
	do {
		if (poll_pipe(&g_to_master_pipe)) {
			buffer[n++] = g_to_master_pipe.m_data[0]; // ignore m_data[1] ie serial line break info
			if (n >= bytes_to_read)
				break;
		}
		nanosleep(&sleep, NULL);
	} while (get_time_stamp_usec() - t0 < timeout_us);
	return n;
}

// -----------------------------------------------------------------------------------------

void PMLIN_end_transfer(uint8_t msg_type) {
	CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_END_TRANSFER,msg_type);
}

#define PMLIN_DIR_SLAVE_TO_MASTER 0
#define PMLIN_DIR_MASTER_TO_SLAVE 1

uint8_t PMLIN_init_transfer(uint8_t msg_type) {
	return CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_INIT_TRANSFER,msg_type);
}

bool PMLIN_handle_byte_received_from_host(uint8_t byte) {
	return CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_STORE_DATA,byte);
}

int16_t PMLIN_get_byte_to_transmit_to_host() {
	return CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_FETCH_DATA,0);
}

void PMLIN_UART_enable_data_register_empty_interrupt(bool enable_interrupt) {
	g_data_register_empty_interrupt_enabled = enable_interrupt;
}

uint16_t PMLIN_random() {
	return (uint16_t) get_time_stamp_usec() + getpid();
}

void PMLIN_store_id(uint8_t id) {
	CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_SET_ID,id);
}

static void* slave_tick_thread(void *arguments) {
	struct timespec sleep = { 0, TIMER_PERIOD_uS * 1000L };
	while (1) {
		nanosleep(&sleep, NULL);
		PMLIN_TIMER_interrupt_handler();
	}
}

static void slave_emulator(uint8_t sn) {
	g_slave_no = sn;
	pmlin_emulated_slave_descriptor_t *sp = &((*g_slave_descriptors)[sn]);
	uint8_t id=sp->m_device_declarition.m_id;
	PMLIN_initialize(id, sp->m_device_declarition.m_device_type, 0x123,1);
	CALL_SLAVE_FUN(PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_SET_ID,id);
	PMLIN_set_timer_period(TIMER_PERIOD_uS);

	// create the emulated timer thread for slave
	pthread_t thread;
	if (pthread_create(&thread, NULL, slave_tick_thread, &g_slave_no))
		report_and_exit("pthread_create 1");

	// create a thread that simulates (via polling) the data register empty interrupt
	if (pthread_create(&thread, NULL, slave_transmit_thread, &g_slave_no))
		report_and_exit("pthread_create 2");

	// and finally simulate (via polling) the data received interrupt
	while (1) {
		//printf("poll %d\n", g_slave_no);
		if (poll_pipe(&g_to_slave_pipes[sn])) {
			PMLIN_UART_data_received_interrupt_handler(g_to_slave_pipes[sn].m_data[0], g_to_slave_pipes[sn].m_data[1]);
		} else
			usleep(1000);
	}
}

void pmlin_start_emulated_slaves(pmlin_emulated_slave_descriptor_t (*slave_descriptors)[], uint8_t slave_count) {
	printf("Starting %d emulated slave devices\n",slave_count );
	g_slave_descriptors = slave_descriptors;
	g_slave_count = slave_count;
	g_to_slave_pipes = malloc(g_slave_count * sizeof(pipe_t));
	g_from_slave_pipes = malloc(g_slave_count * sizeof(pipe_t));
	for (uint8_t i = 0; i < g_slave_count; i++) {
		create_pipe(&g_from_slave_pipes[i]);
		create_pipe(&g_to_slave_pipes[i]);
	}
	create_pipe(&g_to_master_pipe);
	create_pipe(&g_from_master_pipe);

	g_PIDs = malloc(g_slave_count * sizeof(*g_PIDs));

	for (uint8_t sn = 0; sn < g_slave_count; sn++) {
		pid_t cpid = fork();

		if (cpid < 0)
			report_and_exit("fork");

		if (0 == cpid) {
			slave_emulator(sn);
			_exit(0);
		} else
			g_PIDs[sn] = cpid;
	}
}

static pthread_mutex_t g_PMLIN_mutex;

void pmlin_kill_emulated_slaves() {
	// kill all the device simulation processes
	for (uint8_t i = 0; i < g_slave_count; i++) {
		if (kill(g_PIDs[i], SIGKILL))
			report_and_exit("kill");
	}
	free(g_PIDs);
	g_PIDs = NULL;
	printf("all terminated\n");
}

void pmlin_start_emulated_master() {
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
	if (pthread_mutex_init(&g_PMLIN_mutex,&attr))
		report_and_exit("pthread_mutex_init");

	PMLIN_initialize_master( //
			pmlin_master_send_break, //
			pmlin_master_write, //
			pmlin_master_read, //
			&g_PMLIN_mutex, //
			(void*)pthread_mutex_lock, // cast to void to bypass warnings
			(void*)&pthread_mutex_unlock // cast to void to bypass warnings
			);

	// create the thread that simulates 'party line' or open collector bus by distributing eveything to everyone
	pthread_t thread;
	if (pthread_create(&thread, NULL, partyline_thread, NULL))
		report_and_exit("pthread_create 3");
	//term_emu();

}

bool PMLIN_handle_indicator_button(bool enable_indicator, bool set_indicator) {
	return 0;
}

void PMLIN_kill_emulated_master() {
	if (pthread_mutex_destroy(&g_PMLIN_mutex))
		report_and_exit("pthread_mutex_destroy");
}

void PMLIN_run_test_case(PMLIN_test_case_fp test_case) {
	printf("fork to run test case\n");
	pid_t cpid = fork();
	if (cpid == 0) {
		test_case();
	}
	printf("wait for test case to complete\n");
	waitpid(cpid, NULL, 0);

}
