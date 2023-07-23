/*
 * neom8n.h
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#ifndef __NEOM8N_H__
#define __NEOM8N_H__

#include "lptim.h"
#include "math.h"
#include "string.h"
#include "types.h"
#include "usart.h"

/*** NEOM8N structures ***/

/*!******************************************************************
 * \enum NEOM8N_status_t
 * \brief NEOM8N driver error codes.
 *******************************************************************/
typedef enum {
	NEOM8N_SUCCESS = 0,
	NEOM8N_ERROR_NULL_PARAMETER,
	NEOM8N_ERROR_NMEA_FRAME_RECEPTION,
	NEOM8N_ERROR_TIMEOUT,
	NEOM8N_ERROR_CHECKSUM_INDEX,
	NEOM8N_ERROR_CHECKSUM,
	NEOM8N_ERROR_NMEA_FIELD_SIZE,
	NEOM8N_ERROR_NMEA_MESSAGE,
	NEOM8N_ERROR_NMEA_NORTH_FLAG,
	NEOM8N_ERROR_NMEA_EAST_FLAG,
	NEOM8N_ERROR_NMEA_UNIT,
	NEOM8N_ERROR_TIME_INVALID,
	NEOM8N_ERROR_TIME_TIMEOUT,
	NEOM8N_ERROR_POSITION_INVALID,
	NEOM8N_ERROR_POSITION_TIMEOUT,
	NEOM8N_ERROR_TIMEPULSE_FREQUENCY,
	NEOM8N_ERROR_TIMEPULSE_DUTY_CYCLE,
	NEOM8N_ERROR_BASE_USART = 0x0100,
	NEOM8N_ERROR_BASE_LPTIM = (NEOM8N_ERROR_BASE_USART + USART_ERROR_BASE_LAST),
	NEOM8N_ERROR_BASE_STRING = (NEOM8N_ERROR_BASE_LPTIM + LPTIM_ERROR_BASE_LAST),
	NEOM8N_ERROR_BASE_LAST = (NEOM8N_ERROR_BASE_STRING + STRING_ERROR_BASE_LAST)
} NEOM8N_status_t;

/*!******************************************************************
 * \enum NEOM8N_time_t
 * \brief GPS time data.
 *******************************************************************/
typedef struct {
	// Date.
	uint16_t year;
	uint8_t month;
	uint8_t date;
	// Time.
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;
} NEOM8N_time_t;

/*!******************************************************************
 * \enum NEOM8N_position_t
 * \brief GPS position data.
 *******************************************************************/
typedef struct {
	// Latitude.
	uint8_t lat_degrees;
	uint8_t lat_minutes;
	uint32_t lat_seconds; // = (fractionnal part of minutes * 100000).
	uint8_t lat_north_flag; // 0=south, 1=north.
	// Longitude.
	uint8_t long_degrees;
	uint8_t long_minutes;
	uint32_t long_seconds; // = (fractionnal part of minutes * 100000).
	uint8_t long_east_flag; // 0=west, 1=east.
	// Altitude.
	uint32_t altitude;
} NEOM8N_position_t;

/*!******************************************************************
 * \enum NEOM8N_timepulse_config_t
 * \brief Timepulse output parameters.
 *******************************************************************/
typedef struct {
	uint8_t active;
	uint32_t frequency_hz;
	uint8_t duty_cycle_percent;
} NEOM8N_timepulse_config_t;

/*** NEOM8N functions ***/

#ifdef GPSM
/*!******************************************************************
 * \fn void NEOM8N_init(void)
 * \brief Init NEOM8N interface.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void NEOM8N_init(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void NEOM8N_de_init(void)
 * \brief Release NEOM8N interface.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void NEOM8N_de_init(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void NEOM8N_set_backup(uint8_t state)
 * \brief Set NEOM8N backup voltage state.
 * \param[in]  	state: Backup voltage state.
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void NEOM8N_set_backup(uint8_t state);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void uint8_t NEOM8N_get_backup(void)
 * \brief Get NEOM8N backup voltage state.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		Backup voltage state.
 *******************************************************************/
uint8_t NEOM8N_get_backup(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn NEOM8N_status_t NEOM8N_get_time(RTC_time_t* gps_time, uint32_t timeout_seconds, uint32_t* fix_duration_seconds)
 * \brief Perform GPS time acquisition.
 * \param[in]  	timeout_seconds: GPS acquisition timeout in seconds.
 * \param[out] 	gps_time: Pointer to the GPS time data.
 * \param[out]	fix_duration_seconds: Pointer to integer that will contain GPS fix duration in seconds.
 * \retval		Function execution status.
 *******************************************************************/
NEOM8N_status_t NEOM8N_get_time(NEOM8N_time_t* gps_time, uint32_t timeout_seconds, uint32_t* fix_duration_seconds);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn NEOM8N_status_t NEOM8N_get_position(NEOM8N_position_t* gps_position, uint32_t timeout_seconds, uint32_t* fix_duration_seconds)
 * \brief Perform GPS position acquisition.
 * \param[in]  	timeout_seconds: GPS acquisition timeout in seconds.
 * \param[out] 	gps_position: Pointer to the GPS position data.
 * \param[out]	fix_duration_seconds: Pointer to integer that will contain GPS fix duration in seconds.
 * \retval		Function execution status.
 *******************************************************************/
NEOM8N_status_t NEOM8N_get_position(NEOM8N_position_t* gps_position, uint32_t timeout_seconds, uint32_t* fix_duration_seconds);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn NEOM8N_status_t NEOM8N_configure_timepulse(NEOM8N_timepulse_config_t* timepulse_config)
 * \brief Configure GPS timepulse output.
 * \param[in]  	timepulse_config: Timepulse output configuration.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
NEOM8N_status_t NEOM8N_configure_timepulse(NEOM8N_timepulse_config_t* timepulse_config);
#endif

/*******************************************************************/
#define NEOM8N_check_status(error_base) { if (neom8n_status != NEOM8N_SUCCESS) { status = error_base + neom8n_status; goto errors; } }

/*******************************************************************/
#define NEOM8N_stack_error(void) { ERROR_stack_error(neom8n_status, NEOM8N_SUCCESS, ERROR_BASE_NEOM8N); }

/*******************************************************************/
#define NEOM8N_print_error(void) { ERROR_print_error(neom8n_status, NEOM8N_SUCCESS, ERROR_BASE_NEOM8N); }

#endif /* __NEOM8N_H__ */
