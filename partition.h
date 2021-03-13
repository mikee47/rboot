#pragma once

#include <stdbool.h>

#define ESP_PARTITION_MAGIC 0x50AA
#define PARTITION_TYPE_APP 0x00
#define PARTITION_APP_SUBTYPE_FACTORY 0x00 // Corresponds to Slot #0
#define PARTITION_APP_SUBTYPE_OTA0 0x10	// Also corresponds to Slot #0
#define PARTITION_TYPE_DATA 0x01
#define PARTITION_DATA_SUBTYPE_SYSPARAM 0x40 // SDK system parameters
#define ESP_PARTITION_TABLE_MAX_LEN 0x0C00

/**
 * @brief Internal structure describing the binary layout of a partition table entry.
 */
struct esp_partition_info_t {
	uint16_t magic;  ///< Fixed value to identify valid entry, appears as 0xFFFF at end of table
	uint8_t type;	///< Main type of partition
	uint8_t subtype; ///< Sub-type for partition (interpretation dependent upon type)
	uint32_t offset; ///< Start offset
	uint32_t size;   ///< Size of partition in bytes
	char name[16];   ///< Unique identifer for entry
	uint32_t flags;  ///< Various option flags
};

// Read ROM locations from partition table. Return true if config changed
static bool scan_partitions(rboot_config* romconf)
{
	bool config_changed = false;
	uint8_t rom_count = 0;
	struct esp_partition_info_t info;
	uint32_t offset;
	for(offset = 0; offset < ESP_PARTITION_TABLE_MAX_LEN; offset += sizeof(info)) {
		info.magic = 0;
		SPIRead(PARTITION_TABLE_OFFSET + offset, &info, sizeof(info));
		if(info.magic != ESP_PARTITION_MAGIC) {
			break;
		}
		if(info.type != PARTITION_TYPE_APP) {
			continue;
		}
		unsigned index;
		if(info.subtype == PARTITION_APP_SUBTYPE_FACTORY) {
			index = 0;
		} else {
			index = info.subtype - PARTITION_APP_SUBTYPE_OTA0;
			if(index >= MAX_ROMS) {
				continue;
			}
		}
		echof("Found '%s' @ 0x%08x, size 0x%08x, subtype 0x%02X\r\n", info.name, info.offset, info.size, info.subtype);
		if(romconf->roms[index] != info.offset) {
			romconf->roms[index] = info.offset;
			config_changed = true;
		}
		if(index >= rom_count) {
			rom_count = index + 1;
		}
	}
	if(romconf->count != rom_count) {
		config_changed = true;
		romconf->count = rom_count;
	}
	return config_changed;
}

#if defined(BOOT_GPIO_ENABLED) || defined(BOOT_GPIO_SKIP_ENABLED)

// Supports MODE_GPIO_ERASES_SDKCONFIG setting
static void erase_sdk_config()
{
	echof("Erasing SDK config partition.\r\n");

	bool config_changed = false;
	uint8_t rom_count = 0;
	struct esp_partition_info_t info;
	uint32_t offset;
	for(offset = 0; offset < ESP_PARTITION_TABLE_MAX_LEN; offset += sizeof(info)) {
		info.magic = 0;
		if(SPIRead(PARTITION_TABLE_OFFSET + offset, &info, sizeof(info)) != 0) {
			break;
		}
		if(info.magic != ESP_PARTITION_MAGIC) {
			break;
		}
		if(info.type != PARTITION_TYPE_DATA || info.subtype != PARTITION_DATA_SUBTYPE_SYSPARAM) {
			continue;
		}
		uint32_t sector = info.offset / SECTOR_SIZE;
		uint8_t sector_count = info.size / SECTOR_SIZE;
		while(sector_count-- != 0) {
			SPIEraseSector(sector++);
		}
	}
}

#endif
