/*
 * mode.h
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#ifndef __MODE_H__
#define __MODE_H__

#ifdef BPSM
#define BPSM_VSTR_VOLTAGE_DIVIDER_RATIO		2
#endif

#ifdef SM
//#define SM_AIN_ENABLE
//#define SM_DIO_ENABLE
#define SM_DIGITAL_SENSORS_ENABLE
#endif

/*** Debug mode ***/

//#define DEBUG		// Use programming pins for debug purpose if defined.

#endif /* __MODE_H__ */
