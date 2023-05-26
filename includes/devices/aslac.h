/*
 * File:   aslac.h
 * Author: nyholku
 *
 * Created on September 18, 2019, 7:42 PM
 */

#ifndef __ASLAC_H__
#define	__ASLAC_H__

#include "pmlin.h"

#define ASLAC_DEVICE_TYPE 1

#define ASLAC_STATUS_MSG_TYPE 0
#define ASLAC_CONTROL_MSG_TYPE 1
#define ASLAC_CALIBRATION_MSG_TYPE 2
#define ASLAC_INFO_MSG_TYPE 3
#define ASLAC_DEBUG_MSG_TYPE 6

#define ASLAC_STATUS_MSG_LENGTH 2
#define ASLAC_CONTROL_MSG_LENGTH 2
#define ASLAC_CALIB_MSG_LENGTH 3
#define ASLAC_INFO_MSG_LENGTH 7
#define ASLAC_DEBUG_MSG_LENGTH 8

#define ASLAC_BEAM_SHAPE_MASK 0x0F
#define ASLAC_BEAM_LED 0x40
#define ASLAC_BEAM_WHITE 0x80

#define ASLAC_BEAM_SHAPE_UNKNOWN 0
#define ASLAC_BEAM_SHAPE_LINE 1
#define ASLAC_BEAM_SHAPE_POINT 2
#define ASLAC_BEAM_SHAPE_GRID 3
#define ASLAC_BEAM_SHAPE_CIRCLE 4


#define ASLAC_CONTROL_MSG PMLIN_MESSAGE_DEF ( \
	ASLAC_CONTROL_MSG_TYPE,  \
		PMLIN_HOST_TO_SLAVE,  \
		ASLAC_CONTROL_MSG_LENGTH  \
		) \

#define ASLAC_CALIBRATION_MSG PMLIN_MESSAGE_DEF ( \
		ASLAC_CALIBRATION_MSG_TYPE,  \
		PMLIN_HOST_TO_SLAVE, \
                ASLAC_CALIB_MSG_LENGTH  \
		) \

#define ASLAC_STATUS_MSG PMLIN_MESSAGE_DEF ( \
		ASLAC_STATUS_MSG_TYPE,  \
		PMLIN_SLAVE_TO_HOST, \
		ASLAC_STATUS_MSG_LENGTH \
		) \

#define ASLAC_INFO_MSG PMLIN_MESSAGE_DEF ( \
		ASLAC_INFO_MSG_TYPE,  \
		PMLIN_SLAVE_TO_HOST, \
		ASLAC_INFO_MSG_LENGTH \
		) \

#define ASLAC_DEBUG_MSG PMLIN_MESSAGE_DEF ( \
		ASLAC_DEBUG_MSG_TYPE,  \
		PMLIN_SLAVE_TO_HOST, \
		ASLAC_DEBUG_MSG_LENGTH \
		) \

#define ASLAC_DEVICE_DECL(id) \
	PMLIN_DECLARE_DEVICE( \
	    id, \
		ASLAC_DEVICE_TYPE,\
		ASLAC_CALIBRATION_MSG, \
		ASLAC_CONTROL_MSG, \
		ASLAC_STATUS_MSG, \
		ASLAC_INFO_MSG, \
		ASLAC_DEBUG_MSG, \
		)

typedef struct {
    uint8_t m_laser_on; //
    uint8_t m_power_setting; // unit 0.5%, range 10-100%
} ASLAC_control_msg_t;


#define ASLAC_CALIB_CMD_STORE_CALIBRATION 1
#define ASLAC_CALIB_CMD_ADJUST_POWER 2
#define ASLAC_CALIB_CMD_CALIBRATE_TEMPERATURE 3
#define ASLAC_CALIB_CMD_SET_BEAM_TYPE 4
#define ASLAC_CALIB_CMD_SET_LASER_NOMINAL_POWER 5
#define ASLAC_CALIB_CMD_SET_WAVE_LENGTH 6

typedef struct {
    union {
        uint8_t m_adjust_laser_power; // 0.5%  units, range => �63%
        uint8_t m_calibration_temperature; // 0.5 �C units, range => 0 .. 127.5 �C
        uint8_t m_beam_type; // ASLAC_BEAM_SHAPExxx
        uint8_t m_laser_nominal_power; // 4 �W units, range 0..1023 �W
        uint16_t m_wave_length:16; // 1 nm or 1 �K units // NOTE! aligment must be 32 bit boundary
   };
   uint8_t m_calib_command; // see above ASLAC_CALIB_CMD_xxx
} ASLAC_calibration_msg_t;

#define ASLAC_BM_LASER_ON 0x01
#define ASLAC_BM_BUTTON_INPUT 0x02
#define ASLAC_BM_ENABLE_INPUT 0x04
#define ASLAC_BM_OVER_TEMPERATURE 0x08
#define ASLAC_BM_FEEDBACK_OPEN 0x10
#define ASLAC_BM_OVER_CURRENT 0x20
#define ASLAC_BM_NOT_CALIBRATED 0x40
#define ASLAC_BM_RESERVED 0x80

#define ASLAC_BM_ERROR (ASLAC_BM_FEEDBACK_OPEN | ASLAC_BM_OVER_CURRENT | ASLAC_BM_OVER_TEMPERATURE | ASLAC_BM_NOT_CALIBRATED)
// OVER CURRENT LEVEL 60 mA

typedef struct {
    uint8_t m_status; //
    uint8_t m_power_setting; // 0.5 % units => max 100%
} ASLAC_status_msg_t;

typedef struct {
    uint8_t m_temperature; // 0.5 �C unit, full scale 127.5 �C
    uint8_t m_current; // 0.5 mA units => full scale 127.5 mA
    uint8_t m_calibrated_full_power_voltage; // 10 mV unit, full scale 2.55 V
    uint8_t m_beam_type;
    uint16_t m_wave_length; // nm units or �K for white // NOTE! aligment must be 32 bit boundary
    uint8_t m_laser_nominal_power; // 4 �W units, range 0..1023 �W
} ASLAC_info_msg_t;

typedef struct {

    union {
        uint8_t as_uint8[8];
        uint16_t as_uint16[4];
        int8_t as_int8[8];
        uint16_t as_int16[4];
    };
} ASLAC_debug_msg_t;

#endif	/* ASLAC_H */
