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

#ifndef __PMLIN_SLAVE_EMULATOR_H__
#define __PMLIN_SLAVE_EMULATOR_H__

#include "pmlin.h"
#include <stdlib.h>

#define PMLIN_EMULATED_SLAVE_DECL( slave_fun,  slave_data,  dev_decl) { .m_slave_fun=slave_fun, .m_slave_data=slave_data, dev_decl }

#define PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_SET_ID 0
#define PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_INIT_TRANSFER 1
#define PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_FETCH_DATA 2
#define PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_STORE_DATA 3
#define PMLIN_EMULATED_SLAVE_CALLBACK_ACTION_END_TRANSFER 4

typedef uint8_t (*pmlin_emulated_slave_fp)(uint8_t, uint8_t, volatile void*);

typedef struct pmlin_emulated_slave_descriptor_t {
	pmlin_emulated_slave_fp m_slave_fun;
	volatile void *m_slave_data;
	PMLIN_device_decl_t m_device_declarition;
} pmlin_emulated_slave_descriptor_t;

typedef void (*PMLIN_test_case_fp)();

void pmlin_start_emulated_slaves(pmlin_emulated_slave_descriptor_t (*slave_descriptors)[], uint8_t slave_count);

void pmlin_kill_emulated_slaves();

void pmlin_start_emulated_master();

void pmlin_kill_emulated_master();

void pmlin_run_test_case(PMLIN_test_case_fp test_case);

void pmlin_master_send_break();

void pmlin_master_write(uint8_t *buffer, uint16_t len);

uint16_t pmlin_master_read(uint8_t *buffer, uint16_t bytes_to_read, uint32_t timeout_us);

#define report_and_exit(msg) do { fprintf(stderr,"file %s line %d\n",__FILE__,__LINE__); perror(msg); exit(0); } while (0)

#endif /* PMLIN_UNITTEST_H_ */
