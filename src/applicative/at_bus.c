/*
 * at.c
 *
 *  Created on: 18 apr. 2020
 *      Author: Ludo
 */

#include "at_bus.h"

#include "adc.h"
#include "aes.h"
#include "addon_sigfox_rf_protocol_api.h"
#include "bpsm.h"
#include "ddrm.h"
#include "digital.h"
#include "dinfox.h"
#include "error.h"
#include "flash_reg.h"
#include "gpsm.h"
#include "i2c.h"
#include "load.h"
#include "lpuart.h"
#include "lvrm.h"
#include "mapping.h"
#include "math.h"
#include "mode.h"
#include "neom8n.h"
#include "nvic.h"
#include "nvm.h"
#include "parser.h"
#include "pwr.h"
#include "rcc_reg.h"
#include "rf_api.h"
#include "rrm.h"
#include "rtc.h"
#include "sht3x.h"
#include "sigfox_api.h"
#include "sm.h"
#include "string.h"
#include "types.h"
#include "uhfm.h"
#include "version.h"

/*** AT local macros ***/

// Commands.
#define AT_BUS_COMMAND_BUFFER_SIZE			128
// Parameters separator.
#define AT_BUS_CHAR_SEPARATOR				','
// Replies.
#define AT_BUS_REPLY_BUFFER_SIZE			128
#define AT_BUS_STRING_VALUE_BUFFER_SIZE		16
#define AT_BUS_FRAME_END					STRING_CHAR_CR
#define AT_BUS_REPLY_TAB					"     "
#ifdef UHFM
// Duration of RSSI command.
#define AT_BUS_RSSI_REPORT_PERIOD_MS		500
#endif

/*** AT callbacks declaration ***/

static void _AT_BUS_print_ok(void);
static void _AT_BUS_print_command_list(void);
static void _AT_BUS_print_sw_version(void);
static void _AT_BUS_print_error_stack(void);
static void _AT_BUS_adc_callback(void);
static void _AT_BUS_read_callback(void);
static void _AT_BUS_write_callback(void);
#ifdef UHFM
static void _AT_BUS_nvmr_callback(void);
static void _AT_BUS_nvm_callback(void);
static void _AT_BUS_get_id_callback(void);
static void _AT_BUS_set_id_callback(void);
static void _AT_BUS_get_key_callback(void);
static void _AT_BUS_set_key_callback(void);
static void _AT_BUS_so_callback(void);
static void _AT_BUS_sb_callback(void);
static void _AT_BUS_sf_callback(void);
static void _AT_BUS_print_dl_payload(void);
static void _AT_BUS_tm_callback(void);
static void _AT_BUS_cw_callback(void);
static void _AT_BUS_dl_callback(void);
static void _AT_BUS_rssi_callback(void);
#endif /* UHFM */
#ifdef GPSM
static void _AT_BUS_time_callback(void);
static void _AT_BUS_gps_callback(void);
#endif /* GPSM */

/*** AT local structures ***/

typedef struct {
	PARSER_mode_t mode;
	char_t* syntax;
	char_t* parameters;
	char_t* description;
	void (*callback)(void);
} AT_BUS_command_t;

typedef struct {
	// Command.
	volatile char_t command[AT_BUS_COMMAND_BUFFER_SIZE];
	volatile uint32_t command_size;
	volatile uint8_t line_end_flag;
	PARSER_context_t parser;
	// Replies.
	char_t reply[AT_BUS_REPLY_BUFFER_SIZE];
	uint32_t reply_size;
#ifdef UHFM
	// Sigfox RC.
	sfx_rc_t sigfox_rc;
	sfx_u32 sigfox_rc_std_config[SIGFOX_RC_STD_CONFIG_SIZE];
	uint8_t sigfox_rc_idx;
	uint8_t sigfox_dl_payload[SIGFOX_DOWNLINK_DATA_SIZE_BYTES];
	uint8_t sigfox_dl_payload_available;
#endif
} AT_BUS_context_t;

/*** AT local global variables ***/

static const AT_BUS_command_t AT_BUS_COMMAND_LIST[] = {
	{PARSER_MODE_COMMAND, "AT", STRING_NULL, "Ping command", _AT_BUS_print_ok},
	{PARSER_MODE_COMMAND, "AT?", STRING_NULL, "List all available commands", _AT_BUS_print_command_list},
	{PARSER_MODE_COMMAND, "AT$V?", STRING_NULL, "Get SW version", _AT_BUS_print_sw_version},
	{PARSER_MODE_COMMAND, "AT$ERROR?", STRING_NULL, "Read error stack", _AT_BUS_print_error_stack},
	{PARSER_MODE_COMMAND, "AT$RST", STRING_NULL, "Reset MCU", PWR_software_reset},
	{PARSER_MODE_COMMAND, "AT$ADC?", STRING_NULL, "Get ADC measurements", _AT_BUS_adc_callback},
	{PARSER_MODE_HEADER, "AT$R=", "address[hex]", "Read register", _AT_BUS_read_callback},
	{PARSER_MODE_HEADER, "AT$W=", "address[hex],value[hex]", "Write register",_AT_BUS_write_callback},
#ifdef UHFM
	{PARSER_MODE_COMMAND, "AT$NVMR", STRING_NULL, "Reset NVM data", _AT_BUS_nvmr_callback},
	{PARSER_MODE_HEADER,  "AT$NVM=", "address[dec]", "Get NVM data", _AT_BUS_nvm_callback},
	{PARSER_MODE_COMMAND, "AT$ID?", STRING_NULL, "Get Sigfox device ID", _AT_BUS_get_id_callback},
	{PARSER_MODE_HEADER,  "AT$ID=", "id[hex]", "Set Sigfox device ID", _AT_BUS_set_id_callback},
	{PARSER_MODE_COMMAND, "AT$KEY?", STRING_NULL, "Get Sigfox device key", _AT_BUS_get_key_callback},
	{PARSER_MODE_HEADER,  "AT$KEY=", "key[hex]", "Set Sigfox device key", _AT_BUS_set_key_callback},
	{PARSER_MODE_COMMAND, "AT$SO", STRING_NULL, "Sigfox send control message", _AT_BUS_so_callback},
	{PARSER_MODE_HEADER,  "AT$SB=", "data[bit],(bidir_flag[bit])", "Sigfox send bit", _AT_BUS_sb_callback},
	{PARSER_MODE_HEADER,  "AT$SF=", "data[hex],(bidir_flag[bit])", "Sigfox send frame", _AT_BUS_sf_callback},
	{PARSER_MODE_COMMAND,  "AT$DL?", STRING_NULL, "Read last DL payload", _AT_BUS_print_dl_payload},
	{PARSER_MODE_HEADER,  "AT$TM=", "rc_index[dec],test_mode[dec]", "Execute Sigfox test mode", _AT_BUS_tm_callback},
	{PARSER_MODE_HEADER,  "AT$CW=", "frequency[hz],enable[bit],(output_power[dbm])", "Start or stop continuous radio transmission", _AT_BUS_cw_callback},
	{PARSER_MODE_HEADER,  "AT$DL=", "frequency[hz]", "Continuous downlink frames decoding", _AT_BUS_dl_callback},
	{PARSER_MODE_HEADER,  "AT$RSSI=", "frequency[hz],duration[s]", "Start or stop continuous RSSI measurement", _AT_BUS_rssi_callback},
#endif /* UHFM */
#ifdef GPSM
	{PARSER_MODE_HEADER, "AT$TIME=", "timeout[s]", "Get GPS time", _AT_BUS_time_callback},
	{PARSER_MODE_HEADER, "AT$GPS=", "timeout[s]", "Get GPS position", _AT_BUS_gps_callback},
#endif
};

static AT_BUS_context_t at_bus_ctx;

/*** AT local functions ***/

/* GENERIC MACRO TO ADD A CHARACTER TO THE REPLY BUFFER.
 * @param character:	Character to add.
 * @return:				None.
 */
#define _AT_BUS_reply_add_char(character) { \
	at_bus_ctx.reply[at_bus_ctx.reply_size] = character; \
	at_bus_ctx.reply_size = (at_bus_ctx.reply_size + 1) % AT_BUS_REPLY_BUFFER_SIZE; \
}

/* APPEND A STRING TO THE REPONSE BUFFER.
 * @param tx_string:	String to add.
 * @return:				None.
 */
static void _AT_BUS_reply_add_string(char_t* tx_string) {
	// Fill reply buffer with new bytes.
	while (*tx_string) {
		_AT_BUS_reply_add_char(*(tx_string++));
	}
}

/* APPEND A VALUE TO THE REPONSE BUFFER.
 * @param tx_value:		Value to add.
 * @param format:       Printing format.
 * @param print_prefix: Print base prefix is non zero.
 * @return:				None.
 */
static void _AT_BUS_reply_add_value(int32_t tx_value, STRING_format_t format, uint8_t print_prefix) {
	// Local variables.
	STRING_status_t string_status = STRING_SUCCESS;
	char_t str_value[AT_BUS_STRING_VALUE_BUFFER_SIZE];
	uint8_t idx = 0;
	// Reset string.
	for (idx=0 ; idx<AT_BUS_STRING_VALUE_BUFFER_SIZE ; idx++) str_value[idx] = STRING_CHAR_NULL;
	// Convert value to string.
	string_status = STRING_value_to_string(tx_value, format, print_prefix, str_value);
	STRING_error_check();
	// Add string.
	_AT_BUS_reply_add_string(str_value);
}

/* SEND AT REPONSE OVER AT INTERFACE.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_reply_send(void) {
	// Local variables.
	LBUS_status_t lbus_status = LBUS_SUCCESS;
	// Add ending character.
	_AT_BUS_reply_add_char(AT_BUS_FRAME_END);
	// Send reply.
	lbus_status = LBUS_send((uint8_t*) at_bus_ctx.reply, at_bus_ctx.reply_size);
	LBUS_error_check();
	// Flush response buffer.
	at_bus_ctx.reply_size = 0;
}

/* PRINT OK THROUGH AT INTERFACE.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_print_ok(void) {
	_AT_BUS_reply_add_string("OK");
	_AT_BUS_reply_send();
}

/* PRINT ERROR THROUGH AT INTERFACE.
 * @param error:	Error to print.
 * @return:			None.
 */
static void _AT_BUS_print_error(ERROR_t error) {
	// Add error to stack.
	ERROR_stack_add(error);
	// Print error.
	_AT_BUS_reply_add_string("ERROR_");
	if (error < 0x0100) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_HEXADECIMAL, 1);
		_AT_BUS_reply_add_value((int32_t) error, STRING_FORMAT_HEXADECIMAL, 0);
	}
	else {
		_AT_BUS_reply_add_value((int32_t) error, STRING_FORMAT_HEXADECIMAL, 1);
	}
	_AT_BUS_reply_send();
}

/* PRINT ALL SUPPORTED AT COMMANDS.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_print_command_list(void) {
	// Local variables.
	uint32_t idx = 0;
	// Commands loop.
	for (idx=0 ; idx<(sizeof(AT_BUS_COMMAND_LIST) / sizeof(AT_BUS_command_t)) ; idx++) {
		// Print syntax.
		_AT_BUS_reply_add_string(AT_BUS_COMMAND_LIST[idx].syntax);
		// Print parameters.
		_AT_BUS_reply_add_string(AT_BUS_COMMAND_LIST[idx].parameters);
		_AT_BUS_reply_send();
		// Print description.
		_AT_BUS_reply_add_string(AT_BUS_REPLY_TAB);
		_AT_BUS_reply_add_string(AT_BUS_COMMAND_LIST[idx].description);
		_AT_BUS_reply_send();
	}
	_AT_BUS_print_ok();
}

/* PRINT SW VERSION.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_print_sw_version(void) {
	_AT_BUS_reply_add_string("SW");
	_AT_BUS_reply_add_value((int32_t) GIT_MAJOR_VERSION, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string(".");
	_AT_BUS_reply_add_value((int32_t) GIT_MINOR_VERSION, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string(".");
	_AT_BUS_reply_add_value((int32_t) GIT_COMMIT_INDEX, STRING_FORMAT_DECIMAL, 0);
	if (GIT_DIRTY_FLAG != 0) {
		_AT_BUS_reply_add_string(".d");
	}
	_AT_BUS_reply_add_string(" (");
	_AT_BUS_reply_add_value((int32_t) GIT_COMMIT_ID, STRING_FORMAT_HEXADECIMAL, 1);
	_AT_BUS_reply_add_string(")");
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
}

/* PRINT ERROR STACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_print_error_stack(void) {
	// Local variables.
	ERROR_t error = SUCCESS;
	// Read stack.
	if (ERROR_stack_is_empty() != 0) {
		_AT_BUS_reply_add_string("Error stack empty");
	}
	else {
		// Unstack all errors.
		_AT_BUS_reply_add_string("[ ");
		do {
			error = ERROR_stack_read();
			if (error != SUCCESS) {
				_AT_BUS_reply_add_value((int32_t) error, STRING_FORMAT_HEXADECIMAL, 1);
				_AT_BUS_reply_add_string(" ");
			}
		}
		while (error != SUCCESS);
		_AT_BUS_reply_add_string("]");
	}
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
}

/* AT$ADC? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_adc_callback(void) {
	// Local variables.
	ADC_status_t adc1_status = ADC_SUCCESS;
	uint32_t generic_u32 = 0;
	int8_t tmcu_degrees = 0;
#if (defined SM) && (defined SM_AIN_ENABLE)
	uint8_t idx = 0;
#endif
	// Trigger internal ADC conversions.
	adc1_status = ADC1_perform_measurements();
	ADC1_error_check_print();
	// Read and print data.
	// MCU voltage.
	_AT_BUS_reply_add_string("Vmcu=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VMCU_MV, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
	// MCU temperature.
	_AT_BUS_reply_add_string("Tmcu=");
	adc1_status = ADC1_get_tmcu(&tmcu_degrees);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) tmcu_degrees, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("dC");
	_AT_BUS_reply_send();
	// Input voltage.
#ifdef LVRM
	_AT_BUS_reply_add_string("Vcom=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VCOM_MV, &generic_u32);
#endif
#ifdef BPSM
	_AT_BUS_reply_add_string("Vsrc=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VSRC_MV, &generic_u32);
#endif
#if (defined DDRM) || (defined RRM)
	_AT_BUS_reply_add_string("Vin=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VIN_MV, &generic_u32);
#endif
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
#endif
	// Storage element voltage.
#ifdef BPSM
	_AT_BUS_reply_add_string("Vstr=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VSTR_MV, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
#endif
	// Output voltage.
#if (defined LVRM) || (defined DDRM) || (defined RRM)
	_AT_BUS_reply_add_string("Vout=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VOUT_MV, &generic_u32);
#endif
#ifdef BPSM
	_AT_BUS_reply_add_string("Vbkp=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VBKP_MV, &generic_u32);
#endif
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
#endif
	// Output current.
#if (defined LVRM) || (defined DDRM) || (defined RRM)
	_AT_BUS_reply_add_string("Iout=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_IOUT_UA, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("uA");
	_AT_BUS_reply_send();
#endif
	// AINx.
#if (defined SM) && (defined SM_AIN_ENABLE)
	for (idx=ADC_DATA_INDEX_AIN0_MV ; idx<=ADC_DATA_INDEX_AIN3_MV ; idx++) {
		_AT_BUS_reply_add_string("AIN");
		_AT_BUS_reply_add_value((int32_t) (idx - ADC_DATA_INDEX_AIN0_MV), STRING_FORMAT_DECIMAL, 0);
		_AT_BUS_reply_add_string("=");
		adc1_status = ADC1_get_data(idx, &generic_u32);
		ADC1_error_check_print();
		_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
		_AT_BUS_reply_add_string("mV");
		_AT_BUS_reply_send();
	}
#endif
	// Radio voltages.
#ifdef UHFM
	_AT_BUS_reply_add_string("Vrf=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VRF_MV, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
#endif
#ifdef GPSM
	_AT_BUS_reply_add_string("Vgps=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VGPS_MV, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
	_AT_BUS_reply_add_string("Vant=");
	adc1_status = ADC1_get_data(ADC_DATA_INDEX_VANT_MV, &generic_u32);
	ADC1_error_check_print();
	_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("mV");
	_AT_BUS_reply_send();
#endif
	_AT_BUS_print_ok();
errors:
	return;
}

/* AT$R EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_read_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_SUCCESS;
	NVM_status_t nvm_status = NVM_SUCCESS;
	ADC_status_t adc1_status = ADC_SUCCESS;
	MATH_status_t math_status = MATH_SUCCESS;
#ifdef SM
	DIGITAL_status_t digital_status = DIGITAL_SUCCESS;
	SHT3X_status_t sht3x_status = SHT3X_SUCCESS;
#endif
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
	LOAD_status_t load_status = LOAD_SUCCESS;
#endif
	int32_t register_address = 0;
	uint8_t generic_u8 = 0;
	int8_t generic_s8 = 0;
	uint32_t generic_u32 = 0;
	// Read address parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_HEXADECIMAL, STRING_CHAR_NULL, &register_address);
	PARSER_error_check_print();
	// Get data.
	switch (register_address) {
	case DINFOX_REGISTER_NODE_ADDRESS:
		nvm_status = NVM_read_byte(NVM_ADDRESS_SELF_ADDRESS, &generic_u8);
		NVM_error_check_print();
		_AT_BUS_reply_add_value(generic_u8, STRING_FORMAT_HEXADECIMAL, 0);
		break;
	case DINFOX_REGISTER_BOARD_ID:
#ifdef LVRM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_LVRM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef BPSM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_BPSM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef DDRM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_DDRM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef RRM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_RRM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef SM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_SM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef UHFM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_UHFM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
#ifdef GPSM
		_AT_BUS_reply_add_value(DINFOX_BOARD_ID_GPSM, STRING_FORMAT_HEXADECIMAL, 0);
#endif
		break;
	case DINFOX_REGISTER_HW_VERSION_MAJOR:
#ifdef HW1_0
		_AT_BUS_reply_add_value(1, STRING_FORMAT_DECIMAL, 0);
#endif
#ifdef HW2_0
		_AT_BUS_reply_add_value(2, STRING_FORMAT_DECIMAL, 0);
#endif
		break;
	case DINFOX_REGISTER_HW_VERSION_MINOR:
#if (defined HW1_0) || (defined HW2_0)
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
#endif
		break;
	case DINFOX_REGISTER_SW_VERSION_MAJOR:
		_AT_BUS_reply_add_value(GIT_MAJOR_VERSION, STRING_FORMAT_DECIMAL, 0);
		break;
	case DINFOX_REGISTER_SW_VERSION_MINOR:
		_AT_BUS_reply_add_value(GIT_MINOR_VERSION, STRING_FORMAT_DECIMAL, 0);
		break;
	case DINFOX_REGISTER_SW_VERSION_COMMIT_INDEX:
		_AT_BUS_reply_add_value(GIT_COMMIT_INDEX, STRING_FORMAT_DECIMAL, 0);
		break;
	case DINFOX_REGISTER_SW_VERSION_COMMIT_ID:
		_AT_BUS_reply_add_value(GIT_COMMIT_ID, STRING_FORMAT_HEXADECIMAL, 0);
		break;
	case DINFOX_REGISTER_SW_VERSION_DIRTY_FLAG:
		_AT_BUS_reply_add_value(GIT_DIRTY_FLAG, STRING_FORMAT_BOOLEAN, 0);
		break;
	case DINFOX_REGISTER_ERROR_STACK:
		_AT_BUS_reply_add_value(ERROR_stack_read(), STRING_FORMAT_HEXADECIMAL, 0);
		break;
	case DINFOX_REGISTER_RESET_REASON:
		_AT_BUS_reply_add_value((((RCC -> CSR) >> 24) & 0xFF), STRING_FORMAT_HEXADECIMAL, 0);
		break;
	case DINFOX_REGISTER_VMCU_MV:
#ifdef LVRM
	case LVRM_REGISTER_VCOM_MV:
	case LVRM_REGISTER_VOUT_MV:
	case LVRM_REGISTER_IOUT_UA:
#endif
#ifdef BPSM
	case BPSM_REGISTER_VSRC_MV:
	case BPSM_REGISTER_VSTR_MV:
	case BPSM_REGISTER_VBKP_MV:
#endif
#ifdef DDRM
	case DDRM_REGISTER_VIN_MV:
	case DDRM_REGISTER_VOUT_MV:
	case DDRM_REGISTER_IOUT_UA:
#endif
#ifdef RRM
	case RRM_REGISTER_VIN_MV:
	case RRM_REGISTER_VOUT_MV:
	case RRM_REGISTER_IOUT_UA:
#endif
#ifdef SM
	case SM_REGISTER_AIN0_MV:
	case SM_REGISTER_AIN1_MV:
	case SM_REGISTER_AIN2_MV:
	case SM_REGISTER_AIN3_MV:
#endif
#ifdef UHFM
	case UHFM_REGISTER_VRF_MV:
#endif
#ifdef GPSM
	case GPSM_REGISTER_VGPS_MV:
	case GPSM_REGISTER_VANT_MV:
#endif
#if (defined SM) && !(defined SM_AIN_ENABLE)
		// AINx are disabled.
		if (register_address != DINFOX_REGISTER_VMCU_MV) {
			_AT_BUS_print_error(ERROR_REGISTER_UNSUPPORTED);
			goto errors;
		}
#endif
		// Note: indexing only works if registers addresses are ordered in the same way as ADC data indexes.
		adc1_status = ADC1_get_data((register_address - DINFOX_REGISTER_VMCU_MV), &generic_u32);
		ADC1_error_check_print();
		_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
		break;
	case DINFOX_REGISTER_TMCU_DEGREES:
		// Read temperature.
		adc1_status = ADC1_get_tmcu(&generic_s8);
		ADC1_error_check_print();
		// Convert to 1-complement.
		math_status = MATH_one_complement(generic_s8, 7, &generic_u32);
		MATH_error_check_print();
		// Send result.
		_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
		break;
#ifdef SM
	case SM_REGISTER_DIO0:
	case SM_REGISTER_DIO1:
	case SM_REGISTER_DIO2:
	case SM_REGISTER_DIO3:
#if !(defined SM_DIO_ENABLE)
		// DIOx are disabled.
		_AT_BUS_print_error(ERROR_REGISTER_UNSUPPORTED);
		goto errors;
#endif
		// Note: indexing only works if registers addresses are ordered in the same way as digital data indexes.
		digital_status = DIGITAL_read((register_address - SM_REGISTER_DIO0), &generic_u8);
		DIGITAL_error_check_print();
		_AT_BUS_reply_add_value((int32_t) generic_u8, STRING_FORMAT_BOOLEAN, 0);
		break;
	case SM_REGISTER_TAMB_DEGREES:
#if !(defined SM_DIGITAL_SENSORS_ENABLE)
		// Digital sensors are disabled.
		_AT_BUS_print_error(ERROR_REGISTER_UNSUPPORTED);
		goto errors;
#endif
		// Read temperature.
		sht3x_status = SHT3X_get_temperature(&generic_s8);
		SHT3X_error_check_print();
		// Convert to 1-complement.
		math_status = MATH_one_complement(generic_s8, 7, &generic_u32);
		MATH_error_check_print();
		// Send result.
		_AT_BUS_reply_add_value((int32_t) generic_u32, STRING_FORMAT_DECIMAL, 0);
		break;
	case SM_REGISTER_HAMB_PERCENT:
#if !(defined SM_DIGITAL_SENSORS_ENABLE)
		// Digital sensors are disabled.
		_AT_BUS_print_error(ERROR_REGISTER_UNSUPPORTED);
		goto errors;
#endif
		// Read humidity.
		sht3x_status = SHT3X_get_humidity(&generic_u8);
		SHT3X_error_check_print();
		_AT_BUS_reply_add_value((int32_t) generic_u8, STRING_FORMAT_DECIMAL, 0);
		break;
#endif /* SM */
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
#ifdef LVRM
	case LVRM_REGISTER_RELAY_ENABLE:
#endif
#ifdef BPSM
	case BPSM_REGISTER_BACKUP_ENABLE:
#endif
#ifdef DDRM
	case DDRM_REGISTER_DC_DC_ENABLE:
#endif
#ifdef RRM
	case RRM_REGISTER_REGULATOR_ENABLE:
#endif
		load_status = LOAD_get_output_state(&generic_u8);
		LOAD_error_check_print();
		_AT_BUS_reply_add_value((int32_t) generic_u8, STRING_FORMAT_BOOLEAN, 0);
		break;
#endif
#ifdef BPSM
	case BPSM_REGISTER_CHARGE_ENABLE:
		_AT_BUS_reply_add_value(LOAD_get_charge_state(), STRING_FORMAT_BOOLEAN, 0);
		break;
	case BPSM_REGISTER_CHARGE_STATUS:
		_AT_BUS_reply_add_value(LOAD_get_charge_status(), STRING_FORMAT_BOOLEAN, 0);
		break;
#endif
	default:
		_AT_BUS_print_error(ERROR_REGISTER_ADDRESS);
		goto errors;
	}
	// Send response.
	_AT_BUS_reply_send();
errors:
	return;
}

/* AT$W EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_write_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_SUCCESS;
	int32_t register_address = 0;
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
	int32_t register_value = 0;
	LOAD_status_t load_status = LOAD_SUCCESS;
#endif
	// Read address parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_HEXADECIMAL, AT_BUS_CHAR_SEPARATOR, &register_address);
	PARSER_error_check_print();
	// Check address.
#ifdef LVRM
	if (register_address >= LVRM_REGISTER_LAST) {
#endif
#ifdef BPSM
	if (register_address >= BPSM_REGISTER_LAST) {
#endif
#ifdef DDRM
	if (register_address >= DDRM_REGISTER_LAST) {
#endif
#ifdef RRM
	if (register_address >= RRM_REGISTER_LAST) {
#endif
#ifdef SM
	if (register_address >= SM_REGISTER_LAST) {
#endif
#ifdef UHFM
	if (register_address >= UHFM_REGISTER_LAST) {
#endif
#ifdef GPSM
	if (register_address >= GPSM_REGISTER_LAST) {
#endif
		_AT_BUS_print_error(ERROR_REGISTER_ADDRESS);
		goto errors;
	}
	// Write data.
	switch (register_address) {
#if (defined LVRM) || (defined BPSM) || (defined DDRM) || (defined RRM)
#ifdef LVRM
	case LVRM_REGISTER_RELAY_ENABLE:
#endif
#ifdef BPSM
	case BPSM_REGISTER_BACKUP_ENABLE:
#endif
#ifdef DDRM
	case DDRM_REGISTER_DC_DC_ENABLE:
#endif
#ifdef RRM
	case RRM_REGISTER_REGULATOR_ENABLE:
#endif
		// Read new output state.
		parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &register_value);
		PARSER_error_check_print();
		// Set output state.
		load_status = LOAD_set_output_state(register_value);
		LOAD_error_check_print();
		break;
#endif /* LVRM or BPSM or DDRM or RRM */
#ifdef BPSM
	case BPSM_REGISTER_CHARGE_ENABLE:
		// Read new output state.
		parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &register_value);
		PARSER_error_check_print();
		// Set charge state.
		LOAD_set_charge_state(register_value);
		break;
#endif
	default:
		_AT_BUS_print_error(ERROR_REGISTER_READ_ONLY);
		goto errors;
	}
	// Operation completed.
	_AT_BUS_print_ok();
errors:
	return;
}

#ifdef UHFM
/* AT$NVMR EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_nvmr_callback(void) {
	// Local variables.
	NVM_status_t nvm_status = NVM_SUCCESS;
	// Reset all NVM field to default value.
	nvm_status = NVM_reset_default();
	NVM_error_check_print();
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$NVM EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_nvm_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	NVM_status_t nvm_status = NVM_SUCCESS;
	int32_t address = 0;
	uint8_t nvm_data = 0;
	// Read address parameters.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &address);
	PARSER_error_check_print();
	// Read byte at requested address.
	nvm_status = NVM_read_byte((uint16_t) address, &nvm_data);
	NVM_error_check_print();
	// Print data.
	_AT_BUS_reply_add_value(nvm_data, STRING_FORMAT_HEXADECIMAL, 1);
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$ID? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_get_id_callback(void) {
	// Local variables.
	NVM_status_t nvm_status = NVM_SUCCESS;
	uint8_t idx = 0;
	uint8_t id_byte = 0;
	// Retrieve device ID in NVM.
	for (idx=0 ; idx<ID_LENGTH ; idx++) {
		nvm_status = NVM_read_byte((NVM_ADDRESS_SIGFOX_DEVICE_ID + ID_LENGTH - idx - 1), &id_byte);
		NVM_error_check_print();
		_AT_BUS_reply_add_value(id_byte, STRING_FORMAT_HEXADECIMAL, (idx==0 ? 1 : 0));
	}
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$ID EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_set_id_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	NVM_status_t nvm_status = NVM_SUCCESS;
	uint8_t device_id[ID_LENGTH];
	uint8_t extracted_length = 0;
	uint8_t idx = 0;
	// Read ID parameter.
	parser_status = PARSER_get_byte_array(&at_bus_ctx.parser, STRING_CHAR_NULL, ID_LENGTH, 1, device_id, &extracted_length);
	PARSER_error_check_print();
	// Write device ID in NVM.
	for (idx=0 ; idx<ID_LENGTH ; idx++) {
		nvm_status = NVM_write_byte((NVM_ADDRESS_SIGFOX_DEVICE_ID + ID_LENGTH - idx - 1), device_id[idx]);
		NVM_error_check_print();
	}
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$KEY? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_get_key_callback(void) {
	// Local variables.
	NVM_status_t nvm_status = NVM_SUCCESS;
	uint8_t idx = 0;
	uint8_t key_byte = 0;
	// Retrieve device key in NVM.
	for (idx=0 ; idx<AES_BLOCK_SIZE ; idx++) {
		nvm_status = NVM_read_byte((NVM_ADDRESS_SIGFOX_DEVICE_KEY + idx), &key_byte);
		NVM_error_check_print();
		_AT_BUS_reply_add_value(key_byte, STRING_FORMAT_HEXADECIMAL, (idx==0 ? 1 : 0));
	}
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$KEY EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_set_key_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	NVM_status_t nvm_status = NVM_SUCCESS;
	uint8_t device_key[AES_BLOCK_SIZE];
	uint8_t extracted_length = 0;
	uint8_t idx = 0;
	// Read key parameter.
	parser_status = PARSER_get_byte_array(&at_bus_ctx.parser, STRING_CHAR_NULL, AES_BLOCK_SIZE, 1, device_key, &extracted_length);
	PARSER_error_check_print();
	// Write device ID in NVM.
	for (idx=0 ; idx<AES_BLOCK_SIZE ; idx++) {
		nvm_status = NVM_write_byte((NVM_ADDRESS_SIGFOX_DEVICE_KEY + idx), device_key[idx]);
		NVM_error_check_print();
	}
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$SO EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_so_callback(void) {
	// Local variables.
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	// Send Sigfox OOB frame.
	sigfox_api_status = SIGFOX_API_open(&at_bus_ctx.sigfox_rc);
	SIGFOX_API_error_check_print();
	sigfox_api_status = SIGFOX_API_send_outofband(SFX_OOB_SERVICE);
	SIGFOX_API_error_check_print();
	_AT_BUS_print_ok();
errors:
	sigfox_api_status = SIGFOX_API_close();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef UHFM
/* AT$SB EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_sb_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	int32_t data = 0;
	int32_t bidir_flag = 0;
	// First try with 2 parameters.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, AT_BUS_CHAR_SEPARATOR, &data);
	if (parser_status == PARSER_SUCCESS) {
		// Try parsing downlink request parameter.
		parser_status =  PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &bidir_flag);
		PARSER_error_check_print();
		// Reset DL payload availability flag if downlink is asked.
		if (bidir_flag != 0) {
			at_bus_ctx.sigfox_dl_payload_available = 0;
		}
		// Send Sigfox bit with specified downlink request.
		sigfox_api_status = SIGFOX_API_open(&at_bus_ctx.sigfox_rc);
		SIGFOX_API_error_check_print();
		sigfox_api_status = SIGFOX_API_send_bit((sfx_bool) data, at_bus_ctx.sigfox_dl_payload, 2, (sfx_bool) bidir_flag);
		// Catch downlink timeout error.
		if (((sfx_u8) (sigfox_api_status & 0x00FF)) == SFX_ERR_INT_GET_RECEIVED_FRAMES_TIMEOUT) {
			// Reset flag.
			at_bus_ctx.sigfox_dl_payload_available = 0;
		}
		else {
			SIGFOX_API_error_check_print();
			if (bidir_flag != 0) {
				// Set flag.
				at_bus_ctx.sigfox_dl_payload_available = 1;
			}
		}
	}
	else {
		// Try with 1 parameter.
		parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &data);
		PARSER_error_check_print();
		// Send Sigfox bit with no downlink request (by default).
		sigfox_api_status = SIGFOX_API_open(&at_bus_ctx.sigfox_rc);
		SIGFOX_API_error_check_print();
		sigfox_api_status = SIGFOX_API_send_bit((sfx_bool) data, at_bus_ctx.sigfox_dl_payload, 2, 0);
		SIGFOX_API_error_check_print();
	}
	_AT_BUS_print_ok();
errors:
	sigfox_api_status = SIGFOX_API_close();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef UHFM
/* AT$SF EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_sf_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	sfx_u8 data[SIGFOX_UPLINK_DATA_MAX_SIZE_BYTES];
	uint8_t extracted_length = 0;
	int32_t bidir_flag = 0;
	// First try with 2 parameters.
	parser_status = PARSER_get_byte_array(&at_bus_ctx.parser, AT_BUS_CHAR_SEPARATOR, 12, 0, data, &extracted_length);
	if (parser_status == PARSER_SUCCESS) {
		// Try parsing downlink request parameter.
		parser_status =  PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &bidir_flag);
		PARSER_error_check_print();
		// Reset DL payload availability flag if downlink is asked.
		if (bidir_flag != 0) {
			at_bus_ctx.sigfox_dl_payload_available = 0;
		}
		// Send Sigfox frame with specified downlink request.
		sigfox_api_status = SIGFOX_API_open(&at_bus_ctx.sigfox_rc);
		SIGFOX_API_error_check_print();
		sigfox_api_status = SIGFOX_API_send_frame(data, extracted_length, at_bus_ctx.sigfox_dl_payload, 2, bidir_flag);
		// Catch downlink timeout error.
		if (((sfx_u8) (sigfox_api_status & 0x00FF)) == SFX_ERR_INT_GET_RECEIVED_FRAMES_TIMEOUT) {
			// Reset flag.
			at_bus_ctx.sigfox_dl_payload_available = 0;
		}
		else {
			SIGFOX_API_error_check_print();
			if (bidir_flag != 0) {
				// Set flag.
				at_bus_ctx.sigfox_dl_payload_available = 1;
			}
		}
	}
	else {
		// Try with 1 parameter.
		parser_status = PARSER_get_byte_array(&at_bus_ctx.parser, STRING_CHAR_NULL, 12, 0, data, &extracted_length);
		PARSER_error_check_print();
		// Send Sigfox frame with no downlink request (by default).
		sigfox_api_status = SIGFOX_API_open(&at_bus_ctx.sigfox_rc);
		SIGFOX_API_error_check_print();
		sigfox_api_status = SIGFOX_API_send_frame(data, extracted_length, at_bus_ctx.sigfox_dl_payload, 2, 0);
		SIGFOX_API_error_check_print();
	}
	_AT_BUS_print_ok();
errors:
	sigfox_api_status = SIGFOX_API_close();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef UHFM
/* AT$DL? EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_print_dl_payload(void) {
	// Local variables.
	uint8_t idx = 0;
	// Check flag.
	if (at_bus_ctx.sigfox_dl_payload_available != 0) {
		// Print last DL payload.
		for (idx=0 ; idx<SIGFOX_DOWNLINK_DATA_SIZE_BYTES ; idx++) {
			_AT_BUS_reply_add_value(at_bus_ctx.sigfox_dl_payload[idx], STRING_FORMAT_HEXADECIMAL, 0);
		}
	}
	else {
		_AT_BUS_print_error(ERROR_DL_PAYLOAD_UNAVAILABLE);
		goto errors;
	}
	_AT_BUS_reply_send();
errors:
	return;
}
#endif

#ifdef UHFM
/* PRINT SIGFOX DOWNLINK FRAME ON AT INTERFACE.
 * @param dl_payload:	Downlink data to print.
 * @return:				None.
 */
static void _AT_BUS_print_dl_phy_content(sfx_u8* dl_phy_content, int32_t rssi_dbm) {
	// Local variables.
	uint8_t idx = 0;
	// Print DL-PHY content.
	_AT_BUS_reply_add_string("+DL_PHY=");
	for (idx=0 ; idx<SIGFOX_DOWNLINK_PHY_SIZE_BYTES ; idx++) {
		_AT_BUS_reply_add_value(dl_phy_content[idx], STRING_FORMAT_HEXADECIMAL, 0);
	}
	_AT_BUS_reply_add_string(" RSSI=");
	_AT_BUS_reply_add_value(rssi_dbm, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("dBm");
	_AT_BUS_reply_send();
}
#endif

#ifdef UHFM
/* AT$TM EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_tm_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	int32_t rc_index = 0;
	int32_t test_mode = 0;
	// Read RC parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, AT_BUS_CHAR_SEPARATOR, &rc_index);
	PARSER_error_check_print();
	// Read test mode parameter.
	parser_status =  PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &test_mode);
	PARSER_error_check_print();
	// Call test mode function wth public key.
	sigfox_api_status = ADDON_SIGFOX_RF_PROTOCOL_API_test_mode((sfx_rc_enum_t) rc_index, (sfx_test_mode_t) test_mode);
	SIGFOX_API_error_check_print();
	_AT_BUS_print_ok();
errors:
	return;
}
#endif

#ifdef UHFM
/* AT$CW EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_cw_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	S2LP_status_t s2lp_status = S2LP_SUCCESS;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	int32_t enable = 0;
	int32_t frequency_hz = 0;
	int32_t power_dbm = 0;
	// Read frequency parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, AT_BUS_CHAR_SEPARATOR, &frequency_hz);
	PARSER_error_check_print();
	// First try with 3 parameters.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, AT_BUS_CHAR_SEPARATOR, &enable);
	if (parser_status == PARSER_SUCCESS) {
		// There is a third parameter, try to parse power.
		parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &power_dbm);
		PARSER_error_check_print();
		// CW with given output power.
		SIGFOX_API_stop_continuous_transmission();
		if (enable != 0) {
			sigfox_api_status = SIGFOX_API_start_continuous_transmission((sfx_u32) frequency_hz, SFX_NO_MODULATION);
			SIGFOX_API_error_check_print();
			s2lp_status = S2LP_set_rf_output_power((int8_t) power_dbm);
			S2LP_error_check_print();
		}
	}
	else {
		// Power is not given, try to parse enable as last parameter.
		parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_BOOLEAN, STRING_CHAR_NULL, &enable);
		PARSER_error_check_print();
		// CW with last output power.
		SIGFOX_API_stop_continuous_transmission();
		if (enable != 0) {
			sigfox_api_status = SIGFOX_API_start_continuous_transmission((sfx_u32) frequency_hz, SFX_NO_MODULATION);
			SIGFOX_API_error_check_print();
		}
	}
	_AT_BUS_print_ok();
	return;
errors:
	sigfox_api_status = SIGFOX_API_stop_continuous_transmission();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef UHFM
/* AT$DL EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_dl_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	sfx_u8 dl_phy_content[SIGFOX_DOWNLINK_PHY_SIZE_BYTES];
	sfx_s16 rssi_dbm = 0;
	sfx_rx_state_enum_t dl_status = DL_PASSED;
	int32_t frequency_hz = 0;
	// Read frequency parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &frequency_hz);
	PARSER_error_check_print();
	// Start radio.
	sigfox_api_status = RF_API_init(SFX_RF_MODE_RX);
	SIGFOX_API_error_check_print();
	sigfox_api_status = RF_API_change_frequency(frequency_hz);
	SIGFOX_API_error_check_print();
	while (dl_status == DL_PASSED) {
		sigfox_api_status = RF_API_wait_frame(dl_phy_content, &rssi_dbm, &dl_status);
		SIGFOX_API_error_check_print();
		// Check result.
		if (dl_status == DL_PASSED) {
			_AT_BUS_print_dl_phy_content(dl_phy_content, rssi_dbm);
		}
		else {
			_AT_BUS_reply_add_string("RX timeout");
			_AT_BUS_reply_send();
		}
	}
	_AT_BUS_print_ok();
errors:
	sigfox_api_status = RF_API_stop();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef UHFM
/* AT$RSSI EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_rssi_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	S2LP_status_t s2lp_status = S2LP_SUCCESS;
	LPTIM_status_t lptim1_status = LPTIM_SUCCESS;
	sfx_error_t sigfox_api_status = SFX_ERR_NONE;
	int32_t frequency_hz = 0;
	int32_t duration_s = 0;
	int16_t rssi_dbm = 0;
	uint32_t report_loop = 0;
	// Read frequency parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, AT_BUS_CHAR_SEPARATOR, &frequency_hz);
	PARSER_error_check_print();
	// Read duration parameters.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &duration_s);
	PARSER_error_check_print();
	// Init radio.
	sigfox_api_status = RF_API_init(SFX_RF_MODE_RX);
	SIGFOX_API_error_check_print();
	sigfox_api_status = RF_API_change_frequency((sfx_u32) frequency_hz);
	SIGFOX_API_error_check_print();
	// Start continuous listening.
	s2lp_status = S2LP_send_command(S2LP_COMMAND_READY);
	S2LP_error_check_print();
	s2lp_status = S2LP_wait_for_state(S2LP_STATE_READY);
	S2LP_error_check_print();
	// Start radio.
	s2lp_status = S2LP_send_command(S2LP_COMMAND_RX);
	S2LP_error_check_print();
	// Measurement loop.
	while (report_loop < ((duration_s * 1000) / AT_BUS_RSSI_REPORT_PERIOD_MS)) {
		// Read RSSI.
		s2lp_status = S2LP_get_rssi(S2LP_RSSI_TYPE_RUN, &rssi_dbm);
		S2LP_error_check_print();
		// Print RSSI.
		_AT_BUS_reply_add_string("RSSI=");
		_AT_BUS_reply_add_value(rssi_dbm, STRING_FORMAT_DECIMAL, 0);
		_AT_BUS_reply_add_string("dBm");
		_AT_BUS_reply_send();
		// Report delay.
		lptim1_status = LPTIM1_delay_milliseconds(AT_BUS_RSSI_REPORT_PERIOD_MS, LPTIM_DELAY_MODE_ACTIVE);
		LPTIM1_error_check_print();
		report_loop++;
	}
	_AT_BUS_print_ok();
errors:
	sigfox_api_status = RF_API_stop();
	SIGFOX_API_error_check();
	return;
}
#endif

#ifdef GPSM
/* AT$TIME EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_time_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	NEOM8N_status_t neom8n_status = NEOM8N_SUCCESS;
	USART_status_t usart2_status = LPUART_SUCCESS;
	int32_t timeout_seconds = 0;
	RTC_time_t gps_time;
	// Read timeout parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &timeout_seconds);
	PARSER_error_check_print();
	// Power on GPS.
	usart2_status = USART2_power_on();
	USART2_error_check_print();
	NEOM8N_set_backup(1);
	// Start time aquisition.
	_AT_BUS_reply_add_string("GPS running...");
	_AT_BUS_reply_send();
	neom8n_status = NEOM8N_get_time(&gps_time, (uint32_t) timeout_seconds);
	NEOM8N_error_check_print();
	// Year.
	_AT_BUS_reply_add_value((gps_time.year), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("-");
	// Month.
	if ((gps_time.month) < 10) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
	}
	_AT_BUS_reply_add_value((gps_time.month), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("-");
	// Day.
	if ((gps_time.date) < 10) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
	}
	_AT_BUS_reply_add_value((gps_time.date), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string(" ");
	// Hours.
	if ((gps_time.hours) < 10) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
	}
	_AT_BUS_reply_add_value((gps_time.hours), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string(":");
	// Minutes.
	if ((gps_time.minutes) < 10) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
	}
	_AT_BUS_reply_add_value((gps_time.minutes), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string(":");
	// Seconds.
	if ((gps_time.seconds) < 10) {
		_AT_BUS_reply_add_value(0, STRING_FORMAT_DECIMAL, 0);
	}
	_AT_BUS_reply_add_value((gps_time.seconds), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
errors:
	USART2_power_off();
	return;
}
#endif

#ifdef GPSM
/* AT$GPS EXECUTION CALLBACK.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_gps_callback(void) {
	// Local variables.
	PARSER_status_t parser_status = PARSER_ERROR_UNKNOWN_COMMAND;
	NEOM8N_status_t neom8n_status = NEOM8N_SUCCESS;
	USART_status_t usart2_status = LPUART_SUCCESS;
	int32_t timeout_seconds = 0;
	uint32_t fix_duration_seconds = 0;
	NEOM8N_position_t gps_position;
	// Read timeout parameter.
	parser_status = PARSER_get_parameter(&at_bus_ctx.parser, STRING_FORMAT_DECIMAL, STRING_CHAR_NULL, &timeout_seconds);
	PARSER_error_check_print();
	// Power on GPS.
	usart2_status = USART2_power_on();
	USART2_error_check_print();
	NEOM8N_set_backup(1);
	// Start GPS fix.
	_AT_BUS_reply_add_string("GPS running...");
	_AT_BUS_reply_send();
	neom8n_status = NEOM8N_get_position(&gps_position, (uint32_t) timeout_seconds, &fix_duration_seconds);
	NEOM8N_error_check_print();
	// Latitude.
	_AT_BUS_reply_add_string("Lat=");
	_AT_BUS_reply_add_value((gps_position.lat_degrees), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("d");
	_AT_BUS_reply_add_value((gps_position.lat_minutes), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("'");
	_AT_BUS_reply_add_value((gps_position.lat_seconds), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("''");
	_AT_BUS_reply_add_string(((gps_position.lat_north_flag) == 0) ? "S" : "N");
	// Longitude.
	_AT_BUS_reply_add_string(" Long=");
	_AT_BUS_reply_add_value((gps_position.long_degrees), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("d");
	_AT_BUS_reply_add_value((gps_position.long_minutes), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("'");
	_AT_BUS_reply_add_value((gps_position.long_seconds), STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("''");
	_AT_BUS_reply_add_string(((gps_position.long_east_flag) == 0) ? "W" : "E");
	// Altitude.
	_AT_BUS_reply_add_string(" Alt=");
	_AT_BUS_reply_add_value((gps_position.altitude), STRING_FORMAT_DECIMAL, 0);
	// Fix duration.
	_AT_BUS_reply_add_string("m Fix=");
	_AT_BUS_reply_add_value(fix_duration_seconds, STRING_FORMAT_DECIMAL, 0);
	_AT_BUS_reply_add_string("s");
	_AT_BUS_reply_send();
	_AT_BUS_print_ok();
errors:
	USART2_power_off();
	return;
}
#endif

/* RESET AT PARSER.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_reset_parser(void) {
	// Flush buffers.
	at_bus_ctx.command_size = 0;
	at_bus_ctx.reply_size = 0;
	// Reset flag.
	at_bus_ctx.line_end_flag = 0;
	// Reset parser.
	at_bus_ctx.parser.buffer = (char_t*) at_bus_ctx.command;
	at_bus_ctx.parser.buffer_size = 0;
	at_bus_ctx.parser.separator_idx = 0;
	at_bus_ctx.parser.start_idx = 0;
}

/* PARSE THE CURRENT AT COMMAND BUFFER.
 * @param:	None.
 * @return:	None.
 */
static void _AT_BUS_decode(void) {
	// Local variables.
	uint8_t idx = 0;
	uint8_t decode_success = 0;
	// Update parser length.
	at_bus_ctx.parser.buffer_size = at_bus_ctx.command_size;
	// Loop on available commands.
	for (idx=0 ; idx<(sizeof(AT_BUS_COMMAND_LIST) / sizeof(AT_BUS_command_t)) ; idx++) {
		// Check type.
		if (PARSER_compare(&at_bus_ctx.parser, AT_BUS_COMMAND_LIST[idx].mode, AT_BUS_COMMAND_LIST[idx].syntax) == PARSER_SUCCESS) {
			// Execute callback and exit.
			AT_BUS_COMMAND_LIST[idx].callback();
			decode_success = 1;
			break;
		}
	}
	if (decode_success == 0) {
		_AT_BUS_print_error(ERROR_BASE_PARSER + PARSER_ERROR_UNKNOWN_COMMAND); // Unknown command.
		goto errors;
	}
errors:
	_AT_BUS_reset_parser();
	return;
}

/*** AT functions ***/

/* INIT AT MANAGER.
 * @param self_address:	Self bus address.
 * @return:				None.
 */
void AT_BUS_init(NODE_address_t self_address) {
	// Local variables.
	LBUS_status_t lbus_status = LBUS_SUCCESS;
	// Init context.
	_AT_BUS_reset_parser();
#ifdef UHFM
	at_bus_ctx.sigfox_rc = (sfx_rc_t) RC1;
	at_bus_ctx.sigfox_rc_idx = SFX_RC1;
	at_bus_ctx.sigfox_dl_payload_available = 0;
#endif
	// Init LBUS layer.
	lbus_status = LBUS_init(self_address);
	LBUS_error_check();
	// Enable LPUART.
	LPUART1_enable_rx();
}

/* MAIN TASK OF AT COMMAND MANAGER.
 * @param:	None.
 * @return:	None.
 */
void AT_BUS_task(void) {
	// Trigger decoding function if line end found.
	if (at_bus_ctx.line_end_flag != 0) {
		// Decode and execute command.
		LPUART1_disable_rx();
		_AT_BUS_decode();
		LPUART1_enable_rx();
	}
}

/* FILL AT COMMAND BUFFER WITH A NEW BYTE (CALLED BY LPUART INTERRUPT).
 * @param rx_byte:	Incoming byte.
 * @return:			None.
 */
void AT_BUS_fill_rx_buffer(uint8_t rx_byte) {
	// Append byte if line end flag is not allready set.
	if (at_bus_ctx.line_end_flag == 0) {
		// Check ending characters.
		if (rx_byte == AT_BUS_FRAME_END) {
			at_bus_ctx.command[at_bus_ctx.command_size] = STRING_CHAR_NULL;
			at_bus_ctx.line_end_flag = 1;
		}
		else {
			// Store new byte.
			at_bus_ctx.command[at_bus_ctx.command_size] = rx_byte;
			// Manage index.
			at_bus_ctx.command_size = (at_bus_ctx.command_size + 1) % AT_BUS_COMMAND_BUFFER_SIZE;
		}
	}
}

/* PRINT SIGFOX LIBRARY RESULT.
 * @param test_result:	Test result.
 * @param rssi:			Downlink signal rssi in dBm.
 */
void AT_BUS_print_test_result(uint8_t test_result, int16_t rssi_dbm) {
	// Check result.
	if (test_result == 0) {
		_AT_BUS_reply_add_string("Test failed.");
	}
	else {
		_AT_BUS_reply_add_string("Test passed. RSSI=");
		_AT_BUS_reply_add_value(rssi_dbm, STRING_FORMAT_DECIMAL, 0);
		_AT_BUS_reply_add_string("dBm");
	}
	_AT_BUS_reply_send();
}
