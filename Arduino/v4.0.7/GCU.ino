// Version 4.0

/*
Note: This sketch takes up a lot of space for the app and may not be able to flash with default setting on some chips.
  If you see Error like this: "Sketch too big"
  In Arduino IDE go to: Tools > Partition scheme > chose anything that has more than 1.4MB APP with OTA
   - for example "Minimal SPIFFs (1.9MB APP with OTA/190KB SPIFFS)" 
*/

#include "GCU.h"
#include "Provisioning.h"

constexpr char kFirmwareVersion[] = "4.0.7";
// Active hardware model descriptor
const char* kExpectedModel = "v2.2.c";

/*----------------- Basic Config ----------------*/
// You can modify the base configuration to match your research.


// Device Parameters
const uint16_t device_frequency = 100;
const uint16_t calibration_duration = 10000;
constexpr uint32_t kSensorServerPollIntervalMs = 50;
constexpr uint32_t kUdpControlPollIntervalMs = 50;

// Data Format Function
const bool start_flag = GCU_FLAG_ON;
const bool device_num_flag = GCU_FLAG_ON;
const bool sensors_num_flag = GCU_FLAG_ON;
const bool timestamp_flag = GCU_FLAG_ON;
const bool IMU_flag = GCU_FLAG_ON;
const bool end_flag = GCU_FLAG_ON;

// Data Transfer Parameters
const uint16_t port = 13250;

//Normalized Calibration Function
const bool normalized_calibration_flag = GCU_FLAG_OFF;
const float normalized_calibration_max_factor = 0.2;
const float normalized_calibration_min_factor = 0.2;

/*----------------- Advanced Config ----------------*/
// Advanced settings include chip selection, time zone settings, and network configuration.
// If you are unfamiliar with your hardware specifications, it is not recommended to change advanced configurations.

// IMU Chip: BMI270 + BMM150 combo (legacy BMX160 removed)

// RTC hardware (BQ32002) removed; use NTP only

// WiFi Provision
const char *pop = "abcd1234";           // Proof of possession - otherwise called a PIN - string provided by the device, entered by the user in the phone app
const char *service_key = NULL;         // Password used for SofAP method (NULL = no password needed)
bool reset_provisioned = false;          // When true the library will automatically delete previously provisioned data

ProvisioningContext g_provCtx;




// Times Setting
const int UTC = 9;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.nist.gov";
const int  gmtOffset_sec = 3600 * UTC;
const int  daylightOffset_sec = 0;

unsigned int sys_epoch;
unsigned short sys_millis;


bool working_flag = 0;

float BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z;
float BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z;
float BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z;

Ticker data_receiver;
ESP32Time rtc(0*3600);

bool data_ready = false;
uint8_t bootcnt_local = 0;
bool windowCleared = false;

Preferences prefs;
volatile bool g_wifiGotIP = false;


void setup() {
  delay(300);
  Serial.begin(115200);
  delay(100);
  Serial.printf("GCU firmware %s (model %s)\n", kFirmwareVersion, kExpectedModel);
  Serial.setDebugOutput(true);
  esp_log_level_set("*", ESP_LOG_INFO);
  delay(100);
  Wire.begin(GCU_SDA,GCU_SCL,1000000);
  delay(100);

  
  uint64_t mac = ESP.getEfuseMac(); // 64-bit MAC (lower 48 bits used)
  initSensorConfig(mac);

  // Factory reset on higher reboot-count threshold (erases NVS and restarts)
  checkMultiPowerCycleFactoryReset(10);

  // Force re-provision on lower reboot-count threshold
  reset_provisioned = checkMultiPowerCycleForceProvision(5);

  neopixelRainbow(10);
  ledShowColor(LedPalette::Off); // Off / black
  delay(300);

  // if (wasButtonLongPressedOnBoot(0, 3000)) {
  //   Serial.println("Detected long press: force re-provisioning.");
  //   reset_provisioned = true;
  // } else {
  //   Serial.println("No long press: try existing Wi-Fi.");
  // }

  //init IMU
  if (IMU_flag){
    if (!IMU.begin()) {
      Serial.println("Failed to initialize IMU!");
      ledShowColor(LedPalette::FatalImu);
      while (1);
    }
    float gyroRate = IMU.gyroscopeSampleRate();
    Serial.print("Gyroscope sample rate = ");
    Serial.print(gyroRate);
    Serial.println(" Hz");
    Serial.println();
    float accelRate = IMU.accelerationSampleRate();
    Serial.print("Accelerometer sample rate = ");
    Serial.print(accelRate);
    Serial.println(" Hz");
    Serial.println();
    float magnRate = IMU.magneticFieldSampleRate();
    Serial.print("MagneticField sample rate = ");
    Serial.print(magnRate);
    Serial.println(" Hz");
    Serial.println();
  } 
 

  ledShowColor(LedPalette::WifiProvision); // Wi-Fi provisioning

  provisioningBuildContext(g_provCtx, mac);
  Serial.println(g_provCtx.serviceName);




  WiFi.onEvent(SysProvEvent);
  WiFi.begin(); // no SSID/PWD - get it from the Provisioning APP or from NVS (last successful connection)
  

  // BLE provisioning (UUID v1 based on time and device MAC)
  Serial.println("Begin Provisioning using BLE");
  provisioningLogUuid(g_provCtx);
  provisioningStart(g_provCtx, reset_provisioned, pop, service_key);


  Serial.print("MAC Address: ");
  for (int i = 5; i >= 0; i--) {
    Serial.printf("%02X", (uint8_t)((mac >> (8 * i)) & 0xFF));
    if (i > 0) Serial.print(":");
  }
  Serial.println();
  Serial.println(mac, HEX);

  if(normalized_calibration_flag == GCU_FLAG_ON)
  {
    normalizedCalibrationInit(normalized_calibration_method_mean);
    Serial.println("normalizedCalibrationInit: Completed");
  }



  const uint32_t noProvWait = 20000;      // 20s
  const uint32_t provWait   =120000;     // 2min


  bool needProvisionNow = reset_provisioned || !hasSavedWiFiCredentials();
  if (needProvisionNow) {
    // Enter BLE/SoftAP provisioning; long wait with red blinking
    waitForWiFiOrReboot(provWait, true);
  } else {
    // Normal path: connect using existing credentials
    waitForWiFiOrReboot(noProvWait, false);
  }
  beginSensorConfigServer();

  if (!udp.begin(port)) {
    Serial.printf("UDP: failed to bind port %u for control commands\n", port);
  } else {
    Serial.printf("UDP: listening on port %u, waiting for GCU_SUBSCRIBE\n", port);
  }

  while (!init_RTC_from_net()){
    ledBlinkColor(LedPalette::RtcSyncWait, 3, 1000);
  }

  if (!windowCleared) {
    ClearBootCounter();
  }

  if (runOnlineOtaUpdateIfAvailable(kFirmwareVersion)) {
    return;
  }
  

  //set the resolution to 12 bits (0-4096)
  analogReadResolution(12);
  //Enable Timer Interrupt
  ledShowColor(LedPalette::ReadyIdle);
  // data_receiver.attach_ms(1000/device_frequency, dataReceive);
  data_receiver.attach_ms(1000/device_frequency, on_timer);

}

/**
 * @brief Main loop: handle config server, update IMU buffer, and flush frames.
 */
void loop() {
  const unsigned long now = millis();
  static unsigned long lastSensorCfgPoll = 0;
  static unsigned long lastUdpCtrlPoll = 0;

  if (now - lastSensorCfgPoll >= kSensorServerPollIntervalMs) {
    handleSensorConfigServer();
    lastSensorCfgPoll = now;
  }

  if (now - lastUdpCtrlPoll >= kUdpControlPollIntervalMs) {
    serviceUdpControlChannel();
    lastUdpCtrlPoll = now;
  }

  if (data_ready) {
    data_ready = false;
    dataReceive(); 
  }
}


