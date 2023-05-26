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

#include "pmlin-mirror-demo.h"

#include "pmlin.h"
#include "pmlin-master.h"
#include "pmlin-slave.h"
#include "demo-device.h"
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "pmlin-slave-emufun.h"
#include "pmlin-slave-emulator.h"

#define DEVICE_A 1
#define DEVICE_B 2
#define DEVICE_C 3

volatile uint8_t g_device_A[DEMO_DEVICE_CONTROL_MSG_LENGTH] = {0};
volatile uint8_t g_device_B[DEMO_DEVICE_CONTROL_MSG_LENGTH]= {0};
volatile uint8_t g_device_C[DEMO_DEVICE_CONTROL_MSG_LENGTH]= {0};



volatile demo_device_simulated_state_t g_device_A_simstate = {0};
volatile demo_device_simulated_state_t g_device_B_simstate = {0};
volatile demo_device_simulated_state_t g_device_C_simstate = {0};


PMLIN_mirror_def_t g_mirror_defs[] = { //
		PMLIN_MIRROR_DEF(DEVICE_A, DEMO_DEVICE_CONTROL_MSG_TYPE, &g_device_A, 10, 0), //
		PMLIN_MIRROR_DEF(DEVICE_B, DEMO_DEVICE_CONTROL_MSG_TYPE, &g_device_B, 10, 1), //
		PMLIN_MIRROR_DEF(DEVICE_C, DEMO_DEVICE_CONTROL_MSG_TYPE,&g_device_C, 10, 2) //
		};

pmlin_emulated_slave_descriptor_t g_slave_defs[] = {	//
		PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &g_device_A_simstate , DEMO_DEVICE_DEVICE_DECL(DEVICE_A)),	//
		PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &g_device_B_simstate, DEMO_DEVICE_DEVICE_DECL(DEVICE_B)), //
		PMLIN_EMULATED_SLAVE_DECL(demo_device_simu_function, &g_device_C, DEMO_DEVICE_DEVICE_DECL(DEVICE_C)), //
		};

PMLIN_device_decl_t g_device_defs[] = { //
		DEMO_DEVICE_DEVICE_DECL(DEVICE_A), //
		DEMO_DEVICE_DEVICE_DECL(DEVICE_B), //
		DEMO_DEVICE_DEVICE_DECL(DEVICE_C), //
		};




static void* master_tick_thread_fun(void *arguments) {
	uint32_t TIMER_PERIOD_uS = 10000; // mirroring at 100 msec period
	struct timespec sleep = { 0, TIMER_PERIOD_uS * 1000L };
	while (1) {
		nanosleep(&sleep, NULL);
		uint8_t id;
		PMLIN_error_t res = PMLIN_mirror_tick(&id);
		if (PMLIN_OK != res)
			printf("error %s id %d\n", PMLIN_result_to_string(res),id);
	}
}

static void* blink_thread_fun(void *arguments) {
	struct timespec sleep = { 1, 0}; // blink perio 1 sec
	uint8_t cntr = 0;
	while (1) {
		nanosleep(&sleep, NULL);
		cntr++;
		g_device_A[0] ^=1;
		if (cntr%3==0)
			g_device_B[0] ^=1;
		if (cntr%7==0)
			g_device_C[0] ^=1;
	}
}



void mirror_demo(bool emu) {
	printf("run_pmlin_mirror_demo\n");
	PMLIN_DEFINE_DEVICES(g_device_defs);

	uint8_t failed_id;
	PMLIN_error_t res = PMLIN_check_config(&failed_id);
	if (PMLIN_OK != res)
		printf("PMLIN_check_config: error %s id %d\n", PMLIN_result_to_string(res),failed_id);

	PMLIN_DEFINE_MIRRORING(g_mirror_defs);

	// create the master tick thread
	pthread_t tick_thread;
	if (pthread_create(&tick_thread, NULL, master_tick_thread_fun, NULL))
		report_and_exit("");

	// for demo let us put the actual blinking of outputs into an other thread
	pthread_t blink_thread;
	if (pthread_create(&blink_thread, NULL, blink_thread_fun, NULL))
		report_and_exit("");

	while (1) {
		// printf("sleeping...\n");
		sleep(1);
	}
}
