#include "main.h"

void nvs_data_save(const char* data)
{
	if(hnvs)
	{
		bool save = false;
		char key[NVS_KEY_SIZE];
		sprintf(key, "%d", nvs_data_counter);
		esp_err_t err = nvs_set_str(hnvs, key, data);
		if(err == ESP_ERR_NVS_NOT_ENOUGH_SPACE || err == ESP_ERR_NVS_PAGE_FULL)
		{
			nvs_data_counter = 0;
			err = nvs_set_str(hnvs, "0", data);
		}
		if(err == ESP_OK)
		{
			if(nvs_data_counter == NVS_MAX_DATA_COUNTER)
				nvs_data_counter = 0;
			else
				nvs_data_counter++;
			err = nvs_set_i32(hnvs, "data_counter", nvs_data_counter);
			if(err == ESP_OK) save = true;
		}
		if(save)
			err = nvs_commit(hnvs);
	}
}

void nvs_check_timestamp(int32_t checked_time_stamp, bool save)
{
#ifdef DEBUG_NVS
	printf("\nNVS: CHECKED TIMESTAMP: %d\n", checked_time_stamp);
#endif
	if(hnvs)
	{
		int32_t current_time_stamp = 0;
		esp_err_t err = nvs_get_i32(hnvs, "timestamp", &current_time_stamp);
		if(err != ESP_OK)
			current_time_stamp = default_current_time_stamp;
		if(current_time_stamp < checked_time_stamp || checked_time_stamp > default_current_time_stamp)
			current_time_stamp = checked_time_stamp;
		else
			checked_time_stamp = current_time_stamp;
	#ifdef DEBUG_NVS
		printf("\nNVS: CURRENT TIMESTAMP: %d\n", current_time_stamp);
	#endif
		set_time_stamp(current_time_stamp);
		err = nvs_set_i32(hnvs, "timestamp", current_time_stamp);
		if(save && err == ESP_OK) err = nvs_commit(hnvs);
	}
}

void nvs_first_init(int32_t checked_time_stamp)
{
	esp_err_t err = nvs_flash_init();
	if(err == ESP_ERR_NVS_NO_FREE_PAGES)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	// printf("\n+++NVS ERROR=%X+++\n", err);
	ESP_ERROR_CHECK(err);
	err = nvs_open("nvs", NVS_READWRITE, &hnvs);
	if(err == ESP_OK)
	{
	#ifdef DEBUG_NVS_ERASE_ALL
		err = nvs_erase_all(hnvs);
		printf("\nNVS: erase_all = %d\n", err);
		if(err == ESP_OK)
		{
			err = nvs_commit(hnvs);
			printf("\nNVS: commit = %d\n", err);
		}
	#endif
	#ifdef DEBUG_NVS
		printf("\nNVS: nvs_first_init\n");
	#endif
		nvs_check_timestamp(checked_time_stamp, true);
		//////////////////////
		err = nvs_get_i32(hnvs, "data_counter", &nvs_data_counter);
		if(err != ESP_OK)
			err = nvs_set_i32(hnvs, "data_counter", nvs_data_counter);
		//////////////////////
		err = nvs_commit(hnvs);
	}
}

void nvs_task_save_time(void*)
{
	while(1)
	{
		vTaskDelay(600000/portTICK_RATE_MS);
		if(hnvs)
		{
			int32_t current_time_stamp = get_time_stamp();
			#ifdef DEBUG_NVS
				printf("NVS: CURRENT TIMESTAMP: %d", current_time_stamp);
			#endif
			esp_err_t err = nvs_set_i32(hnvs, "timestamp", current_time_stamp);
			if(err != ESP_OK)
			{
			#ifdef DEBUG_NVS
				printf("NVS: ERROR SAVE TIMESTAMP");
			#endif
			}
			else
			{
				err = nvs_commit(hnvs);
				#ifdef DEBUG_NVS
				if(err != ESP_OK)
					printf("NVS: ERROR COMMIT TIMESTAMP");
				#endif
			}
		}
	}
}

void generate_id()
{
	uint8_t* mac = (uint8_t*)malloc(6);
	if(mac)
	{
		memset(mac, 0, 6);
		if(esp_efuse_mac_get_default(mac) == ESP_OK)
		{
			for (byte i = 0; i < 6; ++i)
			{
				char buf[3];
				sprintf(buf, "%02X", (char)mac[i]);
				id_controller += buf;
			}
		}
		else
			id_controller += String("NONAME");
		free(mac);
	}
}

void update_over_gsm_http();

static void cci_task(void*)
{
	while(1)
	{
	#ifdef USE_USB_SERIAL
		if(Serial.available())
	#else
		if(portcci.available())
	#endif
		{
			xSemaphoreTakeFromISR(xSemaphore, NULL);
			vTaskDelay(600000 / portTICK_RATE_MS);
			// any_custom_function();
			xSemaphoreGiveFromISR(xSemaphore, NULL);
		}
		// if(modem.gsm_init()) update_over_gsm_http();
		if(gsm_no_problem && download_firmware)
			update_over_gsm_http();
	// #ifdef DEBUG_WROVER_LCD
		// printf("\nID_CONTROLLER = %s\n", id_controller.c_str());
		// vTaskDelay(5000 / portTICK_RATE_MS);
	// #endif
	}
	if(hnvs)
		nvs_close(hnvs);
	vTaskDelete(NULL);
}

// time_t _check_timestamp_from_http()
// {
	// last_time_stamp_sended = gsm_get_time_from_http();
	// if(last_time_stamp_sended < default_current_time_stamp+1)
	// {
		// last_time_stamp_sended = default_current_time_stamp+1;
	// }
	// set_time_stamp(last_time_stamp_sended);
	// return last_time_stamp_sended;
// }

time_t check_timestamp_from_http()
{
	time_t time_array[]  = {gsm_get_time_from_http(), get_time_stamp(), default_current_time_stamp, last_time_stamp_sended, last_time_stamp};
	last_time_stamp_sended = get_max_time_stamp(time_array);
#ifdef DEBUG_MAIN
	printf("\n%ld\t%ld\t%ld\t%ld\t%ld\n", time_array[0], time_array[1], time_array[2], time_array[3], time_array[4]);
	printf("\nCHECK_TIMESTAMP_FROM_HTTP (last_time_stamp_sended)\n");
#endif
	nvs_check_timestamp(last_time_stamp_sended, true);
	return last_time_stamp_sended;
}

void gsm_reboot()
{
#ifdef DEBUG_MAIN
	printf("\nGSM_REBOOT\n");
#endif
	modem.disconnect();
	modem.shutdown(true);
	// led_on(false);
	gsm_no_problem = modem.gsm_init();
}

void gsm_send_signal(bool get_local_timestamp)
{
	if(get_local_timestamp) last_time_stamp = get_time_stamp();
	if((last_time_stamp - start_point_time_stamp_dbm_signal) > UPDATE_PERIOD_FOR_TIME_STAMP_DBM_SIGNAL)
	{
		if(!gsm_no_problem)
			start_point_time_stamp_dbm_signal = last_time_stamp;
		else
		{
			start_point_time_stamp_dbm_signal = check_timestamp_from_http();
			// gsm_send_http("GSM_"+String(modem.gsm_rssi)+"_dBm|"+String(start_point_time_stamp_dbm_signal)+'|'+id_controller, "sys");
			// gsm_send_http("GSM_"+String(modem.gsm_ber)+"_Percent|"+String(start_point_time_stamp_dbm_signal+1)+'|'+id_controller, "sys");
			gsm_send_http("GSM_"+String(modem.gsm_rssi*10+modem.gsm_ber)+"_dBm|"+String(start_point_time_stamp_dbm_signal)+'|'+id_controller, "sys");
			if(start_point_time_stamp_dbm_signal > last_time_stamp_sended) last_time_stamp_sended = start_point_time_stamp_dbm_signal;
		}
		///////////////////////////////////////////////////////////
		download_firmware = gsm_has_firmware_from_http();
		uint8_t trying_download_firmware = 0;
		while(download_firmware && trying_download_firmware < 15)
		{//FULL TIMEOUT 15 minuts
			vTaskDelay(60000/portTICK_RATE_MS);//1 minuts
			trying_download_firmware++;
		}
		if(download_firmware)
		{
			// modem.disconnect();
			// modem.shutdown();
			// led_on(false);
			esp_restart();
		}
	}
}

void gsm_send_firmware_version()
{
	gsm_send_http("FIRMWARE_"+String(FIRMWARE_VERSION)+"|"+String(get_time_stamp())+'|'+id_controller, "sys");
}

void gsm_send_imei()
{
	char* imei = (char*)malloc(20);//866050038354138
	if(imei)
	{
		memset(imei, 0, 20);
		if(modem.IMEI(imei))
			gsm_send_http("IMEI_"+String(imei)+"|"+String(get_time_stamp())+'|'+id_controller, "sys");
			// gsm_send_http("D_IMEI_"+String(imei)+"|"+String(get_time_stamp())+'|'+id_controller);
		free(imei);
	}
}

void gsm_send_id_sim_card()
{
	char* cimi = (char*)malloc(20);//250997269525682
	if(cimi)
	{
		memset(cimi, 0, 20);
		if(modem.CIMI(cimi))
			gsm_send_http("SIM_"+String(cimi)+"|"+String(get_time_stamp())+'|'+id_controller, "sys");
			// gsm_send_http("D_SIM_"+String(cimi)+"|"+String(get_time_stamp())+'|'+id_controller);
		free(cimi);
	}
}

void gsm_send_location()
{
	char* lat = (char*)malloc(12);
	char* lon = (char*)malloc(12);
	char* dt = (char*)malloc(12);
	char* tm = (char*)malloc(8);
	if(lat && lon && dt && tm)
	{
		memset(lat, 0, 12);memset(lon, 0, 12);memset(dt, 0, 12);memset(tm, 0, 8);
		if(modem.location(lat, lon, dt, tm))
		{
			gsm_send_http("LOCATION_"+String(lat)+","+String(lon)+","+String(dt)+","+String(tm)+"|"+String(get_time_stamp())+'|'+id_controller, "sys");
			// gsm_send_http("D_LOCATION_"+String(lat)+","+String(lon)+","+String(dt)+","+String(tm)+"|"+String(get_time_stamp())+'|'+id_controller);
			last_time_stamp_sended = str_date_time_to_timestamp(dt, tm);
			last_time_stamp = get_time_stamp();
		#ifdef DEBUG_MAIN
			printf("\n===TIME STAMP===\n(last_time_stamp_sended) %d > %d (last_time_stamp)\n===END===\n", last_time_stamp_sended, last_time_stamp);
		#endif
			if(last_time_stamp_sended > last_time_stamp)
				nvs_check_timestamp(last_time_stamp_sended, true);// set_time_stamp(last_time_stamp_sended);
			else if(last_time_stamp_sended < last_time_stamp)
				last_time_stamp_sended = last_time_stamp;
		#ifdef DEBUG_MAIN
			printf("\n===LOCATION===\n%s\n%s\n%s\n%s\n%d\n===END===\n", lat, lon, dt, tm, last_time_stamp_sended);
		#endif
		}
		free(lat); free(lon); free(dt); free(tm);
	}
}

void update_over_gsm_http()
{
#ifdef DEBUG_OTA
	printf("...STARTING UPDATE FIRMWARE OVER GSM HTTP...\n");
#endif
	if(!modem.connect(REMOTE_HOST, REMOTE_PORT))
	{
	#ifdef DEBUG_OTA
		printf("ESP_OTA: connect to %s:%d fail\n", REMOTE_HOST, REMOTE_PORT);
	#endif
	}
	else
	{
		size_t contentLength = 0;
		FILE* f = fopen("/sdcard/update.bin", "wb");
		TinyGsm mdm(modem._serial);
		mdm.factoryDefault();
		TinyGsmClient client(mdm);
		if(!client.connect(REMOTE_HOST, REMOTE_PORT))
		{
		#ifdef DEBUG_OTA
			printf("CONNECT FAIL\n");
		#endif
			return;
		}
		client.print(String("GET /api/firmware/") + FIRMWARE_VERSION + "/" + id_controller + ".bin HTTP/1.0\nHost:" + REMOTE_HOST + "\nConnection:keep-alive\n\n");
		long timeout = millis();
		while(client.available() == 0)
		{
			if(millis() - timeout > 5000L)
			{
			#ifdef DEBUG_OTA
				printf(">>> Client Timeout !\n");
			#endif
				client.stop();
				delay(10000L);
				return;
			}
		}
		while(client.available())
		{
			String line = client.readStringUntil('\n');
			line.trim();
		#ifdef DEBUG_OTA
			printf("LINE:%s\n", line.c_str());
		#endif
			line.toLowerCase();
			if(line.startsWith("content-length:"))
			{
				contentLength = line.substring(line.lastIndexOf(':') + 1).toInt();
			}
			else if(line.length() == 0)
			{
				break;
			}
		}
	#ifdef DEBUG_OTA
		printf("READING RESPONSE DATA = %d\n", contentLength);
	#endif
		timeout = millis();
		uint32_t iterations = 0, iteration = 0;
		size_t BREAK_ITERATION = OTA_BUFFSIZE - 1;
		char c = 0;
		while(iterations < contentLength && client.connected())
		{
			while(client.available())
			{
				c = client.read();
				fprintf(f, "%c", c);
				if(iteration == BREAK_ITERATION)
				{
					iterations += OTA_BUFFSIZE;
				#ifdef DEBUG_OTA
					printf("0: %d / %d\n", OTA_BUFFSIZE, iterations);
				#endif
					iteration = 0;
				}
				else
					iteration++;
			}
			if(iteration > 0)
			{
				iterations += iteration;
			#ifdef DEBUG_OTA
				printf("1: %d / %d\n", iteration, iterations);
			#endif
				iteration = 0;
			}
		}
	#ifdef DEBUG_OTA
		printf("%d\n", iterations);
	#endif
		fclose(f);
		if(iterations == contentLength)
		{
		#ifdef DEBUG_OTA
			printf("Prepare to restart system!\n");
		#endif
			modem.shutdown(true);
			esp_restart();
		}
		modem.disconnect();
	}
	download_firmware = false;
}

void nvs_sync_with_server(void* params)
{
#ifdef DEBUG_NVS
	printf("NVS: **********************\n");
	print_log("NVS: START GSM TASK\n");
	printf("NVS: **********************\n");
#endif
	// while(xSemaphoreTakeFromISR(xSemaphore, NULL) != pdPASS) {vTaskDelay(3000 / portTICK_RATE_MS);}
	gsm_no_problem = modem.gsm_init();
	if(gsm_no_problem)
	{
	#ifdef DEBUG_NVS
		printf("\nNVS: **********************\n");
		print_log("NVS: GSM NO PROBLEM\n");
		printf("NVS: **********************\n");
	#endif
		//// gsm_tcp_send_file();
		// String host = "testftp.ru";//String(REMOTE_HOST);
		// String file_name = String("update.bin");//id_controller + ".bin";
		// String path = String("/test/");//String(FIRMWARE_VERSION) + "/firmware/";
		// if(modem.init_ftp(host.c_str(), 21, "esp", "pass", file_name.c_str(), path.c_str()))
		// {
			// modem.update_esp_ftp();
	// #ifdef DEBUG_NVS
			// printf("\nNVS: **********************\n");
			// printf("NVS: FTP INIT SUCCESS\n");
			// printf("NVS: **********************\n");
	// #endif
		// }
	// #ifdef DEBUG_NVS
		// else
		// {
			// printf("\nNVS: **********************\n");
			// printf("NVS: FTP INIT ERROR\n");
			// printf("NVS: **********************\n");
		// }
	// #endif
		last_time_stamp = check_timestamp_from_http();
		// gsm_send_signal(false);
		gsm_send_firmware_version();
		gsm_send_imei();
		gsm_send_id_sim_card();
		gsm_send_location();
	}
	else
	{
		if(last_time_stamp_sended < default_current_time_stamp)
			last_time_stamp_sended = default_current_time_stamp;
		set_time_stamp(last_time_stamp_sended);
	#ifdef DEBUG_NVS
		print_log("NVS: GSM PROBLEM\n");
	#endif
	}
	global_current_day = get_day_of_month();
	// xSemaphoreGiveFromISR(xSemaphore, NULL);
	if(hnvs)
	{
		int time_counter = 0, water_counter = 0;
		size_t required_size = 0;
		esp_err_t err = ESP_OK;
		bool record_exist = false;
		char* key = (char*)malloc(NVS_KEY_SIZE);
		char* data = (char*)malloc(CCI_BUF_SIZE);
		char* full_data = (char*)malloc(CCI_BUF_SIZE);
		setup_water_quality_pins();
		time_stamp_reboot_gsm = last_time_stamp_sended;
		while(1)
		{
		#ifdef DEBUG_NVS
			print_log("NVS: CASH DATA COUNTER: %d\n", nvs_data_counter);
		#endif
			if(!gsm_no_problem)//DELAY  IF  GSM  PROBLEM
			{
			#if defined(DEBUG_CCI) || defined(DEBUG_NVS) || defined(DEBUG_DB) || defined(DEBUG_GSM) || defined(DEBUG_WATER)
				vTaskDelay(3000/portTICK_RATE_MS);
			#else
				vTaskDelay(60000/portTICK_RATE_MS);
			#endif
				gsm_no_problem = modem.gsm_init();
			}
			for(int32_t i = 0; i < NVS_MAX_DATA_COUNTER; ++i)
			{
			#ifdef INCLUDE_WATER
				if(gsm_no_problem)
				{
					if(has_water_sensor)
					{
						if(send_ppm)
						{
							send_ppm = false;
							gsm_send_water_quality_via_http(current_middle_water_value);
						#ifdef DEBUG_WATER
							print_log("\n=== WATER SENDED TO SERVER ===\n");
						#endif
						}
						if(water_counter > 1000)
						{
							water_counter = 0;
							if(water_current_item < PPM_COUNT)
								read_ppm();
							else
							{
								current_middle_water_value = get_water_quality_value();
							#ifdef DEBUG_WATER
								print_log("\n=== START SEND PPM: %ld (count %d) last %ld ===\n", current_middle_water_value, water_current_item, ppms[water_current_item-1]);
							#endif
								water_current_item = 0;
								send_ppm = true;
							}
						}
						water_counter++;
					}
				}
			#endif
			#ifdef DEBUG_NVS
				print_log("NVS: LOOP INDEX: %d\n", i);
			#endif
				while(xSemaphoreTakeFromISR(xSemaphore, NULL) != pdPASS){vTaskDelay(2000/portTICK_RATE_MS);}
				// printf("\nCLEAR DATA %s\n", data);
				memset(data, 0, CCI_BUF_SIZE);
				vTaskDelay(100/portTICK_RATE_MS);
				sprintf(key, "%d", i);
				err = nvs_get_str(hnvs, key, data, &required_size);
				// printf("\nNVS_GET_STR %d (REQUIRED_SIZE %d)\n", (err == ESP_OK), required_size);
				if(err == ESP_OK && required_size > 0)
				{
				#ifdef DEBUG_NVS
					print_log("NVS: CACHE KEY: %d | DATA: %s\n", i, data);
				#endif
					String d(data);
					if(d.length() > 1)
					{
					#ifdef DB_ENABLE
						db_write_record(d);
					#endif
						if(gsm_no_problem)
						{
							memset(full_data, 0, CCI_BUF_SIZE);
							sprintf(full_data, "%s|%s", data, id_controller.c_str());
							record_exist = gsm_send_http(full_data);
							vTaskDelay(500/portTICK_RATE_MS);
						}
						else
							record_exist = true;
					}
					else
						record_exist = true;
					if(record_exist)
					{
						record_exist = false;
						err = nvs_erase_key(hnvs, key);
					}
					else
					{
						vTaskDelay(5000/portTICK_RATE_MS);
					#ifdef DEBUG_NVS
						printf("\nNVS: 1 SEND KEY RESULT = %d\n", record_exist);
					#endif
						record_exist = gsm_send_http(full_data);
						vTaskDelay(5000/portTICK_RATE_MS);
						if(!record_exist)
						{
						#ifdef DEBUG_NVS
							printf("\nNVS: 2 SEND KEY RESULT = %d\n", record_exist);
						#endif
							record_exist = gsm_send_http(full_data);
							vTaskDelay(5000/portTICK_RATE_MS);
							if(!record_exist)
							{
							#ifdef DEBUG_NVS
								printf("\nNVS: 3 SEND KEY RESULT = %d\n", record_exist);
							#endif
								record_exist = gsm_send_http(full_data);
							}
						#ifdef DEBUG_NVS
							printf("\nNVS: LAST GSM SEND KEY = %d; RESULT = %d\n", i, record_exist);
						#endif
						}
						err = nvs_erase_key(hnvs, key);
					#ifdef DEBUG_NVS
						print_log("\nNVS: KEY = %d DELETED\n", i);
					#endif
						record_exist = false;
					}
					if(time_counter > 100)
					{
						time_counter = 0;
						err = nvs_set_i32(hnvs, "timestamp", get_time_stamp());
					#ifdef DEBUG_NVS
						printf("\nNVS: SAVE CURRENT TIMESTAMP = %d\n", get_time_stamp());
					#endif
					}
					time_counter++;
				}
			// #ifdef DEBUG_NVS
				// else
				// {
					// printf("NVS CASH KEY: %d NOT EXISTS\n", i);
				// }
			// #endif
				memset(key, 0, NVS_KEY_SIZE);
				xSemaphoreGiveFromISR(xSemaphore, NULL);
			}
			vTaskDelay(3000/portTICK_RATE_MS);
			gsm_send_signal(true);
		#ifdef DEBUG_MAIN
			print_log("\nPERIOD_REBOOT_GSM %d | last_time_stamp_sended %d\n", time_stamp_reboot_gsm, last_time_stamp_sended);
		#endif
			if((last_time_stamp_sended - time_stamp_reboot_gsm) > PERIOD_REBOOT_GSM)
			{
				time_stamp_reboot_gsm = last_time_stamp_sended;
			#ifdef DEBUG_MAIN
				print_log("\nPERIOD_REBOOT_GSM %d\n", time_stamp_reboot_gsm);
			#endif
				gsm_reboot();
			}
		#ifdef DB_ENABLE
			if(gsm_no_problem)
			{
				if((last_time_stamp - start_point_time_stamp_check_db_sync) > UPDATE_PERIOD_FOR_CHECK_DB_SYNCHRONIZATION)
				{
					start_point_time_stamp_check_db_sync = get_time_stamp();
					db_sync();
				}
			}
		#endif
		#ifdef DEBUG_NVS
			print_log("NVS RAM left %d\n", esp_get_free_heap_size());
			print_log("NVS task stack: %d\n", uxTaskGetStackHighWaterMark(NULL));
		#endif
		}
		free(key);
		free(data);
		free(full_data);
	}
	vTaskDelete(NULL);
}

extern "C" void app_main()
{
#ifdef DEBUG_MAIN
	print_log("\nFIRMWARE_VERSION = %s", FIRMWARE_VERSION);
	#ifndef DEBUG_WROVER_LCD
		printf("\n");
	#endif
#endif
	generate_id();
#ifdef DEBUG_MAIN
	print_log("ID_CONTROLLER = %s", id_controller.c_str());
	#ifndef DEBUG_WROVER_LCD
		printf("\n");
	#endif
#endif
	init_spi();
	xSemaphore = xSemaphoreCreateMutex();
	initArduino();
#ifdef USE_USB_SERIAL
	Serial.begin(9600);//, SERIAL_8N1, GPIO_NUM_3, GPIO_NUM_1);
#else
	portcci.begin(9600, SERIAL_8N1, GPIO_NUM_33, GPIO_NUM_32);
	// portcci.begin(9600, SERIAL_8N1, GPIO_NUM_9, GPIO_NUM_10);
#endif
#ifdef DEBUG_WROVER_LCD
	wroverlcd.begin();
	wroverlcd.setRotation(1);
	wroverlcd.fillScreen(WROVER_BLACK);
	wroverlcd.setTextColor(WROVER_GREEN);
	wroverlcd.setTextSize(1);
#endif
	nvs_first_init(0);
	if(check_sd_card())
	{
		ota_from_sd_card();
		sd_create_system_info(id_controller + "\nFIRMWARE_" + FIRMWARE_VERSION);
	}
	xTaskCreatePinnedToCore(&nvs_sync_with_server, "SyncTask", stack_size_sync, NULL, 0, NULL, 1);
	xTaskCreatePinnedToCore(&cci_task, "CciUartTask", stack_size_cci, NULL, 32, NULL, 0);
#ifdef DEBUG_MAIN
	print_log("BOOT: FREE MEMORY MINIMUM = %d.", esp_get_minimum_free_heap_size());
	#ifndef DEBUG_WROVER_LCD
		printf("\n");
	#endif
	print_log("BOOT: FREE MEMORY = %d.", esp_get_free_heap_size());
	#ifndef DEBUG_WROVER_LCD
		printf("\n");
	#endif
#endif
}
