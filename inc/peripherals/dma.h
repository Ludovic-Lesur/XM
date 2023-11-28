/*
 * dma.h
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#ifndef __DMA_H__
#define __DMA_H__

#include "types.h"

/*** DMA structures ***/

/*!******************************************************************
 * \fn DMA_transfer_complete_irq_cb
 * \brief DMA transfer complete interrupt callback.
 *******************************************************************/
typedef void (*DMA_transfer_complete_irq_cb_t)(void);

/*** DMA functions ***/

#ifdef GPSM
/*!******************************************************************
 * \fn void DMA1_CH6_init(DMA_transfer_complete_irq_cb_t irq_callback)
 * \brief Init channel 6 of DMA1 channel peripheral for NEOM8N NMEA frames transfer.
 * \param[in]  	irq_callback: Function to call on transfer complete interrupt.
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void DMA1_CH6_init(DMA_transfer_complete_irq_cb_t irq_callback);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void DMA1_CH6_de_init(void)
 * \brief Release channel 6 of DMA1.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void DMA1_CH6_de_init(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void DMA1_CH6_start(void)
 * \brief Start DMA1 channel 3.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void DMA1_CH6_start(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void DMA1_CH6_stop(void)
 * \brief Start DMA1 channel 3.
 * \param[in]  	none
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void DMA1_CH6_stop(void);
#endif

#ifdef GPSM
/*!******************************************************************
 * \fn void DMA1_CH6_set_destination_address(uint32_t destination_buffer_addr, uint16_t destination_buffer_size)
 * \brief Set DMA1 channel 6 destination buffer.
 * \param[in]  	destination_buffer_addr: Destination buffer address.
 * \param[in] 	destination_buffer_size: Destination buffer size (number of bytes to transfer).
 * \param[out] 	none
 * \retval		none
 *******************************************************************/
void DMA1_CH6_set_destination_address(uint32_t destination_buffer_addr, uint16_t destination_buffer_size);
#endif

#endif /* __DMA_H__ */
