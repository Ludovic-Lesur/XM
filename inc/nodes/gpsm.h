/*
 * gpsm.h
 *
 *  Created on: 8 jun. 2023
 *      Author: Ludo
 */

#ifndef __GPSM_H__
#define __GPSM_H__

#include "dinfox.h"
#include "dinfox_reg.h"
#include "dinfox_types.h"
#include "gpsm_reg.h"
#include "node.h"

#ifdef GPSM

/*** GPSM macros ***/

#define NODE_BOARD_ID	DINFOX_BOARD_ID_GPSM

/*** GPSM global variables ***/

static const DINFOX_register_access_t NODE_REG_ACCESS[NODE_REG_ADDR_LAST] = {
	DINFOX_REG_ACCESS
	DINFOX_REG_ACCESS_READ_WRITE,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_WRITE,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_ONLY,
	DINFOX_REG_ACCESS_READ_WRITE,
	DINFOX_REG_ACCESS_READ_WRITE
};

/*** GPSM functions ***/

NODE_status_t GPSM_init_registers(void);

NODE_status_t GPSM_update_register(uint8_t reg_addr);
NODE_status_t GPSM_check_register(uint8_t reg_addr);

NODE_status_t GPSM_mtrg_callback(void);

#endif /* GPSM */

#endif /* __GPSM_H__ */
