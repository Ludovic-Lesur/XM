/*
 * s2lp.c
 *
 *  Created on: 16 aug. 2019
 *      Author: Ludo
 */

#include "s2lp.h"

#include "dma.h"
#include "exti.h"
#include "gpio.h"
#include "lptim.h"
#include "mapping.h"
#include "pwr.h"
#include "s2lp_reg.h"
#include "spi.h"
#include "types.h"

#ifdef UHFM

/*** S2LP local macros ***/

// SPI header bytes.
#define S2LP_HEADER_BYTE_WRITE					0x00
#define S2LP_HEADER_BYTE_READ					0x01
#define S2LP_HEADER_BYTE_COMMAND				0x80
// State waiting timeout.
#define S2LP_TIMEOUT_MS							1000
#define S2LP_TIMEOUT_SUB_DELAY_MS				10
// Crystal frequency ranges.
#define S2LP_XO_FREQUENCY_HZ					49152000
#define S2LP_XO_HIGH_RANGE_THRESHOLD_HZ			48000000
// Digital frequency.
#if (S2LP_XO_FREQUENCY_HZ >= S2LP_XO_HIGH_RANGE_THRESHOLD_HZ)
#define S2LP_PD_CLKDIB_BIT						0
#define S2LP_PLL_PFD_SPLIT_EN_BIT				0
#define S2LP_FDIG_HZ							(S2LP_XO_FREQUENCY_HZ >> 1)
#define S2LP_IF_OFFSET_ANA						0x2A
#else
#define S2LP_PD_CLKDIB_BIT						1
#define S2LP_PLL_PFD_SPLIT_EN_BIT				1
#define S2LP_FDIG_HZ							S2LP_XO_FREQUENCY_HZ
#define S2LP_IF_OFFSET_ANA						0xB8
#endif
// RF frequency range.
#define S2LP_RF_FREQUENCY_HZ_MIN				826000000
#define S2LP_RF_FREQUENCY_HZ_MAX				958000000
// Frequency deviation range.
#define S2LP_DEVIATION_HZ_MIN					150
#define S2LP_DEVIATION_HZ_MAX					500000
// RX bandwidth range.
#define S2LP_RX_BANDWIDTH_HZ_MIN				1100
#define S2LP_RX_BANDWIDTH_HZ_MAX				800100
#define S2LP_RX_BANDWIDTH_TABLE_SIZE			90
#define S2LP_RX_BANDWIDTH_TABLE_FREQUENCY_KHZ	260000
#define S2LP_CHANNEL_FILTER_TABLE_SIZE			3
// Datarate range.
#define S2LP_DATARATE_BPS_MIN					100
#define S2LP_DATARATE_BPS_MAX					125000
// RF output power range.
#define S2LP_RF_OUTPUT_POWER_MIN				-49
#define S2LP_RF_OUTPUT_POWER_MAX				14
// Sync word max length.
#define S2LP_SYNC_WORD_LENGTH_BITS_MAX			32
// RSSI offset.
#define S2LP_RSSI_OFFSET_DB						146
#define S2LP_RF_FRONT_END_GAIN_DB				12
// FIFO.
#define S2LP_TX_FIFO_USE_DMA // Use DMA to fill TX FIFO if defined, standard SPI access otherwise.
#define S2LP_FIFO_THHRESHOLD_BYTES_MAX			0x7F
// Last register address
#define S2LP_REGISTER_ADRESS_LAST				0x7F

/*** S2LP local structures ***/

/*******************************************************************/
typedef struct {
	uint16_t mantissa;
	uint8_t exponent;
} S2LP_mantissa_exponent_t;

/*** S2LP local global variables ***/

/*******************************************************************/
static const uint16_t S2LP_RX_BANDWIDTH_26M[S2LP_RX_BANDWIDTH_TABLE_SIZE] = {
	8001, 7951, 7684, 7368, 7051, 6709, 6423, 5867, 5414,
	4509, 4259, 4032, 3808, 3621, 3417, 3254, 2945, 2703,
	2247, 2124, 2015, 1900, 1807, 1706, 1624, 1471, 1350,
	1123, 1062, 1005, 950,  903,  853,  812,  735,  675,
	561,  530,  502,  474,  451,  426,  406,  367,  337,
	280,  265,  251,  237,  226,  213,  203,  184,  169,
	140,  133,  126,  119,  113,  106,  101,  92,   84,
	70,   66,   63,   59,   56,   53,   51,   46,   42,
	35,   33,   31,   30,   28,   27,   25,   23,   21,
	18,   17,   16,   15,   14,   13,   13,   12,   11
};

/*** S2LP local functions ***/

/*******************************************************************/
#define S2LP_abs(a) ((a) > 0 ? (a) : -(a))

/*******************************************************************/
static S2LP_status_t _S2LP_write_register(uint8_t addr, uint8_t value) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	SPI_status_t spi1_status = SPI_SUCCESS;
	// Falling edge on CS pin.
	GPIO_write(&GPIO_S2LP_CS, 0);
	// Write sequence.
	spi1_status = SPI1_write_byte(S2LP_HEADER_BYTE_WRITE); // A/C='0' and W/R='0'.
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(addr);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(value);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
errors:
	GPIO_write(&GPIO_S2LP_CS, 1); // Set CS pin.
	return status;
}

/*******************************************************************/
static S2LP_status_t _S2LP_read_register(uint8_t addr, uint8_t* value) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	SPI_status_t spi1_status = SPI_SUCCESS;
	// Falling edge on CS pin.
	GPIO_write(&GPIO_S2LP_CS, 0);
	// Read sequence.
	spi1_status = SPI1_write_byte(S2LP_HEADER_BYTE_READ); // A/C='0' and W/R='1'.
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(addr);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_read_byte(0xFF, value);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
errors:
	GPIO_write(&GPIO_S2LP_CS, 1); // Set CS pin.
	return status;
}

/*******************************************************************/
static uint32_t _S2LP_compute_datarate(S2LP_mantissa_exponent_t* datarate_setting) {
	// Local variables.
	uint64_t dr = 0;
	uint32_t datarate_bps = 0;
	// Compute formula.
	if ((datarate_setting -> exponent) == 0) {
		dr = (uint64_t) S2LP_FDIG_HZ * (uint64_t) (datarate_setting -> mantissa);
		datarate_bps = (uint32_t) (dr >> 32);
	}
	else {
		dr = ((uint64_t) S2LP_FDIG_HZ) * (((uint64_t) (datarate_setting -> mantissa)) + 65536);
		datarate_bps = (uint32_t) (dr >> (33 - (datarate_setting -> exponent)));
	}
	return datarate_bps;
}

/*******************************************************************/
static S2LP_status_t _S2LP_compute_mantissa_exponent_datarate(uint32_t datarate_bps, S2LP_mantissa_exponent_t* datarate_setting) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	S2LP_mantissa_exponent_t tmp_setting;
	uint16_t mantissa = 0;
	uint8_t exponent = 0;
	uint32_t datarate = 0;
	uint64_t tgt1 = 0;
	uint64_t tgt2 = 0;
	uint64_t tgt = 0;
	// Check range.
	if (datarate_bps < S2LP_DATARATE_BPS_MIN) {
		status = S2LP_ERROR_DATARATE_UNDERFLOW;
		goto errors;
	}
	if (datarate_bps > S2LP_DATARATE_BPS_MAX) {
		status = S2LP_ERROR_DATARATE_OVERFLOW;
		goto errors;
	}
	// Compute exponent.
	for (exponent=0 ; exponent!=12 ; exponent++) {
		tmp_setting.mantissa = 0xFFFF;
		tmp_setting.exponent = exponent;
		datarate = _S2LP_compute_datarate(&tmp_setting);
		if (datarate_bps <= datarate) break;
	}
	// Compute mantissa.
	if (exponent == 0) {
		tgt = ((uint64_t) datarate_bps) << 32;
		mantissa = (uint16_t) (tgt / S2LP_FDIG_HZ);
		tgt1 = (uint64_t) S2LP_FDIG_HZ * (uint64_t) mantissa;
		tgt2 = (uint64_t) S2LP_FDIG_HZ * (uint64_t) (mantissa + 1);
	}
	else {
		tgt = ((uint64_t) datarate_bps) << (33 - exponent);
		mantissa = (uint16_t) ((tgt / S2LP_FDIG_HZ) - 65536);
		tgt1 = (uint64_t) S2LP_FDIG_HZ * (uint64_t) (mantissa + 65536);
		tgt2 = (uint64_t) S2LP_FDIG_HZ * (uint64_t) (mantissa + 65536 + 1);
	}
	(*datarate_setting).mantissa = ((tgt2 - tgt) < (tgt - tgt1)) ? (mantissa + 1) : (mantissa);
	(*datarate_setting).exponent = exponent;
errors:
	return status;
}

/*******************************************************************/
static uint32_t _S2LP_compute_deviation(S2LP_mantissa_exponent_t* deviation_setting) {
	// Local variables.
	uint32_t deviation_hz = 0;
	// Check exponent.
	if ((deviation_setting -> exponent) ==0) {
		deviation_hz = (uint32_t) (((uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) (deviation_setting -> mantissa)) >> 22);
	}
	else {
		deviation_hz = (uint32_t) (((uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) (256 + (deviation_setting -> mantissa))) >> (23 - (deviation_setting -> exponent)));
	}
	return deviation_hz;
}

/*******************************************************************/
static S2LP_status_t _S2LP_compute_mantissa_exponent_deviation(uint32_t deviation_hz, S2LP_mantissa_exponent_t* deviation_setting) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	S2LP_mantissa_exponent_t tmp_setting;
	uint16_t mantissa = 0;
	uint8_t exponent = 0;
	uint32_t deviation = 0;
	uint64_t tgt1 = 0;
	uint64_t tgt2 = 0;
	uint64_t tgt = 0;
	// Check range.
	if (deviation_hz < S2LP_DEVIATION_HZ_MIN) {
		status = S2LP_ERROR_DEVIATION_UNDERFLOW;
		goto errors;
	}
	if (deviation_hz > S2LP_DEVIATION_HZ_MAX) {
		status = S2LP_ERROR_DEVIATION_OVERFLOW;
		goto errors;
	}
	// Compute exponent.
	for (exponent=0 ; exponent!=12 ; exponent++) {
		tmp_setting.mantissa = 0xFF;
		tmp_setting.exponent = exponent;
		deviation = _S2LP_compute_deviation(&tmp_setting);
		if (deviation_hz < deviation) break;
	}
	// Compute mantissa.
	if (exponent == 0) {
		tgt = ((uint64_t) deviation_hz) << 22;
		mantissa = (uint32_t) (tgt / S2LP_XO_FREQUENCY_HZ);
		tgt1 = (uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) mantissa;
		tgt2 = (uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) (mantissa + 1);
	}
	else {
		tgt = ((uint64_t) deviation_hz) << (23 - exponent);
		mantissa = (uint32_t) (tgt / S2LP_XO_FREQUENCY_HZ) - 256;
		tgt1 = (uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) (mantissa + 256);
		tgt2 = (uint64_t) S2LP_XO_FREQUENCY_HZ * (uint64_t) (mantissa + 256 + 1);
	}
	(*deviation_setting).mantissa = ((tgt2 - tgt) < (tgt - tgt1)) ? (mantissa + 1) : (mantissa);
	(*deviation_setting).exponent = exponent;
errors:
	return status;
}

/*******************************************************************/
static S2LP_status_t _S2LP_compute_mantissa_exponent_rx_bandwidth(uint32_t rx_bandwidth_hz, S2LP_mantissa_exponent_t* rx_bandwidth_setting) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t idx = 0;
	uint8_t idx_tmp = 0;
	uint8_t j = 0;
	int32_t channel_filter[S2LP_CHANNEL_FILTER_TABLE_SIZE];
	uint32_t channel_filter_delta = 0xFFFFFFFF;
	// Check range.
	if (rx_bandwidth_hz < S2LP_RX_BANDWIDTH_HZ_MIN) {
		status = S2LP_ERROR_RX_BANDWIDTH_UNDERFLOW;
		goto errors;
	}
	if (rx_bandwidth_hz > S2LP_RX_BANDWIDTH_HZ_MAX) {
		status = S2LP_ERROR_RX_BANDWIDTH_OVERFLOW;
		goto errors;
	}
	// Search channel filter bandwidth table index.
	for (idx=0 ; idx<S2LP_RX_BANDWIDTH_TABLE_SIZE ; idx++) {
		if (rx_bandwidth_hz >= (uint32_t) (((uint64_t) S2LP_RX_BANDWIDTH_26M[idx] * (uint64_t) S2LP_FDIG_HZ) / ((uint64_t) S2LP_RX_BANDWIDTH_TABLE_FREQUENCY_KHZ))) break;
	}
	if (idx != 0) {
		// Finds the index value with best approximation in adjacent elements.
		idx_tmp = idx;
		for(j=0 ; j<S2LP_CHANNEL_FILTER_TABLE_SIZE ; j++) {
			// Compute channel filter.
			if (((idx_tmp + j - 1) >= 0) && ((idx_tmp + j - 1) < S2LP_RX_BANDWIDTH_TABLE_SIZE)) {
				channel_filter[j] = (int32_t) rx_bandwidth_hz - (int32_t) (((uint64_t) S2LP_RX_BANDWIDTH_26M[idx_tmp + j - 1] * (uint64_t) S2LP_FDIG_HZ) / ((uint64_t) S2LP_RX_BANDWIDTH_TABLE_FREQUENCY_KHZ));
			}
			else {
				channel_filter[j] = 0x7FFFFFFF;
			}
		}
		channel_filter_delta = 0xFFFFFFFF;
		// Check delta.
		for (j=0 ; j<S2LP_CHANNEL_FILTER_TABLE_SIZE ; j++) {
			if (S2LP_abs(channel_filter[j]) < channel_filter_delta) {
				channel_filter_delta = S2LP_abs(channel_filter[j]);
				idx = (idx_tmp + j - 1);
			}
		}
	}
	(*rx_bandwidth_setting).mantissa = (idx % 9);
	(*rx_bandwidth_setting).exponent = (idx / 9);
errors:
	return status;
}

/*** S2LP functions ***/

/*******************************************************************/
void S2LP_init(void) {
	// Configure TCXO power control pin.
	GPIO_configure(&GPIO_TCXO_POWER_ENABLE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_write(&GPIO_TCXO_POWER_ENABLE, 0);
	// Configure shutdown pin.
	GPIO_configure(&GPIO_S2LP_SDN, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	// Configure GPIO.
	GPIO_configure(&GPIO_S2LP_GPIO0, GPIO_MODE_INPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	// Configure TX/RX switch control pins.
	GPIO_configure(&GPIO_RF_TX_ENABLE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_write(&GPIO_RF_TX_ENABLE, 0);
	GPIO_configure(&GPIO_RF_RX_ENABLE, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	GPIO_write(&GPIO_RF_RX_ENABLE, 0);
}

/*******************************************************************/
S2LP_status_t S2LP_tcxo(uint8_t tcxo_enable) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	// Turn TCXO on or off.
	GPIO_write(&GPIO_TCXO_POWER_ENABLE, tcxo_enable);
	// Warm-up delay.
	if (tcxo_enable != 0) {
		lptim1_status = LPTIM1_delay_milliseconds(S2LP_TCXO_DELAY_MS, LPTIM_DELAY_MODE_SLEEP);
		LPTIM1_check_status(S2LP_ERROR_BASE_LPTIM);
	}
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_shutdown(uint8_t shutdown_enable) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	// Configure GPIO.
	if (shutdown_enable == 0) {
		// Put SDN low.
		GPIO_configure(&GPIO_S2LP_SDN, GPIO_MODE_OUTPUT, GPIO_TYPE_PUSH_PULL, GPIO_SPEED_LOW, GPIO_PULL_NONE);
		// Wait for reset time.
		lptim1_status = LPTIM1_delay_milliseconds(S2LP_SHUTDOWN_DELAY_MS, LPTIM_DELAY_MODE_SLEEP);
		LPTIM1_check_status(S2LP_ERROR_BASE_LPTIM);
	}
	else {
		// Put SDN in high impedance (pull-up resistor used).
		GPIO_configure(&GPIO_S2LP_SDN, GPIO_MODE_ANALOG, GPIO_TYPE_OPEN_DRAIN, GPIO_SPEED_LOW, GPIO_PULL_NONE);
	}
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_send_command(S2LP_command_t command) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	SPI_status_t spi1_status = SPI_SUCCESS;
	// Check command.
	if (command >= S2LP_COMMAND_LAST) {
		status = S2LP_ERROR_COMMAND;
		goto errors;
	}
	// Falling edge on CS pin.
	GPIO_write(&GPIO_S2LP_CS, 0);
	// Write sequence.
	spi1_status = SPI1_write_byte(S2LP_HEADER_BYTE_COMMAND); // A/C='1' and W/R='0'.
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(command);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
errors:
	GPIO_write(&GPIO_S2LP_CS, 1); // Set CS pin.
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_wait_for_state(S2LP_state_t new_state) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t state = 0;
	uint8_t reg_value = 0;
	// Poll MC_STATE until state is reached.
	do {
		status = _S2LP_read_register(S2LP_REG_MC_STATE0, &reg_value);
		if (status != S2LP_SUCCESS) goto errors;
		state = (reg_value >> 1) & 0x7F;
	}
	while (state != new_state);
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_oscillator(S2LP_oscillator_t oscillator) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Check parameter.
	if (oscillator >= S2LP_OSCILLATOR_LAST) {
		status = S2LP_ERROR_OSCILLATOR;
		goto errors;
	}
	// Set RFDIV to 0, disable external RCO, configure EXT_REF bit.
	reg_value = (oscillator == S2LP_OSCILLATOR_TCXO) ? 0xB0 : 0x30;
	status = _S2LP_write_register(S2LP_REG_XO_RCO_CONF0, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set digital clock divider according to crytal frequency.
	status = _S2LP_read_register(S2LP_REG_XO_RCO_CONF1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0xEF;
	reg_value |= (S2LP_PD_CLKDIB_BIT << 5);
	status = _S2LP_write_register(S2LP_REG_XO_RCO_CONF1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_wait_for_oscillator(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	uint8_t xo_on = 0;
	uint8_t reg_value = 0;
	uint32_t delay_ms = 0;
	// Poll MC_STATE until XO bit is set.
	do {
		status = _S2LP_read_register(S2LP_REG_MC_STATE0, &reg_value);
		if (status != S2LP_SUCCESS) goto errors;
		xo_on = (reg_value & 0x01);
		// Internal delay.
		lptim1_status = LPTIM1_delay_milliseconds(S2LP_TIMEOUT_SUB_DELAY_MS, LPTIM_DELAY_MODE_STOP);
		LPTIM1_check_status(S2LP_ERROR_BASE_LPTIM);
		// Exit if timeout.
		delay_ms += S2LP_TIMEOUT_SUB_DELAY_MS;
		if (delay_ms > S2LP_TIMEOUT_MS) {
			status = S2LP_ERROR_OSCILLATOR_TIMEOUT;
			goto errors;
		}
	}
	while (xo_on == 0);
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_configure_charge_pump(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Set PLL_CP_ISEL to '010'.
	status = _S2LP_read_register(S2LP_REG_SYNT3, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0x1F;
	reg_value |= (0b010 << 5);
	status = _S2LP_write_register(S2LP_REG_SYNT3, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set PLL_PFD_SPLIT_EN bit according to crystal frequency.
	status = _S2LP_read_register(S2LP_REG_SYNTH_CONFIG2, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0xFB;
	reg_value |= (S2LP_PLL_PFD_SPLIT_EN_BIT << 2);
	status = _S2LP_write_register(S2LP_REG_SYNTH_CONFIG2, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_smps_frequency(uint32_t frequency_hz) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint64_t krm = 0;
	// Compute KRM value.
	krm = (((uint64_t) frequency_hz) << 15) / ((uint64_t) S2LP_FDIG_HZ);
	// Check value.
	if (krm > 0x7FFF) {
		status = S2LP_ERROR_SMPS_FREQUENCY_OVERFLOW;
		goto errors;
	}
	// Program registers.
	status = _S2LP_write_register(S2LP_REG_PM_CONF3, (uint8_t) (((krm >> 8) & 0x7F) | 0x80));
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_PM_CONF2, (uint8_t) (krm & 0xFF));
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_modulation(S2LP_modulation_t modulation) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t mod2_reg_value = 0;
	// Check parameter.
	if (modulation >= S2LP_MODULATION_LAST) {
		status = S2LP_ERROR_MODULATION;
		goto errors;
	}
	// Read register.
	status = _S2LP_read_register(S2LP_REG_MOD2, &mod2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Change required bits.
	mod2_reg_value &= 0x0F;
	mod2_reg_value |= (modulation << 4);
	// Write register.
	status = _S2LP_write_register(S2LP_REG_MOD2, mod2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_rf_frequency(uint32_t frequency_hz) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint64_t synt_value = 0;
	uint8_t synt_reg_value = 0;
	// Check range.
	if (frequency_hz < S2LP_RF_FREQUENCY_HZ_MIN) {
		status = S2LP_ERROR_RF_FREQUENCY_UNDERFLOW;
		goto errors;
	}
	if (frequency_hz > S2LP_RF_FREQUENCY_HZ_MAX) {
		status = S2LP_ERROR_RF_FREQUENCY_OVERFLOW;
		goto errors;
	}
	// Set IF to 300kHz.
	status = _S2LP_write_register(S2LP_REG_IF_OFFSET_ANA, S2LP_IF_OFFSET_ANA);
	if (status != S2LP_SUCCESS) goto errors;
	// See equation p.27 of S2LP datasheet.
	// Set CHNUM to 0.
	status = _S2LP_write_register(S2LP_REG_CHNUM, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	// B=4 for 868MHz (high band, BS=0). REFDIV was set to 0 in oscillator configuration function.
	// SYNT = (fRF * 2^20 * B/2 * D) / (fXO) = (fRF * 2^21) / (fXO).
	synt_value = (((uint64_t) frequency_hz) << 21) / ((uint64_t) S2LP_XO_FREQUENCY_HZ);
	// Write registers.
	status = _S2LP_read_register(S2LP_REG_SYNT3, &synt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	synt_reg_value &= 0xE0; // BS=0 to select high band.
	synt_reg_value |= ((synt_value >> 24) & 0x0F);
	status = _S2LP_write_register(S2LP_REG_SYNT3, synt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	synt_reg_value = (synt_value >> 16) & 0xFF;
	status = _S2LP_write_register(S2LP_REG_SYNT2, synt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	synt_reg_value = (synt_value >> 8) & 0xFF;
	status = _S2LP_write_register(S2LP_REG_SYNT1, synt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	synt_reg_value = (synt_value >> 0) & 0xFF;
	status = _S2LP_write_register(S2LP_REG_SYNT0, synt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_fsk_deviation(uint32_t deviation_hz) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	S2LP_mantissa_exponent_t deviation_setting;
	uint8_t mod1_reg_value = 0;
	// Compute registers.
	status = _S2LP_compute_mantissa_exponent_deviation(deviation_hz, &deviation_setting);
	if (status != S2LP_SUCCESS) goto errors;
	// Write registers.
	status = _S2LP_write_register(S2LP_REG_MOD0, deviation_setting.mantissa);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_read_register(S2LP_REG_MOD1, &mod1_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	mod1_reg_value &= 0xF0;
	mod1_reg_value |= deviation_setting.exponent;
	status = _S2LP_write_register(S2LP_REG_MOD1, mod1_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_datarate(uint32_t datarate_bps) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	S2LP_mantissa_exponent_t datarate_setting;
	uint8_t mod2_reg_value = 0;
	// Compute registers.
	status = _S2LP_compute_mantissa_exponent_datarate(datarate_bps, &datarate_setting);
	if (status != S2LP_SUCCESS) goto errors;
	// Write registers.
	status = _S2LP_write_register(S2LP_REG_MOD4, (datarate_setting.mantissa >> 8) & 0x00FF);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_MOD3, (datarate_setting.mantissa >> 0) & 0x00FF);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_read_register(S2LP_REG_MOD2, &mod2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	mod2_reg_value &= 0xF0;
	mod2_reg_value |= (datarate_setting.exponent);
	status = _S2LP_write_register(S2LP_REG_MOD2, mod2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_configure_pa(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Disable PA power ramping and select slot 0.
	status = _S2LP_write_register(S2LP_REG_PA_POWER0, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	// Disable FIR.
	status = _S2LP_read_register(S2LP_REG_PA_CONFIG1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0xFD;
	status = _S2LP_write_register(S2LP_REG_PA_CONFIG1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Enable interpolator.
	_S2LP_read_register(S2LP_REG_MOD1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value |= 0x80;
	status = _S2LP_write_register(S2LP_REG_MOD1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_rf_output_power(int8_t output_power_dbm) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	uint8_t pa_reg_value = 0;
	// Check parameter.
	if (output_power_dbm > S2LP_RF_OUTPUT_POWER_MAX) {
		status = S2LP_ERROR_RF_OUTPUT_POWER_OVERFLOW;
		goto errors;
	}
	if (output_power_dbm < S2LP_RF_OUTPUT_POWER_MIN) {
		status = S2LP_ERROR_RF_OUTPUT_POWER_UNDERFLOW;
		goto errors;
	}
	// Compute register value.
	pa_reg_value = (uint8_t) (29 - 2 * output_power_dbm);
	// Program register.
	status = _S2LP_read_register(S2LP_REG_PA_POWER1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0x80;
	reg_value |= (pa_reg_value & 0x7F);
	status = _S2LP_write_register(S2LP_REG_PA_POWER1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_tx_source(S2LP_tx_source_t tx_source) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Check parameter.
	if (tx_source >= S2LP_TX_SOURCE_LAST) {
		status = S2LP_ERROR_TX_SOURCE;
		goto errors;
	}
	// Read register.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set bits.
	reg_value &= 0xF3;
	reg_value |= (tx_source << 2);
	// Write register.
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_rx_source(S2LP_rx_source_t rx_source) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Check parameter.
	if (rx_source >= S2LP_RX_SOURCE_LAST) {
		status = S2LP_ERROR_RX_SOURCE;
		goto errors;
	}
	// Read register.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL3, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set bits.
	reg_value &= 0xCF;
	reg_value |= (rx_source << 4);
	// Write register.
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL3, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_rx_bandwidth(uint32_t rx_bandwidth_hz) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	S2LP_mantissa_exponent_t rx_bandwidth_setting;
	uint8_t chflt_reg_value = 0;
	// Compute registers.
	status = _S2LP_compute_mantissa_exponent_rx_bandwidth(rx_bandwidth_hz, &rx_bandwidth_setting);
	if (status != S2LP_SUCCESS) goto errors;
	// Write register.
	chflt_reg_value = ((rx_bandwidth_setting.mantissa << 4) & 0xF0) + (rx_bandwidth_setting.exponent & 0x0F);
	status = _S2LP_write_register(S2LP_REG_CHFLT, chflt_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_disable_equa_cs_ant_switch(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t ant_select_conf_reg_value = 0;
	// Read register.
	status = _S2LP_read_register(S2LP_REG_ANT_SELECT_CONF, &ant_select_conf_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Disable equalization.
	ant_select_conf_reg_value &= 0x83;
	// Program register.
	status = _S2LP_write_register(S2LP_REG_ANT_SELECT_CONF, ant_select_conf_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_disable_afc(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t afc2_reg_value = 0;
	// Read register.
	status = _S2LP_read_register(S2LP_REG_AFC2, &afc2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Disable AFC.
	afc2_reg_value &= 0x1F;
	// Program register.
	status = _S2LP_write_register(S2LP_REG_AFC2, afc2_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_configure_clock_recovery(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Configure registers.
	reg_value = 0x20;
	status = _S2LP_write_register(S2LP_REG_CLOCKREC2, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value = 0x70;
	status = _S2LP_write_register(S2LP_REG_CLOCKREC1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_rssi_threshold(int16_t rssi_threshold_dbm) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	// Check parameter.
	if ((rssi_threshold_dbm < (0 - S2LP_RSSI_OFFSET_DB)) || (rssi_threshold_dbm > (255 - S2LP_RSSI_OFFSET_DB))) {
		status = S2LP_ERROR_RSSI_THRESHOLD;
		goto errors;
	}
	// Program register.
	status = _S2LP_write_register(S2LP_REG_RSSI_TH, (uint8_t) (rssi_threshold_dbm + S2LP_RSSI_OFFSET_DB));
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_get_rssi(S2LP_rssi_t rssi_type, int16_t* rssi_dbm) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t rssi_level_reg_value = 0;
	// Check parameter.
	if (rssi_dbm == NULL) {
		status = S2LP_ERROR_NULL_PARAMETER;
		goto errors;
	}
	// Read accurate register.
	switch (rssi_type) {
	case S2LP_RSSI_TYPE_RUN:
		status = _S2LP_read_register(S2LP_REG_RSSI_LEVEL_RUN, &rssi_level_reg_value);
		break;
	case S2LP_RSSI_TYPE_SYNC_WORD:
		status = _S2LP_read_register(S2LP_REG_RSSI_LEVEL, &rssi_level_reg_value);
		break;
	default:
		status = S2LP_ERROR_RSSI_TYPE;
		break;
	}
	if (status != S2LP_SUCCESS) goto errors;
	// Convert to dBm.
	(*rssi_dbm) = (int16_t) rssi_level_reg_value - (int16_t) S2LP_RSSI_OFFSET_DB - (int16_t) S2LP_RF_FRONT_END_GAIN_DB;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_configure_gpio(S2LP_gpio_t gpio, S2LP_gpio_mode_t mode, uint8_t function, S2LP_fifo_flag_direction_t fifo_flag_direction) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Check parameters.
	if (gpio >= S2LP_GPIO_LAST) {
		status = S2LP_ERROR_GPIO_INDEX;
		goto errors;
	}
	if (mode >= S2LP_GPIO_MODE_LAST) {
		status = S2LP_ERROR_GPIO_MODE;
		goto errors;
	}
	if ((function >= S2LP_GPIO_INPUT_FUNCTION_LAST) && (function >= S2LP_GPIO_OUTPUT_FUNCTION_LAST)) {
		status = S2LP_ERROR_GPIO_FUNCTION;
		goto errors;
	}
	if (fifo_flag_direction >= S2LP_FIFO_FLAG_DIRECTION_LAST) {
		status = S2LP_ERROR_FIFO_FLAG_DIRECTION;
		goto errors;
	}
	// Read corresponding register.
	status = _S2LP_read_register((S2LP_REG_GPIO0_CONF + gpio), &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set required bits.
	reg_value &= 0x04; // Bit 2 is reserved.
	reg_value |= ((mode & 0x02) << 0);
	reg_value |= ((function & 0x1F) << 3);
	// Write register.
	status = _S2LP_write_register((S2LP_REG_GPIO0_CONF + gpio), reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Select FIFO flags.
	status = _S2LP_read_register(S2LP_REG_PROTOCOL2, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	reg_value &= 0xFB;
	reg_value |= ((fifo_flag_direction & 0x01) << 2);
	status = _S2LP_write_register(S2LP_REG_PROTOCOL2, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_configure_irq(S2LP_irq_index_t irq_index, uint8_t irq_enable) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	uint8_t reg_addr_offset = 0;
	uint8_t irq_bit_offset = 0;
	// Check parameter.
	if (irq_index >= S2LP_IRQ_INDEX_LAST) {
		status = S2LP_ERROR_IRQ_INDEX;
		goto errors;
	}
	// Get register and bit offsets.
	reg_addr_offset = (irq_index / 8);
	irq_bit_offset = (irq_index % 8);
	// Read register.
	status = _S2LP_read_register((S2LP_REG_IRQ_MASK0 - reg_addr_offset), &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set bit.
	reg_value &= ~(0b1 << irq_bit_offset);
	reg_value |= (((irq_enable == 0) ? 0b0 : 0b1) << irq_bit_offset);
	// Program register.
	status = _S2LP_write_register((S2LP_REG_IRQ_MASK0 - reg_addr_offset), reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_get_irq_flag(S2LP_irq_index_t irq_index, uint8_t* irq_flag) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	uint8_t reg_addr_offset = 0;
	uint8_t irq_bit_offset = 0;
	// Check parameter.
	if (irq_index >= S2LP_IRQ_INDEX_LAST) {
		status = S2LP_ERROR_IRQ_INDEX;
		goto errors;
	}
	// Get register and bit offsets.
	reg_addr_offset = (irq_index / 8);
	irq_bit_offset = (irq_index % 8);
	// Read register.
	status = _S2LP_read_register((S2LP_REG_IRQ_STATUS0 - reg_addr_offset), &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Read bit.
	(*irq_flag) = (reg_value >> irq_bit_offset) & 0x01;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_disable_all_irq(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	// Reset all masks.
	status = _S2LP_write_register(S2LP_REG_IRQ_MASK3, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_IRQ_MASK2, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_IRQ_MASK1, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_IRQ_MASK0, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_clear_all_irq(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Read IRQ status to clear flags.
	status = _S2LP_read_register(S2LP_REG_IRQ_STATUS3, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_read_register(S2LP_REG_IRQ_STATUS2, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_read_register(S2LP_REG_IRQ_STATUS1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_read_register(S2LP_REG_IRQ_STATUS0, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_packet_length(uint8_t packet_length_bytes) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	// Set length.
	status = _S2LP_write_register(S2LP_REG_PCKTLEN1, 0x00);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_PCKTLEN0, packet_length_bytes);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_preamble_detector(uint8_t preamble_length_2bits, S2LP_preamble_pattern_t preamble_pattern) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t pcktctrlx_reg_value = 0;
	// Check parameter.
	if (preamble_pattern >= S2LP_PREAMBLE_PATTERN_LAST) {
		status = S2LP_ERROR_PREAMBLE_PATTERN;
		goto errors;
	}
	// Set length.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL6, &pcktctrlx_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	pcktctrlx_reg_value &= 0xFC;
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL6, pcktctrlx_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL5, preamble_length_2bits);
	if (status != S2LP_SUCCESS) goto errors;
	// Set pattern.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL3, &pcktctrlx_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	pcktctrlx_reg_value &= 0xFC;
	pcktctrlx_reg_value |= (preamble_pattern & 0x03);
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL3, pcktctrlx_reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_sync_word(uint8_t* sync_word, uint8_t sync_word_length_bits) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t sync_word_length_bytes = 0;
	uint8_t generic_byte = 0;
	// Check parameters.
	if (sync_word == NULL) {
		status = S2LP_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (sync_word_length_bits > S2LP_SYNC_WORD_LENGTH_BITS_MAX) {
		status = S2LP_ERROR_SYNC_WORD_LENGTH;
		goto errors;
	}
	// Set synchronization word.
	sync_word_length_bytes = (sync_word_length_bits / 8);
	if ((sync_word_length_bits - (sync_word_length_bytes * 8)) > 0) {
		sync_word_length_bytes++;
	}
	for (generic_byte=0 ; generic_byte<sync_word_length_bytes ; generic_byte++) {
		status = _S2LP_write_register((S2LP_REG_SYNC0 - generic_byte), sync_word[generic_byte]);
		if (status != S2LP_SUCCESS) goto errors;
	}
	// Set length.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL6, &generic_byte);
	if (status != S2LP_SUCCESS) goto errors;
	generic_byte &= 0x03;
	generic_byte |= (sync_word_length_bits << 2);
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL6, generic_byte);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_disable_crc(void) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	uint8_t reg_value = 0;
	// Read register.
	status = _S2LP_read_register(S2LP_REG_PCKTCTRL1, &reg_value);
	if (status != S2LP_SUCCESS) goto errors;
	// Set bits.
	reg_value &= 0x1F;
	// Write register.
	status = _S2LP_write_register(S2LP_REG_PCKTCTRL1, reg_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_set_fifo_threshold(S2LP_fifo_threshold_t fifo_threshold, uint8_t threshold_value) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	// Check parameters.
	if (fifo_threshold >= S2LP_FIFO_THRESHOLD_LAST) {
		status = S2LP_ERROR_FIFO_THRESHOLD;
		goto errors;
	}
	if (threshold_value > S2LP_FIFO_THHRESHOLD_BYTES_MAX) {
		status = S2LP_ERROR_FIFO_THRESHOLD_VALUE;
		goto errors;
	}
	// Write register.
	status = _S2LP_write_register(fifo_threshold, threshold_value);
	if (status != S2LP_SUCCESS) goto errors;
errors:
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_write_fifo(uint8_t* tx_data, uint8_t tx_data_length_bytes) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	SPI_status_t spi1_status = SPI_SUCCESS;
#ifndef S2LP_TX_FIFO_USE_DMA
	uint8_t idx = 0;
#endif
	// Check parameters.
	if (tx_data == NULL) {
		status = S2LP_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (tx_data_length_bytes > S2LP_FIFO_SIZE_BYTES) {
		status = S2LP_ERROR_TX_DATA_LENGTH;
		goto errors;
	}
#ifdef S2LP_TX_FIFO_USE_DMA
	// Set buffer address.
	DMA1_CH3_set_source_address((uint32_t) tx_data, tx_data_length_bytes);
#endif
	// Falling edge on CS pin.
	GPIO_write(&GPIO_S2LP_CS, 0);
	// Access FIFO.
	spi1_status = SPI1_write_byte(S2LP_HEADER_BYTE_WRITE); // A/C='1' and W/R='0'.
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(S2LP_REG_FIFO);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
#ifdef S2LP_TX_FIFO_USE_DMA
	// Transfer buffer with DMA.
	DMA1_CH3_start();
	while (DMA1_CH3_get_transfer_status() == 0) {
		PWR_enter_sleep_mode();
	}
	DMA1_CH3_stop();
#else
	for (idx=0 ; idx<tx_data_length_bytes ; idx++) {
		spi1_status = SPI1_write_byte(tx_data[idx]);
		SPI1_check_status(S2LP_ERROR_BASE_SPI);
	}
#endif
errors:
	GPIO_write(&GPIO_S2LP_CS, 1); // Set CS pin.
	return status;
}

/*******************************************************************/
S2LP_status_t S2LP_read_fifo(uint8_t* rx_data, uint8_t rx_data_length_bytes) {
	// Local variables.
	S2LP_status_t status = S2LP_SUCCESS;
	SPI_status_t spi1_status = SPI_SUCCESS;
	uint8_t idx = 0;
	// Check parameters.
	if (rx_data == NULL) {
		status = S2LP_ERROR_NULL_PARAMETER;
		goto errors;
	}
	if (rx_data_length_bytes > S2LP_FIFO_SIZE_BYTES) {
		status = S2LP_ERROR_RX_DATA_LENGTH;
		goto errors;
	}
	// Falling edge on CS pin.
	GPIO_write(&GPIO_S2LP_CS, 0);
	// Burst read sequence.
	spi1_status = SPI1_write_byte(S2LP_HEADER_BYTE_READ); // A/C='0' and W/R='1'.
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	spi1_status = SPI1_write_byte(S2LP_REG_FIFO);
	SPI1_check_status(S2LP_ERROR_BASE_SPI);
	for (idx=0 ; idx<rx_data_length_bytes ; idx++) {
		spi1_status = SPI1_read_byte(0xFF, &(rx_data[idx]));
		SPI1_check_status(S2LP_ERROR_BASE_SPI);
	}
errors:
	GPIO_write(&GPIO_S2LP_CS, 1); // Set CS pin.
	return status;
}

#endif /* UHFM */
