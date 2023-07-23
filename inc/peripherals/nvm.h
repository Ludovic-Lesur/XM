/*
 * nvm.h
 *
 *  Created on: 19 jun. 2018
 *      Author: Ludo
 */

#ifndef __NVM_H__
#define __NVM_H__

#include "types.h"
#ifdef UHFM
#include "sigfox_types.h"
#endif

/*** NVM macros ***/

/*!******************************************************************
 * \enum NVM_status_t
 * \brief NVM driver error codes.
 *******************************************************************/
typedef enum {
	NVM_SUCCESS = 0,
	NVM_ERROR_NULL_PARAMETER,
	NVM_ERROR_ADDRESS,
	NVM_ERROR_UNLOCK,
	NVM_ERROR_LOCK,
	NVM_ERROR_WRITE,
	NVM_ERROR_BASE_LAST = 0x0100
} NVM_status_t;

/*!******************************************************************
 * \enum NVM_address_t
 * \brief NVM address mapping.
 *******************************************************************/
typedef enum {
	NVM_ADDRESS_SELF_ADDRESS = 0,
#ifdef UHFM
	NVM_ADDRESS_SIGFOX_EP_ID = 1,
	NVM_ADDRESS_SIGFOX_EP_KEY = (NVM_ADDRESS_SIGFOX_EP_ID + SIGFOX_EP_ID_SIZE_BYTES),
	NVM_ADDRESS_SIGFOX_EP_LIB_DATA = (NVM_ADDRESS_SIGFOX_EP_KEY + SIGFOX_EP_KEY_SIZE_BYTES),
	NVM_ADDRESS_LAST = (NVM_ADDRESS_SIGFOX_EP_LIB_DATA + SIGFOX_NVM_DATA_SIZE_BYTES)
#else
	NVM_ADDRESS_LAST
#endif
} NVM_address_t;

/*** NVM functions ***/

/*!******************************************************************
 * \fn void NVM_init(void)
 * \brief Init NVM interface.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void NVM_init(void);

/*!******************************************************************
 * \fn NVM_status_t NVM_read_byte(NVM_address_t address_offset, uint8_t* data)
 * \brief Read byte in NVM.
 * \param[in]  	address: Address to read.
 * \param[out] 	data: Pointer to byte that will contain the read value.
 * \retval		Function execution status.
 *******************************************************************/
NVM_status_t NVM_read_byte(NVM_address_t address, uint8_t* data);

/*!******************************************************************
 * \fn NVM_status_t NVM_write_byte(NVM_address_t address, uint8_t data)
 * \brief Write byte in NVM.
 * \param[in]  	address: Address to write.
 * \param[out] 	data: Byte to write.
 * \retval		Function execution status.
 *******************************************************************/
NVM_status_t NVM_write_byte(NVM_address_t address, uint8_t data);

/*******************************************************************/
#define NVM_check_status(error_base) { if (nvm_status != NVM_SUCCESS) { status = error_base + nvm_status; goto errors; } }

/*******************************************************************/
#define NVM_stack_error(void) { ERROR_stack_error(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM); }

/*******************************************************************/
#define NVM_print_error(void) { ERROR_print_error(nvm_status, NVM_SUCCESS, ERROR_BASE_NVM); }

#endif /* __NVM_H__ */
