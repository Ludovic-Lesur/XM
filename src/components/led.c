/*
 * led.c
 *
 *  Created on: 22 aug. 2020
 *      Author: Ludo
 */

#include "led.h"

#include "gpio.h"
#include "mapping.h"
#include "tim.h"
#include "types.h"

#if (defined LVRM) || (defined DDRM) || (defined RRM)

/*** LED functions ***/

/* INIT LED.
 * @param:	None.
 * @return:	None.
 */
void LED_init(void) {
	// Configure pins as output high.
	GPIO_write(&GPIO_LED_RED, 1);
	GPIO_write(&GPIO_LED_GREEN, 1);
	GPIO_write(&GPIO_LED_BLUE, 1);
	GPIO_configure(&GPIO_LED_RED, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_LED_GREEN, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_LED_BLUE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
}

/* START A SINGLE LED BLINK.
 * @param blink_period_ms:	Blink duration in ms.
 * @param led_color:		Color to set.
 * @return status:			Function execution status.
 */
LED_status_t LED_start_single_blink(uint32_t blink_duration_ms, LED_color_t color) {
	// Local variables.
	LED_status_t status = LED_SUCCESS;
	// Check parameters.
	if (blink_duration_ms == 0) {
		status = LED_ERROR_NULL_DURATION;
		goto errors;
	}
	if (color >= LED_COLOR_LAST) {
		status = LED_ERROR_COLOR;
		goto errors;
	}
	// Set color according to thresholds.
	TIM2_set_color_mask(color);
	// Start blink.
	TIM2_start();
	TIM21_start(blink_duration_ms);
errors:
	return status;
}

uint8_t LED_is_single_blink_done(void) {
	return TIM21_is_single_blink_done();
}

/* STOP LED BLINK.
 * @param:	None.
 * @return:	None.
 */
void LED_stop_blink(void) {
	// Stop timers.
	TIM2_stop();
	TIM21_stop();
	// Turn LED off.
	LED_init();
}

#endif /* LVRM or DDRM or RRM */
