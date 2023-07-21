#ifndef __GCU_H__
#define __GCU_H__


#include "esp32-hal.h"
#include "esp32-hal-rgb-led.h"
#include <ESP32Time.h>
#include "time.h"
#include "sntp.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <ArduinoOTA.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Ticker.h>
#include <Wire.h>


//System Define
//! DONT NEED CHANGE
#define GCU_SDA 48
#define GCU_SCL 47
#define BQ32002_I2C_ADDRESS 0x68

#define BQ32002_SECONDS_Register 0x00
#define BQ32002_MINUTES_Register 0x01
#define BQ32002_CENT_HOURS_Register 0x02
#define BQ32002_DAY_Register 0x03
#define BQ32002_DATE_Register 0x04
#define BQ32002_MONTH_Register 0x05
#define BQ32002_YEARS_Register 0x06

#define TCP 1
#define UDP 0


//BQ32002 Chip Time Register Struct
struct bq32002_tm
{
  byte secs;
  byte mins;
  byte cent_hours;
  byte day;
  byte date;
  byte month;
  byte years;
};

extern const char* loginIndex;
extern const char* serverIndex;

//External Variable Declaration
// Device Parameters
extern unsigned char device_number;
extern const unsigned char device_frequency;

// Sensor Numbers
extern const unsigned char sensors_rows_num;
extern const unsigned char sensors_columns_num;
extern const unsigned char sensors_num;

// Data Format Function
extern const bool start_flag;
extern const bool device_num_flag;
extern const bool sensors_num_flag;
extern const bool timestamp_flag;
extern const bool data_validation_flag;
extern const bool end_flag;

extern const int data_num;

//RTC
extern ESP32Time rtc;
extern const char* ntpServer1;
extern const char* ntpServer2;
extern const int  gmtOffset_sec;
extern const int  daylightOffset_sec;

// WiFi Parameters
extern const char* host;
extern const char* SSID;
extern const char* password;
extern const char* SeverIP;
extern const uint16_t port;
extern const bool TCP_UDP_Flag;


// Define ADIO(sensor_rows) and SelectIO(sensor_columns)
extern const int analogReadIO[];
extern const int SelectIO[];

extern bool working_flag;
extern unsigned char data[];
extern unsigned char * data_p;
extern uint32_t check_sum;


extern WebServer server;


//Function Declaration
void basic_OTA();
void OTA_web_updater();

void neopixelWrite38(uint8_t red_val, uint8_t green_val, uint8_t blue_val);

bool init_RTC_from_net();
bool init_RTC_from_bq32002();
bool check_bq32002_status();
void RTC_error();

struct tm bq32002tm_to_tm(struct bq32002_tm _bq32002_tm);
struct bq32002_tm tm_to_bq32002tm(struct tm _timeinfo);
bool write_tm_to_bq32002(struct tm _timeinfo);
struct tm read_tm_from_bq32002();

void data_receive();

#endif // __GCU_H__