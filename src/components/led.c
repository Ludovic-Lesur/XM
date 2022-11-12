/*
 * led.c
 *
 *  Created on: 22 aug. 2020
 *      Author: Ludo
 */

#include "led.h"

#include "gpio.h"
#include "mapping.h"
#include "mode.h"
#include "tim.h"
#include "types.h"

/*** LED functions ***/

/* INIT LED.
 * @param:	None.
 * @return:	None.
 */
void LED_init(void) {
	// Configure pins as output high.
	GPIO_configure(&GPIO_LED_RED, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_LED_GREEN, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_LED_BLUE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_write(&GPIO_LED_RED, 1);
	GPIO_write(&GPIO_LED_GREEN, 1);
	GPIO_write(&GPIO_LED_BLUE, 1);
}

#ifdef RSM
/* START A SINGLE LED BLINK.
 * @param blink_period_ms:	Blink duration in ms.
 * @param led_color:		Color to set.
 * @return:					None.
 */
void LED_start_blink(uint32_t blink_duration_ms, TIM2_channel_mask_t color) {
	// Init required peripheral.
	TIM2_init();
	TIM21_init(blink_duration_ms);
	// Set color according to thresholds.
	TIM2_set_color_mask(color);
	// Start blink.
	TIM2_start();
	TIM21_start();
}

/* STOP LED BLINK.
 * @param:	None.
 * @return:	None.
 */
void LED_stop_blink(void) {
	// Stop timers.
	TIM2_stop();
	TIM21_stop();
	// Turn peripherals off.
	TIM2_disable();
	TIM21_disable();
	// Turn LED off.
	LED_init();
}

#endif /* RSM */

