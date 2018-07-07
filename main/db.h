#ifndef _DB_H_
#define _DB_H_

// #define DEBUG_DB

#ifdef DEBUG_DB
#define UPDATE_PERIOD_FOR_CHECK_DB_SYNCHRONIZATION 15//15 seconds
#else
#define UPDATE_PERIOD_FOR_CHECK_DB_SYNCHRONIZATION 300//5 minuts
#endif

FILE* dbfile = NULL;

int db_size()
{
	int result = 0;
	current_data_file_name = String("/sdcard/db/")+DateNowAsString()+String(".txt");
	FILE* f = fopen(current_data_file_name.c_str(), "r");
	if(f)
	{
		char *line = (char*)malloc(256);
		if(line)
		{
			memset(line, 0, 256);
			while(fgets(line, 256, f) != NULL)
			{
			#ifdef DEBUG_DB
				printf("DB: LINE FROM DB FILE %s\n", line);
			#endif
				if(strlen(line))
					result++;
				memset(line, 0, 256);
			}
			free(line);
		}
		fclose(f);
	}
#ifdef DEBUG_DB
	else
		printf("DB: ERROR OPEN FILE %s\n", current_data_file_name.c_str());
#endif
	return result;
}

void db_file_open()
{
	if(has_sd_card)
	{
		if(stat("/db", &st) != 0)
			mkdir("/sdcard/db", 0);
		current_data_file_name = String("/sdcard/db/")+DateNowAsString()+String(".txt");
		const char* file_name = current_data_file_name.c_str();
		dbfile = fopen(file_name, "a");
		// setvbuf(dbfile, NULL, _IONBF, 0);//No buffering: No buffer is used. Each I/O operation is written as soon as possible. In this case, the buffer and size parameters are ignored.
		// setvbuf(dbfile, NULL, _IOFBF, 32);//Full buffering: On output, data is written once the buffer is full (or flushed). On Input, the buffer is filled when an input operation is requested and the buffer is empty.
		// setvbuf(dbfile, NULL, _IOLBF, 32);//Line buffering: On output, data is written when a newline character is inserted into the stream or when the buffer is full (or flushed), whatever happens first. On Input, the buffer is filled up to the next newline character when an input operation is requested and the buffer is empty.
		// struct stat st;
		// if(stat(file_name, &st) == 0)
		// {
			// dbfile = fopen(file_name, "rb+");
			// fseek(dbfile, 0, SEEK_END);
		// }
		// else
			// dbfile = fopen(file_name, "w");
	#ifdef DEBUG_DB
		if(dbfile == NULL)
		{printf("DB: Not open file %s for writing\n", file_name);}
		else
		{printf("DB: File %s opened for writing\n", file_name);}
	#endif
	}
#ifdef DEBUG_DB
	else
		printf("\nDB: SD CARD NOT FOUND\n");
#endif
}

void db_write_record(String data)
{
	FILE* db_file = NULL;
	if(!db_file)
	{
		if(stat("/db", &st) != 0)
			mkdir("/sdcard/db", 0);
		current_data_file_name = String("/sdcard/db/")+DateNowAsString()+String(".txt");
		db_file = fopen(current_data_file_name.c_str(), "a");
		// setvbuf(db_file, NULL, _IONBF, 0);//No buffering: No buffer is used. Each I/O operation is written as soon as possible. In this case, the buffer and size parameters are ignored.
	#ifdef DEBUG_DB
		if(db_file)
		{printf("DB: File %s opened for writing\n", current_data_file_name.c_str());}
		else
		{
			perror(("fopen() "+current_data_file_name).c_str());
			printf("DB: Not open file %s for writing\n", current_data_file_name.c_str());
		}
	#endif
	}
#ifdef DEBUG_DB
	printf("DB: WRITE (%s) TO %s (%p)\n", data.c_str(), current_data_file_name.c_str(), db_file);
#endif
	if(db_file)
	{
		// fseek(db_file, 0, SEEK_END);
		fprintf(db_file, "%s\n", data.c_str());
		// fgets((char*)data.c_str(), data.length(), db_file);
		// vTaskDelay(1000 / portTICK_RATE_MS);
		// fflush(db_file);
		fclose(db_file);
	}
#ifdef DEBUG_DB
	else
		printf("DB: ERROR OPEN FILE %s = %p\n", current_data_file_name.c_str(), db_file);
#endif
}

void db_sync()
{
	int32_t current_day = get_day_of_month();
#ifdef DEBUG_DB
	if(global_current_day != current_day-1)
#else
	if(global_current_day != current_day)
#endif
	{
		String yesterday_data_file_name = String("/sdcard/db/")+YesterdayAsString()+String(".txt");
	#ifdef DEBUG_DB
		printf("\nDB: CHECK IF EXISTS SYNC FILE %s\n", yesterday_data_file_name.c_str());
	#endif
		FILE* f = fopen(yesterday_data_file_name.c_str(), "r");
		if(f)
		{
		#ifdef DEBUG_DB
			printf("DB: START SYNC FILE %s\n", yesterday_data_file_name.c_str());
		#endif
			char *line = (char*)malloc(256);
			if(line)
			{
				memset(line, 0, 256);
				while(fgets(line, 256, f) != NULL)
				{
					clear_string(line);
				#ifdef DEBUG_DB
					printf("DB: LINE FROM DB FILE %s\n", line);
				#endif
					if(strlen(line))
					{
						int i = 0;
					#ifdef DEBUG_DB
						printf("DB: TRY SEND %d\n", i);
					#endif
						while(!gsm_send_http(String(line)+"|"+id_controller) && i < 3)
						{
							vTaskDelay(1000 / portTICK_RATE_MS);
							i++;
						#ifdef DEBUG_DB
							printf("DB: TRY SEND %d\n", i);
						#endif
						}
					#ifdef DEBUG_DB
						printf("DB: LINE SENDED OVER %d TRYING\n", i);
					#endif
					}
					memset(line, 0, 256);
				}
				free(line);
			}
			fclose(f);
			String new_yesterday_data_file_name = String("/sdcard/db/")+YesterdayAsString()+String(".syn");
		#ifdef DEBUG_DB
			int res = rename(yesterday_data_file_name.c_str(), new_yesterday_data_file_name.c_str());
			if(res == 0)
				printf("DB: RENAMED FILE = %s\n", new_yesterday_data_file_name.c_str());
			else
				perror(("rename("+yesterday_data_file_name+")").c_str());
		#else
			rename(yesterday_data_file_name.c_str(), new_yesterday_data_file_name.c_str());
		#endif
		}
	#ifdef DEBUG_DB
		else
		// {
			perror(("fopen("+yesterday_data_file_name+")").c_str());
			// printf("DB: ERROR OPEN FILE %s\n", yesterday_data_file_name.c_str());
		// }
	#endif
		global_current_day = current_day;
	}
#ifdef DEBUG_DB
	else
		printf("\nDB: NOW = %d IS SAME AS GLOBAL DAY = %d\n", current_day, global_current_day);
#endif
}

#endif
