/*
 * fat32_driver.c
 *
 *  Created on: Nov 7, 2020
 *      Author: aRobe
 */

#include "sd_controller.h"
#include "fat32_driver.h"

#include <string.h>
#include <stdlib.h>

uint32_t fat32_partition_first_sector = 0;
fat32_bios_parameter_block_t fat32_bpb;
fat32_fat_sector_t fat32_fat_sector;

fat32_cluster_map_t fat32_selected_file_cluster_map;

/*
 * Function Name: fat32_init
 * Description: Searches for a FAT partition in the first partition of the MBR
 * Inputs:
 * 			- None
 * Outputs:
 * 			- fat32_status_t - FAT32_STATUS_SUCCESS if successful
 */
fat32_status_t fat32_init()
{
	uint32_t block_addr = 0;
	uint32_t data[512 / sizeof(uint32_t)];
	uint16_t crc16;
	sd_status_t sd_status;

	sd_status = sd_read_block(block_addr, (uint8_t*)data, &crc16);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		return FAT32_STATUS_DISK_READ_ERROR;
	}

	mbr_partition_entry_t *partition = (mbr_partition_entry_t*)(&data[MBR_PART1 / sizeof(uint32_t)]);

	block_addr = (partition->lba_address) >> 16;

	sd_status = sd_read_block(block_addr, (uint8_t*)data, &crc16);

	if (sd_status != SD_STATUS_SUCCESS)
	{
		return FAT32_STATUS_DISK_READ_ERROR;
	}

	if ((data[0] & 0xff) == 0xEB || (data[0] & 0xff) == 0xE9)
	{
		fat32_partition_first_sector = block_addr;

		fat32_bios_parameter_block_t *bpb = (fat32_bios_parameter_block_t*)data;
		memcpy(&fat32_bpb, bpb, sizeof(fat32_bios_parameter_block_t));

		uint32_t fat_first_sector = fat32_partition_first_sector + fat32_bpb.bpb_rsvd_sec_count;

		// Load first sector of FAT
		sd_status = sd_read_block(fat_first_sector, (uint8_t*)fat32_fat_sector.clusters, &crc16);
		fat32_fat_sector.loaded_sector = fat_first_sector;

		if (sd_status != SD_STATUS_SUCCESS)
		{
			return FAT32_STATUS_DISK_READ_ERROR;
		}

		return FAT32_STATUS_SUCCESS;
	}
	else
	{
		return FAT32_STATUS_PARTITION_NOT_FOUND;
	}
}

uint32_t fat32_get_root_sector()
{
	return (uint32_t)(fat32_partition_first_sector + fat32_bpb.bpb_rsvd_sec_count + (fat32_bpb.bpb_fat_size_32 * fat32_bpb.bpb_num_fats));
}

/*
 * Function Name: fat32_get_next_cluster
 * Description: Gets the next cluster number of a file given a cluster number
 * Inputs:
 * 			- cluster - Cluster number
 * 			- *next_cluster_out - Pointer to cluster number output
 * Outputs:
 * 			- Returns: fat32_status_t - FAT32_STATUS_SUCCESS if successful
 * 			- *next_cluster_out - Next cluster number
 * Notes: (Untested) TODO Test
 */
fat32_status_t fat32_get_next_cluster(uint32_t cluster, uint32_t *next_cluster_out)
{
	uint32_t fat_sector = fat32_partition_first_sector + fat32_bpb.bpb_rsvd_sec_count + (cluster / FAT_ENTRIES_PER_SECTOR);
	uint32_t entry_offset = cluster % FAT_ENTRIES_PER_SECTOR;

	sd_status_t sd_status;
	uint16_t crc16;

	if (fat32_fat_sector.loaded_sector != fat_sector)
	{
		sd_status = sd_read_block(fat_sector, (uint8_t*)fat32_fat_sector.clusters, &crc16);
		fat32_fat_sector.loaded_sector = fat_sector;

		if (sd_status != SD_STATUS_SUCCESS)
		{
			return FAT32_STATUS_DISK_READ_ERROR;
		}
	}

	*next_cluster_out = fat32_fat_sector.clusters[entry_offset];

	return FAT32_STATUS_SUCCESS;
}

fat32_status_t fat32_walk_dir(uint32_t first_cluster, fat32_directory_list_t *dirs, fat32_directory_list_t *files, int32_t entry_offset, char *extension)
{
	uint32_t cluster = first_cluster;
	fat32_status_t fat32_status;

	while (cluster != CLUSTER_EOF)
	{
		uint32_t data[512 / sizeof(uint32_t)];

		uint32_t cluster_sector = fat32_get_cluster_sector(cluster);

		// Loop through sectors in cluster
		int32_t sector_offset = 0;
		uint32_t sector_number;
		uint16_t crc16;
		for (sector_offset = 0; sector_offset < fat32_bpb.bpb_sec_per_cluster; sector_offset++)
		{
			sector_number = cluster_sector + sector_offset;
			sd_status_t sd_status = sd_read_block(sector_number, (uint8_t*)data, &crc16);

			if (sd_status != SD_STATUS_SUCCESS)
			{
				return FAT32_STATUS_DISK_READ_ERROR;
			}

			// Loop through entries in sector
			fat32_directory_entry_t *entry = (fat32_directory_entry_t*)data; // Point to first entry in sector data
			int32_t e = 0;
			entry += entry_offset;
			for (e = entry_offset; e < 16; e++)
			{
				// Do not check if this is a system or hidden folder
				if ((entry->dir_attributes & (ATTR_SYSTEM | ATTR_HIDDEN)) == 0)
				{
					if ((entry->dir_attributes & ATTR_DIRECTORY) != 0)
					{
						// Add to directory list
						fat32_directory_list_add(dirs, entry);
					}
					else
					{
						// Check if this is the last entry
						if (entry->dir_name[0] == '\0')
						{
							return FAT32_STATUS_SUCCESS;
						}

						// Check if file is available (empty)
						if (entry->dir_name[0] != CHAR_AVAILABLE)
						{
							// Check if no extension specified OR
							// if extension matches
							if (extension == '\0' || memcmp(extension, &(entry->dir_name[8]), 3) == 0)
							{
								// Add to file list
								fat32_directory_list_add(files, entry);
							}
						}
					}
				}

				entry++; // Move to next entry
			}
		}

		fat32_status = fat32_get_next_cluster(cluster, &cluster);

		if (fat32_status != FAT32_STATUS_SUCCESS)
		{
			return fat32_status;
		}
	}

	return FAT32_STATUS_SUCCESS;
}

fat32_status_t fat32_walk_dir_recursive(uint32_t first_cluster, fat32_directory_list_t *dirs, fat32_directory_list_t *files, int32_t entry_offset, char *extension)
{
	fat32_status_t fat32_status;
	int32_t dir_index = 0;

	fat32_status = fat32_walk_dir(first_cluster, dirs, files, entry_offset, extension);

	if (fat32_status != FAT32_STATUS_SUCCESS)
	{
		return fat32_status;
	}

	// Go to first subdirectory
	fat32_directory_entry_t *dir = fat32_directory_list_get_at_index(dirs, 0);
	while (dir != NULL)
	{
		uint32_t cluster = (((uint32_t)dir->dir_first_cluster_hi) << 16) | (dir->dir_first_cluster_lo);

		// Walk subdirectory.  Offset is 2 to skip "." and ".."
		fat32_status = fat32_walk_dir(cluster, dirs, files, 2, extension);

		if (fat32_status != FAT32_STATUS_SUCCESS)
		{
			return fat32_status;
		}

		// Move to next directory found.
		dir_index++;
		dir = fat32_directory_list_get_at_index(dirs, dir_index);
	}

	return FAT32_STATUS_SUCCESS;
}

void fat32_directory_list_add(fat32_directory_list_t *list, fat32_directory_entry_t *entry)
{
	if (list == NULL)
	{
		// Cannot add to null list
		return;
	}

	// Loop through list fragments
	while (list != NULL)
	{
		if (list->count < 8)
		{
			// Copy into first available slot
			memcpy(&(list->entries[list->count]), entry, sizeof(fat32_directory_entry_t));

			// Increment count
			list->count++;

			// Added.  Return.
			return;
		}

		if (list->next == NULL)
		{
			// Allocate a new list fragment
			list->next = calloc(sizeof(fat32_directory_list_t), sizeof(uint8_t));
		}

		list = list->next;
	}

	// Could not allocate list fragment
}

void fat32_directory_list_clear(fat32_directory_list_t *list)
{
	fat32_directory_list_t *next = list->next;

	// First fragment is static, clear memory
	memset(list, 0, sizeof(fat32_directory_list_t));
	list = next;

	while (list != NULL)
	{
		next = list->next;

		// Subsequent fragments are dynamic, free memory
		free(list);
		list = next;
	}
}

void fat32_cluster_map_add(fat32_cluster_map_t *map, uint32_t cluster)
{
	if (map == NULL)
	{
		// Cannot add to null map
		return;
	}

	// Loop through map fragments
	while (map != NULL)
	{
		if (map->count < 64)
		{
			// Copy into first available slot
			memcpy(&(map->cluster[map->count]), &cluster, sizeof(uint32_t));

			// Increment count
			map->count++;

			// Added.  Return.
			return;
		}

		if (map->next == NULL)
		{
			// Allocate a new map fragment
			map->next = calloc(sizeof(fat32_cluster_map_t), 1);
		}

		map = map->next;
	}

	// Could not allocate map fragment
}

void fat32_cluster_map_clear(fat32_cluster_map_t *map)
{
	fat32_cluster_map_t *next = map->next;

	// First fragment is static, clear memory
	memset(map, 0, sizeof(fat32_cluster_map_t));
	map = next;

	while (map != NULL)
	{
		next = map->next;

		// Subsequent fragments are dynamic, free memory
		free(map);
		map = next;
	}
}

fat32_status_t fat32_assemble_cluster_map(uint32_t first_cluster, fat32_cluster_map_t *map_out)
{
	uint32_t cluster = first_cluster;
	fat32_status_t fat32_status;

	// Loop until EOF cluster is found
	while (cluster != CLUSTER_EOF)
	{
		// Add cluster to map
		fat32_cluster_map_add(map_out, cluster);

		// Get next cluster in the chain
		fat32_status = fat32_get_next_cluster(cluster, &cluster);

		if (fat32_status != FAT32_STATUS_SUCCESS)
		{
			return fat32_status;
		}
	}

	return FAT32_STATUS_SUCCESS;
}

fat32_directory_entry_t* fat32_directory_list_get_at_index(fat32_directory_list_t *list, int32_t index)
{
	// Move through fragments until the correct fragment is reached.
	while (index >= 8)
	{
		list = list->next;
		index -= 8;

		if (list == NULL)
		{
			// Error end of list
			return NULL;
		}
	}

	// Check if there is an entry at this index in the fragment
	if (index > list->count - 1)
	{
		return NULL;
	}

	return &(list->entries[index]);
}

uint32_t fat32_cluster_map_get_at_index(fat32_cluster_map_t *map, int32_t index)
{
	// Move through fragments until the correct fragment is reached.
	while (index >= 64)
	{
		map = map->next;
		index -= 64;

		if (map == NULL)
		{
			// Error end of list
			return 0;
		}
	}

	// Check if there is an entry at this index in the fragment
	if (index > map->count - 1)
	{
		return NULL;
	}

	return map->cluster[index];
}

uint32_t fat32_get_cluster_sector(uint32_t cluster)
{
	uint32_t cluster_offset_from_root = cluster - 2;

	return fat32_get_root_sector() + (cluster_offset_from_root * fat32_bpb.bpb_sec_per_cluster);
}

fat32_status_t fat32_select_file(uint32_t first_cluster)
{
	fat32_cluster_map_clear(&fat32_selected_file_cluster_map);

	fat32_status_t fat32_status = fat32_assemble_cluster_map(first_cluster, &fat32_selected_file_cluster_map);

	return fat32_status;
}

fat32_status_t fat32_read_file_bytes(int32_t byte_offset, int32_t byte_count, uint8_t *data_out)
{
	fat32_status_t fat32_status;
	uint32_t data[512 / sizeof(uint32_t)];
	uint16_t crc16;

	// Index of next byte to copy into output
	int32_t byte_index = 0;

	int32_t cluster_index; // Index into the cluster map of the cluster containing the first byte to be copied
	int32_t byte_in_cluster; // Byte number of the first byte in the cluster
	int32_t sector_in_cluster; // Sector offset in the cluster of the sector containing the first byte
	int32_t byte_in_sector; // Byte offset in sector of the first byte to be copied
	int32_t cluster; // Absolute cluster number

	uint32_t sector_number; // Absolute sector number

	// Calculate cluster index of first byte
	cluster_index = byte_offset / (fat32_bpb.bpb_bytes_per_sec * fat32_bpb.bpb_sec_per_cluster);
	// Calculate byte offset in cluster
	byte_in_cluster = byte_offset % (fat32_bpb.bpb_bytes_per_sec * fat32_bpb.bpb_sec_per_cluster);

	// Calculate sector offset in cluster
	sector_in_cluster = byte_in_cluster / fat32_bpb.bpb_bytes_per_sec;
	// Calculate byte offset in sector
	byte_in_sector = byte_in_cluster % fat32_bpb.bpb_bytes_per_sec;

	// Get cluster number from cluster map
	cluster = fat32_cluster_map_get_at_index(&fat32_selected_file_cluster_map, cluster_index);

	// Loop through clusters in file, starting at cluster index of first byte
	while (cluster != CLUSTER_EOF)
	{
		// Calculate absolute sector number
		sector_number = fat32_get_cluster_sector(cluster) + sector_in_cluster;

		// Loop through sectors in cluster
		while (sector_in_cluster < fat32_bpb.bpb_sec_per_cluster)
		{
			int32_t bytes_to_copy = byte_count - byte_index;
			bytes_to_copy = (bytes_to_copy > (512 - byte_in_sector)) ? (512 - byte_in_sector) : bytes_to_copy;

			// First and last reads may not be 512.
			// Read into temp and copy
			if (byte_index == 0 || byte_count - byte_index < fat32_bpb.bpb_bytes_per_sec)
			{
				fat32_status = sd_read_block(sector_number, (uint8_t*)data, &crc16);

				// Copy bytes into output
				memcpy(&data_out[byte_index], ((uint8_t*)data) + byte_in_sector, bytes_to_copy);
			}
			else
			{
				// Other sectors are 512 bytes.  Copy directly into output
				// Read data from sector
				fat32_status = sd_read_block(sector_number, (uint8_t*)(data_out + byte_index), &crc16);
			}

			if (fat32_status != FAT32_STATUS_SUCCESS)
			{
				return fat32_status;
			}

			byte_index += bytes_to_copy;

			// Check if all bytes have been copied
			if (byte_index >= byte_count)
			{
				return FAT32_STATUS_SUCCESS;
			}

			// Move to next sector in cluster
			sector_in_cluster++;
			sector_number++;

			// Move to first byte in sector
			byte_in_sector = 0;
		}

		// Move to next cluster
		cluster_index++;
		cluster = fat32_cluster_map_get_at_index(&fat32_selected_file_cluster_map, cluster_index);

		// Go to first sector in the cluster
		sector_in_cluster = 0;
	}

	// Error EOF reached before expected
	return FAT32_STATUS_UNEXPECTED_EOF;
}
