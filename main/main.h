#ifndef _MAIN_H_
#define _MAIN_H_

#define FIRMWARE_VERSION "000003"

#define DEBUG_MAIN
#define DEBUG_OTA
#define DEBUG_NVS
#define DEBUG_NVS_ERASE_ALL
// #define DEBUG_WROVER_LCD
#define DB_ENABLE
#define INCLUDE_WATER
#define CONTENT_LENGTH_BUFFSIZE 138
#define USE_USB_SERIAL

#ifdef DEBUG_WROVER_LCD
#include "HardwareSerial.h"
#include "Adafruit_GFX.h"
#include "WROVER_KIT_LCD.h"
WROVER_KIT_LCD wroverlcd;
#define printf(format, ...) if(wroverlcd.getCursorY()>220){wroverlcd.fillScreen(WROVER_BLACK);wroverlcd.setCursor(0, 0);}wroverlcd.printf(format, ##__VA_ARGS__); wroverlcd.println("");
#endif

#define CCI_BUF_SIZE (128)
#define NVS_KEY_SIZE (5)
#define BUF_SIZE (512)
#define AT_BUF_SIZE (64)
#define TIME_BUFSIZE (64)
#define SIZE_AT_CIPSTART (64)
#define SIZE_AT_CIPSEND (16)
#define SIZE_AT_POST (256)
#define NVS_MAX_DATA_COUNTER (100)

#define OTA_BUFFER_SIZE 8192
#define OTA_SD_BUFFER_SIZE 4096
#define OTA_SD_FILE_NAME "/sdcard/update.bin"

#define SENDED_FILE_NAME "/db/sended.txt"
#define REMOTE_PORT 80
#define REMOTE_HOST "ya.ru"
#define WEB_URL_API "http://" REMOTE_HOST "/api"
// #define FTP_SERVER "127.0.0.1"
// #define FTP_PORT 21
// #define FTP_USER "user"
// #define FTP_PASSWORD "pass"
//FTP ERRORS
// 61 Net error
// 62 DNS error
// 63 connect error
// 64 timeout
// 65 server error
// 66 operation not allow
// 70 replay error
// 71 user error
// 72 password error
// 73 type error
// 74 rest error
// 75 passive error
// 76 active error
// 77 operate error
// 78 upload error
// 79 download error

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/unistd.h>
#include <sys/stat.h>
#include "driver/gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_spi_flash.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/uart.h"
#include "soc/uart_struct.h"
// #include "apps/sntp/sntp.h"
////////////////////
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "esp_ota_ops.h"
////////////////////
#include "Arduino.h"
// #include "HardwareSerial.h"
#ifdef USE_USB_SERIAL
const int CCI_UART_NUM = UART_NUM_0;
#else
const int CCI_UART_NUM = UART_NUM_2;
HardwareSerial portcci = HardwareSerial(CCI_UART_NUM);
// HardwareSerial portcci = Serial;
#endif
////////////////////
#include <SD.h>

#define TINY_GSM_RX_BUFFER 1460
#define TINY_GSM_MODEM_SIM800
#include "TinyGsmClient.h"

#include "str_tools.h"
#include "date_time.h"
#include "sd_tools.h"

#include "sim800.h"
static bool gsm_no_problem = false, download_firmware = false;
sim800 modem = sim800();
static uint16_t http_critical_status = 599;

SemaphoreHandle_t xSemaphore = NULL;

static int32_t nvs_data_counter = 0;
nvs_handle hnvs = 0;

String id_controller("GSM");

#include "gsm_tools.h"

#ifdef DB_ENABLE
#include "db.h"
#endif

static int send_ppm = 1;
#include "water_sensor.h"

// Mount path for the partition
// const char *base_path = "/nvsdb";

const size_t stack_size_cci = 4*1024;
const size_t stack_size_sync = 12*1024;
char* time_data;// = "4630E0F5";

void led_on(bool on = true)
{
	pinMode(GPIO_NUM_21, OUTPUT);
	if(on)
	{
		digitalWrite(GPIO_NUM_21, HIGH);
		pinMode(GPIO_NUM_21, INPUT_PULLUP);// make pin unused (do not leak)
	}
	else
	{
		digitalWrite(GPIO_NUM_21, LOW);
		vTaskDelay(1100 / portTICK_RATE_MS);
		digitalWrite(GPIO_NUM_21, HIGH);
		vTaskDelay(1100 / portTICK_RATE_MS);
		// vTaskDelay(35000 / portTICK_RATE_MS);
		digitalWrite(GPIO_NUM_21, HIGH);
		pinMode(GPIO_NUM_21, INPUT_PULLUP);// make pin unused (do not leak)
	}
}

/////////////OTA SD//////////////
void ota_from_sd_card()
{
	if(!has_sd_card)
	{
	#ifdef DEBUG_OTA
		printf("\nOTA_SD: SD CARD NOT FOUND\n");
	#endif
	}
	else
	{
	#ifdef DEBUG_OTA
		printf("\nOTA_SD: SEARCH FILE %s\n", OTA_SD_FILE_NAME);
	#endif
		if(stat(OTA_SD_FILE_NAME, &st) != 0)
		{
		#ifdef DEBUG_OTA
			printf("\nOTA_SD: FILE NOT EXISTS\n");
		#endif
			return;
		}
		else
		{
			FILE* update_file = fopen(OTA_SD_FILE_NAME, "rb");
			if(!update_file)
			{
			#ifdef DEBUG_OTA
				printf("\nOTA_SD: FILE NOT OPENED\n");
			#endif
				return;
			}
			else
			{
				char* buf = (char*)malloc(OTA_SD_BUFFER_SIZE);
				if(buf)
				{
					esp_err_t err;
					esp_ota_handle_t ota_handle = 0 ;
					const esp_partition_t *update_partition = NULL;
				#ifdef DEBUG_OTA
					printf("\nOTA_SD: Starting OTA...\n");
					const esp_partition_t *configured = esp_ota_get_boot_partition();
					const esp_partition_t *running = esp_ota_get_running_partition();
					if(configured != running)
					{
						printf("\nOTA_SD: Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n", configured->address, running->address);
						printf("\nOTA_SD: (This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n");
					}
					printf("\nOTA_SD: Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);
				#endif
					update_partition = esp_ota_get_next_update_partition(NULL);
					assert(update_partition != NULL);
				#ifdef DEBUG_OTA
					printf("\nOTA_SD: Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);
				#endif
					err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
					if(err != ESP_OK)
					{
					#ifdef DEBUG_OTA
						printf("\nOTA_SD: esp_ota_begin failed, error=%d\n", err);
					#endif
					}
					else
					{
					#ifdef DEBUG_OTA
						printf("\nOTA_SD: ESP_OTA_BEGIN SUCCEEDED\n");
						uint32_t readed_ota_bytes = 0, step = 0;
					#endif
						uint32_t buff_len = OTA_SD_BUFFER_SIZE;
					#ifdef DEBUG_OTA
						printf("\n\nOTA_SD: ESP_OTA_WRITE RESULT: %d (LENGTH %d)", err, buff_len);
					#endif
						// while(fgets(buf, OTA_SD_BUFFER_SIZE, update_file) != NULL)
						while(fread(buf, 1, OTA_SD_BUFFER_SIZE, update_file) > 0)
						{
							// memset(buf, 0, OTA_SD_BUFFER_SIZE);
							// buff_len = update_file.read(buf, OTA_SD_BUFFER_SIZE);
							err = esp_ota_write(ota_handle, (const void*)buf, buff_len);
							memset(buf, 0, OTA_SD_BUFFER_SIZE);
						#ifdef DEBUG_OTA
							readed_ota_bytes += buff_len;
							printf("\rOTA_SD: ESP_OTA_WRITE RESULT: %d (LENGTH %d) STEP=%d", err, buff_len, step);
							step++;
						#endif
							ESP_ERROR_CHECK(err);
							if(err != ESP_OK) break;
						}
						fclose(update_file);
					#ifdef DEBUG_OTA
						printf("\nOTA_SD: Total Write binary data length : %d\n", readed_ota_bytes);
					#endif
						if(esp_ota_end(ota_handle) != ESP_OK)
						{
						#ifdef DEBUG_OTA
							printf("\nOTA_SD: esp_ota_end failed!\n");
						#endif
						}
						else
						{
							err = esp_ota_set_boot_partition(update_partition);
							if(err != ESP_OK)
							{
							#ifdef DEBUG_OTA
								printf("\nOTA_SD: esp_ota_set_boot_partition failed! err=0x%x\n", err);
							#endif
							}
							else
							{
							#ifdef DEBUG_OTA
								printf("\nOTA_SD: Prepare to restart system!\n");
							#endif
								free(buf);
								if(remove(OTA_SD_FILE_NAME) != 0)
									rename(OTA_SD_FILE_NAME, "/sdcard/update.bak");
								close_sd_card();
								esp_restart();
							}
						}
					}
					free(buf);
				}
				else
				{
				#ifdef DEBUG_OTA
					printf("\nOTA_SD: ERROR BUFFER ALLOCATION\n");
				#endif
				}
			}
		}
	}
}

#endif
