#ifndef _DATE_TIME_H_
#define _DATE_TIME_H_

extern "C" {
#include <time.h>
}
#include <sys/time.h>

// #define DEBUG_DATE_TIME

// #define SEND_LOCATION_TIMEOUT 3600//1 hour
// #define SEND_LOCATION_TIMEOUT 900//15 minuts
#define UPDATE_PERIOD_FOR_TIME_STAMP_DBM_SIGNAL 30//30 seconds
// #define UPDATE_PERIOD_FOR_TIME_STAMP_DBM_SIGNAL 900//15 minuts
// #define UPDATE_PERIOD_FOR_TIME_STAMP 21600//6 hours
#define PERIOD_REBOOT_GSM 600//10 minuts
// #define PERIOD_REBOOT_GSM 3600//1 hour

#define DATE_TIME_SIZE 20
char date_time[DATE_TIME_SIZE];
time_t time_stampt;
struct tm *time_struct;
#define print_log(format, ...) memset(date_time, 0, DATE_TIME_SIZE);time_stampt = time(NULL);time_struct = localtime(&time_stampt);if(time_struct != NULL){strftime(date_time, DATE_TIME_SIZE, "%Y-%m-%d %H:%M:%S", time_struct);}printf("%s ", date_time);printf(format, ##__VA_ARGS__);

static int32_t global_current_day = -1;
int32_t start_point_time_stamp_check_db_sync = 0;
int32_t last_time_stamp = 0, start_point_time_stamp_dbm_signal = 0;
int32_t last_time_stamp_sended = 0, time_stamp_reboot_gsm = 0;
int32_t default_current_time_stamp = 1530729865UL;//1518307200
time_t coffee_last_time = 0;
time_t coffee_current_time = 0;

time_t get_max_time_stamp(time_t* arr)
{
	time_t max;
	size_t arr_size = sizeof(arr)/sizeof(*arr);
	if(arr[0] > arr[1])
		max = arr[0];
	else
		max = arr[1];
	for(size_t i=2; i<arr_size; i++)
	{
		if(arr[i] > max)
			max = arr[i];
	}
	return max;
}

time_t str_to_timestamp(const char* value)
{
	long tstamp = strtol(value, NULL, 16);
	struct tm t;
	t.tm_year = (tstamp >> 26) + 100;//2000 - 1900;
	t.tm_mon = ((tstamp >> 22) & 0xF) - 1;// Month, 0 - jan
	t.tm_mday = (tstamp >> 17) & 0x1F;// Day of the month
	t.tm_hour = (tstamp >> 12) & 0x1F;
	t.tm_min = (tstamp >> 6) & 0x2F;
	t.tm_sec = tstamp & 0x2F;
	t.tm_isdst = 1;// Is DST on? 1 = yes, 0 = no, -1 = unknown
	return mktime(&t)-10800;
}

time_t str_date_time_to_timestamp(char* sdate, char* stime)
{
	char year[] = {sdate[0], sdate[1], sdate[2], sdate[3], 0};
	char month[] = {sdate[5], sdate[6], 0};
	char day[] = {sdate[8], sdate[9], 0};
	char hours[] = {stime[0], stime[1], 0};
	char minuts[] = {stime[3], stime[4], 0};
	char seconds[] = {stime[6], stime[7], 0};
#ifdef DEBUG_DATE_TIME//1525694840-1356974970=168719870
	printf("\n===DATE===\n%s\n%s\n%s\n%s\n%s\n%s\n===END===\n", year, month, day, hours, minuts, seconds);
#endif
	struct tm t;
	t.tm_year = atoi(year);
	t.tm_mon = atoi(month);
	t.tm_mday = atoi(day);
	t.tm_hour = atoi(hours);
	t.tm_min = atoi(minuts);
	t.tm_sec = atoi(seconds);
	t.tm_isdst = 0;
#ifdef DEBUG_DATE_TIME
	printf("\n===DDATE===\n%d\n%d\n%d\n%d\n%d\n%d\n===END===\n", t.tm_year, t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
#endif
	return mktime(&t)+168719744;
}

int32_t get_day_of_month()
{
	time_t cur_time;
	struct tm* cur_tm;
	struct timeval t = {.tv_sec = 0, .tv_usec = 0};
	gettimeofday(&t, NULL);
	cur_time = t.tv_sec;
	cur_tm = localtime(&cur_time);
	return cur_tm->tm_mday;
}

int32_t get_time_stamp()
{
	struct timeval t = {.tv_sec = 0, .tv_usec = 0};
	gettimeofday(&t, NULL);
	return t.tv_sec;
}
void set_time_stamp(int32_t current_time_stamp)
{
	struct timeval t = {.tv_sec = current_time_stamp, .tv_usec = 0};
	settimeofday(&t, NULL);
}

void date_to_string(time_t t, char* buf, int bufsize)
{
	if(!t) t = time(NULL);
	struct tm *tmp = localtime(&t);
	if(tmp != NULL) strftime(buf, bufsize, "%Y%m%d", tmp);
}

String DateNowAsString()
{
	String result;
	char* d = (char*)malloc(9);
	if(d)
	{
		memset(d, 0, 9);
		date_to_string(0, d, 9);
		result = d;
		free(d);
	}
	return result;
}

void yesterday_to_string(char* buf, int bufsize)
{
	time_t t = time(NULL);
	struct tm *tmp = localtime(&t);
	tmp->tm_mday--;
	mktime(tmp); /* Normalise ts */
	if(tmp != NULL) strftime(buf, bufsize, "%Y%m%d", tmp);
}

String YesterdayAsString()
{
	String result;
	char* d = (char*)malloc(9);
	if(d)
	{
		memset(d, 0, 9);
		yesterday_to_string(d, 9);
		result = d;
		free(d);
	}
	return result;
}

void time_to_string(time_t t, char* buf, int bufsize)
{
	if(!t) t = time(NULL);
	struct tm *tmp = localtime(&t);
	if(tmp != NULL) strftime(buf, bufsize, "%Y-%m-%d_%H:%M:%S", tmp);
}

void local_time_to_string(char* buf, int bufsize)
{
	time_t t;
	struct tm *tmp;
	t = time(NULL);
	tmp = localtime(&t);
	if(tmp != NULL) strftime(buf, bufsize, "%Y-%m-%d_%H:%M:%S", tmp);
}

String coffee_time_to_string(String data)
{
	String result("DT"+data);
	char* d = (char*)malloc(20);
	if(d)
	{
		memset(d, 0, 20);
		time_to_string(str_to_timestamp(data.c_str())+10800, d, 20);
		result += "("+String(d)+")";
		free(d);
	}
	return result;
}

#endif
