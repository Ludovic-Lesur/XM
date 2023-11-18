/*
 * gpsm_reg.h
 *
 *  Created on: 26 mar. 2023
 *      Author: Ludo
 */

#ifndef __GPSM_REG_H__
#define __GPSM_REG_H__

#include "common_reg.h"

/*** GPSM registers address ***/

/*!******************************************************************
 * \enum GPSM_register_address_t
 * \brief GPSM registers map.
 *******************************************************************/
typedef enum {
	GPSM_REG_ADDR_STATUS_1 = COMMON_REG_ADDR_LAST,
	GPSM_REG_ADDR_CONTROL_1,
	GPSM_REG_ADDR_ANALOG_DATA_1,
	GPSM_REG_ADDR_TIMEOUT,
	GPSM_REG_ADDR_TIME_DATA_0,
	GPSM_REG_ADDR_TIME_DATA_1,
	GPSM_REG_ADDR_TIME_DATA_2,
	GPSM_REG_ADDR_GEOLOC_DATA_0,
	GPSM_REG_ADDR_GEOLOC_DATA_1,
	GPSM_REG_ADDR_GEOLOC_DATA_2,
	GPSM_REG_ADDR_GEOLOC_DATA_3,
	GPSM_REG_ADDR_TIMEPULSE_CONFIGURATION_0,
	GPSM_REG_ADDR_TIMEPULSE_CONFIGURATION_1,
	GPSM_REG_ADDR_LAST,
} GPSM_register_address_t;

/*** GPSM number of specific registers ***/

#define GPSM_NUMBER_OF_SPECIFIC_REG							(GPSM_REG_ADDR_LAST - COMMON_REG_ADDR_LAST)

/*** GPSM registers mask ***/

#define GPSM_REG_STATUS_1_MASK_TFS							0x00000001
#define GPSM_REG_STATUS_1_MASK_GFS							0x00000002
#define GPSM_REG_STATUS_1_MASK_TPST							0x00000004
#define GPSM_REG_STATUS_1_MASK_PWST							0x00000008
#define GPSM_REG_STATUS_1_MASK_BKENST						0x00000030
#define GPSM_REG_STATUS_1_MASK_AAF							0x00000040

#define GPSM_REG_CONTROL_1_MASK_TTRG						0x00000001
#define GPSM_REG_CONTROL_1_MASK_GTRG						0x00000002
#define GPSM_REG_CONTROL_1_MASK_TPEN						0x00000004
#define GPSM_REG_CONTROL_1_MASK_PWMD						0x00000008
#define GPSM_REG_CONTROL_1_MASK_PWEN						0x00000010
#define GPSM_REG_CONTROL_1_MASK_BKEN						0x00000020

#define GPSM_REG_ANALOG_DATA_1_MASK_VGPS					0x0000FFFF
#define GPSM_REG_ANALOG_DATA_1_MASK_VANT					0xFFFF0000

#define GPSM_REG_TIMEOUT_MASK_TIME_TIMEOUT					0x0000FFFF
#define GPSM_REG_TIMEOUT_MASK_GEOLOC_TIMEOUT				0xFFFF0000

#define GPSM_REG_TIME_DATA_0_MASK_YEAR						0x00FF0000
#define GPSM_REG_TIME_DATA_0_MASK_MONTH						0x00000F00
#define GPSM_REG_TIME_DATA_0_MASK_DATE						0x000000F8
#define GPSM_REG_TIME_DATA_0_MASK_DAY						0x00000007

#define GPSM_REG_TIME_DATA_1_MASK_HOUR						0x001F0000
#define GPSM_REG_TIME_DATA_1_MASK_MINUTE					0x00003F00
#define GPSM_REG_TIME_DATA_1_MASK_SECOND					0x0000003F

#define GPSM_REG_TIME_DATA_2_MASK_FIX_DURATION				0x0000FFFF

#define GPSM_REG_GEOLOC_DATA_0_MASK_NF						0x80000000
#define GPSM_REG_GEOLOC_DATA_0_MASK_SECOND					0x7FFFC000
#define GPSM_REG_GEOLOC_DATA_0_MASK_MINUTE					0x00003F00
#define GPSM_REG_GEOLOC_DATA_0_MASK_DEGREE					0x000000FF

#define GPSM_REG_GEOLOC_DATA_1_MASK_EF						0x80000000
#define GPSM_REG_GEOLOC_DATA_1_MASK_SECOND					0x7FFFC000
#define GPSM_REG_GEOLOC_DATA_1_MASK_MINUTE					0x00003F00
#define GPSM_REG_GEOLOC_DATA_1_MASK_DEGREE					0x000000FF

#define GPSM_REG_GEOLOC_DATA_2_MASK_ALTITUDE				0x0000FFFF

#define GPSM_REG_GEOLOC_DATA_3_MASK_HDOP					0xFFF00000
#define GPSM_REG_GEOLOC_DATA_3_MASK_NSAT					0x000F0000
#define GPSM_REG_GEOLOC_DATA_3_MASK_FIX_DURATION			0x0000FFFF

#define GPSM_REG_TIMEPULSE_CONFIGURATION_0_MASK_FREQUENCY	DINFOX_REG_MASK_ALL

#define GPSM_REG_TIMEPULSE_CONFIGURATION_1_MASK_DUTY_CYCLE	0x000000FF

#endif /* __GPSM_REG_H__ */
