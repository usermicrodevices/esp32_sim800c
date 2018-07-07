#define NVSFAT_DEBUG 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"

static const char *TAG = "NVSFAT";
FILE *ptr_nvs_file = NULL;

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/db";
const char *file_path = "/db/nvs.txt";

void nvsfat_test(void)
{
	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "Mounting FAT filesystem");
	#endif
	const esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = true, .max_files = 4};
	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "fatfs", &mount_config, &s_wl_handle);
	if (err != ESP_OK)
	{
	#if NVSFAT_DEBUG
		ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
	#endif
		return;
	}
	String fn = String("/db/")+DateNowAsString()+String(".txt");
	#if NVSFAT_DEBUG
		ESP_LOGI(TAG, "Opening file %s", (char*)fn.c_str());
	#endif
	FILE *ptr_nvs_file = fopen((char*)fn.c_str(), "wb");
	if (ptr_nvs_file == NULL)
	{
	#if NVSFAT_DEBUG
		ESP_LOGE(TAG, "Failed to open file for writing");
	#endif
		return;
	}
	fprintf(ptr_nvs_file, "written using ESP-IDF %s\n", esp_get_idf_version());
	fclose(ptr_nvs_file);
	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "File written");
	#endif

	// Open file for reading
	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "Reading file");
	#endif
	ptr_nvs_file = fopen((char*)fn.c_str(), "rb");
	if (ptr_nvs_file == NULL)
	{
	#if NVSFAT_DEBUG
		ESP_LOGE(TAG, "Failed to open file for reading");
	#endif
		return;
	}
	char line[128];
	fgets(line, sizeof(line), ptr_nvs_file);
	fclose(ptr_nvs_file);
	// strip newline
	char *pos = strchr(line, '\n');
	if (pos) {*pos = '\0';}
	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "Read from file: '%s'", line);
	#endif

	// Unmount FATFS
	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "Unmounting FAT filesystem");
	#endif
	ESP_ERROR_CHECK( esp_vfs_fat_spiflash_unmount(base_path, s_wl_handle));

	#if NVSFAT_DEBUG
	ESP_LOGI(TAG, "Done");
	#endif
}

void nvsfat_begin()
{
	const esp_vfs_fat_mount_config_t mount_config = {.format_if_mount_failed = true, .max_files = 4};
	esp_err_t err = esp_vfs_fat_spiflash_mount(base_path, "fatfs", &mount_config, &s_wl_handle);
	if(err != ESP_OK)
	{
	#if NVSFAT_DEBUG
		ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
	#endif
	}
	#if NVSFAT_DEBUG
		ESP_LOGI(TAG, "Opening file %s", file_path);
	#endif
	ptr_nvs_file = fopen(file_path, "rb+");
	if(ptr_nvs_file == NULL)
	{
	#if NVSFAT_DEBUG
		ESP_LOGW(TAG, "File %s not found. Try create new...", file_path);
	#endif
		ptr_nvs_file = fopen(file_path, "wb");
		if(ptr_nvs_file == NULL)
		{
		#if NVSFAT_DEBUG
			ESP_LOGE(TAG, "Error create file %s", file_path);
		#endif
		}
	}
}
