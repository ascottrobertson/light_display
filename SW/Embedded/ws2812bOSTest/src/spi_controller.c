/*
 * spi_controller.c
 *
 *  Created on: Oct 24, 2020
 *      Author: aRobe
 */

#include "system_timer.h"
#include "spi_controller.h"

#ifdef USE_ALT_SPI
#include "altera_avalon_spi.h"
#include "altera_avalon_spi_regs.h"
#endif

#define SPI_PIO_MISO 0x08u
#define SPI_PIO_SS_N 0x04u
#define SPI_PIO_MOSI 0x02u
#define SPI_PIO_SCL  0x01u

volatile uint32_t *spi_rx_reg = (uint32_t *)(SPI_BASE);
volatile uint32_t *spi_tx_reg = (uint32_t *)(SPI_BASE + 4);
volatile uint32_t *spi_status_reg = (uint32_t *)(SPI_BASE + 8);
volatile uint32_t *spi_control_reg = (uint32_t *)(SPI_BASE + 12);
// (SPI_BASE + 16): Reserved
volatile uint32_t *spi_slaveselect_reg = (uint32_t *)(SPI_BASE + 20);
volatile uint32_t *spi_eop_value_reg = (uint32_t *)(SPI_BASE + 24);
//volatile uint32_t *spi_pio_data = (uint32_t *)(SPI_PIO_BASE);
//volatile uint32_t *spi_pio_direction = (uint32_t *)(SPI_PIO_BASE + 4);

/*
 * Function Name: spi_transfer
 * Description: Transmits and receives data via the SPI core
 * Inputs:
 * 			- txdata: Data to be transmitted.  May be null if txlength is 0.
 * 			- rxdata: Buffer for received data.  May be null if rxlength is 0.
 * 			- txlength: Number of data bytes to be transmitted. (remaining bytes set to 0)
 * 			- rxlength: Number of data bytes to be received, after transmitted data. (previous received data ignored)
 * 			- slave_number: (0-31) number of the slave being interfaced (special case: -1.  No SS asserted)
 * Outputs:
 * 			- Returns: (spi_status) SPI_STATUS_SUCCESS on success, SPI_STATUS_TIMEOUT if SPI core took too long to respond
 * 			- rxdata: Received data bytes
 */
//spi_status_t spi_transfer(const uint8_t txdata[], uint8_t rxdata[], int32_t txlength, int32_t rxlength, int8_t slave_number, uint32_t flags)
//{
//	uint8_t tmp;
//	uint32_t spi_status;
//	uint32_t start_time_1, start_time_2;
//
//	uint8_t tx_index = 0;
//	uint8_t rx_index = 0;
//
//	uint32_t slave_mask;
//	if (slave_number == -1)
//	{
//		slave_mask = 0x00;
//	}
//	else
//	{
//		slave_mask = 1 << slave_number;
//	}
//
//	// Set SSO Bit
//	*spi_slaveselect_reg = slave_mask;
//	*spi_control_reg = SPI_CONTROL_SSO;
//
//	// Clear stale data
//	tmp = *spi_rx_reg;
//	start_time_1 = system_get_time_ms();
//
//	while(1)
//	{
//		// Wait until either TX or RX is ready
//		start_time_2 = system_get_time_ms();
//		do
//		{
//			spi_status = *spi_status_reg;
//			if (system_get_time_ms() - start_time_2 > SPI_TIMEOUT_MS)
//			{
//				return SPI_STATUS_TIMEOUT;
//			}
//		}
//		while(((spi_status & SPI_STATUS_TRDY) == 0) && ((spi_status & SPI_STATUS_RRDY) == 0));
//
//		// If TX is ready, transmit
//		if ((spi_status & SPI_STATUS_TRDY) != 0)
//		{
//			// If data remains to be written, write data
//			if (tx_index < txlength)
//			{
//				*spi_tx_reg = txdata[tx_index];
//				tx_index++;
//			}
//			else
//			{
//				// Otherwise, write zeros
//				*spi_tx_reg = 0xff;
//			}
//		}
//
//		// If RX is ready, receive
//		if ((spi_status & SPI_STATUS_RRDY) != 0)
//		{
//			tmp = *spi_rx_reg;
//
//			// Only capture read data if command is no longer transmitting
//			if (tx_index >= txlength)
//			{
//				rxdata[rx_index] = tmp;
//				rx_index++;
//			}
//		}
//
//		// If all data has been transmitted/received, break out of TX/RX loop
//		if (tx_index >= txlength && rx_index >= rxlength)
//		{
//			break;
//		}
//
//		if (system_get_time_ms() - start_time_1 > SPI_TIMEOUT_MS)
//		{
//			return SPI_STATUS_TIMEOUT;
//		}
//	}
//
//	// Wait for SPI core to stop transmitting
//	start_time_1 = system_get_time_ms();
//	do
//	{
//		spi_status = *spi_status_reg;
//
//		if (system_get_time_ms() - start_time_1 > SPI_TIMEOUT_MS)
//		{
//			return SPI_STATUS_TIMEOUT;
//		}
//	} while ((spi_status & SPI_STATUS_TMT) == 0);
//
//	// Deassert SSO Bit
////	*spi_control_reg = 0;
//
//	return SPI_STATUS_SUCCESS;
//}

spi_status_t spi_transfer(const uint8_t txdata[], uint8_t rxdata[], int32_t txlength, int32_t rxlength, int8_t slave_number, uint32_t flags)
{
	const uint8_t * tx_end = txdata + txlength;
	uint8_t *rx_end = rxdata + rxlength;

	uint32_t write_ones = rxlength;
	uint32_t read_ignore = txlength;
	uint32_t status;

	uint8_t *txindex = txdata;
	uint8_t *rxindex = rxdata;

	uint32_t credits = 1;

	if (slave_number >= 0)
	{
		*spi_slaveselect_reg = (1 << slave_number);
	}

	// SPI Toggle (omitted)

	uint8_t rx_tmp = *spi_rx_reg;

	while (1)
	{
		do
		{
			status = *spi_status_reg;
		}
		while (((status & SPI_STATUS_TRDY) == 0 || credits == 0) &&
            (status & SPI_STATUS_RRDY) == 0);

		if ((status & SPI_STATUS_TRDY) != 0 && credits > 0)
		{
			credits--;

			if (txindex < tx_end)
			{
				*spi_tx_reg = *txindex++;
			}
			else if (write_ones > 0)
			{
				write_ones--;
				*spi_tx_reg = 0xff;
			}
			else
			{
				credits = -1024;
			}
		};

		if ((status & SPI_STATUS_RRDY) != 0)
		{
			rx_tmp = (uint8_t)*spi_rx_reg;

			if (read_ignore > 0)
			{
				read_ignore--;
			}
			else
			{
				*rxindex++ = rx_tmp;
			}
			credits++;

			if (read_ignore == 0 && rxindex == rx_end)
			{
				break;
			}
		}
	}

	do
	{
		status = *spi_status_reg;
	}
	while ((status & SPI_STATUS_TMT) == 0);

	// Slave Select (omitted)

	return SPI_STATUS_SUCCESS;
}


spi_status_t spi_init()
{
//	*spi_pio_direction = 0x00000007;

#ifndef USE_ALT_SPI

//	int32_t b = 0;
//	for (b = 0; b < 74; b++)
//	{
//		*spi_pio_data = SPI_PIO_SS_N | SPI_PIO_MOSI;
//		*spi_pio_data = SPI_PIO_SS_N | SPI_PIO_MOSI | SPI_PIO_SCL;
//	}
//
//	*spi_pio_data = SPI_PIO_SS_N;

#endif

	return SPI_STATUS_SUCCESS;
}

//spi_status_t spi_transfer(const uint8_t txdata[], uint8_t rxdata[], int32_t txlength, int32_t rxlength, int8_t slave_number, uint32_t flags)
//{
//#ifdef USE_ALT_SPI
//	alt_avalon_spi_command(SPI_BASE, slave_number, txlength, txdata, rxlength, rxdata, flags);
//	return SPI_STATUS_SUCCESS;
//#endif
//
//	uint8_t tx_byte;
//	uint8_t rx_byte;
//
//	int32_t byte = 0;
//	int32_t b = 0;
//	for (byte = 0; byte < txlength; byte++)
//	{
//		tx_byte = txdata[byte];
//
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
//
//		if ((tx_byte & 0x80) != 0)
//		{
//			*spi_pio_data = SPI_PIO_MOSI;
//			*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
//		}
//		else
//		{
//			*spi_pio_data = 0;
//			*spi_pio_data = SPI_PIO_SCL;
//		}
//
//		tx_byte = tx_byte << 1;
////		for (b = 0; b < 8; b++)
////		{
////			if ((tx_byte & 0x80) != 0)
////			{
////				*spi_pio_data = SPI_PIO_MOSI;
////				*spi_pio_data = SPI_PIO_MOSI | SPI_PIO_SCL;
////			}
////			else
////			{
////				*spi_pio_data = 0;
////				*spi_pio_data = SPI_PIO_SCL;
////			}
////
////			tx_byte = tx_byte << 1;
////		}
//	}
//
//	int8_t zero_flag = 0;
//	for (byte = 0; byte < rxlength; byte++)
//	{
//		uint8_t val_in = 0;
//		rx_byte = 0;
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//		rx_byte = rx_byte << 1;
//
//		*spi_pio_data = SPI_PIO_MOSI;
//		val_in = *spi_pio_data & SPI_PIO_MISO;
//		*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
//
//		if (val_in != 0)
//		{
//			rx_byte |= 0x01;
//		}
//
////		for (b = 0; b < 8; b++)
////		{
////			rx_byte = rx_byte << 1;
////
////			*spi_pio_data = SPI_PIO_MOSI;
////			val_in = *spi_pio_data & SPI_PIO_MISO;
////			*spi_pio_data = SPI_PIO_SCL | SPI_PIO_MOSI;
////
////			if (val_in != 0)
////			{
////				rx_byte |= 0x01;
////			}
////		}
//		rxdata[byte] = rx_byte;
//	}
//
//	*spi_pio_data = 0x00;
//
//	return SPI_STATUS_SUCCESS;
//}

spi_status_t spi_deassert_ss()
{
//	*spi_pio_data = SPI_PIO_SS_N;
	*spi_control_reg = 0;

//#ifdef USE_ALT_SPI
//	IOWR_ALTERA_AVALON_SPI_CONTROL(SPI_BASE, 0);
//#endif

	return SPI_STATUS_SUCCESS;
}
