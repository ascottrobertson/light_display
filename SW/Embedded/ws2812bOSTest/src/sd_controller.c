/*
 * sd_controller.c
 *
 *  Created on: Oct 24, 2020
 *      Author: aRobe
 */
#include "sd_controller.h"

#include <string.h>

#include "system_timer.h"
#include "spi_controller.h"
#include "altera_avalon_spi.h"
#include "altera_avalon_spi_regs.h"

volatile uint32_t *sd_ctrl_reg = (uint32_t *)(SD_CTRL);
volatile uint32_t *sd_read_addr = (uint32_t *)(SD_BASE);
volatile uint32_t *sd_read_data = (uint32_t *)(SD_BASE);
volatile uint32_t *sd_read_ctrl = (uint32_t *)(SD_BASE + 4);
volatile uint32_t *sd_read_status = (uint32_t *)(SD_BASE + 4);

sd_ctrl_t active_ctrl = SD_CTRL_ALT_SPI;
extern uint8_t *_ledg;

//volatile uint32_t *spi_slaveselect_reg = (uint32_t*)(SPI_BASE + 20);
//volatile uint32_t *spi_control_reg = (uint32_t*)(SPI_BASE + 12);

#define POLY 0x89

uint8_t crc_table[256];

sd_status_t sd_init()
{
	sd_calc_crc_table(crc_table);

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_calc_crc_table(uint8_t table[])
{
	int i, j;

	for (i = 0; i < 256; ++i) {
		crc_table[i] = (i & 0x80) ? i ^ POLY : i;
		for (j = 1; j < 8; ++j) {
			crc_table[i] <<= 1;
			if (crc_table[i] & 0x80)
			{
				crc_table[i] ^= POLY;
			}
		}
	}
	return SD_STATUS_SUCCESS;
}

uint8_t sd_add_crc7(uint8_t crc, uint8_t byte)
{
	return crc_table[(crc << 1) ^ byte];
}

uint8_t sd_calc_crc7(const uint8_t message[], int32_t len)
{
	int i;
	uint8_t crc = 0;

	for (i = 0; i < len; ++i)
	{
		crc = sd_add_crc7(crc, message[i]);
	}

	return crc;
}

sd_status_t sd_spi_init()
{
	sd_select_ctrl(SD_CTRL_ALT_SPI); // Use the Altera SPI Core
	spi_status_t spi_status = SPI_STATUS_SUCCESS;

	//IOWR_ALTERA_AVALON_SPI_CONTROL(SPI_BASE, 0);

	// Set MOSI to 1 and CS_n to 1
	// Pulse clock 80 times (> 74)
	uint8_t txdata[10] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	uint8_t rxdata[512] = {0};

	spi_init();
	spi_deassert_ss();

	spi_transfer(txdata, 0, 10, 0, -1, 0);

	// Pulse clock with MISO and CS_n high for 80 cycles (> 74)
#ifdef USE_ALT_SPI
	alt_avalon_spi_command(SPI_BASE, 1, 10, txdata, 0, rxdata, 0);
#endif

	//IOWR_ALTERA_AVALON_SPI_CONTROL(SPI_BASE, ALTERA_AVALON_SPI_CONTROL_SSO_MSK);

	// Send CMD0
	sd_build_cmd(SD_COMMAND_CMD0, 0x00000000, txdata);
	spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
	//alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, 0);

	sd_status_t sd_status = sd_receive_r1(&rxdata[0]);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		*_ledg = 0xf0;
		while(1);
		return sd_status;
	}

	if (rxdata[0] != 0x01)
	{
		return SD_STATUS_UNEXPECTED_RESPONSE;
	}

	// Send CMD 8

	sd_build_cmd(SD_COMMAND_CMD8, 0x000001aa, txdata);
	spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
	//alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, ALT_AVALON_SPI_COMMAND_MERGE);

	sd_status = sd_receive_r7(rxdata);

	sd_status_t error = SD_STATUS_SUCCESS;

	if (sd_status != SD_STATUS_SUCCESS)
	{
		error = sd_status;
	}
	else
	{
		error = SD_STATUS_SUCCESS;
	}

	if ((rxdata[0] & SD_R1_COM_CRC_ERROR) != 0 || (rxdata[0] & SD_R1_ILLEGAL_COMMAND) != 0)
	{
		error = SD_STATUS_GENERAL_ERROR;
	}
	if (error != SD_STATUS_SUCCESS)
	{
		sd_build_cmd(SD_COMMAND_CMD58, 0x00000000, txdata);
		spi_status = spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
//		alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, ALT_AVALON_SPI_COMMAND_MERGE);

		sd_status = sd_receive_r7(rxdata);

		if (sd_status != SD_STATUS_SUCCESS)
		{
			*_ledg = 0x20;
			while(1);
			return sd_status;
		}

		if ((rxdata[0] & SD_R1_COM_CRC_ERROR) != 0)
		{
			*_ledg = 0x30;
			while(1);
			return SD_STATUS_CRC_ERROR;
		}
		if ((rxdata[0] & SD_R1_ILLEGAL_COMMAND) != 0)
		{
			*_ledg = 0x40;
			while(1);
			return SD_STATUS_ILLEGAL_COMMAND;
		}
	}

	uint32_t start_time = system_get_time_ms();
	do
	{
		memset(rxdata, 0, sizeof(rxdata));
		// Send CMD 55
		sd_build_cmd(SD_COMMAND_CMD55, 0x00000000, txdata);
		spi_status = spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
//	alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, ALT_AVALON_SPI_COMMAND_MERGE);

//		if (spi_status != SPI_STATUS_SUCCESS)
//		{
//			switch (spi_status)
//			{
//				case SPI_STATUS_TIMEOUT:
//				{
//					return SD_STATUS_TIMEOUT;
//				}
//				default:
//				{
//					return SD_STATUS_GENERAL_ERROR;
//				}
//			}
//		}

		sd_receive_r1(&rxdata[0]);

		if (rxdata[0] != 0x01)
		{
			*_ledg = 0x50;
			while(1);
			return SD_STATUS_UNEXPECTED_RESPONSE;
		}

		// Send ACMD 41
		sd_build_cmd((sd_command_t)SD_APP_COMMAND_ACMD_41, SD_ACMD41_HCS, txdata);
		spi_status = spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
		//alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, ALT_AVALON_SPI_COMMAND_MERGE);

//		if (spi_status != SPI_STATUS_SUCCESS)
//		{
//			switch (spi_status)
//			{
//				case SPI_STATUS_TIMEOUT:
//				{
//					return SD_STATUS_TIMEOUT;
//				}
//				default:
//				{
//					return SD_STATUS_GENERAL_ERROR;
//				}
//			}
//		}
		memset(rxdata, 0, sizeof(rxdata));

		sd_receive_r1(&rxdata[0]);

		if (system_get_time_ms() - start_time > 60000)
		{
			*_ledg = 0x60;
			while(1);
			return SD_STATUS_TIMEOUT;
		}

	} while ((rxdata[0] & SD_R1_IDLE) != 0);

	// CMD 6
	sd_build_cmd(SD_COMMAND_CMD6, 0x8000fff1, txdata);
	spi_status = spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);

	sd_receive_r1(&rxdata[0]);

	if (rxdata[0] != 0x00)
	{
		*_ledg = 0x70;
		while(1);
		return SD_STATUS_UNEXPECTED_RESPONSE;
	}

	uint16_t crc16;
	sd_status = sd_receive_block(rxdata, &crc16);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		*_ledg = 0x80;
		while(1);
		return sd_status;
	}

	spi_deassert_ss();

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_receive_r1(uint8_t data[])
{
	uint32_t start_time = system_get_time_ms();
	spi_status_t spi_status;
	uint8_t buffer[128];
	int32_t i = 0;

	memset(buffer, 0, sizeof(buffer));

	do
	{
		spi_status = spi_transfer(0, data, 0, 1, 0, ALT_AVALON_SPI_COMMAND_MERGE); // (txdata, rxdata, txlength, rxlength, slave_number)
		//alt_avalon_spi_command(SPI_BASE, 0, 0, 0, 1, data, ALT_AVALON_SPI_COMMAND_MERGE);

		if (system_get_time_ms() - start_time > SD_TIMEOUT)
		{
			return SD_STATUS_TIMEOUT;
		}

		if (spi_status != SPI_STATUS_SUCCESS)
		{
			*data = 0xff;
		}
		if (i < 128)
		{
			buffer[i++] = *data;
		}
	} while ((*data & 0x80) != 0);

	spi_deassert_ss();

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_receive_r7(uint8_t data[])
{
	uint32_t start_time = system_get_time_ms();
	spi_status_t spi_status;

	uint8_t buffer[128];
	memset(buffer, 0, sizeof(buffer));
	int32_t i = 0;

	do
	{
		spi_status = spi_transfer(0, data, 0, 1, 0, ALT_AVALON_SPI_COMMAND_MERGE);
		//alt_avalon_spi_command(SPI_BASE, 0, 0, 0, 1, data, ALT_AVALON_SPI_COMMAND_MERGE);

		if (system_get_time_ms() - start_time > SD_TIMEOUT)
		{
			return SD_STATUS_TIMEOUT;
		}

		if (spi_status != SPI_STATUS_SUCCESS)
		{
			*data = 0xff;
		}
		if (i < 128)
		{
			buffer[i++] = *data;
		}
	} while ((*data & 0x80) != 0);

	spi_status = spi_transfer(0, data + 1, 0, 4, 0, 0);

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_receive_block(uint8_t data[], uint16_t *crc16_out)
{
	uint32_t start_time = system_get_time_ms();
	spi_status_t spi_status;

	uint8_t buffer[1024];
	int32_t b = 0;
	memset(buffer, 0, sizeof(buffer));

	// Receive bytes until start block byte received
	do
	{
		spi_status = spi_transfer(0, data, 0, 1, 0, ALT_AVALON_SPI_COMMAND_MERGE);
//		alt_avalon_spi_command(SPI_BASE, 0, 0, 0, 1, data, ALT_AVALON_SPI_COMMAND_MERGE);

		if (system_get_time_ms() - start_time > SD_TIMEOUT)
		{
			return SD_STATUS_TIMEOUT;
		}
		if (b < 1024)
		{
			buffer[b++] = data[0];
		}
	} while (data[0] != SD_START_BLOCK);

	// Receive data
	spi_status = spi_transfer(0, data, 0, 512, 0, ALT_AVALON_SPI_COMMAND_MERGE);
//	alt_avalon_spi_command(SPI_BASE, 0, 0, 0, 512, data, ALT_AVALON_SPI_COMMAND_MERGE);

//	if (spi_status != SPI_STATUS_SUCCESS)
//	{
//		return SD_STATUS_TIMEOUT;
//	}

	// Receive CRC16
	spi_status = spi_transfer(0, (uint8_t*)crc16_out, 0, 2, 0, 0);
//	alt_avalon_spi_command(SPI_BASE, 0, 0, 0, 2, crc16_out, ALT_AVALON_SPI_COMMAND_MERGE);

//	if (spi_status != SPI_STATUS_SUCCESS)
//	{
//		return SD_STATUS_TIMEOUT;
//	}

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_build_cmd(sd_command_t cmd, uint32_t arg, uint8_t data_out[])
{
	data_out[0] = 0x40 | cmd;
	data_out[1] = (arg >> 24) & 0xff;
	data_out[2] = (arg >> 16) & 0xff;
	data_out[3] = (arg >> 8) & 0xff;
	data_out[4] = arg & 0xff;

	uint8_t crc7 = sd_calc_crc7(data_out, 5);

	data_out[5] = (crc7 << 1) | 0x01;

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_read_block(uint32_t block_address, uint8_t *data_out, uint16_t *crc16_out)
{
	switch (active_ctrl)
	{
		case SD_CTRL_ALT_SPI:
		{
			return sd_read_block_alt(block_address, data_out, crc16_out);
		}
		case SD_CTRL_SD_READ:
		{
			return sd_read_block_sd_reader(block_address, (uint32_t*)data_out);
		}
	}

	return SD_STATUS_GENERAL_ERROR;
}

sd_status_t sd_read_block_alt(uint32_t block_address, uint8_t *data_out, uint16_t *crc16_out)
{
	sd_status_t sd_status;

	uint8_t txdata[6];
	uint8_t rxdata[6];

	// Transmit CMD17 (Read Single Block)
	sd_build_cmd(SD_COMMAND_CMD17, block_address, txdata);
	spi_transfer(txdata, 0, 6, 0, 0, ALT_AVALON_SPI_COMMAND_MERGE);
//	alt_avalon_spi_command(SPI_BASE, 0, 6, txdata, 0, rxdata, ALT_AVALON_SPI_COMMAND_MERGE);

	sd_status = sd_receive_r1(&rxdata[0]);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		return sd_status;
	}

	if ((rxdata[0] & SD_R1_ILLEGAL_COMMAND) != 0)
	{
		return SD_STATUS_ILLEGAL_COMMAND;
	}
	if ((rxdata[0] & SD_R1_COM_CRC_ERROR) != 0)
	{
		return SD_STATUS_CRC_ERROR;
	}

	sd_status = sd_receive_block(data_out, crc16_out);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		return sd_status;
	}

	spi_deassert_ss();

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_read_block_sd_reader(uint32_t block_address, uint32_t data_out[])
{
	// Start Transfer
	*sd_read_addr = block_address;

	uint32_t sd_reader_status;
	do
	{
		sd_reader_status = *sd_read_status;
	} while(sd_reader_status & SD_READER_STATUS_BUSY);

	if ((sd_reader_status & SD_READER_STATUS_CMD_ERR) != 0)
	{
		return SD_STATUS_GENERAL_ERROR;
	}
	else if ((sd_reader_status & SD_READER_STATUS_TIMEOUT_ERR) != 0)
	{
		return SD_STATUS_TIMEOUT;
	}
	else if ((sd_reader_status & SD_READER_STATUS_CRC_ERR) != 0)
	{
		return SD_STATUS_CRC_ERROR;
	}

	sd_reader_status = *sd_read_status;
	uint32_t tmp;// = *sd_read_data;

	if ((sd_reader_status & SD_READER_STATUS_VALID) == 0)
	{
		sd_reader_status = *sd_read_status;
		return SD_STATUS_GENERAL_ERROR;
	}

	int32_t word = 0;
	while ((sd_reader_status & SD_READER_STATUS_VALID) != 0)
	{
		if (word > 127)
		{
			break;
		}
		*(data_out + word) = *sd_read_data;
		word++;
		sd_reader_status = *sd_read_status;
	}

	return SD_STATUS_SUCCESS;
}

sd_status_t sd_select_ctrl(sd_ctrl_t ctrl)
{
	switch (ctrl)
	{
		case SD_CTRL_ALT_SPI:
		{
			*sd_ctrl_reg = 0x00000000;
			break;
		}
		case SD_CTRL_SD_READ:
		{
			*sd_ctrl_reg = 0x00000001;
			break;
		}
	}

	active_ctrl = ctrl;

	return SD_STATUS_SUCCESS;
}
