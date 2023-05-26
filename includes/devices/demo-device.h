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


#ifndef __DEMO_DEVICE_H__
#define	__DEMO_DEVICE_H__

#include "pmlin.h"

#define DEMO_DEVICE_DEVICE_TYPE 2
#define DEMO_DEVICE_FIRMWARE_VERSION 0x0100 // Note that this #define is for the slave, master must ask the actual device in question for this
#define DEMO_DEVICE_HARDWARE_REVISION 1

#define DEMO_DEVICE_STATUS_MSG_TYPE 0
#define DEMO_DEVICE_CONTROL_MSG_TYPE 1

#define DEMO_DEVICE_STATUS_MSG_LENGTH 2
#define DEMO_DEVICE_CONTROL_MSG_LENGTH 2

#define DEMO_DEVICE_CONTROL_MSG PMLIN_MESSAGE_DEF ( \
    DEMO_DEVICE_CONTROL_MSG_TYPE,  \
    PMLIN_HOST_TO_SLAVE,  \
	DEMO_DEVICE_CONTROL_MSG_LENGTH  \
	) \

#define DEMO_DEVICE_CALIBRATION_MSG PMLIN_MESSAGE_DEF ( \
	DEMO_DEVICE_CALIBRATION_MSG_TYPE,  \
	PMLIN_HOST_TO_SLAVE, \
    DEMO_DEVICE_CALIB_MSG_LENGTH  \
    ) \

#define DEMO_DEVICE_STATUS_MSG PMLIN_MESSAGE_DEF ( \
    DEMO_DEVICE_STATUS_MSG_TYPE,  \
    PMLIN_SLAVE_TO_HOST, \
    DEMO_DEVICE_STATUS_MSG_LENGTH \
    ) \

#define DEMO_DEVICE_DEVICE_DECL(id) \
	PMLIN_DECLARE_DEVICE( \
	    id, \
		DEMO_DEVICE_DEVICE_TYPE,\
		DEMO_DEVICE_CONTROL_MSG, \
		DEMO_DEVICE_STATUS_MSG, \
		)

typedef struct {
    uint8_t m_output;
    uint8_t m_more_control;
} DEMO_DEVICE_control_msg_t;

typedef struct {
    uint8_t m_input; 
    uint8_t m_other_status;
}DEMO_DEVICE_status_msg_t;

#endif	/*DEMO_DEVICE_H */
