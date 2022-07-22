/*
 * light_display_file.c
 *
 *  Created on: Nov 8, 2020
 *      Author: aRobe
 */

#include "light_display_file.h"

#include <stdlib.h>

#include "fat32_driver.h"
#include "system_memory_map.h"

volatile uint32_t* tx_channels[8] = {
		(uint32_t*)TX_CHANNEL_0,
		(uint32_t*)TX_CHANNEL_1,
		(uint32_t*)TX_CHANNEL_2,
		(uint32_t*)TX_CHANNEL_3,
		(uint32_t*)TX_CHANNEL_4,
		(uint32_t*)TX_CHANNEL_5,
		(uint32_t*)TX_CHANNEL_6,
		(uint32_t*)TX_CHANNEL_7,
};

light_display_config_v_0_0_0_2_t ldf_config_page;
int32_t ldf_bytes_per_frame;
int32_t ldf_buffer_pages;

int32_t ldf_frame_index = 0;

uint32_t *ldf_buffer;

void ldf_init()
{
	// Read in configuration page
	fat32_read_file_bytes(0, sizeof(light_display_config_v_0_0_0_2_t), (uint8_t*)&ldf_config_page);

	// Calculate the total number of bytes in a frame
	int32_t c = 0;
	ldf_bytes_per_frame = 0;
	for (c = 0; c < ldf_config_page.channels; c++)
	{
		ldf_bytes_per_frame += ldf_get_bytes_in_channel(c);
	}

	ldf_buffer_pages = (ldf_bytes_per_frame / 512) + 2;

	ldf_buffer = calloc(ldf_buffer_pages * 512, sizeof(uint8_t));

	ldf_frame_index = 0;
}

void ldf_get_channel_frame_data(int32_t channel, int32_t frame, uint8_t *data_out)
{
	int32_t first_byte_in_frame = sizeof(light_display_config_v_0_0_0_2_t) + (frame * ldf_bytes_per_frame);

	int32_t channel_byte_offset = 0;
	int32_t c = 0;
	for (c = 0; c < channel; c++)
	{
		channel_byte_offset += ldf_get_bytes_in_channel(c);
	}

	int32_t first_byte_in_channel = first_byte_in_frame + channel_byte_offset;

	// Read channel bytes
	fat32_read_file_bytes(first_byte_in_channel, ldf_get_bytes_in_channel(channel), data_out);
}

int32_t ldf_get_bytes_in_channel(int32_t channel)
{
	return ldf_config_page.leds_in_channel[channel] * sizeof(uint32_t);
}

void ldf_playback()
{
	schedule_interval(&ldf_next_frame, ldf_config_page.frame_duration_ms, false);
	//schedule_interval(&ldf_next_frame, 16, false);
}

task_status_t ldf_next_frame(uint32_t task_id)
{
	uint32_t start_time = system_get_time_ms();

	// Load data for this frame
	ldf_load_frame(ldf_frame_index, ldf_buffer);

	int32_t channel;
	int32_t byte_index = 0;
	int32_t bytes_in_channel;
	for (channel = 0; channel < ldf_config_page.channels; channel++)
	{
		bytes_in_channel = ldf_get_bytes_in_channel(channel);
		ldf_stream_channel(channel, (uint8_t*)(ldf_buffer + (byte_index / sizeof(uint32_t))), bytes_in_channel);

		byte_index += bytes_in_channel;
	}

	for (channel = 0; channel < ldf_config_page.channels; channel++)
	{
		// Start transmit
		*(tx_channels[channel] + 1) = 0x01;
	}

	ldf_frame_index++;
	if (ldf_frame_index > ldf_config_page.frames_total - 1)
	{
		ldf_frame_index = 0;
	}

	uint32_t delta = system_get_time_ms() - start_time;

	return TASK_STATUS_GOOD;
}

void ldf_stream_channel(int32_t channel, uint8_t *channel_data, int32_t byte_count)
{
	uint8_t *last_byte = (uint8_t*)(channel_data + byte_count);
	//uint32_t word_to_write = 0;
	while (channel_data < last_byte)
	{
		//memcpy(&word_to_write, channel_data, 4);

		// Stream word
		*tx_channels[channel] = *((uint32_t*)channel_data);//word_to_write; //
		channel_data += sizeof(uint32_t);
	}
}

fat32_status_t ldf_load_frame(int32_t frame, uint32_t *data_out)
{
	int32_t first_byte = sizeof(light_display_config_v_0_0_0_2_t) + (ldf_bytes_per_frame * frame);

	fat32_status_t sd_status = fat32_read_file_bytes(first_byte, ldf_bytes_per_frame, (uint8_t*)data_out);

	return FAT32_STATUS_SUCCESS;
}
