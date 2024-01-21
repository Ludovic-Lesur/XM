/*
 * common_reg.h
 *
 *  Created on: 12 nov 2022
 *      Author: Ludo
 */

#ifndef __COMMON_REG_H__
#define __COMMON_REG_H__

#include "types.h"

/*** COMMON registers address ***/

/*!******************************************************************
 * \enum COMMON_register_address_t
 * \brief Common registers map.
 *******************************************************************/
typedef enum {
	COMMON_REG_ADDR_NODE_ID = 0,
	COMMON_REG_ADDR_HW_VERSION,
	COMMON_REG_ADDR_SW_VERSION_0,
	COMMON_REG_ADDR_SW_VERSION_1,
	COMMON_REG_ADDR_ERROR_STACK,
	COMMON_REG_ADDR_STATUS_0,
	COMMON_REG_ADDR_CONTROL_0,
	COMMON_REG_ADDR_ANALOG_DATA_0,
	COMMON_REG_ADDR_LAST
} COMMON_register_address_t;

/*** COMMON registers mask ***/

#define COMMON_REG_NODE_ID_MASK_NODE_ADDR			0x0000007F
#define COMMON_REG_NODE_ID_MASK_BOARD_ID			0x0000FF00

#define COMMON_REG_HW_VERSION_MASK_MAJOR			0x000000FF
#define COMMON_REG_HW_VERSION_MASK_MINOR			0x0000FF00

#define COMMON_REG_SW_VERSION_0_MASK_MAJOR			0x000000FF
#define COMMON_REG_SW_VERSION_0_MASK_MINOR			0x0000FF00
#define COMMON_REG_SW_VERSION_0_MASK_COMMIT_INDEX	0x00FF0000
#define COMMON_REG_SW_VERSION_0_MASK_DTYF			0x01000000

#define COMMON_REG_SW_VERSION_1_MASK_COMMIT_ID		0x0FFFFFFF

#define COMMON_REG_ERROR_STACK_MASK_ERROR			0x0000FFFF

#define COMMON_REG_STATUS_0_MASK_RESET_FLAGS		0x000000FF
#define COMMON_REG_STATUS_0_MASK_FW					0x00000001
#define COMMON_REG_STATUS_0_MASK_OBL				0x00000002
#define COMMON_REG_STATUS_0_MASK_PIN				0x00000004
#define COMMON_REG_STATUS_0_MASK_POR				0x00000008
#define COMMON_REG_STATUS_0_MASK_SFT				0x00000010
#define COMMON_REG_STATUS_0_MASK_IWDG				0x00000020
#define COMMON_REG_STATUS_0_MASK_WWDG				0x00000040
#define COMMON_REG_STATUS_0_MASK_LPWR				0x00000080
#define COMMON_REG_STATUS_0_MASK_BF					0x00000100
#define COMMON_REG_STATUS_0_MASK_ESF				0x00000200

#define COMMON_REG_CONTROL_0_MASK_RTRG				0x00000001
#define COMMON_REG_CONTROL_0_MASK_MTRG				0x00000002
#define COMMON_REG_CONTROL_0_MASK_BFC				0x00000004

#define COMMON_REG_ANALOG_DATA_0_MASK_VMCU			0x0000FFFF
#define COMMON_REG_ANALOG_DATA_0_MASK_TMCU			0x00FF0000

/*** COMMON registers access ***/

#define COMMON_REG_ACCESS \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_ONLY,  \
	DINFOX_REG_ACCESS_READ_WRITE, \
	DINFOX_REG_ACCESS_READ_ONLY,  \

#endif /* __COMMON_REG_H__ */
