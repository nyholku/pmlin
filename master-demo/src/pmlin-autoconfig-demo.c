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

#include "pmlin-autoconfig-demo.h"

#include <stdio.h>
#include <string.h>
#include "pmlin.h"
#include "pmlin-master.h"
#include "demo-device.h"
#include "pmlin-slave-emulator.h"
#include "pmlin-command-line-demo.h"

void autoconfig_demo(bool emu) {
	printf("autoconfig_demo\n");

	if (!emu) {
		printf("This demo WILL re-assign device ids!\n");
		printf("If you run the demo multiple times you may\n");
		printf("have to manually reset the ids between runs.\n");
		printf("\n");
		printf("Hit enter if you want to continue else press CTRL-C\n");
		getchar();
	}
	PMLIN_device_decl_t devices[] = { //
			DEMO_DEVICE_DEVICE_DECL(1), // id 1
			DEMO_DEVICE_DEVICE_DECL(2), // id 2
			DEMO_DEVICE_DEVICE_DECL(3), // Id 3
			};

	uint8_t no_of_devices = sizeof(devices) / sizeof(devices[0]);
	PMLIN_define_devices(devices, no_of_devices);

	PMLIN_print_out_devices();

	// this will cause an error in the network that the auto_config will correct
	if (emu) {
		PMLIN_error_t res = PMLIN_renum_id(1, 2);
		if (res != PMLIN_OK)
			printf("PMLIN_renum_id: error %s\n", PMLIN_result_to_string(res));
		res = PMLIN_renum_id(3, 4);
		if (res != PMLIN_OK)
			printf("PMLIN_renum_id: error %s\n", PMLIN_result_to_string(res));
	}

	PMLIN_error_t renum[PMLIN_MAX_NUM_ID];
	PMLIN_error_t res = PMLIN_auto_config(renum);
	printf("PMLIN_autoconfig: %s\n", PMLIN_result_to_string(res));

	// do it again to see if it worked
	res = PMLIN_auto_config(renum);
	if (res == PMLIN_OK)
		printf("PMLIN_autoconfig succeeded and fixed all the conflicts!\n");
	else
		printf("PMLIN_autoconfig: %s\n", PMLIN_result_to_string(res));
}
