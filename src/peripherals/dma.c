/*
 * dma.c
 *
 *  Created on: 16 aug. 2020
 *      Author: Ludo
 */

#include "dma.h"

#include "dma_reg.h"
#include "nvic.h"
#include "rcc_reg.h"
#include "spi_reg.h"
#include "types.h"

#ifdef UHFM

/*** DMA local global variables ***/

static volatile uint8_t dma1_channel3_tcif = 0;

/*** DMA local functions ***/

/* DMA1 CHANNEL 3 INTERRUPT HANDLER.
 * @param:	None.
 * @return:	None.
 */
void __attribute__((optimize("-O0"))) DMA1_Channel2_3_IRQHandler(void) {
	// Transfer complete interrupt (TCIF3='1').
	if (((DMA1 -> ISR) & (0b1 << 9)) != 0) {
		// Set local flag.
		if (((DMA1 -> CCR3) & (0b1 << 1)) != 0) {
			dma1_channel3_tcif = 1;
		}
		// Clear flag.
		DMA1 -> IFCR |= (0b1 << 9); // CTCIF3='1'.
	}
}

/* CONFIGURE DMA1 CHANNEL3 FOR SPI1 TX TRANSFER (S2LP TX POLAR MODULATION).
 * @param:	None.
 * @return:	None.
 */
void DMA1_init_channel3(void) {
	// Enable peripheral clock.
	RCC -> AHBENR |= (0b1 << 0); // DMAEN='1'.
	// Disable DMA channel before configuration (EN='0').
	// Memory and peripheral data size are 8 bits (MSIZE='00' and PSIZE='00').
	// Disable memory to memory mode (MEM2MEM='0').
	// Peripheral increment mode disabled (PINC='0').
	// Circular mode disabled (CIRC='0').
	// Read from memory (DIR='1').
	// Very high priority (PL='11').
	// Memory increment mode enabled (MINC='1').
	// Enable transfer complete interrupt (TCIE='1').
	DMA1 -> CCR3 |= (0b11 << 12) | (0b1 << 7) | (0b1 << 4) | (0b1 << 1);
	// Configure peripheral address.
	DMA1 -> CPAR3 = (uint32_t) &(SPI1 -> DR); // Peripheral address = SPI1 TX register.
	// Configure channel 3 for SPI1 TX (request number 1).
	DMA1 -> CSELR |= (0b0001 << 8); // DMA channel mapped on SPI1_TX (C3S='0001').
	// Set interrupt priority.
	NVIC_set_priority(NVIC_INTERRUPT_DMA1_CH_2_3, 1);
}

/* START DMA1 CHANNEL 3 TRANSFER.
 * @param:	None.
 * @return:	None.
 */
void DMA1_start_channel3(void) {
	// Clear all flags.
	dma1_channel3_tcif = 0;
	DMA1 -> IFCR |= 0x00000F00;
	NVIC_enable_interrupt(NVIC_INTERRUPT_DMA1_CH_2_3);
	// Start transfer.
	DMA1 -> CCR3 |= (0b1 << 0); // EN='1'.
}

/* STOP DMA1 CHANNEL 3 TRANSFER.
 * @param:	None.
 * @return:	None.
 */
void DMA1_stop_channel3(void) {
	// Stop transfer.
	dma1_channel3_tcif = 0;
	DMA1 -> CCR3 &= ~(0b1 << 0); // EN='0'.
	NVIC_disable_interrupt(NVIC_INTERRUPT_DMA1_CH_2_3);
}

/* SET DMA1 CHANNEL 3 SOURCE BUFFER ADDRESS.
 * @param dest_buf_addr:	Address of source buffer (Sigfox modulation stream).
 * @param dest_buf_size:	Size of destination buffer.
 * @return:					None.
 */
void DMA1_set_channel3_source_addr(uint32_t source_buf_addr, uint16_t source_buf_size) {
	// Set address and buffer size.
	DMA1 -> CMAR3 = source_buf_addr;
	DMA1 -> CNDTR3 = source_buf_size;
}

/* GET DMA1 CHANNEL 3 TRANSFER STATUS.
 * @param:	None.
 * @return:	'1' if the transfer is complete, '0' otherwise.
 */
uint8_t DMA1_get_channel3_status(void) {
	return dma1_channel3_tcif;
}

#endif /* UHFM */
