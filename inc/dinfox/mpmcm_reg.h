/*
 * mpmcm_reg.h
 *
 *  Created on: 03 sep. 2023
 *      Author: Ludo
 */

#ifndef __MPMCM_REG_H__
#define __MPMCM_REG_H__

#include "common_reg.h"
#include "types.h"

/*** MPMCM registers address ***/

/*******************************************************************/
#define MPMCM_DATA_REG_SET(channel, data)  \
	MPMCM_REG_ADDR_CH##channel##_##data##_0,  \
	MPMCM_REG_ADDR_CH##channel##_##data##_1, \

/*******************************************************************/
#define MPMCM_CHANNEL_REG_SET(channel)          \
	MPMCM_DATA_REG_SET(channel, ACTIVE_POWER)   \
	MPMCM_DATA_REG_SET(channel, RMS_VOLTAGE)    \
	MPMCM_DATA_REG_SET(channel, RMS_CURRENT)    \
	MPMCM_DATA_REG_SET(channel, APPARENT_POWER) \
	MPMCM_DATA_REG_SET(channel, POWER_FACTOR)

/*!******************************************************************
 * \enum MPMCM_register_address_t
 * \brief MPMCM registers map.
 *******************************************************************/
typedef enum {
	MPMCM_REG_ADDR_STATUS_1 = COMMON_REG_ADDR_LAST,
	MPMCM_REG_ADDR_CONTROL_1,
	MPMCM_REG_ADDR_MAINS_FREQUENCY_0,
	MPMCM_REG_ADDR_MAINS_FREQUENCY_1,
	MPMCM_CHANNEL_REG_SET(1)
	MPMCM_CHANNEL_REG_SET(2)
	MPMCM_CHANNEL_REG_SET(3)
	MPMCM_CHANNEL_REG_SET(4)
	MPMCM_REG_ADDR_LAST,
} MPMCM_register_address_t;

/*** MPMCM number of specific registers ***/

#define MPMCM_NUMBER_OF_SPECIFIC_REG			(MPMCM_REG_ADDR_LAST - COMMON_REG_ADDR_LAST)
#define MPMCM_NUMBER_OF_REG_PER_DATA			(MPMCM_REG_ADDR_CH2_ACTIVE_POWER_0 - MPMCM_REG_ADDR_CH1_ACTIVE_POWER_0)

/*** MPMCM registers mask ***/

#define MPMCM_REG_STATUS_1_MASK_CH1D	0x00000001
#define MPMCM_REG_STATUS_1_MASK_CH2D	0x00000002
#define MPMCM_REG_STATUS_1_MASK_CH3D	0x00000004
#define MPMCM_REG_STATUS_1_MASK_CH4D	0x00000008
#define MPMCM_REG_STATUS_1_MASK_MVD		0x00000010

#define MPMCM_REG_CONTROL_1_MASK_CH1S	0x00000001
#define MPMCM_REG_CONTROL_1_MASK_CH2S	0x00000002
#define MPMCM_REG_CONTROL_1_MASK_CH3S	0x00000004
#define MPMCM_REG_CONTROL_1_MASK_CH4S	0x00000008
#define MPMCM_REG_CONTROL_1_MASK_FRQS	0x00000010

#define MPMCM_REG_X_0_RUN_MASK			0x0000FFFF
#define MPMCM_REG_X_0_MEAN_MASK			0xFFFF0000
#define MPMCM_REG_X_1_MIN_MASK			0x0000FFFF
#define MPMCM_REG_X_1_MAX_MASK			0xFFFF0000

#endif /* __MPMCM_REG_H__ */
