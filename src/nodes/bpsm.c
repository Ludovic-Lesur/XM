/*
 * bpsm.c
 *
 *  Created on: 4 jun. 2023
 *      Author: Ludo
 */

#include "bpsm.h"

#include "adc.h"
#include "dinfox.h"
#include "error.h"
#include "load.h"
#include "bpsm_reg.h"
#include "node.h"

/*** BPSM local structures ***/

typedef union {
	struct {
		unsigned chen : 1;
		unsigned bken : 1;
	};
	uint8_t all;
} BPSM_flags_t;

/*** BPSM local global variables ***/

#ifdef BPSM
static BPSM_flags_t bpsm_flags;
#endif

/*** BPSM local functions ***/

#ifdef BPSM
/*******************************************************************/
static void _BPSM_reset_analog_data(void) {
	// Local variables.
	NODE_status_t node_status = NODE_SUCCESS;
	// Reset fields to error value.
	node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_1, BPSM_REG_ANALOG_DATA_1_MASK_VSRC, DINFOX_VOLTAGE_ERROR_VALUE);
	NODE_stack_error();
	node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_1, BPSM_REG_ANALOG_DATA_1_MASK_VSTR, DINFOX_VOLTAGE_ERROR_VALUE);
	NODE_stack_error();
	node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_2, BPSM_REG_ANALOG_DATA_2_MASK_VBKP, DINFOX_VOLTAGE_ERROR_VALUE);
	NODE_stack_error();
}
#endif

/*** BPSM functions ***/

#ifdef BPSM
/*******************************************************************/
void BPSM_init_registers(void) {
	// Local variables.
	NODE_status_t node_status = NODE_SUCCESS;
	LOAD_status_t load_status = LOAD_SUCCESS;
	uint8_t state = 0;
	// Read init state.
	state = LOAD_get_charge_state();
	// Init context.
	bpsm_flags.chen = (state == 0) ? 0 : 1;
	// Status and control register 1.
	node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_CHEN, (uint32_t) state);
	NODE_stack_error();
	// Read init state.
	load_status = LOAD_get_output_state(&state);
	LOAD_stack_error();
	// Init context.
	bpsm_flags.bken = (state == 0) ? 0 : 1;
	// Status and control register 1.
	node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_BKEN, (uint32_t) state);
	NODE_stack_error();
	// Load default values.
	_BPSM_reset_analog_data();
}
#endif

#ifdef BPSM
/*******************************************************************/
NODE_status_t BPSM_update_register(uint8_t reg_addr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_status_t node_status = NODE_SUCCESS;
	LOAD_status_t load_status = LOAD_SUCCESS;
	uint8_t state = 0;
	// Check address.
	switch (reg_addr) {
	case BPSM_REG_ADDR_STATUS_CONTROL_1:
		// Charge status.
		state = LOAD_get_charge_status();
		// Write field.
		node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_CHST, (uint32_t) state);
		NODE_stack_error();
		// Charge state.
		state = LOAD_get_charge_state();
		// Write field.
		node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_CHEN, (uint32_t) state);
		NODE_stack_error();
		// Backup_output state.
		load_status = LOAD_get_output_state(&state);
		LOAD_stack_error();
		// Write field.
		node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_BKEN, (uint32_t) state);
		NODE_stack_error();
		break;
	default:
		// Nothing to do for other registers.
		break;
	}
	return status;
}
#endif

#ifdef BPSM
/*******************************************************************/
NODE_status_t BPSM_check_register(uint8_t reg_addr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_status_t node_status = NODE_SUCCESS;
	LOAD_status_t load_status = LOAD_SUCCESS;
	uint32_t state = 0;
	// Check address.
	switch (reg_addr) {
	case BPSM_REG_ADDR_STATUS_CONTROL_1:
		// Check charge control bit.
		node_status = NODE_read_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_CHEN, &state);
		NODE_stack_error();
		// Check bit change.
		if (bpsm_flags.chen != state) {
			// Set charge state.
			LOAD_set_charge_state((uint8_t) state);
			// Update local flag.
			bpsm_flags.chen = state;
		}
		// Check relay control bit.
		node_status = NODE_read_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_STATUS_CONTROL_1, BPSM_REG_STATUS_CONTROL_1_MASK_BKEN, &state);
		NODE_stack_error();
		// Check bit change.
		if (bpsm_flags.bken != state) {
			// Set relay state.
			load_status = LOAD_set_output_state((uint8_t) state);
			LOAD_stack_error();
			// Update local flag.
			bpsm_flags.bken = state;
		}
		break;
	default:
		// Nothing to do for other registers.
		break;
	}
	return status;
}
#endif

#ifdef BPSM
/*******************************************************************/
NODE_status_t BPSM_mtrg_callback(ADC_status_t* adc_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	NODE_status_t node_status = NODE_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	ADC_status_t adc1_status = ADC_SUCCESS;
	uint32_t adc_data = 0;
	// Reset results.
	_BPSM_reset_analog_data();
	// Perform analog measurements.
	power_status = POWER_enable(POWER_DOMAIN_ANALOG, LPTIM_DELAY_MODE_SLEEP);
	POWER_stack_error();
	adc1_status = ADC1_perform_measurements();
	ADC1_stack_error();
	power_status = POWER_disable(POWER_DOMAIN_ANALOG);
	POWER_stack_error();
	// Update parameter.
	if (adc_status != NULL) {
		(*adc_status) = adc1_status;
	}
	// Check status.
	if (adc1_status == ADC_SUCCESS) {
		// Relay common voltage.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_VSRC_MV, &adc_data);
		ADC1_stack_error();
		if (adc1_status == ADC_SUCCESS) {
			node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_1, BPSM_REG_ANALOG_DATA_1_MASK_VSRC, (uint32_t) DINFOX_convert_mv(adc_data));
			NODE_stack_error();
		}
		// Relay output voltage.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_VSTR_MV, &adc_data);
		ADC1_stack_error();
		if (adc1_status == ADC_SUCCESS) {
			node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_1, BPSM_REG_ANALOG_DATA_1_MASK_VSTR, (uint32_t) DINFOX_convert_mv(adc_data));
			NODE_stack_error();
		}
		// Relay output current.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_VBKP_MV, &adc_data);
		ADC1_stack_error();
		if (adc1_status == ADC_SUCCESS) {
			node_status = NODE_write_field(NODE_REQUEST_SOURCE_INTERNAL, BPSM_REG_ADDR_ANALOG_DATA_2, BPSM_REG_ANALOG_DATA_2_MASK_VBKP, (uint32_t) DINFOX_convert_mv(adc_data));
			NODE_stack_error();
		}
	}
	return status;
}
#endif
