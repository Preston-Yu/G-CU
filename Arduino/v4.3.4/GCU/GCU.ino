// Version 4.3

#include "GCU.h"
#include "PowerManager.h"
#include "Provisioning.h"
#include "ControlServer.h"


constexpr char kFirmwareVersion[] = "4.3.4";
// Active hardware model descriptor (configured in ModelConfig.h or build flags)
const char* kExpectedModel = GCU_EXPECTED_MODEL;

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
  Serial.begin(115200);
  delay(100);
  logLoadConfig();
  LOGI("SYS", "firmware=%s model=%s", kFirmwareVersion, kExpectedModel);
  Serial.setDebugOutput(false);
  esp_log_level_set("*", ESP_LOG_WARN);
  delay(100);
  Wire.begin(GCU_SDA, GCU_SCL, GCU_I2C_HZ);
  delay(100);
  powerManagerSetup();
  if (!SPIFFS.begin(false)) { // Do not auto-format; attempt one format if mount fails
    LOGW("FS", "spiffs_mount status=fail action=format");
    if (SPIFFS.begin(true)) {
      logBegin();
      LOGI("SYS", "firmware=%s model=%s", kFirmwareVersion, kExpectedModel);
      LOGI("FS", "spiffs_mount status=formatted");
    } else {
      LOGE("FS", "spiffs_mount status=fail_after_format");
    }
  } else {
    logBegin();
    LOGI("SYS", "firmware=%s model=%s", kFirmwareVersion, kExpectedModel);
    LOGI("FS", "spiffs_mount status=ok");
  }

  uint8_t macBytes[6] = {0};
  esp_read_mac(macBytes, ESP_MAC_WIFI_STA);
  uint64_t mac = 0;
  for (int i = 0; i < 6; ++i) {
    mac = (mac << 8) | macBytes[i];
  }
  initSensorConfig(mac);
  licenseInit(mac);

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
      LOGE("IMU", "init status=failed");
      ledShowColor(LedPalette::FatalImu);
      while (1);
    }
    float gyroRate = IMU.gyroscopeSampleRate();
    float accelRate = IMU.accelerationSampleRate();
    float magnRate = IMU.magneticFieldSampleRate();
    LOGI("IMU", "init gyro_hz=%.2f accel_hz=%.2f magn_hz=%.2f", gyroRate, accelRate, magnRate);
  } 
 

  ledShowColor(LedPalette::WifiProvision); // Wi-Fi provisioning

  provisioningBuildContext(g_provCtx, mac);
  LOGI("PROV", "service_name=%s", g_provCtx.serviceName);




  WiFi.onEvent(SysProvEvent);
  WiFi.begin(); // no SSID/PWD - get it from the Provisioning APP or from NVS (last successful connection)
  

  // BLE provisioning (UUID v1 based on time and device MAC)
  LOGI("PROV", "begin_ble_provision");
  provisioningLogUuid(g_provCtx);
  provisioningStart(g_provCtx, reset_provisioned, pop, service_key);


  char macStr[18] = {0};
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           macBytes[0], macBytes[1], macBytes[2],
           macBytes[3], macBytes[4], macBytes[5]);
  LOGI("SYS", "mac=%s hex=%llX", macStr, static_cast<unsigned long long>(mac));

  if(normalized_calibration_flag == GCU_FLAG_ON)
  {
    normalizedCalibrationInit(normalized_calibration_method_mean);
    LOGI("CAL", "normalized_calibration status=completed");
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
  beginControlServer();

  if (!udp.begin(port)) {
    LOGE("UDP", "bind status=fail port=%u", port);
  } else {
    LOGI("UDP", "listening port=%u waiting=GCU_SUBSCRIBE", port);
  }

  while (!init_RTC_from_net()){
    ledBlinkColor(LedPalette::RtcSyncWait, 3, 1000);
  }

  bool licenseOk = licenseValidateStored(rtc.getEpoch());
  if (licenseOk) {
    time_t exp = static_cast<time_t>(g_licenseInfo.expiryEpoch);
    struct tm tm_exp;
    gmtime_r(&exp, &tm_exp);
    LOGI("LICENSE", "status=ok tier=%s expiry=%04d-%02d-%02dT%02d:%02d:%02dZ",
          licenseTierName(g_licenseInfo.tier),
          tm_exp.tm_year + 1900, tm_exp.tm_mon + 1, tm_exp.tm_mday,
          tm_exp.tm_hour, tm_exp.tm_min, tm_exp.tm_sec);
    setLicenseWarning(false);
  } else {
    const char* reason = licenseLastError();
    LOGW("LICENSE", "status=invalid reason=%s", reason ? reason : "unknown");
    ledShowColor(LedPalette::LicenseInvalid);
    setLicenseWarning(true);
  }
  calibrationLoadFromSpiffs();
  g_calibrationEnabled = calibrationHasCompleteData();
  filterInitFromNvs();

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
    handleControlServer();
    lastSensorCfgPoll = now;
  }

  if (now - lastUdpCtrlPoll >= kUdpControlPollIntervalMs) {
    serviceUdpControlChannel();
    lastUdpCtrlPoll = now;
  }
  logService();

  if (data_ready) {
    data_ready = false;
    dataReceive(); 
  }
}
