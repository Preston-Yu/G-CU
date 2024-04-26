// Version 2.0

#include "GCU.h"


// Device Parameters
// If the device number is set to the default value of 0x00, the device will automatically convert the chip ID to the device number
uint8_t device_number = 0x00; 
const uint16_t device_frequency = 100;
const uint16_t calibration_duration = 10000;

// Sensor Numbers
constexpr unsigned char sensors_rows_num = 1;
constexpr unsigned char sensors_columns_num = 10;

// Data Format Function
const bool start_flag = GCU_FLAG_ON;
const bool device_num_flag = GCU_FLAG_ON;
const bool sensors_num_flag = GCU_FLAG_ON;
const bool timestamp_flag = GCU_FLAG_ON;
const bool IMU_flag = GCU_FLAG_ON;
const bool end_flag = GCU_FLAG_ON;


//sensors_dataformat: Four_Bytes_Sensors_Data or Two_Bytes_Sensors_Data
// This option will removed in next version
// Recommand change data format to four bytes
#define sensors_dataformat_define Four_Bytes_Sensors_Data


/*********Normalized calibration function flag**********/
/*        If normalized_calibration is ON              */
/*        Sensors Dataformat must be Four Bytes        */
const bool normalized_calibration_flag = GCU_FLAG_OFF;
const float normalized_calibration_max_factor = 0.2;
const float normalized_calibration_min_factor = 0.2;


//IMU Chip : GCU_BMI270_BMM150 or GCU_BMX160(old version)
//
const bool IMU_chip = GCU_BMI270_BMM150;


//RTC Chip
const bool RTC_chip = GCU_FLAG_OFF;

// WiFi Parameters
// #define SSID "GCU-wifi"
// #define password "12345678"
const char* host = "esp32s3";
const char* SSID       = "CNLab-IoT";
const char* password   = "12345678";
const char* SeverIP = "255.255.255.255";
const uint16_t port = 1337;
const bool TCP_UDP_Flag = UDP;


// Times Setting
const int UTC = 9;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const int  gmtOffset_sec = 3600 * UTC;
const int  daylightOffset_sec = 0;


// Define ADIO(sensor_rows) and SelectIO(sensor_columns)
const int analogReadIO[]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const int SelectIO[]={18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};


// Data Array Size = (start_flag + sensors_num + end_flag ) * 2 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 36
const unsigned char sensors_num = sensors_rows_num * sensors_columns_num;

#if sensors_dataformat_define == Four_Bytes_Sensors_Data
const unsigned char data_num = (start_flag + end_flag ) * 2 + sensors_num * 4 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 4 * 3 * 3;
const bool sensors_dataformat = Four_Bytes_Sensors_Data;
#else
const unsigned char data_num = (start_flag + sensors_num + end_flag ) * 2 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 4 * 3 * 3;
#endif


bool working_flag = 0;
unsigned char data[data_num];
unsigned char * data_p = data;
uint32_t check_sum = 0;

float maxMillVolts[sensors_num];
float minMillVolts[sensors_num];

WebServer server(80);
Ticker data_receiver;
WiFiMulti WiFiMulti;
ESP32Time rtc(0*3600);
DFRobot_BMX160 bmx160;

void setup() {

  neopixelRainbow(30);
  neopixelWrite(0,0,0); // Off / black
  delay(300);
  

  Serial.begin(115200);
  delay(10);
  Wire.begin(GCU_SDA,GCU_SCL);
  delay(10);

  //init IMU
  if (IMU_flag){
    if (IMU_chip){
      if (!IMU.begin()) {
      Serial.println("Failed to initialize IMU!");
      neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);
      while (1);
      }
    }
    else{
      if (bmx160.begin() != true){
        Serial.println("IMU init false");
        neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);
        while(1);
      }
    }
  } 
  

  //set the resolution to 12 bits (0-4096)
  analogReadResolution(12);

  neopixelWrite(GCU_RGB_BRIGHTNESS,0,0); // Red
  WiFiMulti.addAP(SSID, password);
  

  Serial.println();
  Serial.println();
  Serial.print("Waiting for WiFi... ");
  while(WiFiMulti.run() != WL_CONNECTED){
    Serial.print(".");
  }
  neopixelWrite(0,0,0);

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  basic_OTA();
  OTA_web_updater();
  delay(500);

  neopixelWrite(0,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);

  if (RTC_chip){
    if (!init_RTC_from_net()){
      neopixelBlink(0, GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 3, 1000);
      if (!init_RTC_from_bq32002()){
        RTC_error();
      }
    }
  }
  else{
    if (!init_RTC_from_net()){
      neopixelBlink(0, GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 3, 1000);
      RTC_error();
    }
  }
  

  if(start_flag){
    data[0] = 0x5a;
    data[1] = 0x5a;
    data_p += 2;
  }

  if(device_num_flag){
    if(!device_number){
      for(int i=0; i<17; i=i+8) {
	    data[2] |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
      Serial.print("Chip ID: "); Serial.println(data[2]);
      data_p += 1;
    }
    else{
      data[2] = device_number;
      data_p += 1;
    }
    
  }

  if(sensors_num_flag){
    data[3] = sensors_num;
    data_p += 1;
  }


  if(end_flag){
    data[data_num - 2] = 0xa5;
    data[data_num - 1] = 0xa5;
  }

  for(int i=0;i < sizeof(SelectIO)/sizeof(SelectIO[0]);i++){
    pinMode(SelectIO[i],INPUT);
  }

  if(normalized_calibration_flag == GCU_FLAG_ON)
  {
    normalizedCalibrationInit(normalized_calibration_method_mean);
    Serial.println("normalizedCalibrationInit: Completed");
  }
  
  //Enable Timer Interrupt
  neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,0);
  data_receiver.attach_ms(1000/device_frequency, dataReceive);

}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
}

