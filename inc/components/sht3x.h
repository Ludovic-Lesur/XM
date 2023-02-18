/*
 * sht3x.h
 *
 *  Created on: 16 feb. 2023
 *      Author: Ludo
 */

#ifndef __SHT3X_H__
#define __SHT3X_H__

#include "i2c.h"
#include "lptim.h"
#include "types.h"

#ifdef SM

/*** SHT3x macros ***/

#define SHT3X_I2C_ADDRESS	0x44

/*** SHT3x structures ***/

typedef enum {
	SHT3X_SUCCESS = 0,
	SHT3X_ERROR_NULL_PARAMETER,
	SHT3X_ERROR_BASE_I2C = 0x0100,
	SHT3X_ERROR_BASE_LPTIM = (SHT3X_ERROR_BASE_I2C + I2C_ERROR_BASE_LAST),
	SHT3X_ERROR_BASE_LAST = (SHT3X_ERROR_BASE_LPTIM + LPTIM_ERROR_BASE_LAST)
} SHT3X_status_t;

/*** SHT3x functions ***/

SHT3X_status_t SHT3X_perform_measurements(uint8_t i2c_address);
SHT3X_status_t SHT3X_get_temperature(int8_t* temperature_degrees);
SHT3X_status_t SHT3X_get_humidity(uint8_t* humidity_percent);

#define SHT3X_status_check(error_base) { if (sht3x_status != SHT3X_SUCCESS) { status = error_base + sht3x_status; goto errors; }}
#define SHT3X_error_check() { ERROR_status_check(sht3x_status, SHT3X_SUCCESS, ERROR_BASE_SHT3X); }
#define SHT3X_error_check_print() { ERROR_status_check_print(sht3x_status, SHT3X_SUCCESS, ERROR_BASE_SHT3X); }

#endif /* SM */

#endif /* __SHT3X_H__ */
