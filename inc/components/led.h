/*
 * led.h
 *
 *  Created on: 22 aug. 2020
 *      Author: Ludo
 */

#ifndef LED_H
#define LED_H

#include "tim.h"
#include "types.h"

/*** LED structures ***/

/*!******************************************************************
 * \enum LED_status_t
 * \brief LED driver error codes.
 *******************************************************************/
typedef enum {
	LED_SUCCESS,
	LED_ERROR_NULL_DURATION,
	LED_ERROR_COLOR,
	LED_ERROR_BASE_LAST = 0x0100
} LED_status_t;

#if (defined LVRM) || (defined DDRM) || (defined RRM)
/*!******************************************************************
 * \enum LED_color_t
 * \brief LED colors list.
 *******************************************************************/
typedef enum {
	LED_COLOR_OFF = TIM2_CHANNEL_MASK_OFF,
	LED_COLOR_RED = TIM2_CHANNEL_MASK_RED,
	LED_COLOR_GREEN = TIM2_CHANNEL_MASK_GREEN,
	LED_COLOR_YELLOW = TIM2_CHANNEL_MASK_YELLOW,
	LED_COLOR_BLUE = TIM2_CHANNEL_MASK_BLUE,
	LED_COLOR_MAGENTA = TIM2_CHANNEL_MASK_MAGENTA,
	LED_COLOR_CYAN = TIM2_CHANNEL_MASK_CYAN,
	LED_COLOR_WHITE = TIM2_CHANNEL_MASK_WHITE,
	LED_COLOR_LAST
} LED_color_t;
#endif
#ifdef GPSM
/*!******************************************************************
 * \enum LED_color_t
 * \brief LED colors list.
 *******************************************************************/
typedef enum {
	LED_COLOR_OFF = 0,
	LED_COLOR_RED,
	LED_COLOR_GREEN,
	LED_COLOR_YELLOW,
	LED_COLOR_BLUE,
	LED_COLOR_MAGENTA,
	LED_COLOR_CYAN,
	LED_COLOR_WHITE,
	LED_COLOR_LAST
} LED_color_t;
#endif

/*** LED functions ***/

#if (defined LVRM) || (defined DDRM) || (defined RRM) || (defined GPSM)
/*!******************************************************************
 * \fn void LED_init(void)
 * \brief Init LED driver.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LED_init(void);
#endif

#if (defined LVRM) || (defined DDRM) || (defined RRM)
/*!******************************************************************
 * \fn LED_status_t LED_start_single_blink(uint32_t blink_duration_ms, LED_color_t color)
 * \brief Start single blink.
 * \param[in]  	blink_duration_ms: Blink duration in ms.
 * \param[in]	color: LED color.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
LED_status_t LED_start_single_blink(uint32_t blink_duration_ms, LED_color_t color);
#endif

#if (defined LVRM) || (defined DDRM) || (defined RRM)
/*!******************************************************************
 * \fn uint8_t LED_is_single_blink_done(void)
 * \brief Get blink status.
 * \param[in]  	none
 * \param[in]	none
 * \param[out] 	none
 * \retval		1 of the LED blink is complete, 0 otherwise.
 *******************************************************************/
uint8_t LED_is_single_blink_done(void);
#endif

#if (defined LVRM) || (defined DDRM) || (defined RRM)
/*!******************************************************************
 * \fn void LED_stop_blink(void)
 * \brief Stop LED blink.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void LED_stop_blink(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn LED_status_t LED_set(LED_color_t color
 * \brief Set LED state.
 * \param[in]	color: LED color.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
LED_status_t LED_set(LED_color_t color);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn LED_status_t LED_toggle(LED_color_t color)
 * \brief Toggle LED state.
 * \param[in]	color: LED color.
 * \param[out] 	none
 * \retval		Function execution status.
 *******************************************************************/
LED_status_t LED_toggle(LED_color_t color);
#endif

/*******************************************************************/
#define LED_check_status(error_base) { if (led_status != LED_SUCCESS) { status = error_base + led_status; goto errors; } }

/*******************************************************************/
#define LED_stack_error() { ERROR_stack_error(led_status, LED_SUCCESS, ERROR_BASE_LED); }

/*******************************************************************/
#define LED_print_error() { ERROR_print_error(led_status, LED_SUCCESS, ERROR_BASE_LED); }

#endif /* LED_H */
