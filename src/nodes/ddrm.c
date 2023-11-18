/*
 * ddrm.c
 *
 *  Created on: 04 jun. 2023
 *      Author: Ludo
 */

#include "ddrm.h"

#include "adc.h"
#include "dinfox.h"
#include "error.h"
#include "load.h"
#include "ddrm_reg.h"
#include "node.h"

/*** DDRM local structures ***/

/*******************************************************************/
typedef struct {
	DINFOX_bit_representation_t ddenst;
} DDRM_context_t;

/*** DDRM local global variables ***/

#ifdef DDRM
static DDRM_context_t ddrm_ctx;
#endif

/*** DDRM local functions ***/

#ifdef DDRM
/*******************************************************************/
static void _DDRM_reset_analog_data(void) {
	// Local variables.
	uint32_t analog_data_1 = 0;
	uint32_t analog_data_1_mask = 0;
	uint32_t analog_data_2 = 0;
	uint32_t analog_data_2_mask = 0;
	// VIN / VOUT.
	DINFOX_write_field(&analog_data_1, &analog_data_1_mask, DINFOX_VOLTAGE_ERROR_VALUE, DDRM_REG_ANALOG_DATA_1_MASK_VIN);
	DINFOX_write_field(&analog_data_1, &analog_data_1_mask, DINFOX_VOLTAGE_ERROR_VALUE, DDRM_REG_ANALOG_DATA_1_MASK_VOUT);
	NODE_write_register(NODE_REQUEST_SOURCE_INTERNAL, DDRM_REG_ADDR_ANALOG_DATA_1, analog_data_1_mask, analog_data_1);
	// IOUT.
	DINFOX_write_field(&analog_data_2, &analog_data_2_mask, DINFOX_VOLTAGE_ERROR_VALUE, DDRM_REG_ANALOG_DATA_2_MASK_IOUT);
	NODE_write_register(NODE_REQUEST_SOURCE_INTERNAL, DDRM_REG_ADDR_ANALOG_DATA_2, analog_data_2_mask, analog_data_2);
}
#endif

/*** DDRM functions ***/

#ifdef DDRM
/*******************************************************************/
void DDRM_init_registers(void) {
	// Read init state.
	DDRM_update_register(DDRM_REG_ADDR_STATUS_1);
	// Load default values.
	_DDRM_reset_analog_data();
}
#endif

#ifdef DDRM
/*******************************************************************/
NODE_status_t DDRM_update_register(uint8_t reg_addr) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	uint32_t reg_value = 0;
	uint32_t reg_mask = 0;
	// Check address.
	switch (reg_addr) {
	case DDRM_REG_ADDR_STATUS_1:
		// DC-DC state.
		ddrm_ctx.ddenst = LOAD_get_output_state();
		DINFOX_write_field(&reg_value, &reg_mask, ((uint32_t) ddrm_ctx.ddenst), DDRM_REG_STATUS_1_MASK_DDENST);
		break;
	default:
		// Nothing to do for other registers.
		break;
	}
	// Write register.
	NODE_write_register(NODE_REQUEST_SOURCE_INTERNAL, reg_addr, reg_mask, reg_value);
	return status;
}
#endif

#ifdef DDRM
/*******************************************************************/
NODE_status_t DDRM_check_register(uint8_t reg_addr, uint32_t reg_mask) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	LOAD_status_t load_status = LOAD_SUCCESS;
	uint32_t reg_value = 0;
	DINFOX_bit_representation_t dden = DINFOX_BIT_ERROR;
	// Read register.
	NODE_read_register(NODE_REQUEST_SOURCE_INTERNAL, reg_addr, &reg_value);
	// Check address.
	switch (reg_addr) {
	case DDRM_REG_ADDR_CONTROL_1:
		// DDEN.
		if ((reg_mask & DDRM_REG_CONTROL_1_MASK_DDEN) != 0) {
			// Read bit.
			dden = DINFOX_read_field(reg_value, DDRM_REG_CONTROL_1_MASK_DDEN);
			// Compare to current state.
			if (dden != ddrm_ctx.ddenst) {
				// Set DC-DC state.
				load_status = LOAD_set_output_state(dden);
				LOAD_exit_error(NODE_ERROR_BASE_LOAD);
			}
		}
		break;
	default:
		// Nothing to do for other registers.
		break;
	}
errors:
	// Update status register.
	DDRM_update_register(DDRM_REG_ADDR_STATUS_1);
	return status;
}
#endif

#ifdef DDRM
/*******************************************************************/
NODE_status_t DDRM_mtrg_callback(ADC_status_t* adc_status) {
	// Local variables.
	NODE_status_t status = NODE_SUCCESS;
	POWER_status_t power_status = POWER_SUCCESS;
	ADC_status_t adc1_status = ADC_SUCCESS;
	uint32_t adc_data = 0;
	uint32_t analog_data_1 = 0;
	uint32_t analog_data_1_mask = 0;
	uint32_t analog_data_2 = 0;
	uint32_t analog_data_2_mask = 0;
	// Reset results.
	_DDRM_reset_analog_data();
	// Perform analog measurements.
	power_status = POWER_enable(POWER_DOMAIN_ANALOG, LPTIM_DELAY_MODE_ACTIVE);
	POWER_exit_error(NODE_ERROR_BASE_POWER);
	adc1_status = ADC1_perform_measurements();
	ADC1_exit_error(NODE_ERROR_BASE_ADC1);
	power_status = POWER_disable(POWER_DOMAIN_ANALOG);
	POWER_exit_error(NODE_ERROR_BASE_POWER);
	// Check status.
	if (adc1_status == ADC_SUCCESS) {
		// Relay common voltage.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_VIN_MV, &adc_data);
		ADC1_exit_error(NODE_ERROR_BASE_ADC1);
		if (adc1_status == ADC_SUCCESS) {
			DINFOX_write_field(&analog_data_1, &analog_data_1_mask, (uint32_t) DINFOX_convert_mv(adc_data), DDRM_REG_ANALOG_DATA_1_MASK_VIN);
		}
		// Relay output voltage.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_VOUT_MV, &adc_data);
		ADC1_exit_error(NODE_ERROR_BASE_ADC1);
		if (adc1_status == ADC_SUCCESS) {
			DINFOX_write_field(&analog_data_1, &analog_data_1_mask, (uint32_t) DINFOX_convert_mv(adc_data), DDRM_REG_ANALOG_DATA_1_MASK_VOUT);
		}
		// Relay output current.
		adc1_status = ADC1_get_data(ADC_DATA_INDEX_IOUT_UA, &adc_data);
		ADC1_exit_error(NODE_ERROR_BASE_ADC1);
		if (adc1_status == ADC_SUCCESS) {
			DINFOX_write_field(&analog_data_2, &analog_data_2_mask, (uint32_t) DINFOX_convert_ua(adc_data), DDRM_REG_ANALOG_DATA_2_MASK_IOUT);
		}
		// Write registers.
		NODE_write_register(NODE_REQUEST_SOURCE_INTERNAL, DDRM_REG_ADDR_ANALOG_DATA_1, analog_data_1_mask, analog_data_1);
		NODE_write_register(NODE_REQUEST_SOURCE_INTERNAL, DDRM_REG_ADDR_ANALOG_DATA_2, analog_data_2_mask, analog_data_2);
	}
	// Update ADC status.
	if (adc_status != NULL) {
		(*adc_status) = adc1_status;
	}
errors:
	POWER_disable(POWER_DOMAIN_ANALOG);
	// Update ADC status.
	if (adc_status != NULL) {
		(*adc_status) = adc1_status;
	}
	return status;
}
#endif
