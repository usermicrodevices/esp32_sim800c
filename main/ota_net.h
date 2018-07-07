#ifndef _OTA_NET_H_
#define _OTA_NET_H_

#define DEBUG_OTA

#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
static char ota_write_data[BUFFSIZE + 1] = {0};
static char text[BUFFSIZE + 1] = {0};
static int binary_file_length = 0;
static int socket_id = -1;

#include "esp_ota_ops.h"

static int read_until(char *buffer, char delim, int len)
{
	int i = 0;
	while(buffer[i] != delim && i < len) ++i;
	return i + 1;
}

static bool read_past_http_header(char text[], int total_len, esp_ota_handle_t update_handle)
{
	int i = 0, i_read_len = 0;/* i means current position */
	while(text[i] != 0 && i < total_len)
	{
		i_read_len = read_until(&text[i], '\n', total_len);
		if(i_read_len == 2)// if we resolve \r\n line,we think packet header is finished
		{
			int i_write_len = total_len - (i + 2);
			memset(ota_write_data, 0, BUFFSIZE);
			memcpy(ota_write_data, &(text[i + 2]), i_write_len);/*copy first http packet body to write buffer*/
			esp_err_t err = esp_ota_write(update_handle, (const void *)ota_write_data, i_write_len);
			if(err != ESP_OK)
			{
				#ifdef DEBUG_OTA
					printf("\nOTA: Error: esp_ota_write failed! err=0x%x\n", err);
				#endif
				return false;
			}
			else
			{
				#ifdef DEBUG_OTA
					printf("\nOTA: esp_ota_write header OK\n");
				#endif
				binary_file_length += i_write_len;
			}
			return true;
		}
		i += i_read_len;
	}
	return false;
}

void ota_task()
{
	esp_err_t err;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0 ;
	const esp_partition_t *update_partition = NULL;
	#ifdef DEBUG_OTA
		printf("\nOTA: Starting OTA example...\n");
	#endif
	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();
	if (configured != running)
	{
	#ifdef DEBUG_OTA
		printf("\nOTA: Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n", configured->address, running->address);
	#endif
	}
	#ifdef DEBUG_OTA
		printf("\nOTA: Running partition type %d subtype %d (offset 0x%08x)\n",running->type, running->subtype, running->address);
	#endif
	/* Wait for the callback to set the CONNECTED_BIT in the event group.*/
	#ifdef DEBUG_OTA
		printf("\nOTA: Wait connection to Wifi ....\n");
	#endif
	// xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
	#ifdef DEBUG_OTA
		printf("\nOTA: Connect to Wifi ! Start to Connect to Server....\n");
	#endif
	// if (connect_to_http_server())/*connect to http server*/
	// {
		// #ifdef DEBUG_OTA
			// printf("\nOTA: Connected to http server\n");
		// #endif
	// }
	// else
	// {
		// #ifdef DEBUG_OTA
			// printf("\nOTA: Connect to http server failed!\n");
		// #endif
		// task_fatal_error();
	// }
	int res = -1;/*send GET request to http server*/
	// res = send(socket_id, http_request, strlen(http_request), 0);
	if (res == -1)
	{
		#ifdef DEBUG_OTA
			printf("\nOTA: Send GET request to server failed\n");
		#endif
		// task_fatal_error();
	}
	else
	{
		#ifdef DEBUG_OTA
			printf("\nOTA: Send GET request to server succeeded\n");
		#endif
	}
	update_partition = esp_ota_get_next_update_partition(NULL);
	assert(update_partition != NULL);
	#ifdef DEBUG_OTA
		printf("\nOTA: Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);
	#endif
	err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
	if (err != ESP_OK)
	{
		#ifdef DEBUG_OTA
			printf("\nOTA: esp_ota_begin failed, error=%d\n", err);
		#endif
		// task_fatal_error();
	}
	#ifdef DEBUG_OTA
		printf("\nOTA: esp_ota_begin succeeded\n");
	#endif
	bool resp_body_start = false, flag = true;
	while(flag)/*deal with all receive packet*/
	{
		memset(text, 0, TEXT_BUFFSIZE);
		memset(ota_write_data, 0, BUFFSIZE);
		int buff_len = -1;//recv(socket_id, text, TEXT_BUFFSIZE, 0);
		if(buff_len < 0)/*receive error*/
		{ 
			#ifdef DEBUG_OTA
				printf("\nOTA: Error: receive data error! errno=%d\n", errno);
			#endif
			// task_fatal_error();
		}
		else if(buff_len > 0 && !resp_body_start)/*deal with response header*/
		{
			memcpy(ota_write_data, text, buff_len);
			resp_body_start = read_past_http_header(text, buff_len, update_handle);
		}
		else if(buff_len > 0 && resp_body_start)/*deal with response body*/
		{
			memcpy(ota_write_data, text, buff_len);
			err = esp_ota_write( update_handle, (const void *)ota_write_data, buff_len);
			if(err != ESP_OK)
			{
				#ifdef DEBUG_OTA
					printf("\nOTA: Error: esp_ota_write failed! err=0x%x\n", err);
				#endif
				// task_fatal_error();
			}
			binary_file_length += buff_len;
			#ifdef DEBUG_OTA
				printf("\nOTA: Have written image length %d\n", binary_file_length);
			#endif
		}
		else if(buff_len == 0)/*packet over*/
		{
			flag = false;
			#ifdef DEBUG_OTA
				printf("\nOTA: Connection closed, all packets received\n");
			#endif
			close(socket_id);
		}
		else
		{
			#ifdef DEBUG_OTA
				printf("\nOTA: Unexpected recv result\n");
			#endif
		}
	}
	#ifdef DEBUG_OTA
		printf("\nOTA: Total Write binary data length : %d\n", binary_file_length);
	#endif
	if(esp_ota_end(update_handle) != ESP_OK)
	{
		#ifdef DEBUG_OTA
			printf("\nOTA: esp_ota_end failed!\n");
		#endif
		// task_fatal_error();
	}
	err = esp_ota_set_boot_partition(update_partition);
	if(err != ESP_OK)
	{
		#ifdef DEBUG_OTA
			printf("\nOTA: esp_ota_set_boot_partition failed! err=0x%x\n", err);
		#endif
		// task_fatal_error();
	}
	#ifdef DEBUG_OTA
		printf("\nOTA: Prepare to restart system!\n");
	#endif
	esp_restart();
	return ;
}

#endif
