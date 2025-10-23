// Version 3.0

/*
Note: This sketch takes up a lot of space for the app and may not be able to flash with default setting on some chips.
  If you see Error like this: "Sketch too big"
  In Arduino IDE go to: Tools > Partition scheme > chose anything that has more than 1.4MB APP
   - for example "No OTA (2MB APP/2MB SPIFFS)"
注意：
此示例程序体积较大，某些芯片在默认分区下可能无法烧录成功（Sketch too big）。
若遇到 “Sketch too big”，请在 Arduino IDE：
Tools → Partition scheme → 选择 APP 分区 大于 1.4MB 的方案
例如 “No OTA (2MB APP/2MB SPIFFS)”
*/

#include "GCU.h"


// Device Parameters
// If the device number is set to the default value of 0x00, the device will automatically convert the chip ID to the device number
uint8_t device_number = 0x00; 
bool data_ready = false;
const uint16_t device_frequency = 100;
const uint16_t calibration_duration = 10000;

// Sensor Numbers
constexpr unsigned char sensors_rows_num = 11;
constexpr unsigned char sensors_columns_num = 13;

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
 const char* host = "esp32s3_pu_new_insole_l";
// New Wi-Fi method dont needs these Data
// const char* SSID       = "CNLab-IoT";
// const char* password   = "12345678";
// UDP broadcast
const char* SeverIP = "255.255.255.255";
// pu: left:1370 right:1371 glove_v1:1381
// test: 1401
const uint16_t port = 13251;
const bool TCP_UDP_Flag = UDP;


// #define USE_SOFT_AP // Uncomment if you want to enforce using the Soft AP method instead of BLE
const char *pop = "abcd1234";           // Proof of possession - otherwise called a PIN - string provided by the device, entered by the user in the phone app
const char *service_name = "PROV_123";  // Name of your device (the Espressif apps expects by default device name starting with "Prov_")
const char *service_key = NULL;         // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = false;          // When true the library will automatically delete previously provisioned data.


// Times Setting
const int UTC = 9;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const int  gmtOffset_sec = 3600 * UTC;
const int  daylightOffset_sec = 0;

unsigned int sys_epoch;
unsigned short sys_millis;


// Define ADIO(sensor_rows) and SelectIO(sensor_columns)
// new right
// const int analogReadIO[]={1,2,3,4,5,6,7};
// const int SelectIO[]={19,20,21,35,36};

// new left
const int analogReadIO[]={1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const int SelectIO[]={17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};

// // glove
// const int analogReadIO[]={6, 5, 4, 21, 20, 19, 7};
// // const int SelectIO[]={19, 20, 21, 35, 36};
// const int SelectIO[]={1, 2, 3, 36, 35};

// Data Array Size = (start_flag + sensors_num + end_flag ) * 2 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 36
const unsigned char sensors_num = sensors_rows_num * sensors_columns_num;

#if sensors_dataformat_define == Four_Bytes_Sensors_Data
const unsigned int data_num = (start_flag + end_flag ) * 2 + sensors_num * 4 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 4 * 3 * 3;
const bool sensors_dataformat = Four_Bytes_Sensors_Data;
#else
const unsigned int data_num = (start_flag + sensors_num + end_flag ) * 2 + device_num_flag + sensors_num_flag + timestamp_flag * 6 + IMU_flag * 4 * 3 * 3;
const bool sensors_dataformat = Two_Bytes_Sensors_Data;
#endif


bool working_flag = 0;
unsigned char data[data_num];
unsigned char * data_p = data;
uint32_t check_sum = 0;

float maxMillVolts[sensors_num];
float minMillVolts[sensors_num];

float BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z;
float BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z;
float BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z;
sBmx160SensorData_t Omagn, Ogyro, Oaccel;

WebServer server(80);
Ticker data_receiver;
WiFiMulti WiFiMulti;
ESP32Time rtc(0*3600);
DFRobot_BMX160 bmx160;

unsigned long windowDeadline = 0;
uint8_t bootcnt_local = 0;
bool windowCleared = false;

Preferences prefs;


WiFiClient client;
WiFiUDP udp;
volatile bool g_wifiGotIP = false;



void setup() {
  Serial.begin(115200);
  delay(10);
  Wire.begin(GCU_SDA,GCU_SCL,1000000);
  delay(10);

  reset_provisioned = checkMultiPowerCycleForceProvision(8000, 3);

  neopixelRainbow(30);
  neopixelWrite(0,0,0); // Off / black
  delay(300);

  // if (wasButtonLongPressedOnBoot(0, 3000)) {
  //   Serial.println("➡️ Detected long press: force re-provisioning.");
  //   reset_provisioned = true;
  // } else {
  //   Serial.println("➡️ No long press: try existing Wi-Fi.");
  // }

  //init IMU
  if (IMU_flag){
    if (IMU_chip){
      if (!IMU.begin()) {
      Serial.println("Failed to initialize IMU!");
      neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);
      while (1);
      }
      Serial.print("Gyroscope sample rate = ");
      Serial.print(IMU.gyroscopeSampleRate());
      Serial.println(" Hz");
      Serial.println();
      Serial.print("Accelerometer sample rate = ");
      Serial.print(IMU.accelerationSampleRate());
      Serial.println(" Hz");
      Serial.println();
      Serial.print("MagneticField sample rate = ");
      Serial.print(IMU.magneticFieldSampleRate());
      Serial.println(" Hz");
      Serial.println();
    }
    else{
      if (bmx160.begin() != true){
        Serial.println("IMU init false");
        neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);
        while(1);
      }
    }
  } 
  

  neopixelWrite(GCU_RGB_BRIGHTNESS,0,0); // Red

  /*     Old Wi-Fi Function     */ 
  // WiFiMulti.addAP(SSID, password);
  // Serial.println();
  // Serial.println();
  // Serial.print("Waiting for WiFi... ");
  // while(WiFiMulti.run() != WL_CONNECTED){
  //   delay(500);
  //   Serial.print("\nConnect to WiFi again...");
  // }
  // neopixelWrite(0,0,0);

  // Serial.println("");
  // Serial.println("WiFi connected");
  // Serial.println("IP address: ");
  // Serial.println(WiFi.localIP());

  /*     New Wi-Fi Function     */ 
  WiFi.onEvent(SysProvEvent);
  WiFi.begin(); // no SSID/PWD - get it from the Provisioning APP or from NVS (last successful connection)
  

  // BLE Provisioning using the ESP SoftAP Prov works fine for any BLE SoC, including ESP32, ESP32S3 and ESP32C3.
#if (defined(CONFIG_BLUEDROID_ENABLED) || defined(CONFIG_NIMBLE_ENABLED)) && __has_include("esp_bt.h") && !defined(USE_SOFT_AP)
  Serial.println("Begin Provisioning using BLE");
  // Sample uuid that user can pass during provisioning using BLE
  uint8_t uuid[16] = {0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02};
  Serial.print(reset_provisioned);
  WiFiProv.beginProvision(
    NETWORK_PROV_SCHEME_BLE, NETWORK_PROV_SCHEME_HANDLER_FREE_BLE, NETWORK_PROV_SECURITY_1, pop, service_name, service_key, uuid, reset_provisioned
  );
  // log_d("ble qr");
  // WiFiProv.printQR(service_name, pop, "ble");
#else
  Serial.println("Begin Provisioning using Soft AP");
  WiFiProv.beginProvision(NETWORK_PROV_SCHEME_SOFTAP, NETWORK_PROV_SCHEME_HANDLER_NONE, NETWORK_PROV_SECURITY_1, pop, service_name, service_key);
  // log_d("wifi qr");
  // WiFiProv.printQR(service_name, pop, "softap");
#endif


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



  basic_OTA();
  OTA_web_updater();
  delay(500);

  const uint32_t noProvWait = 30000;      // 30s
  const uint32_t provWait   = 300000;     // 5min

  // 配网阶段闪红灯，普通等待不闪
  waitForWiFiOrReboot(reset_provisioned ? provWait : noProvWait, reset_provisioned);
  delay(100);

  neopixelWrite(0,GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS);

  if (RTC_chip){
    while (!init_RTC_from_net()){
      neopixelBlink(0, GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 3, 1000);
      // if (!init_RTC_from_bq32002()){
      //   RTC_error();
      // }
    }
  }
  else{
    while (!init_RTC_from_net()){
      neopixelBlink(0, GCU_RGB_BRIGHTNESS, GCU_RGB_BRIGHTNESS, 3, 1000);
      // RTC_error();
    }
  }

  ClearBootCounter();
  

  //set the resolution to 12 bits (0-4096)
  analogReadResolution(12);
  //Enable Timer Interrupt
  neopixelWrite(GCU_RGB_BRIGHTNESS,GCU_RGB_BRIGHTNESS,0);
  // data_receiver.attach_ms(1000/device_frequency, dataReceive);
  data_receiver.attach_ms(1000/device_frequency, on_timer);

}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  if (IMU_flag){
    if (IMU_chip){
      IMU.readGyroscope(BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z);
      IMU.readAcceleration(BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z);
      IMU.readMagneticField(BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z);
    }
    else{
      bmx160.getAllData(&Omagn, &Ogyro, &Oaccel);
    }
  }
  if (data_ready) {
    data_ready = false;
    dataReceive(); 
  }
}

