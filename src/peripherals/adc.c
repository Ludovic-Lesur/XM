/*
 * adc.c
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#include "adc.h"

#include "adc_reg.h"
#include "gpio.h"
#include "lptim.h"
#include "mapping.h"
#include "math.h"
#include "rcc_reg.h"

/*** ADC local macros ***/

#define ADC_CHANNEL_VIN						7
#define ADC_CHANNEL_VOUT					4
#define ADC_CHANNEL_IOUT					0
#define ADC_CHANNEL_VREFINT					17
#define ADC_CHANNEL_TMCU					18

#define ADC_MEDIAN_FILTER_LENGTH			9
#define ADC_CENTER_AVERAGE_LENGTH			3

#define ADC_FULL_SCALE_12BITS				4095

#define ADC_VREFINT_VOLTAGE_MV				((VREFINT_CAL * VREFINT_VCC_CALIB_MV) / (ADC_FULL_SCALE_12BITS))
#define ADC_VMCU_DEFAULT_MV					3000

#define ADC_VOLTAGE_DIVIDER_RATIO_VIN		10
#define ADC_VOLTAGE_DIVIDER_RATIO_VOUT		10

#define ADC_LT6106_VOLTAGE_GAIN				59
#define ADC_LT6106_SHUNT_RESISTOR_MOHMS		10
#define ADC_LT6106_OFFSET_CURRENT_UA		25000 // 250µV maximum / 10mR = 25mA.

#define ADC_TIMEOUT_COUNT					1000000

/*** ADC local structures ***/

typedef struct {
	unsigned int vrefint_12bits;
	unsigned int data[ADC_DATA_IDX_MAX];
	unsigned char tmcu_degrees_comp1;
	signed char tmcu_degrees_comp2;
} ADC_context_t;

/*** ADC local global variables ***/

static ADC_context_t adc_ctx;

/*** ADC local functions ***/

/* PERFORM A SINGLE ADC CONVERSION.
 * @param adc_channel:			Channel to convert.
 * @param adc_result_12bits:	Pointer to int that will contain ADC raw result on 12 bits.
 * @return:						None.
 */
static void ADC1_single_conversion(unsigned char adc_channel, unsigned int* adc_result_12bits) {
	// Select input channel.
	ADC1 -> CHSELR &= 0xFFF80000; // Reset all bits.
	ADC1 -> CHSELR |= (0b1 << adc_channel);
	// Read raw supply voltage.
	ADC1 -> CR |= (0b1 << 2); // ADSTART='1'.
	unsigned int loop_count = 0;
	while (((ADC1 -> ISR) & (0b1 << 2)) == 0) {
		// Wait end of conversion ('EOC='1') or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) return;
	}
	(*adc_result_12bits) = (ADC1 -> DR);
}

/* PERFORM SEVERAL CONVERSIONS FOLLOWED BY A MEDIAN FIILTER.
 * @param adc_channel:			Channel to convert.
 * @param adc_result_12bits:	Pointer to int that will contain ADC filtered result on 12 bits.
 * @return:						None.
 */
static void ADC1_filtered_conversion(unsigned char adc_channel, unsigned int* adc_result_12bits) {
	// Perform all conversions.
	unsigned int adc_sample_buf[ADC_MEDIAN_FILTER_LENGTH] = {0x00};
	unsigned char idx = 0;
	for (idx=0 ; idx<ADC_MEDIAN_FILTER_LENGTH ; idx++) {
		ADC1_single_conversion(adc_channel, &(adc_sample_buf[idx]));
	}
	// Apply median filter.
	(*adc_result_12bits) = MATH_median_filter(adc_sample_buf, ADC_MEDIAN_FILTER_LENGTH, ADC_CENTER_AVERAGE_LENGTH);
}

/* COMPUTE INPUT VOLTAGE.
 * @param:	None.
 * @return:	None.
 */
static void ADC1_ComputeVin(void) {
	// Get raw result.
	unsigned int vin_12bits = 0;
	ADC1_filtered_conversion(ADC_CHANNEL_VIN, &vin_12bits);
	// Convert to mV using bandgap result.
	adc_ctx.data[ADC_DATA_IDX_VIN_MV] = (ADC_VREFINT_VOLTAGE_MV * vin_12bits * ADC_VOLTAGE_DIVIDER_RATIO_VIN) / (adc_ctx.vrefint_12bits);
}

/* COMPUTE OUTPUT VOLTAGE.
 * @param:	None.
 * @return:	None.
 */
static void ADC1_compute_vout(void) {
	// Get raw result.
	unsigned int vout_12bits = 0;
	ADC1_filtered_conversion(ADC_CHANNEL_VOUT, &vout_12bits);
	// Convert to mV using bandgap result.
	adc_ctx.data[ADC_DATA_IDX_VOUT_MV] = (ADC_VREFINT_VOLTAGE_MV * vout_12bits * ADC_VOLTAGE_DIVIDER_RATIO_VOUT) / (adc_ctx.vrefint_12bits);
}

/* COMPUTE OUTPUT CURRENT.
 * @param:	None.
 * @return:	None.
 */
static void ADC1_compute_iout(void) {
	// Get raw result.
	unsigned int iout_12bits = 0;
	ADC1_filtered_conversion(ADC_CHANNEL_IOUT, &iout_12bits);
	// Convert to uA using bandgap result.
	unsigned long long num = iout_12bits;
	num *= ADC_VREFINT_VOLTAGE_MV;
	num *= 1000000;
	unsigned long long den = adc_ctx.vrefint_12bits;
	den *= ADC_LT6106_VOLTAGE_GAIN;
	den *= ADC_LT6106_SHUNT_RESISTOR_MOHMS;
	adc_ctx.data[ADC_DATA_IDX_IOUT_UA] = (num) / (den);
	// Remove offset current.
	if (adc_ctx.data[ADC_DATA_IDX_IOUT_UA] < ADC_LT6106_OFFSET_CURRENT_UA) {
		adc_ctx.data[ADC_DATA_IDX_IOUT_UA] = 0;
	}
	else {
		adc_ctx.data[ADC_DATA_IDX_IOUT_UA] -= ADC_LT6106_OFFSET_CURRENT_UA;
	}
}

/* COMPUTE MCU SUPPLY VOLTAGE.
 * @param:	None.
 * @return:	None.
 */
static void ADC1_compute_vmcu(void) {
	// Retrieve supply voltage from bandgap result.
	adc_ctx.data[ADC_DATA_IDX_VMCU_MV] = (VREFINT_CAL * VREFINT_VCC_CALIB_MV) / (adc_ctx.vrefint_12bits);
}

/* COMPUTE MCU TEMPERATURE THANKS TO INTERNAL VOLTAGE REFERENCE.
 * @param:	None.
 * @return:	None.
 */
static void ADC1_compute_tmcu(void) {
	// Set sampling time (see p.88 of STM32L031x4/6 datasheet).
	ADC1 -> SMPR |= (0b111 << 0); // Sampling time for temperature sensor must be greater than 10us, 160.5*(1/ADCCLK) = 20us for ADCCLK = SYSCLK/2 = 8MHz;
	// Wake-up VREFINT and temperature sensor.
	ADC1 -> CCR |= (0b11 << 22); // TSEN='1' and VREFEF='1'.
	LPTIM1_delay_milliseconds(10, 0); // Wait internal reference stabilization (max 3ms).
	// Read raw temperature.
	int raw_temp_sensor_12bits = 0;
	ADC1_filtered_conversion(ADC_CHANNEL_TMCU, &raw_temp_sensor_12bits);
	// Compute temperature according to MCU factory calibration (see p.301 and p.847 of RM0377 datasheet).
	int raw_temp_calib_mv = (raw_temp_sensor_12bits * adc_ctx.data[ADC_DATA_IDX_VMCU_MV]) / (TS_VCC_CALIB_MV) - TS_CAL1; // Equivalent raw measure for calibration power supply (VCC_CALIB).
	int temp_calib_degrees = raw_temp_calib_mv * ((int) (TS_CAL2_TEMP-TS_CAL1_TEMP));
	temp_calib_degrees = (temp_calib_degrees) / ((int) (TS_CAL2 - TS_CAL1));
	adc_ctx.tmcu_degrees_comp2 = temp_calib_degrees + TS_CAL1_TEMP;
	// Switch temperature sensor off.
	ADC1 -> CCR &= ~(0b1 << 23); // TSEN='0'.
	// Convert to 1-complement value.
	adc_ctx.tmcu_degrees_comp1 = 0;
	if (adc_ctx.tmcu_degrees_comp2 < 0) {
		adc_ctx.tmcu_degrees_comp1 |= 0x80;
		unsigned char temperature_abs = (-1) * (adc_ctx.tmcu_degrees_comp2);
		adc_ctx.tmcu_degrees_comp1 |= (temperature_abs & 0x7F);
	}
	else {
		adc_ctx.tmcu_degrees_comp1 = (adc_ctx.tmcu_degrees_comp2 & 0x7F);
	}
}

/*** ADC functions ***/

/* INIT ADC1 PERIPHERAL.
 * @param:	None.
 * @return:	None.
 */
void ADC1_init(void) {
	// Init GPIOs.
	GPIO_configure(&GPIO_ADC1_IN0, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_ADC1_IN4, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_configure(&GPIO_ADC1_IN6, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	// Init context.
	adc_ctx.vrefint_12bits = 0;
	unsigned char data_idx = 0;
	for (data_idx=0 ; data_idx<ADC_DATA_IDX_MAX ; data_idx++) adc_ctx.data[data_idx] = 0;
	adc_ctx.data[ADC_DATA_IDX_VMCU_MV] = ADC_VMCU_DEFAULT_MV;
	adc_ctx.tmcu_degrees_comp2 = 0;
	adc_ctx.tmcu_degrees_comp1 = 0;
	// Enable peripheral clock.
	RCC -> APB2ENR |= (0b1 << 9); // ADCEN='1'.
	// Ensure ADC is disabled.
	if (((ADC1 -> CR) & (0b1 << 0)) != 0) {
		ADC1 -> CR |= (0b1 << 1); // ADDIS='1'.
	}
	// Enable ADC voltage regulator.
	ADC1 -> CR |= (0b1 << 28);
	LPTIM1_delay_milliseconds(5, 0);
	// ADC configuration.
	ADC1 -> CFGR2 &= ~(0b11 << 30); // Reset bits 30-31.
	ADC1 -> CFGR2 |= (0b01 << 30); // Use (PCLK2/2) as ADCCLK = SYSCLK/2 (see RCC_init() function).
	ADC1 -> CFGR1 &= (0b1 << 13); // Single conversion mode.
	ADC1 -> CFGR1 &= ~(0b11 << 0); // Data resolution = 12 bits (RES='00').
	ADC1 -> CCR &= 0xFC03FFFF; // No prescaler.
	ADC1 -> SMPR |= (0b111 << 0); // Maximum sampling time.
	// ADC calibration.
	ADC1 -> CR |= (0b1 << 31); // ADCAL='1'.
	unsigned int loop_count = 0;
	while ((((ADC1 -> CR) & (0b1 << 31)) != 0) && (((ADC1 -> ISR) & (0b1 << 11)) == 0)) {
		// Wait until calibration is done or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) break;
	}
	// Clear all flags.
	ADC1 -> ISR |= 0x0000089F;
}

/* DISABLE INTERNAL ADC PERIPHERAL.
 * @param:	None.
 * @return:	None.
 */
void ADC1_disable(void) {
	// Disable peripheral.
	if (((ADC1 -> CR) & (0b1 << 0)) != 0) {
		ADC1 -> CR |= (0b1 << 1); // ADDIS='1'.
	}
	// Clear all flags.
	ADC1 -> ISR |= 0x0000089F;
	// Disable peripheral clock.
	RCC -> APB2ENR &= ~(0b1 << 9); // ADCEN='0'.
}

/* PERFORM INTERNAL ADC MEASUREMENTS.
 * @param:	None.
 * @return:	None.
 */
void ADC1_perform_measurements(void) {
	// Enable ADC peripheral.
	ADC1 -> CR |= (0b1 << 0); // ADEN='1'.
	unsigned int loop_count = 0;
	while (((ADC1 -> ISR) & (0b1 << 0)) == 0) {
		// Wait for ADC to be ready (ADRDY='1') or timeout.
		loop_count++;
		if (loop_count > ADC_TIMEOUT_COUNT) return;
	}
	// Perform measurements.
	ADC1_filtered_conversion(ADC_CHANNEL_VREFINT, &adc_ctx.vrefint_12bits);
	ADC1_ComputeVin();
	ADC1_compute_vout();
	ADC1_compute_iout();
	ADC1_compute_vmcu();
	ADC1_compute_tmcu();
	// Clear all flags.
	ADC1 -> ISR |= 0x0000089F; // Clear all flags.
	// Disable ADC peripheral.
	if (((ADC1 -> CR) & (0b1 << 0)) != 0) {
		ADC1 -> CR |= (0b1 << 1); // ADDIS='1'.
	}
}

/* GET ADC DATA.
 * @param data_idx:		Index of the data to retrieve.
 * @param data:				Pointer that will contain ADC data.
 * @return:					None.
 */
void ADC1_get_data(ADC_data_index_t data_idx, unsigned int* data) {
	(*data) = adc_ctx.data[data_idx];
}

/* GET MCU TEMPERATURE.
 * @param tmcu_degrees:	Pointer to signed value that will contain MCU temperature in degrees (2-complement).
 * @return:				None.
 */
void ADC1_get_tmcu_comp2(signed char* tmcu_degrees) {
	(*tmcu_degrees) = adc_ctx.tmcu_degrees_comp2;
}

/* GET MCU TEMPERATURE.
 * @param tmcu_degrees:	Pointer to unsigned value that will contain MCU temperature in degrees (1-complement).
 * @return:				None.
 */
void ADC1_get_tmcu_comp1(unsigned char* tmcu_degrees) {
	(*tmcu_degrees) = adc_ctx.tmcu_degrees_comp1;
}
