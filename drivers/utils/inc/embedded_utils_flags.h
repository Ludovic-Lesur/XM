/*
 * embedded_utils_flags.h
 *
 *  Created on: 31 dec. 2024
 *      Author: Ludo
 */

#ifndef __EMBEDDED_UTILS_FLAGS_H__
#define __EMBEDDED_UTILS_FLAGS_H__

#include "lmac.h"
#include "xm_flags.h"

/*** Embedded utility functions compilation flags ***/

#define EMBEDDED_UTILS_HW_INTERFACE_ERROR_BASE_LAST     0

#define EMBEDDED_UTILS_AT_BAUD_RATE                     1200
#define EMBEDDED_UTILS_AT_REPLY_END                     "\r"
//#define EMBEDDED_UTILS_AT_FORCE_OK
//#define EMBEDDED_UTILS_AT_INTERNAL_COMMANDS_ENABLE
#define EMBEDDED_UTILS_AT_COMMANDS_LIST_SIZE            4
#define EMBEDDED_UTILS_AT_BUFFER_SIZE                   64
#ifdef EMBEDDED_UTILS_AT_INTERNAL_COMMANDS_ENABLE
#define EMBEDDED_UTILS_AT_SW_VERSION_MAJOR              0
#define EMBEDDED_UTILS_AT_SW_VERSION_MINOR              0
#define EMBEDDED_UTILS_AT_SW_VERSION_INDEX              0
#define EMBEDDED_UTILS_AT_SW_VERSION_DIRTY_FLAG         0
#define EMBEDDED_UTILS_AT_SW_VERSION_ID                 0
#endif

#define EMBEDDED_UTILS_ERROR_STACK_DEPTH                32
#define EMBEDDED_UTILS_ERROR_STACK_SUCCESS_VALUE        0
#ifdef UHFM
#define EMBEDDED_UTILS_ERROR_STACK_SIGFOX
#endif

#define EMBEDDED_UTILS_MATH_PRECISION                   0
//#define EMBEDDED_UTILS_MATH_COS_TABLE
//#define EMBEDDED_UTILS_MATH_SIN_TABLE
//#define EMBEDDED_UTILS_MATH_ATAN2

//#define EMBEDDED_UTILS_STRING_HEXADECIMAL_UPPER_CASE

#define EMBEDDED_UTILS_TERMINAL_INSTANCES_NUMBER        1
#define EMBEDDED_UTILS_TERMINAL_BUFFER_SIZE             64
//#define EMBEDDED_UTILS_TERMINAL_MODE_BUS

#endif /* __EMBEDDED_UTILS_FLAGS_H__ */
