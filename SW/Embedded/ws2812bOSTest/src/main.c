/*
 * main.c
 *
 *  Created on: Oct 31, 2020
 *      Author: aRobe
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#include "system_memory_map.h"
#include "system_timer.h"
#include "task_scheduler_if.h"
#include "led_task.h"
#include "sd_controller.h"
#include "master_boot_record.h"
#include "fat32_driver.h"
#include "light_display_file.h"

volatile uint8_t *_ledg = (uint8_t*)LEDG;

void system_init();
void main_loop();

int main(int argc, char *argv[])
{
	*_ledg = 0xFF;

	system_init();

	while (1)
	{
		main_loop();
	}

	return 0;
}

void system_init()
{
	*_ledg = 0x10;

	scheduler_init();

	sd_status_t sd_status;
	fat32_status_t fat32_status;

	*_ledg = 0x20;

	sd_status = sd_init();

	// Initialize SD
	do
	{
		*_ledg = 0x30;
		sd_status = sd_spi_init();
	} while (sd_status != SD_STATUS_SUCCESS);

	*_ledg = 0x40;

	// Switch to SD Reader Core
	sd_select_ctrl(SD_CTRL_SD_READ);

	*_ledg = 0x50;

	do
	{
		fat32_status = fat32_init();
	} while (fat32_status != FAT32_STATUS_SUCCESS);

	*_ledg = 0x60;

	// Allocate memory for directory and file lists
	fat32_directory_list_t dirs;
	fat32_directory_list_t files;
	memset(&dirs, 0, sizeof(fat32_directory_list_t));
	memset(&files, 0, sizeof(fat32_directory_list_t));

	// Walk directories
	fat32_status = fat32_walk_dir_recursive(2, &dirs, &files, 0, " ");

	*_ledg = 0x70;

	int d = 0;
	fat32_directory_entry_t *dir = fat32_directory_list_get_at_index(&dirs, 0);
	while (dir != NULL)
	{
		if (memcmp((char*)dir->dir_name, "AUTORUN", 7) == 0)
		{
			break;
		}

		d++;
		dir = fat32_directory_list_get_at_index(&dirs, d);
	}

	*_ledg = 0x80;

	fat32_directory_entry_t *file;
	uint32_t cluster;
	if (dir != NULL)
	{
		cluster = (((uint32_t)dir->dir_first_cluster_hi) << 16) | (dir->dir_first_cluster_lo);
		fat32_directory_list_clear(&files);
		fat32_directory_list_clear(&dirs);
		fat32_status = fat32_walk_dir(cluster, &dirs, &files, 2, "LDF");

		file = fat32_directory_list_get_at_index(&files, 0);
	}

	*_ledg = 0x90;

	cluster = (((uint32_t)file->dir_first_cluster_hi) << 16) | (file->dir_first_cluster_lo);
	fat32_select_file(cluster);

	*_ledg = 0xa0;

	uint8_t file_bytes[1024];

	// Initialize LDF file
	ldf_init();

	*_ledg = 0xb0;

	// Start LDF playback
	ldf_playback();

	*_ledg = 0xc0;
}

void main_loop()
{
	process_tasks();
}
