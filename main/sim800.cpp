extern "C" {
#include <time.h>
}
#include <sys/time.h>
#include <Arduino.h>
#include "sim800.h"

#define sscanf_P(i, p, ...)    sscanf((i), (p), __VA_ARGS__)
#define println_param(prefix, p) print(F(prefix)); print(F(",\"")); print(p); println(F("\""));

#ifdef DEBUG_SIM800
#define print_log(format, ...) memset(date_time, 0, DATE_TIME_SIZE);time_stampt = time(NULL);time_struct = localtime(&time_stampt);if(time_struct != NULL){strftime(date_time, DATE_TIME_SIZE, "%Y-%m-%d %H:%M:%S", time_struct);}printf("%s ", date_time);printf(format, ##__VA_ARGS__);
#define PRINT(s) Serial.print(F(s))
#define PRINTLN(s) Serial.println(F(s))
#define DEBUG(...) Serial.print(__VA_ARGS__)
#define DEBUGQ(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.print("'")
#define DEBUGLN(...) Serial.println(__VA_ARGS__)
#define DEBUGQLN(...) Serial.print("'"); Serial.print(__VA_ARGS__); Serial.println("'")
#else
#define PRINT(s)
#define PRINTLN(s)
#define DEBUG(...)
#define DEBUGQ(...)
#define DEBUGLN(...)
#define DEBUGQLN(...)
#endif

sim800::sim800(){}

const char* sim800::status_description(uint16_t status)
{
	switch(status)
	{
		case 200: return "OK";
		case 404: return "PAGE NOT FOUND";
		case 600: return "Not HTTP PDU";
		case 601: return "Network Error";
		case 602: return "No memory";
		case 603: return "DNS Error";
		case 604: return "Stack Busy";
		default: return "UNKNOWN";
	}
}

void sim800::begin()
{
	if(!serial_worked)
	{
		pinMode(SIM800_KEY, OUTPUT);
		pinMode(SIM800_PS, INPUT);
		// _serial.setTimeout(50);
		_serial.setTimeout(SIM800_SERIAL_TIMEOUT);
		_serial.begin(_serialSpeed, SERIAL_8N1, SIM800_RX, SIM800_TX);
	#ifdef DEBUG_SIM800
		printf("\n_serial.begin(%d, %X, %d, %d)\n", _serialSpeed, SERIAL_8N1, SIM800_RX, SIM800_TX);
	#endif
		serial_worked = true;
	}
}

bool sim800::reset(bool flag_reboot)
{
	bool ok = false;
	if(flag_reboot)
	{
		ok = expect_AT_OK(F("+CFUN=1,1"));
		vTaskDelay(5000 / portTICK_RATE_MS);
	}
	ok = expect_AT_OK(F(""));if(!ok)expect_AT_OK(F(""));
	println(F("ATZ"));
	vTaskDelay(1000 / portTICK_RATE_MS);
	ok = expect_OK(5000);//if(!ok){println(F("ATZ"));vTaskDelay(1000 / portTICK_RATE_MS);expect_OK(5000);}
	println(F("ATE0"));
	vTaskDelay(1000 / portTICK_RATE_MS);
	ok = expect_OK(5000);if(!ok){println(F("ATE0"));vTaskDelay(1000 / portTICK_RATE_MS);ok = expect_OK(5000);}
	ok = expect_AT_OK(F("+CFUN=1"));if(!ok)expect_AT_OK(F("+CFUN=1"));
	return ok;
}

bool sim800::wakeup()
{
#ifdef DEBUG_AT
	print_log("SIM800 wakeup\n");
#endif
	bool result = false;
	// if(digitalRead(SIM800_PS) == LOW)
	// {
		if(!expect_AT_OK(F(""), 100))
		{
			bool flag_reboot = false;
			if(!expect_AT_OK(F(""), 100))// check if the chip is already awake, otherwise start wakeup
			{
			#ifdef DEBUG_AT
				print_log("SIM800 using PWRKEY wakeup procedure\n");
			#endif
				// pinMode(SIM800_KEY, OUTPUT);
				// pinMode(SIM800_PS, INPUT);
				if(digitalRead(SIM800_PS) == LOW)
				{
				#ifdef DEBUG_AT
					print_log("digitalRead(SIM800_PS) == LOW\n");
				#endif
			#ifdef USE_POWER_SWITCH
				#ifdef DEBUG_AT
					print_log("=== SIM800_POWER BEFORE ===\n");
				#endif
					pinMode(SIM800_POWER, OUTPUT);
					digitalWrite(SIM800_POWER, LOW);
				#ifdef DEBUG_AT
					print_log("=== SIM800_POWER AFTER ===\n");
				#endif
			#endif
					int trying = 0;
					do
					{
					#ifdef DEBUG_AT
						print_log("TRYING SIM800 POWER ON\n");
					#endif
						digitalWrite(SIM800_KEY, HIGH);
						vTaskDelay(3000 / portTICK_RATE_MS);
						trying++;
						if(trying > 10) break;
					} while (digitalRead(SIM800_PS) == LOW);
				}
				else
				{
				#ifdef DEBUG_AT
					print_log("digitalRead(SIM800_PS) == HIGH\n");
				#endif
					do {
					// digitalWrite(SIM800_KEY, LOW);
					// vTaskDelay(35000 / portTICK_RATE_MS);
					// digitalWrite(SIM800_KEY, HIGH);
					digitalWrite(GPIO_NUM_21, LOW);
					vTaskDelay(1100 / portTICK_RATE_MS);
					digitalWrite(GPIO_NUM_21, HIGH);
					vTaskDelay(1100 / portTICK_RATE_MS);
					digitalWrite(GPIO_NUM_21, HIGH);
					vTaskDelay(100 / portTICK_RATE_MS);
					} while (digitalRead(SIM800_PS) == HIGH);
				}
				pinMode(SIM800_KEY, INPUT_PULLUP);// make pin unused (do not leak)
			#ifdef DEBUG_AT
				print_log("SIM800 ok\n");
			#endif
			}
			else
			{
			#ifdef DEBUG_AT
				print_log("SIM800 already awake\n");
			#endif
			}
		#ifdef DEBUG_AT
			print_log("SIM800 using PWRKEY wakeup procedure END\n");
		#endif
			result = reset(flag_reboot);
		}
		else
			result = true;
	// }
#ifdef DEBUG_AT
	print_log("SIM800 using PWRKEY wakeup procedure END\n");
#endif
	return result;
}

bool sim800::shutdown(bool hard)
{
#ifdef DEBUG_AT
	print_log("SIM800 shutdown\n");
#endif
	if(hard)
	{
	#ifdef USE_POWER_SWITCH
		pinMode(SIM800_POWER, OUTPUT);
		digitalWrite(SIM800_POWER, HIGH);
		vTaskDelay(5000 / portTICK_RATE_MS);
	#else
		pinMode(SIM800_KEY, OUTPUT);
		digitalWrite(SIM800_KEY, LOW);
		vTaskDelay(1100 / portTICK_RATE_MS);
		digitalWrite(SIM800_KEY, HIGH);
		vTaskDelay(1100 / portTICK_RATE_MS);
		digitalWrite(SIM800_KEY, HIGH);
	#endif
	}
	else
	{
		bool reboot = expect_AT_OK(F("+CFUN=1,1"));
		if(reboot) vTaskDelay(5000 / portTICK_RATE_MS);
		else
		{
			if (digitalRead(SIM800_PS) == HIGH)
			{
			#ifdef DEBUG_AT
				print_log("SIM800 shutdown using PWRKEY\n");
			#endif
				pinMode(SIM800_KEY, OUTPUT);
				digitalWrite(SIM800_KEY, LOW);
				vTaskDelay(1100 / portTICK_RATE_MS);
				digitalWrite(SIM800_KEY, HIGH);
				vTaskDelay(1100 / portTICK_RATE_MS);
				digitalWrite(SIM800_KEY, HIGH);
			}
		}
	}
#ifdef DEBUG_AT
	print_log("SIM800 shutdown ok\n");
#endif
	return true;
}

void sim800::setAPN(const __FlashStringHelper *apn, const __FlashStringHelper *user, const __FlashStringHelper *pass)
{
	_apn = apn;
	_user = user;
	_pass = pass;
}

bool sim800::unlock(const __FlashStringHelper *pin)
{
	print(F("AT+CPIN="));
	println(pin);
	return expect_OK();
}

bool sim800::get_time(char *date, char *time, char *tz)
{
	println(F("AT+CCLK?"));
	return expect_scan(F("+CCLK: \"%8s,%8s%3s\""), date, time, tz);
}

bool sim800::IMEI(char *imei)
{
	println(F("AT+GSN"));
	expect_scan(F("%s"), imei);
	return expect_OK();
}

bool sim800::CIMI(char *cimi)//ID sim card
{
	println(F("AT+CIMI"));
	expect_scan(F("%s"), cimi);
	return expect_OK();
}

bool sim800::battery(uint16_t &bat_status, uint16_t &bat_percent, uint16_t &bat_voltage) {
  println(F("AT+CBC"));
  if(!expect_scan(F("+CBC: %d,%d,%d"), &bat_status, &bat_percent, &bat_voltage)) {
    Serial.println(F("BAT status lookup failed"));
  }
  return expect_OK();
}

bool sim800::location(char *&lat, char *&lon, char *&date, char *&time_value)
{
	uint16_t loc_status;
	char reply[64];
	println(F("AT+CIPGSMLOC=1,1"));
	vTaskDelay(3000 / portTICK_RATE_MS);
	if (!expect_scan(F("+CIPGSMLOC: %d,%s"), &loc_status, reply, 10000)) {
		#ifdef DEBUG_AT
		Serial.println(F("GPS lookup failed"));
		#endif
	} else {
		lon = strdup(strtok(reply, ","));
		lat = strdup(strtok(NULL, ","));
		date = strdup(strtok(NULL, ","));
		time_value = strdup(strtok(NULL, ","));
	}
	return expect_OK() && loc_status == 0 && lat && lon;
}

bool sim800::registerNetwork(uint16_t timeout)
{
#ifdef DEBUG_AT
	print_log("SIM800 waiting for network registration\n");
#endif
	expect_AT_OK(F(""));
	while (timeout -= 1000)
	{
		unsigned short int n = 0;
		println(F("AT+CREG?"));
		expect_scan(F("+CREG: 0,%hu"), &n);
	#ifdef DEBUG_PROGRESS
		switch (n)
		{
			case 0:
				PRINT("_");
				break;
			case 1:
				PRINT("H");
				break;
			case 2:
				PRINT("S");
				break;
			case 3:
				PRINT("D");
				break;
			case 4:
				PRINT("?");
				break;
			case 5:
				PRINT("R");
				break;
			default:
				DEBUG(n);
				break;
		}
	#endif
		if ((n == 1 || n == 5))
		{
		#ifdef DEBUG_PROGRESS
			PRINTLN("|");
		#endif
			return true;
		}
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	return false;
}

bool sim800::enableGPRS(uint16_t timeout)
{
	expect_AT(F("+CIPSHUT"), F("SHUT OK"), 5000);
	expect_AT_OK(F("+CIPMUX=1")); // enable multiplex mode
	expect_AT_OK(F("+CIPRXGET=1")); // we will receive manually
	bool attached = false;
	while (!attached && timeout > 0)
	{
		attached = expect_AT_OK(F("+CGATT=1"), 10000);
		vTaskDelay(1000 / portTICK_RATE_MS);
		timeout -= 1000;
	}
	if (!attached) return false;
	if (!expect_AT_OK(F("+SAPBR=3,1,\"CONTYPE\",\"GPRS\""), 10000)) return false;
	if(_apn)// set bearer profile access point name
	{
		print(F("AT+SAPBR=3,1,\"APN\",\""));
		print(_apn);
		println(F("\""));
		if (!expect_OK()) return false;
		if (_user)
		{
			print(F("AT+SAPBR=3,1,\"USER\",\""));
			print(_user);
			println(F("\""));
			if (!expect_OK()) return false;
		}
		if(_pass)
		{
			print(F("AT+SAPBR=3,1,\"PWD\",\""));
			print(_pass);
			println(F("\""));
			if (!expect_OK()) return false;
		}
	}
	expect_AT_OK(F("+SAPBR=1,1"), 30000);// open GPRS context
	print(F("+CGDCONT=1,\"IP\",\""));print(_apn);println(F("\""));
	if (!expect_OK()) return false;
	do
	{
		println(F("AT+CGATT?"));
		attached = expect(F("+CGATT: 1"));
		vTaskDelay(1000 / portTICK_RATE_MS);
	}
	while(--timeout && !attached);
	return attached;
}

bool sim800::disableGPRS()
{
	expect_AT(F("+CIPSHUT"), F("SHUT OK"));
	if (!expect_AT_OK(F("+SAPBR=0,1"), 30000)) return false;
	return expect_AT_OK(F("+CGATT=0"));
}

unsigned short int sim800::HTTP_get(const char *url, unsigned long int *length)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	if (!expect_AT_OK(F("+HTTPACTION=0"))) return 1004;
	unsigned short int status;
	expect_scan(F("+HTTPACTION: 0,%hu,%lu"), &status, &length, 60000);
	return status;
}

unsigned short int sim800::HTTP_get(const char *url, unsigned long int *length, STREAM &file)
{
	unsigned short int status = HTTP_get(url, length);
	if (length == 0) return status;
	char *buffer = (char *) malloc(SIM800_BUFSIZE);
	uint32_t pos = 0;
	do
	{
		size_t r = HTTP_read(buffer, pos, SIM800_BUFSIZE);
	#ifdef DEBUG_PROGRESS
		if((pos % 10240) == 0)
		{
			PRINT(" ");
			DEBUGLN(pos);
		}
		else if(pos % (1024) == 0)
		{PRINT("<");}
	#endif
		pos += r;
		file.write(buffer, r);
	}
	while(pos < *length);
	free(buffer);
	return status;
}

size_t sim800::HTTP_read(char *buffer, uint32_t start, size_t length)
{
	println(F("AT+HTTPREAD"));
	unsigned long int available;
	expect_scan(F("+HTTPREAD: %lu"), &available);
#ifdef DEBUG_PACKETS
	PRINT("~~~ PACKET: ");
	DEBUGLN(available);
#endif
	if(available > CRITICAL_BUFFER_HTTPREAD)
	{
		PRINT("~~~ BUFFER_HTTPREAD: ");DEBUGLN(available);
		return -1;//2148341393
	}
	size_t idx = 0;
	if(available <= length) idx = read(buffer, (size_t) available);
	else idx = read(buffer, length);
	if(!expect_OK()) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ DONE: ");
	DEBUGLN(idx);
#endif
	return idx;
}

size_t sim800::HTTP_read_ota(esp_ota_handle_t ota_handle, uint32_t start, size_t length)
{
	println(F("AT+HTTPREAD"));
	unsigned long int available;
	expect_scan(F("+HTTPREAD: %lu"), &available);
#ifdef DEBUG_PACKETS
	PRINT("~~~ OTA PACKET: ");
	DEBUGLN(available);
#endif
	if(available > CRITICAL_BUFFER_HTTPREAD)
	{
		PRINT("~~~ OTA BUFFER_HTTPREAD: ");DEBUGLN(available);
		return -1;//2148341393
	}
	size_t idx = 0;
	if(available <= length) idx = read_ota(ota_handle, (size_t)available);
	else idx = read_ota(ota_handle, length);
	if(!expect_OK()) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ OTA DONE: ");
	DEBUGLN(idx);
#endif
	return idx;
}

unsigned short int sim800::HTTP_post(const char *url, unsigned long int *length)
{
	length = 0;
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1001;
	unsigned short int status;
	expect_scan(F("+HTTPACTION: 0,%hu,%lu"), &status, &length, 60000);
	return status;
}

unsigned short int sim800::HTTP_post(const char *url, unsigned long int *length, char *buffer, uint32_t size)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	length = 0;
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	if (!expect_AT_OK(F("+HTTPPARA=\"CONTENT\",\"application/x-www-form-urlencoded\""))) return 1102;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	print(F("AT+HTTPDATA="));
	print(size);
	print(F(","));
	println((uint32_t) 3000);
	if (!expect(F("DOWNLOAD"))) return 0;
#ifdef DEBUG_PACKETS
	PRINT("~~~ '");
	DEBUG(buffer);
	PRINTLN("'");
#endif
	_serial.write((const uint8_t*)buffer, size);
	if (!expect_OK(5000)) return 1005;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1004;
	uint16_t status;
	while (!expect_scan(F("+HTTPACTION: 1,%hu,%lu"), &status, &length, 5000));// wait for the action to be completed, give it 5s for each try
	return status;
}


unsigned short int sim800::HTTP_post(const char *url, unsigned long int &length, STREAM &file, uint32_t size)
{
	expect_AT_OK(F("+HTTPTERM"));
	vTaskDelay(100 / portTICK_RATE_MS);
	if (!expect_AT_OK(F("+HTTPINIT"))) return 1000;
	if (!expect_AT_OK(F("+HTTPPARA=\"CID\",1"))) return 1101;
	// if (!expect_AT_OK(F("+HTTPPARA=\"UA\",\"UBIRCH#1\""))) return 1102;
	// if (!expect_AT_OK(F("+HTTPPARA=\"REDIR\",1"))) return 1103;
	println_param("AT+HTTPPARA=\"URL\"", url);
	if (!expect_OK()) return 1110;
	print(F("AT+HTTPDATA="));
	print(size);
	print(F(","));
	println((uint32_t) 120000);
	if (!expect(F("DOWNLOAD"))) return 0;
	uint8_t *buffer = (uint8_t *) malloc(SIM800_BUFSIZE);
	uint32_t pos = 0, r = 0;
	do
	{
		for (r = 0; r < SIM800_BUFSIZE; r++)
		{
			int c = file.read();
			if (c == -1) break;
			_serial.write((uint8_t) c);
		}

		if (r < SIM800_BUFSIZE)
		{
		#ifdef DEBUG_PROGRESS
			PRINTLN("EOF");
		#endif
			break;
		}
	#ifdef DEBUG_SIM800
		if ((pos % 10240) == 0)
		{
			PRINT(" ");
			DEBUGLN(pos);
		}
		else if(pos % (1024) == 0)
		{ PRINT(">"); }
	#endif
		pos += r;
	}
	while(r == SIM800_BUFSIZE);
	free(buffer);
	PRINTLN("");
	if (!expect_OK(5000)) return 1005;
	if (!expect_AT_OK(F("+HTTPACTION=1"))) return 1004;
	uint16_t status;
	while(!expect_scan(F("+HTTPACTION: 1,%hu,%lu"), &status, &length, 5000));// wait for the action to be completed, give it 5s for each try
	return status;
}

inline size_t sim800::read(char* buffer, size_t length, uint32_t offset)
{
	// uint32_t idx = offset;
	// while(length && _serial.available())
	// {
		// buffer[idx++] = (char)_serial.read();
		// length--;//if(!_serial.available())vTaskDelay(1000 / portTICK_RATE_MS);
	// }
	// return idx;
	return _serial.readBytes(buffer+offset, length);
}

inline size_t sim800::read_ota(size_t length, esp_ota_handle_t ota_handle)
{
	esp_err_t err = -1;
	size_t idx = 0;
	char* buffer = (char*)malloc(GSM_MAX_BUFFSIZE);
	if(buffer)
	{
		memset(buffer, 0, GSM_MAX_BUFFSIZE);
		while(length && _serial.available())
		{
			buffer[idx] = (char) _serial.read();
			idx++;
			length--;
			if(idx == GSM_MAX_BUFFSIZE-1)
			{
				idx = 0;
				err = esp_ota_write(ota_handle, (const void *)buffer, GSM_MAX_BUFFSIZE);
				memset(buffer, 0, GSM_MAX_BUFFSIZE);
				// ESP_ERROR_CHECK( err );
				if(err != ESP_OK) break;
			}
		}
		free(buffer);
	}
	return idx;
}

bool sim800::connect(const char *address, unsigned short int port, uint16_t timeout, int connection)
{
	if(!expect_AT(F("+CIPSHUT"), F("SHUT OK"))) return false;
	if(!expect_AT_OK(F("+CMEE=2"))) return false;
	if(!expect_AT_OK(F("+CIPMUX=1"))) return false;//1 = multi ip connections
	if(!expect_AT_OK(F("+CIPQSEND=1"))) return false;
	if(!expect_AT_OK(F("+CIPRXGET=1"))) return false;//1 = enable custom reading
	// if(!expect_AT_OK((String("AT+CSTT=\"")+_apn+"\"").c_str())) return false;// bring connection up, force it
	print(F("AT+CSTT=\""));print(_apn);println(F("\""));
	if(!expect_OK()) return false;
	if(!expect_AT_OK(F("+CIICR"))) return false;
	bool connected;
	do// try five times to get an IP address
	{
		char ipaddress[23];
		println(F("AT+CIFSR"));
		expect_scan(F("%s"), ipaddress);
		connected = strcmp_P(ipaddress, PSTR("ERROR")) != 0;
		if(!connected) vTaskDelay(1 / portTICK_RATE_MS);
	} while(timeout-- && !connected);
	if(!connected) return false;
	if(!expect_AT_OK(F("+CDNSCFG=\"8.8.8.8\",\"8.8.4.4\""))) return false;
	// expect_AT_OK((String("+CIPCLOSE=")+connection).c_str());
	if(!expect_AT_OK(F("+CIPSSL=0"))) return false;
	println((String("AT+CIPSTART=")+connection+",\"TCP\",\"" + address + "\"," + port).c_str());
	vTaskDelay(5000 / portTICK_RATE_MS);
	if(!expect_OK(timeout)) return false;
	if(!expect((String(connection)+", CONNECT OK").c_str(), timeout)) return false;
	return true;
}

bool sim800::status(const __FlashStringHelper *expected, size_t size, int connection)
{
	println((String("AT+CIPSTATUS=")+connection).c_str());
	// vTaskDelay(1000 / portTICK_RATE_MS);
	// return fullexpect(expected, size);
	return expect(expected);
}

bool sim800::disconnect(int connection)
{
	return expect_AT_OK((String("+CIPCLOSE=")+connection).c_str());
}

bool sim800::send(char *buffer, size_t size, unsigned long int &accepted, int connection)
{
	#ifdef DEBUG_SIM800
		DEBUGLN(buffer);
	#endif
	println((String("AT+CIPSEND=")+connection+","+size).c_str());
	// print((uint32_t) );
	if(!expect(F("> "))) return false;
	_serial.write((const uint8_t*)buffer, size);
	if(!expect_scan(F("DATA ACCEPT: %*d,%lu"), &accepted, 3000))
	{// we have a buffer of 319488 bytes, so we are optimistic, even if a temporary fail occurs and just carry on (verified!)
	//return false;
	}
	return accepted == size;
}

size_t sim800::receive(char *buffer, size_t size, int connection)
{
	// size_t actual = 0; unsigned long int requested, confirmed;
	// while(actual < size)
	// {// printf("======SIZE=====%d\n", size);
		// size_t chunk = (size_t) min(size - actual, GSM_MAX_BUFFSIZE);// printf("======CHUNK=====%d\n", chunk);
		// println((String("AT+CIPRXGET=2,")+connection+","+chunk).c_str());
		// if(!expect_scan(F("+CIPRXGET: 2,%*d,%lu,%lu"), &requested, &confirmed)) return 0;
		// actual += read(buffer, (size_t)confirmed, actual);// printf("======ACTUAL=====%d\n", actual);
	// }
	// return actual;
	size_t requested, confirmed;
	println((String("AT+CIPRXGET=2,")+connection+","+size).c_str());
	if(!expect_scan(F("+CIPRXGET: 2,%*d,%d,%d"), &requested, &confirmed)) return 0;
	return read(buffer, confirmed);
}

size_t sim800::get_content_length(int connection)
{
	unsigned long int requested, confirmed;
	println((String("AT+CIPRXGET=2,")+connection+","+GSM_MAX_BUFFSIZE).c_str());
	if(!expect_scan(F("+CIPRXGET: 2,%*d,%lu,%lu"), &requested, &confirmed)) return 0;
#ifdef DEBUG_SIM800
	printf("REQUESTED=%lu, CONFIRMED=%lu\n", requested, confirmed);
#endif
	char* buffer = (char*)malloc(requested);
	memset(buffer, 0, requested);
	size_t sz = read(buffer, requested);
#ifdef DEBUG_SIM800
	printf("===HTTP HEADERS %d===\n%s\n", sz, buffer);
#endif
	if(!sz) sz = 0;
	String a0(buffer);
	String a1 = a0.substring(a0.indexOf("Content-Length:")+15);
	memset(buffer, 0, confirmed);
	sz = _serial.readBytes(buffer, confirmed);
#ifdef DEBUG_SIM800
	printf("===HTTP HEADERS %d===\n%s\n============\n", sz, buffer);
#endif
	free(buffer);
	return a1.substring(0, a1.indexOf('\r')).toInt();
}

// size_t sim800::get_content_length(int connection)
// {
	// unsigned long int requested, confirmed;
	// println((String("AT+CIPRXGET=2,")+connection+","+GSM_MAX_BUFFSIZE).c_str());
	// if(!expect_scan(F("+CIPRXGET: 2,%*d,%lu,%lu"), &requested, &confirmed)) return 0;
// #ifdef DEBUG_SIM800
	// printf("REQUESTED=%lu, CONFIRMED=%lu\n", requested, confirmed);
// #endif
	// vTaskDelay(1000/portTICK_RATE_MS);
	// if(_serial.findUntil("Content-Length", ":"))
		// return _serial.readStringUntil('\r').toInt();
	// else
	// {
		// String a0 = _serial.readString();
	// #ifdef DEBUG_SIM800
		// printf("===HTTP HEADERS===\n%s\n===\n", a0.c_str());
	// #endif
		// String a1 = a0.substring(a0.indexOf("Content-Length:")+15);
		// return a1.substring(0, a1.indexOf('\r')).toInt();
	// }
	// while(_serial.available())_serial.read();
	// return 0;
// }

size_t sim800::get_remain_tcp_data(int connection)
{
	size_t remained = 0;
	println((String("AT+CIPRXGET=4,")+connection).c_str());
	vTaskDelay(1000 / portTICK_RATE_MS);
	if(!expect_scan(F("+CIPRXGET: 4,%*d,%d"), &remained)) return 0;
	return remained;
}

/* ===========================================================================
 * PROTECTED
 * ===========================================================================
 */

// read a line
size_t sim800::readline(char *buffer, size_t max, uint16_t timeout)
{
	uint16_t idx = 0;
	while(--timeout)
	{
		while(_serial.available())
		{
			char c = (char)_serial.read();
			if(c == '\r') continue;
			if(c == '\n')
			{
				if (!idx) continue;
				timeout = 0;
				break;
			}
			if(max - idx) buffer[idx++] = c;
		}
		if(timeout == 0) break;
		vTaskDelay(1 / portTICK_RATE_MS);
	}
	buffer[idx] = 0;
	return idx;
}

void sim800::eat_echo()
{
	while (_serial.available())
	{
		_serial.read();
		// don't be too quick or we might not have anything available
		// when there actually is...
		vTaskDelay(1 / portTICK_RATE_MS);
	}
}

void sim800::print(const __FlashStringHelper *s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGQLN(s);
#endif
	_serial.print(s);
}

void sim800::print(uint32_t s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");
	DEBUGLN(s);
#endif
	_serial.print(s);
}


void sim800::println(const __FlashStringHelper *s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");DEBUGQLN(s);
#endif
	_serial.print(s);
	eat_echo();
	_serial.println();
	_serial.flush();
}

void sim800::println(uint32_t s)
{
#ifdef DEBUG_AT
	PRINT("+++ ");DEBUGLN(s);
#endif
	_serial.print(s);
	eat_echo();
	_serial.println();
	_serial.flush();
}

bool sim800::fullexpect(const __FlashStringHelper *expected, size_t count_read_bytes)
{
	char buf[count_read_bytes];
#ifdef DEBUG_AT
	size_t len = _serial.readBytes(buf, count_read_bytes);
	PRINT("--- (");DEBUG(len);PRINT(") ");DEBUGQLN(buf);
#else
	_serial.readBytes(buf, count_read_bytes);
#endif
	// _serial.flush();
	// return strcmp_P(buf, (const char PROGMEM *) expected) == 0;
	return strstr(buf, (const char PROGMEM *) expected) != NULL;
}

bool sim800::expect(const __FlashStringHelper *expected, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");DEBUG(len);PRINT(") ");DEBUGQLN(buf);
#endif
	_serial.flush();
	// return strcmp_P(buf, (const char PROGMEM *) expected) == 0;
	return strstr(buf, (const char PROGMEM *) expected) != NULL;
}

bool sim800::expect_AT(const __FlashStringHelper *cmd, const __FlashStringHelper *expected, uint16_t timeout, uint16_t delay_seconds)
{
	print(F("AT"));
	println(cmd);
	vTaskDelay(delay_seconds / portTICK_RATE_MS);
	return expect(expected, timeout);
}

bool sim800::expect_AT_OK(const __FlashStringHelper *cmd, uint16_t timeout)
{
	return expect_AT(cmd, F("OK"), timeout);
}

bool sim800::expect_OK(uint16_t timeout)
{
	return expect(F("OK"), timeout);
}

bool sim800::expect_scan_fast(const __FlashStringHelper *pattern, void *ref, void *ref1, size_t length)
{
	char buf[32];
	size_t len=0, i=0;
	do{len = readline(buf, 32, 60); i++; if(i>1)break;} while(!len);
// #ifdef DEBUG_AT
	// PRINT("--- |");DEBUG(len);PRINT("| ");DEBUGQLN(buf);
// #endif
	// _serial.flush();
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1) == 2;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");DEBUG(len);PRINT(") ");DEBUGQLN(buf);
#endif
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref) == 1;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	// _serial.flush();
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1) == 2;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2, void *ref3, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	// _serial.flush();
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1, ref2, ref3) == 4;
}

bool sim800::expect_scan(const __FlashStringHelper *pattern, void *ref, void *ref1, void *ref2, uint16_t timeout)
{
	char buf[SIM800_BUFSIZE];
	size_t len, i=0;
	do{len = readline(buf, SIM800_BUFSIZE, timeout); i++; if(i>5)break;} while(is_urc(buf, len));
#ifdef DEBUG_AT
	PRINT("--- (");
	DEBUG(len);
	PRINT(") ");
	DEBUGQLN(buf);
#endif
	// _serial.flush();
	return sscanf_P(buf, (const char PROGMEM *)pattern, ref, ref1, ref2) == 3;
}

bool sim800::is_urc(const char *line, size_t len)
{
	urc_status = 0xff;
	for(uint8_t i = 0; i < 17; i++)
	{
	// #ifdef __AVR__
		// const char *urc = (const char *) pgm_read_word(&(_urc_messages[i]));
	// #else
		const char *urc = _urc_messages[i];
	// #endif
		size_t urc_len = strlen(urc);
		if(len >= urc_len && !strncmp(urc, line, urc_len))
		{
		#ifdef DEBUG_URC
			print_log("SIM800 URC(");DEBUG(i);PRINT(") ");DEBUGLN(urc);
		#endif
			urc_status = i;
			return true;
		}
	}
	return false;
}

bool sim800::check_sim_card()
{
	#ifdef DEBUG_URC
		print_log("SIM800 check SIM card inserted...\n");
	#endif
	println(F("AT+CSMINS?"));
	return expect(F("+CSMINS: 0,1"), 3000);
	// println(F("AT+CPIN?"));
	// return expect(F("CPIN: READY"), 3000);
}

int sim800::get_signal(int& ber)
{
	int rssi = 0;
	println("AT+CSQ");
	vTaskDelay(3000 / portTICK_RATE_MS);
	expect_scan(F("+CSQ: %2d"), (void*)&rssi, (void*)ber);
#ifdef DEBUG_URC
	print_log("SIM800 RSSI ");DEBUG(rssi);PRINTLN(" dBm");
	print_log("SIM800 BER ");DEBUG(ber);PRINTLN(" %");
#endif
	return rssi;
}

void sim800::set_operator()
{
	int operator_index = -1;
	char* operator_name = (char*)malloc(64);
	if(operator_name)
	{
		memset(operator_name, 0, 64);
		println(F("AT+COPS?"));
		if(expect_scan(F("%s"), operator_name, 3000))
		{
		#ifdef DEBUG_SIM800
			print_log("\n SIM800: AT RESPONSE: [%s]", operator_name);
		#endif
			for(int i = 0; i < 4; ++i)
			{
				if(strstr(operator_name, operators[i]))
				{
					operator_index = i;
					break;
				}
			}
		}
		free(operator_name);
	}
	if(operator_index > -1 && operator_index != current_operator)
	{
		current_operator = operator_index;
		setAPN(apns[current_operator], users[current_operator], pwds[current_operator]);
	}
}

bool sim800::gsm_init()
{
	uint16_t ip0=0, ip1=0, ip2=0, ip3=0;
	begin();
	while(!wakeup())
	{
		vTaskDelay(3000 / portTICK_RATE_MS);
	}
	expect_AT_OK(F(""));
	expect_AT_OK(F("+IPR?"));//check uart speed (0 = auto)
	expect_AT_OK(F("+IPR=115200"));//set uart speed 115200
	expect_AT_OK(F("+IPR?"));//check uart speed (0 = auto)
	expect_AT_OK(F("+CSCLK=0"));//disable sleep mode
	expect_AT_OK(F("+CNMI=0,0,0,0,0"));//disable incoming SMS
	expect_AT_OK(F("+GSMBUSY=1"));//disable incoming calls
	expect_AT_OK(F("+CBC"), 2000);//power monitor
	expect_AT_OK(F("+CADC?"), 2000);//acp monitor
	if(!check_sim_card())
	{
		vTaskDelay(3000 / portTICK_RATE_MS);
		while(!check_sim_card()) vTaskDelay(60000 / portTICK_RATE_MS);
	}
	gsm_rssi = get_signal(gsm_ber);
	while(gsm_rssi < 1 || gsm_rssi > 98)
	{
		vTaskDelay(10000 / portTICK_RATE_MS);
		gsm_rssi = get_signal(gsm_ber);
	}
	while(!registerNetwork())
	{
		vTaskDelay(20000 / portTICK_RATE_MS);
		shutdown();
		wakeup();
	}
	println(F("AT+CGATT?"));
	expect(F("+CGATT: "), 3000);
	set_operator();
	println(F("AT+SAPBR=1,1"));
	expect_OK();
	println(F("AT+SAPBR=2,1"));
	vTaskDelay(2000 / portTICK_RATE_MS);
	bool result = expect_scan(F("+SAPBR: 1,1,\"%d.%d.%d.%d\""), &ip0, &ip1, &ip2, &ip3);
#ifdef DEBUG_SIM800
	printf("\n");
	print_log("SIM800: CONNECT: %s\n", result ? "TRUE" : "FALSE");
#endif
	if(ip0==0 && ip1==0 && ip2==0 && ip3==0){println(F("AT+SAPBR=2,1"));vTaskDelay(2000/portTICK_RATE_MS);result=expect_OK();}
	// while(!result)
	// {
		// vTaskDelay(5000 / portTICK_RATE_MS);
		// result = expect_scan(F("+SAPBR: 1,1,\"%d.%d.%d.%d\""), &ip0, &ip1, &ip2, &ip3);
	// #ifdef DEBUG_SIM800
		// printf("\n!!! SIM800: CONNECT: %s", result ? "TRUE" : "FALSE");
	// #endif
	// }
	// if(ip0==0 && ip1==0 && ip2==0 && ip3==0){println(F("AT+SAPBR=2,1"));vTaskDelay(5000/portTICK_RATE_MS);result=expect_OK();}
	return result;
}

void sim800::update_esp_http(const char *host, unsigned short int port, String url_update)
{//ONLY 14 kbytes
#ifdef DEBUG_SIM800
	printf("\n");
	print_log("SIM800: START UPDATE %s\n", url_update.c_str());
#endif
	if(connect(host, port, 15000))
	{
		vTaskDelay(10000 / portTICK_RATE_MS);
		unsigned long int accepted = 0;
		if(send((char*)("GET " + url_update + "HTTP/1.1\r\nHost:" + host + "\r\nConnection:keep-alive").c_str(), url_update.length(), accepted))
		{
			char* buffer = (char*)malloc(1024);
			if(buffer)
			{
				memset(buffer, 0, 1024);
				esp_err_t err;
				esp_ota_handle_t update_handle = 0;/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
				const esp_partition_t *update_partition = NULL;
			#ifdef DEBUG_SIM800
				print_log("Starting OTA ...\n");
				const esp_partition_t *configured = esp_ota_get_boot_partition();
				const esp_partition_t *running = esp_ota_get_running_partition();
				if(configured != running)
				{
					print_log("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n", configured->address, running->address);
					print_log("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n");
				}
				print_log("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);
			#endif
				update_partition = esp_ota_get_next_update_partition(NULL);
				assert(update_partition != NULL);
			#ifdef DEBUG_SIM800
				print_log("Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);
			#endif
				err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
				if(err != ESP_OK)
				{
				#ifdef DEBUG_SIM800
					print_log("esp_ota_begin failed, error=%d\n", err);
				#endif
					return;
				}
				else
				{
				#ifdef DEBUG_SIM800
					print_log("esp_ota_begin succeeded\n");
				#endif
					size_t total_result_read = 0;
					size_t result_read = receive(buffer, 1024);
					while(result_read > 0)
					{
						total_result_read += result_read;
					#ifdef DEBUG_SIM800
						print_log("SIM800: TCP read received length = %d\r", total_result_read);
					#endif
						err = esp_ota_write(update_handle, (const void*)buffer, result_read);
						memset(buffer, 0, 1024);
						result_read = receive(buffer, 1024);
					}
				#ifdef DEBUG_SIM800
					print_log("Total Write binary data length : %d\n", total_result_read);
				#endif
					if(esp_ota_end(update_handle) == ESP_OK)
					{
						err = esp_ota_set_boot_partition(update_partition);
						if(err == ESP_OK)
						{
						#ifdef DEBUG_SIM800
							print_log("Prepare to restart system!\n");
						#endif
							esp_restart();
						}
					#ifdef DEBUG_SIM800
						else
							print_log("esp_ota_set_boot_partition failed! err=0x%x\n", err);
					#endif
					}
				#ifdef DEBUG_SIM800
					else
						print_log("esp_ota_end failed!\n");
				#endif
				}
				free(buffer);
			}
		}
	#ifdef DEBUG_SIM800
		else
			print_log("SIM800: STATUS CONNECTION FAILED!\n");
	#endif
		disconnect();
	}
#ifdef DEBUG_SIM800
	else
		print_log("SIM800: CONNECT FAILED!\n");
#endif
}

////////////////////// FTP /////////////////////////

bool sim800::init_ftp(const char* host, int port, const char* user, const char* password, const char* file_name, const char* path)
{
	bool result = false;
	bool ftpcid = expect_AT_OK(F("+FTPCID=1"), 1000);
	if(!ftpcid)
	{
		if(expect_AT_OK(F("+FTPQUIT"), 1000))
		{
			vTaskDelay(5000 / portTICK_RATE_MS);
			ftpcid = expect_AT_OK(F("+FTPCID=1"), 1000);
		}
	}
	if(ftpcid)
	{
		String ftp_server = "+FTPSERV=\"" + String(host) + "\"";
		if(expect_AT_OK(F(ftp_server.c_str()), 1000))
		{
			String ftp_port = "+FTPPORT=" + String(port);
			if(expect_AT_OK(F(ftp_port.c_str()), 1000))
			{
				String ftp_user = "+FTPUN=\"" + String(user) + "\"";
				if(expect_AT_OK(F(ftp_user.c_str()), 1000))
				{
					String ftp_password = "+FTPPW=\"" + String(password) + "\"";
					if(expect_AT_OK(F(ftp_password.c_str()), 1000))
					{
						String ftp_file = "+FTPGETNAME=\"" + String(file_name) + "\"";
						if(expect_AT_OK(F(ftp_file.c_str()), 1000))
						{
							String ftp_path = "+FTPGETPATH=\"" + String(path) + "\"";
							if(expect_AT_OK(F(ftp_path.c_str()), 1000))
							{
							#ifdef DEBUG_SIM800
								printf("\nGSM: FTP INIT OK\n");
							#endif
								result = true;
							}
						#ifdef DEBUG_SIM800
							else
							{
								printf("\nGSM: FTP INIT ERROR\n");
							}
						#endif
						}
					}
				}
			}
		}
	}
	return result;
}

void sd_create_write_file(const char* fn, char* data)
{
	FILE* f = fopen(fn, "wb");
	if(f)
	{
		fprintf(f, "%s\n", data);
		fflush(f);
		fseek(f , 0, SEEK_END);
		printf("SD: FILE %s CREATED, SIZE %ld\n", fn, ftell(f));
		fclose(f);
	}
	else
	{
		perror(fn);
		printf("SD: ERROR OPEN FILE %s = %p\n", fn, f);
	}
}

void sim800::update_esp_ftp()
{
	// bool ret = expect_AT(F("+FTPGET=1"), F("OK\r\n+FTPGET: 1,1"), 1000, 10000);
	// vTaskDelay(5000 / portTICK_RATE_MS);
	bool ret = expect_AT(F("+FTPGET=1"), F("+FTPGET: 1,1"), 1000, 8000);
	// if(!ret) ret = expect(F("+FTPGET: 1,1"), 1000);
	char* b = (char*)malloc(14);
	if(b)
	{
		memset(b, 0, 14);
		// int i = 0;
		// while(_serial.available())
		// {
			// b[i] = _serial.read();// printf("%c", b[i]);
			// i++;
			// if(i > 13) break;
		// }
		read(b, 14);
		printf("%s\n", b);
		ret = (bool)strstr(b, "+FTPGET: 1,1");
		free(b);
	}
	if(ret)
	{
		char* buf = (char*)malloc(GSM_MAX_BUFFSIZE);
		if(buf)
		{
			memset(buf, 0, GSM_MAX_BUFFSIZE);
			esp_err_t err;
			esp_ota_handle_t ota_handle = 0;
			const esp_partition_t *update_partition = NULL;
			printf("Starting OTA example...\n");
			const esp_partition_t *configured = esp_ota_get_boot_partition();
			const esp_partition_t *running = esp_ota_get_running_partition();
			if(configured != running)
			{
				printf("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x\n", configured->address, running->address);
				printf("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)\n");
			}
			printf("Running partition type %d subtype %d (offset 0x%08x)\n", running->type, running->subtype, running->address);
			update_partition = esp_ota_get_next_update_partition(NULL);
			assert(update_partition != NULL);
			printf("Writing to partition subtype %d at offset 0x%x\n", update_partition->subtype, update_partition->address);
			err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
			if(err != ESP_OK)
			{
				printf("esp_ota_begin failed, error=%d\n", err);
				return;
			}
			else
			{
			#ifdef DEBUG_SIM800
				printf("esp_ota_begin succeeded\n");
				uint32_t full_read_bytes = 0;
			#endif
				uint32_t buff_len = 0;
				println(F("AT+FTPGET=2,1024"));
				// vTaskDelay(10000 / portTICK_RATE_MS);
				////////////////////////////////////////////
				while(_serial.available())
				{
					printf("%c", _serial.read());
					// buf[buff_len] = _serial.read();
					// buff_len++;
				}
				////////////////////////////////////////////
				// expect_scan(F("+FTPGET=2,%d\r\n%s\r\nOK\r\n"), &buff_len, &buf);
				buff_len = _serial.readBytesUntil('\n', buf, GSM_MAX_BUFFSIZE);
				printf("\nGSM: 0 FTPGET %d|%s\n", buff_len, buf);
				memset(buf, 0, GSM_MAX_BUFFSIZE);
				buff_len = _serial.readBytesUntil('\n', buf, GSM_MAX_BUFFSIZE);
				printf("\nGSM: 1 FTPGET %d|%s\n", buff_len, buf);
				memset(buf, 0, GSM_MAX_BUFFSIZE);
				uart_port_t gsm_uart_port = UART_NUM_1;
				uart_wait_tx_done(gsm_uart_port, 1000 / portTICK_RATE_MS);
				buff_len = uart_read_bytes(gsm_uart_port, (uint8_t*)buf, GSM_MAX_BUFFSIZE, 1000 / portTICK_RATE_MS);
				// buff_len = _serial.readBytesUntil('\r', buf, GSM_MAX_BUFFSIZE);
				printf("\nGSM: 2 FTPGET %d|%s\n", buff_len, buf);
				sd_create_write_file("/sdcard/test.bin", buf);
				// while(buff_len > 0)
				while(_serial.available())
				{
				#ifdef DEBUG_SIM800
					full_read_bytes += buff_len;
					printf("\nGSM: 3 FTPGET %d\n", buff_len);
				#endif
					err = esp_ota_write(ota_handle, (const void*)buf, buff_len);
					// ESP_ERROR_CHECK( err );
					if(err != ESP_OK || buff_len < 1024) break;
					memset(buf, 0, GSM_MAX_BUFFSIZE);
					println((String("+FTPGET=2,")+GSM_MAX_BUFFSIZE).c_str());
					// if(!expect_scan(F("+FTPGET=2,%d\r\n%s\r\nOK\r\n"), &buff_len, &buf))
					buff_len = read(buf, GSM_MAX_BUFFSIZE);
					if(buff_len < 32)
					{
					#ifdef DEBUG_SIM800
						printf("\nGSM: FTP STOP READ\n");
					#endif
						break;
					}
				}
			#ifdef DEBUG_SIM800
				printf("Total Write binary data length : %d\n", full_read_bytes);
			#endif
				if(esp_ota_end(ota_handle) != ESP_OK)
				{
					err = esp_ota_set_boot_partition(update_partition);
					if(err == ESP_OK)
					{
						if(!expect_AT_OK(F("+FTPQUIT"), 1000))
						{
						#ifdef DEBUG_SIM800
							printf("\nGSM: FTPQUIT ERROR\n");
						#endif
						}
					#ifdef DEBUG_SIM800
						printf("Prepare to restart system!\n");
					#endif
						esp_restart();
					}
				#ifdef DEBUG_SIM800
					else
						printf("esp_ota_set_boot_partition failed! err=0x%x\n", err);
				#endif
				}
			#ifdef DEBUG_SIM800
				else
					printf("esp_ota_end failed!\n");
			#endif
			}
			free(buf);
		}
	}
#ifdef DEBUG_SIM800
	else
		printf("\nGSM: FTPGET ERROR\n");
#endif
	if(!expect_AT_OK(F("+FTPQUIT"), 1000))
	{
	#ifdef DEBUG_SIM800
		printf("\nGSM: FTPQUIT ERROR\n");
	#endif
	}
}
