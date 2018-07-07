#ifndef _WATER_SENSOR_H_
#define _WATER_SENSOR_H_

// #define SENSOR_VIA_TRANSISTOR
// #define DEBUG_WATER

#ifdef DEBUG_WATER
#define PPM_TIMEOUT_READ 1200
#define PPM_TIMEOUT_SEND 96000
#else
#define PPM_TIMEOUT_READ 600000//10minuts
#define PPM_TIMEOUT_SEND 24000000//4hours
#endif

#define MAX_VOLTAGE 4095
#define PPM_COUNT PPM_TIMEOUT_SEND/PPM_TIMEOUT_READ+1
#define THRESHOLD 3

#include "freertos/timers.h"

static bool has_water_sensor = false;
// static const float resolution = voltage / 1024;
// static const float resistorValue = 10000.0;

int water_current_item = 0;
long current_middle_water_value = 0;
long ppms[PPM_COUNT];
TimerHandle_t timer_ppm;

#ifdef SENSOR_VIA_TRANSISTOR
bool hasWaterSensor()
{
	pinMode(GPIO_NUM_34, OUTPUT);
	digitalWrite(GPIO_NUM_34, 1);
	delay(100);
	pinMode(GPIO_NUM_34, INPUT);
	return !digitalRead(GPIO_NUM_34);
}
#endif

long middle_sum_array()
{
	long sum=0;
	long items = 0;
	for(int i=0; i<PPM_COUNT; ++i)
	{
		if(ppms[i])
		{
			items++;
			sum = sum + ppms[i];
		// #ifdef DEBUG_WATER
			// printf("\n=== PPM[%d]>0: %ld | sum %ld ===\n", i, ppms[i], sum);
		// #endif
		}
		// else
		// {
		// #ifdef DEBUG_WATER
			// printf("\n=== ppm[%d]<0: %ld | sum %ld ===\n", i, ppms[i], sum);
		// #endif
		// }
	}
	if(sum < 0) sum = sum * (-1);
	return(sum/items);
}

void read_ppm()
{
	// pinMode(GPIO_NUM_34, OUTPUT);
	// digitalWrite(GPIO_NUM_34, 1);
	// pinMode(GPIO_NUM_34, INPUT);
	// has_water_sensor = !digitalRead(GPIO_NUM_34);
	// pinMode(GPIO_NUM_34, OUTPUT);
	// digitalWrite(GPIO_NUM_34, 0);
	has_water_sensor = true;
#ifdef DEBUG_WATER
	printf("\n=== WATER HAS SENSOR %d (%d) ===", has_water_sensor, water_current_item);
#endif
	if(!has_water_sensor){/*xTimerStop(timer_ppm, 0);*//*xTimerDelete(timer_ppm, 0);*/return;}
	// int analogValue=MAX_VOLTAGE - analogRead(GPIO_NUM_35);
	// int oldAnalogValue=1000;
	// float returnVoltage=0.0;
	// float resistance=0.0;
	// double dresistance;
	// while(((oldAnalogValue - analogValue) > THRESHOLD) || (oldAnalogValue < 50))
	// {
		// oldAnalogValue = analogValue;
		// analogValue = MAX_VOLTAGE - analogRead(GPIO_NUM_35);
	// }
// #ifdef DEBUG_WATER
	// printf("\n=== READ PPM ANALOG %d (%d) ===\n", analogValue, water_current_item);
// #endif
	// returnVoltage = voltage - (analogValue * resolution); 
	// resistance = ((5.00 * resistorValue) / returnVoltage) - resistorValue;
	// dresistance = 1.0 / (resistance / 1000000);
	// if(150 <= dresistance && dresistance <= 800)
		// dresistance += 120;
	// else if(801 <= dresistance ||  dresistance >= 1900)
		// dresistance += 300;
	// ppms[water_current_item] = long(500 * (dresistance / 1000) * 1000000);
	vTaskDelay(2000/portTICK_RATE_MS);
	pinMode(GPIO_NUM_32, OUTPUT);
	digitalWrite(GPIO_NUM_32, 3);
	vTaskDelay(1000/portTICK_RATE_MS);
	pinMode(GPIO_NUM_32, INPUT);
	ppms[water_current_item] = MAX_VOLTAGE - analogRead(GPIO_NUM_32);
	pinMode(GPIO_NUM_32, OUTPUT);
	digitalWrite(GPIO_NUM_32, 0);
	// ppms[water_current_item] = MAX_VOLTAGE - analogRead(GPIO_NUM_35);
#ifdef DEBUG_WATER
	printf("\n=== READ PPM[%d]: %ld ===\n", water_current_item, ppms[water_current_item]);
#endif
	water_current_item++;
}

long get_water_quality_value()
{
#ifdef DEBUG_WATER
	printf("\n=== GET MIDDLE WATER VALUE: %ld ===\n", middle_sum_array());
#endif
	return middle_sum_array();
}

void vTimerPPMCallback(TimerHandle_t xTimer)
{
#ifdef DEBUG_WATER
	printf("\n=== vTimerPPMCallback has_water_sensor %d ===\n", has_water_sensor);
#endif
	if(water_current_item < PPM_COUNT)
		read_ppm();
	else
	{
		current_middle_water_value = get_water_quality_value();
	#ifdef DEBUG_WATER
		printf("\n=== START SEND PPM: %ld (count %d) last %ld ===\n", current_middle_water_value, water_current_item, ppms[water_current_item-1]);
	#endif
		// save_to_queue(String(water_current_item) + '|' + String(last_time_stamp_sended) + '|' + id_controller);
		water_current_item = 0;
		send_ppm = 1;
	}
	// if(!has_water_sensor){xTimerStop(timer_ppm, 0);xTimerDelete(timer_ppm, 0);}
}

void setup_water_quality_pins()
{
#ifdef SENSOR_VIA_TRANSISTOR
	has_water_sensor = hasWaterSensor();
#else
	pinMode(GPIO_NUM_34, INPUT);
	has_water_sensor = !digitalRead(GPIO_NUM_34);
#endif
#ifdef DEBUG_WATER
	printf("\n=== HAS WATER SENSOR: %d ===\n", has_water_sensor);
#endif
	// if(1)
	if(has_water_sensor)
	{
		digitalWrite(GPIO_NUM_32, 1);
		pinMode(GPIO_NUM_32, INPUT);
		current_middle_water_value = MAX_VOLTAGE - analogRead(GPIO_NUM_32);
		pinMode(GPIO_NUM_32, OUTPUT);
		digitalWrite(GPIO_NUM_32, 0);
	#ifdef DEBUG_WATER
		printf("\n=== WATER SENSOR VALUE: %ld ===\n", current_middle_water_value);
	#endif
		timer_ppm = xTimerCreate("TimerPPM", PPM_TIMEOUT_READ/portTICK_RATE_MS, pdTRUE, (void*)0, vTimerPPMCallback);
		if(timer_ppm == NULL)
		{
		#ifdef DEBUG_WATER
			printf("\nWATER ERROR: The PPM timer was not created.\n");
		#endif
		}
		else
		{
			if(xTimerStart(timer_ppm, 0) != pdPASS)
			{
			#ifdef DEBUG_WATER
				printf("\nWATER ERROR: The PPM timer could not be set into the Active state.\n");
			#endif
			}
		}
	}
}

#endif
