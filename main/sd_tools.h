#ifndef _SD_TOOLS_H_
#define _SD_TOOLS_H_

// #define DEBUG_SD

#ifdef DEBUG_WROVER_LCD
#define PIN_NUM_MISO GPIO_NUM_2
#define PIN_NUM_MOSI GPIO_NUM_15
#define PIN_NUM_CLK GPIO_NUM_14
#define PIN_NUM_CS GPIO_NUM_13
#else
//sck=18, miso=19, mosi=23, ss=5
#define PIN_NUM_MISO GPIO_NUM_19
#define PIN_NUM_MOSI GPIO_NUM_23
#define PIN_NUM_CLK GPIO_NUM_18
#define PIN_NUM_CS GPIO_NUM_5
#endif

#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"

static bool has_sd_card = false;
static char system_file_name[] = "/sdcard/system.inf";

struct stat st;
sdmmc_card_t* card;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();
sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config;// = {.format_if_mount_failed = true, .max_files = 33, .allocation_unit_size = 128};
String current_data_file_name = String("EMPTY");

void init_spi()
{
	// host.slot = HSPI_HOST;
	// host.flags = SDMMC_HOST_FLAG_4BIT;
	// host.command_timeout_ms = 10000;
	host.max_freq_khz = SDMMC_FREQ_PROBING;
	// host.max_freq_khz = SDMMC_FREQ_DEFAULT;
	// host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;
	slot_config.gpio_miso = PIN_NUM_MISO;
	slot_config.gpio_mosi = PIN_NUM_MOSI;
	slot_config.gpio_sck = PIN_NUM_CLK;
	slot_config.gpio_cs = PIN_NUM_CS;
// #ifdef DEBUG_WROVER_LCD
	// slot_config.gpio_cd = GPIO_NUM_21;//SDMMC_SLOT_NO_CD;
	// slot_config.gpio_wp = GPIO_NUM_4;//SDMMC_SLOT_NO_WP;
// #endif
	mount_config.format_if_mount_failed = true;
	mount_config.max_files = 33;
	mount_config.allocation_unit_size = 128;
#ifdef DEBUG_WROVER_LCD
	gpio_set_pull_mode(GPIO_NUM_5, GPIO_PULLUP_ONLY);   // CMD, needed in 4- and 1- line modes
	gpio_set_pull_mode(GPIO_NUM_18, GPIO_PULLUP_ONLY);    // D0, needed in 4- and 1-line modes
	gpio_set_pull_mode(GPIO_NUM_19, GPIO_PULLUP_ONLY);    // D1, needed in 4-line mode only
	gpio_set_pull_mode(GPIO_NUM_23, GPIO_PULLUP_ONLY);   // D2, needed in 4-line mode only
	// gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);   // D3, needed in 4- and 1-line modes
#endif
#ifdef DEBUG_SD
	printf("\nSD: miso = %d, mosi = %d, sck(clk) = %d, cs(ss) = %d\n", PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK, PIN_NUM_CS);
#endif
}

const char* error_description(int err_code)
{
	switch(err_code)
	{
		case ESP_ERR_NO_MEM:
			return "NO MEMORY";
		case ESP_ERR_INVALID_ARG:
			return "INVALID ARGUMENT";
		case ESP_ERR_INVALID_STATE:
			return "INVALID STATE";
		case ESP_ERR_INVALID_SIZE:
			return "INVALID SIZE";
		case ESP_ERR_NOT_FOUND:
			return "NOT FOUND";
		case ESP_ERR_NOT_SUPPORTED:
			return "NOT SUPPORTED";
		case ESP_ERR_TIMEOUT:
			return "TIMEOUT";
		case ESP_ERR_INVALID_RESPONSE:
			return "INVALID RESPONSE";
		case ESP_ERR_INVALID_CRC:
			return "INVALID CRC";
		case ESP_ERR_INVALID_VERSION:
			return "INVALID VERSION";
		case ESP_ERR_INVALID_MAC:
			return "INVALID MAC";
		default:
			return "UNKNOWN ERROR";
	}
}

long file_size(FILE* f)
{
	fseek(f , 0, SEEK_END);
	long lSize = ftell(f);
	rewind(f);
	return lSize;
}

void sd_create_binary_file(const char* fn, char* data)
{
	FILE* f = fopen(fn, "wb");
	if(f)
	{
		fprintf(f, "%s\n", data);
	#ifdef DEBUG_SD
		fflush(f);
		fseek(f , 0, SEEK_END);
		printf("SD: FILE %s CREATED, SIZE %ld\n", fn, ftell(f));
	#endif
		fclose(f);
	}
#ifdef DEBUG_SD
	else
	{
		perror(fn);
		printf("SD: ERROR OPEN FILE %s = %p\n", fn, f);
	}
#endif
}

void sd_create_system_info(String data)
{
	FILE* f = fopen(system_file_name, "w");
	if(f)
	{
		fprintf(f, "%s\n", data.c_str());
		fclose(f);
	#ifdef DEBUG_SD
		printf("SD: FILE %s CREATED\n", system_file_name);
	#endif
	}
#ifdef DEBUG_SD
	else
	{
		perror(system_file_name);
		printf("SD: ERROR OPEN FILE %s = %p\n", system_file_name, f);
	}
#endif
}

bool check_sd_card()
{
	esp_err_t ret;
	if(has_sd_card)
		ret = esp_vfs_fat_sdmmc_unmount();
	ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
	if(ret == ESP_OK)
	{
		has_sd_card = true;
	#ifdef DEBUG_SD
		sdmmc_card_print_info(stdout, card);
		// const char* file_name = "/sdcard/debug.txt";
		// FILE* f = NULL;
		// if (stat(file_name, &st) == 0)
		// {
			// f = fopen(file_name, "rb+");
			// fseek(f, 0, SEEK_END);
		// }
		// else
		// {f = fopen(file_name, "wb");}
		// if(f == NULL)
		// {printf("SD: Failed to open file %s for writing\n", file_name);}
		// else
		// {
			// printf("SD: File %s opened for writing\n", file_name);
			// fprintf(f, "SD CID = %s!\n", card->cid.name);
			// for(int i = 0; i < 10000; ++i)
				// {fprintf(f, "ITEM %d\n", i);}
			// fclose(f);
		// }
	#endif
	}
	else
	{
		has_sd_card = false;
	#ifdef DEBUG_SD
		if(ret == ESP_FAIL)
		{printf("SD: Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");}
		else
		{
			printf("SD: Failed to initialize the card (%s).\n", error_description(ret));
			printf("SD: Make sure SD card lines have pull-up resistors in place or you not correct setup pins.");
		}
	#endif
	}
	return has_sd_card;
}

void close_sd_card()
{
	// fcloseall();
	esp_vfs_fat_sdmmc_unmount();
}

///////////////WIFI CONFIG///////////////
// void wifi_read_config()
// {
	// if(has_sd_card)
	// {
		// const char* file_name = "/sdcard/wifi.cfg";
		// if(stat(file_name, &st) != 0)
		// {
		// #ifdef DEBUG_MAIN
			// printf("SD: NOT exists %s\n", file_name);
		// #endif
		// }
		// else
		// {
			// FILE* cfgfile = fopen(file_name, "r");
			// if(!cfgfile)
			// {
			// #ifdef DEBUG_MAIN
				// printf("SD: ERROR open file %s\n", file_name);
			// #endif
			// }
			// else
			// {
				// long bufsize = file_size(cfgfile);
				// char* buf = (char*)malloc(bufsize+1);
				// if(buf)
				// {
					// memset(buf, 0, bufsize+1);
					// if(fread(buf, 1, bufsize, cfgfile) == bufsize)
					// {
						// String content(buf);
						// int offset = 1;
						// size_t pos = content.indexOf('\n');
						// size_t pos1 = content.indexOf('\r');
						// if(pos1 > -1)
						// {
							// pos = pos1;
							// offset = 2;
						// }
						// if(pos > 0)
						// {
							// ssid = content.substring(0, pos-1);// vTaskDelay(1000/portTICK_RATE_MS);
							// password = content.substring(pos+offset);
							// password.replace(String('\r'), String(""));
							// password.replace(String('\n'), String(""));
						// }
					// }
					// free(buf);
				// }
				// fclose(cfgfile);
			// }
		// }
	// }
// }

#endif
