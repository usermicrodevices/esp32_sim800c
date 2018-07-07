#ifndef _GSM_TOOLS_H_
#define _GSM_TOOLS_H_

// #define DEBUG_GSM

////////////////GSM////////////////
void gsm_restart()
{
	// modem.shutdown();
	// gsm_no_problem = gsm_init();
	// gsm_reboot_try++;
	// if(gsm_reboot_try > GSM_MAX_REBOOT)
	// {
	if(last_time_stamp_sended < default_current_time_stamp + 1) last_time_stamp_sended = default_current_time_stamp + 2;
	while(xSemaphoreTakeFromISR(xSemaphore, NULL) != pdPASS){vTaskDelay(1000/portTICK_RATE_MS);}
	// hnvs.putInt("timestamp", last_time_stamp_sended);
	// hnvs.end();
	esp_err_t err = nvs_set_i32(hnvs, "timestamp", get_time_stamp());
	if(err == ESP_OK)
	{
		err = nvs_commit(hnvs);
	#ifdef DEBUG_NVS
		printf("\nNVS: COMMIT(NVS_HANDLE) %d\n", (err == ESP_OK));
	#endif
	}
	nvs_close(hnvs);
	xSemaphoreGiveFromISR(xSemaphore, NULL);
#ifdef DEBUG_GSM
	printf("GSM: Saved last time %d\n", last_time_stamp_sended);
	printf("GSM: ESP_RESTART\n");
#endif
	esp_restart();
	// }
}

const char* data_type(const char* data)
{
	switch(data[0])
	{
		case 'B':
			return "beverage";
		case 'I':
			return "beverage";
		case 'D':
			if(strstr(data, "D30"))
				return "counter";
			else
				return "data";
		default:
			return "data";
	}
	return "data";
}

void gsm_send_water_quality_via_http(long value)
{
	unsigned long int len = 0;
	String url = String(WEB_URL_API) + "/water_control/" + id_controller + '|' + String(value);
#ifdef DEBUG_GSM
	printf("GSM: SEND GET %s\n", url.c_str());
#endif
	/*uint16_t stat = */modem.HTTP_get(url.c_str(), &len);
}

bool gsm_send_http(String data, const char* type = "")
{
	bool result = false;
	unsigned long int len = 0;
	String url = String(WEB_URL_API) + String("/");
	if(strlen(type))
		url += type;
	else
		url += data_type(data.c_str());
#ifdef DEBUG_GSM
	printf("GSM: SEND POST URL = %s\n", url.c_str());
	printf("GSM: DATA = %s\n", data.c_str());
#endif
	uint16_t stat = modem.HTTP_post(url.c_str(), &len, (char*)data.c_str(), data.length());
#ifdef DEBUG_GSM
	printf("GSM: ANSWER %d (RECEIVED LENGTH = %ld)\n", stat, len);
#endif
	if(stat > http_critical_status)
	{
	#ifdef DEBUG_GSM
		printf("GSM: SECOND SEND POST %s\n", url.c_str());
	#endif
		stat = modem.HTTP_post(url.c_str(), &len, (char*)data.c_str(), data.length());
	#ifdef DEBUG_GSM
		printf("GSM: HTTP STATE %d\n", stat);
	#endif
		if(stat > http_critical_status)
			gsm_restart();
	}
	else if(stat == 200)// && len > 0)
	{
		char* buffer = (char*)malloc(16);
		if(buffer)
		{
			memset(buffer, 0, 16);
			size_t result_read = modem.HTTP_read(buffer, 0, 16);
			buffer[15] = 0;
			// Serial.print("SIM800 HTTP read = ");Serial.print(buffer);Serial.print("; received length = ");Serial.println(result_read);
		#ifdef DEBUG_GSM
			printf("GSM: HTTP_READ=%s (RESULT=%d)\n", buffer, result_read);
		#endif
			if(result_read == -1)
				gsm_restart();
			// if(len == 1 && buffer[0] == '1')
			if(buffer[0] == '1' && strlen(buffer) == 1)
				result = true;
			else if(strstr(buffer, "Record exist"))
				result = true;
			free(buffer);
		}
	}
	return result;
}

time_t gsm_read_time_from_http()
{
	time_t result = 0;
	char* buffer = (char*)malloc(16);
	if(buffer)
	{
		memset(buffer, 0, 16);
		size_t result_read = modem.HTTP_read(buffer, 0, 16);
		if(result_read == -1)
			gsm_restart();
		if(result_read > 9)
			result = (time_t)atol(buffer);
		// Serial.print("TIME HTTP read = ");Serial.print(buffer);Serial.print("; received length = ");Serial.println(result_read);
		free(buffer);
	}
	return result;
}

time_t gsm_get_time_from_http()
{
	time_t result = 0;
	unsigned long int len = 0;
	String url = String(WEB_URL_API) + "/timestamp";
	uint16_t stat = modem.HTTP_get(url.c_str(), &len);
	vTaskDelay(3000 / portTICK_RATE_MS);
	// Serial.print("TIME HTTP status = ");Serial.print(stat);Serial.print("; received length = ");Serial.println(len);
	if(stat > http_critical_status)
	{
		stat = modem.HTTP_get(url.c_str(), &len);
		// Serial.print("TIME HTTP status = ");Serial.print(stat);Serial.print("; received length = ");Serial.println(len);
		if(stat > http_critical_status)
			gsm_restart();
		else
			result = gsm_read_time_from_http();
	}
	// else if(stat == 200 && len > 0)
	else if(stat == 37 || stat == 200)
		result = gsm_read_time_from_http();
	return result;
}

bool gsm_has_firmware_from_http()
{
	unsigned long int len = 0;
	String url = String(WEB_URL_API) + "/firmware/" + FIRMWARE_VERSION + "/" + id_controller + ".bin";
	uint16_t stat = modem.HTTP_get(url.c_str(), &len);
	return (stat == 200 || stat == 602);
}

#ifdef DB_ENABLE
void gsm_tcp_send_file()
{
	current_data_file_name = String("/sdcard/db/")+DateNowAsString()+String(".txt");
	FILE* f = fopen(current_data_file_name.c_str(), "rb");
	if(f)
	{
		if(modem.connect("node2.service-pb.ru", 11111, 5000))
		{
			if(modem.status())
			{
				char* buf = (char*)malloc(GSM_MAX_BUFFSIZE);
				if(buf)
				{
					#ifdef DEBUG_GSM
						printf("GSM: START SEND FILE %s\n", current_data_file_name.c_str());
					#endif
					unsigned long int accepted = 0;
					size_t readed = fread((void*)buf, 1, GSM_MAX_BUFFSIZE, f);
					while(readed)//while(!feof(f))
					{
						if(!modem.send(buf, readed, accepted))
							break;
					#ifdef DEBUG_GSM
						else
							printf("GSM: FILE SEND %ld\r", accepted);
					#endif
						memset(buf, 0, GSM_MAX_BUFFSIZE);
						readed = fread((void*)buf, 1, GSM_MAX_BUFFSIZE, f);
					}
					free(buf);
				}
			}
			modem.disconnect();
		}
	}
}
#endif

#endif
