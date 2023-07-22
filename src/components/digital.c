/*
 * digital.c
 *
 *  Created on: 17 feb. 2023
 *      Author: Ludo
 */

#include "digital.h"

#include "gpio.h"
#include "lptim.h"
#include "mapping.h"
#include "types.h"

/*** DIGITAL local structures ***/

typedef struct {
	uint8_t data[DIGITAL_DATA_INDEX_LAST];
} DIGITAL_context_t;

/*** DIGITAL local global variables ***/

#ifdef SM
static const GPIO_pin_t* DIGITAL_INPUTS[DIGITAL_DATA_INDEX_LAST] = {&GPIO_DIO0, &GPIO_DIO1, &GPIO_DIO2, &GPIO_DIO3};
static DIGITAL_context_t digital_ctx;
#endif

/*** DIGITAL functions ***/

#ifdef SM
/*******************************************************************/
void DIGITAL_init(void) {
	// Local variables.
	uint8_t idx = 0;
	// Power control.
	GPIO_configure(&GPIO_DIG_POWER_ENABLE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	// Configure digital inputs.
	for (idx=0 ; idx<DIGITAL_DATA_INDEX_LAST ; idx++) {
		GPIO_configure(DIGITAL_INPUTS[idx], GPIO_MODE_INPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	}
}
#endif

#ifdef SM
/*******************************************************************/
DIGITAL_status_t DIGITAL_perform_measurements(void) {
	// Local variables.
	DIGITAL_status_t status = DIGITAL_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	uint8_t idx = 0;
	// Turn digital front-end on.
	GPIO_write(&GPIO_DIG_POWER_ENABLE, 1);
	// Wait for stabilization.
	lptim1_status = LPTIM1_delay_milliseconds(100, LPTIM_DELAY_MODE_STOP);
	LPTIM1_check_status(DIGITAL_ERROR_BASE_LPTIM);
	// Channels loop.
	for (idx=0 ; idx<DIGITAL_DATA_INDEX_LAST ; idx++) {
		digital_ctx.data[idx] = GPIO_read(DIGITAL_INPUTS[idx]);
	}
errors:
	// Turn digital front-end off.
	GPIO_write(&GPIO_DIG_POWER_ENABLE, 0);
	return status;
}
#endif

#ifdef SM
/*******************************************************************/
DIGITAL_status_t DIGITAL_read(DIGITAL_data_index_t data_idx, uint8_t* state) {
	// Local variables.
	DIGITAL_status_t status = DIGITAL_SUCCESS;
	// Check index.
	if (data_idx >= DIGITAL_DATA_INDEX_LAST) {
		status = DIGITAL_ERROR_DATA_INDEX;
		goto errors;
	}
	// Read GPIO.
	(*state) = digital_ctx.data[data_idx];
errors:
	return status;
}
#endif
