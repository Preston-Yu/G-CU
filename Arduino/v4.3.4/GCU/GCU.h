#ifndef __GCU_H__
#define __GCU_H__


#include "esp32-hal.h"
#include "esp_mac.h"
#include "esp32-hal-rgb-led.h"
#include "esp_task_wdt.h"
#include <ESP32Time.h>
#include "time.h"
#include "sntp.h"
#include "WiFi.h"
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <Update.h>
#include <Ticker.h>
#include <Wire.h>
#include "Arduino_BMI270_BMM150.h"
#include "sdkconfig.h"
#include "WiFiProv.h"
#include <Preferences.h>
#include "SensorConfig.h"
#include "Filter.h"
#include "Calibration.h"
#include <FS.h>
#include <SPIFFS.h>
#include <vector>
#include <License.h>
#include "ModelConfig.h"

class Print;

// Unified logging format with level control (DBG lowest, ERR highest)
enum class LogLevel : uint8_t {
  Debug = 0,
  Info = 1,
  Warn = 2,
  Error = 3
};

extern LogLevel g_logLevel;

inline bool logEnabled(LogLevel lvl) {
  return static_cast<uint8_t>(lvl) >= static_cast<uint8_t>(g_logLevel);
}

void logPrintf(const char* level_str, const char* module, LogLevel lvl, const char* fmt, ...);

#define LOG_BASE(level_str, module, lvl_enum, fmt, ...) \
  do { \
    if (logEnabled(lvl_enum)) { \
      logPrintf(level_str, module, lvl_enum, fmt, ##__VA_ARGS__); \
    } \
  } while (0)
#define LOGI(module, fmt, ...)  LOG_BASE("INFO", module, LogLevel::Info, fmt, ##__VA_ARGS__)
#define LOGW(module, fmt, ...)  LOG_BASE("WARN", module, LogLevel::Warn, fmt, ##__VA_ARGS__)
#define LOGE(module, fmt, ...)  LOG_BASE("ERR",  module, LogLevel::Error, fmt, ##__VA_ARGS__)
#define LOGD(module, fmt, ...)  LOG_BASE("DBG",  module, LogLevel::Debug, fmt, ##__VA_ARGS__)

//System Define
//! DONT NEED CHANGE
#define GCU_SDA 48
#define GCU_SCL 47
#define GCU_LED_PIN 38
#define GCU_RGB_BRIGHTNESS 20
constexpr size_t GCU_LOG_FILE_MAX_BYTES = 32 * 1024;
// RTC hardware BQ32002 has been removed from project

#define GCU_FLAG_ON 1
#define GCU_FLAG_OFF 0

#define normalized_calibration_method_peak 0x00
#define normalized_calibration_method_mean 0x01

// Boot counter window (ms) -?exceed to auto-clear during long waits
#define BOOTCNT_CLEAR_WINDOW_MS 5000

struct LedColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

constexpr uint8_t LED_LEVEL_HIGH = GCU_RGB_BRIGHTNESS;
constexpr uint8_t LED_LEVEL_MED = (GCU_RGB_BRIGHTNESS * 3) / 4;
constexpr uint8_t LED_LEVEL_LOW = GCU_RGB_BRIGHTNESS / 4;

namespace LedPalette {
  constexpr LedColor Off{0, 0, 0};
  constexpr LedColor FatalImu{LED_LEVEL_HIGH, LED_LEVEL_HIGH, LED_LEVEL_HIGH};
  constexpr LedColor WifiProvision{LED_LEVEL_HIGH, 0, 0};
  constexpr LedColor WifiTimeout{LED_LEVEL_HIGH, LED_LEVEL_LOW, 0};
  constexpr LedColor RtcSyncWait{0, LED_LEVEL_HIGH, LED_LEVEL_LOW};
  constexpr LedColor ReadyIdle{LED_LEVEL_HIGH, LED_LEVEL_MED, 0};
  constexpr LedColor StreamActive{0, LED_LEVEL_HIGH, 0};
  constexpr LedColor StreamSocketError{LED_LEVEL_HIGH, LED_LEVEL_LOW, LED_LEVEL_LOW};
  constexpr LedColor CalibPrep{LED_LEVEL_LOW, LED_LEVEL_MED, LED_LEVEL_HIGH};
  constexpr LedColor CalibSampling{LED_LEVEL_HIGH, LED_LEVEL_LOW, LED_LEVEL_HIGH};
  constexpr LedColor CalibError{LED_LEVEL_HIGH, 0, LED_LEVEL_HIGH};
  constexpr LedColor OtaManifest{0, 0, LED_LEVEL_HIGH};
  constexpr LedColor OtaPreparing{LED_LEVEL_LOW, 0, LED_LEVEL_HIGH};
  constexpr LedColor OtaSuccess{LED_LEVEL_LOW, LED_LEVEL_HIGH, LED_LEVEL_HIGH};
  constexpr LedColor OtaError{LED_LEVEL_HIGH, 0, LED_LEVEL_LOW};
  // Amber/orange to avoid confusion with OTA blue
  constexpr LedColor LicenseInvalid{LED_LEVEL_HIGH, LED_LEVEL_LOW, 0};
}



// (BQ32002 types removed)

//External Variable Declaration
// Device Parameters
extern bool data_ready;
extern const uint16_t device_frequency;
extern const uint16_t calibration_duration;

// Sensor Numbers
extern uint8_t sensors_rows_num;
extern uint8_t sensors_columns_num;
extern uint16_t sensors_num;

// Data Format Function
extern const bool start_flag;
extern const bool device_num_flag;
extern const bool sensors_num_flag;
extern const bool timestamp_flag;
extern const bool IMU_flag;
extern const bool end_flag;

extern const char* kExpectedModel;

extern const bool normalized_calibration_flag;
extern const float normalized_calibration_max_factor;
extern const float normalized_calibration_min_factor;

// (RTC hardware flag removed)

extern size_t data_frame_len;
extern size_t sensor_data_offset;
extern size_t sensor_data_bytes;

//RTC
extern ESP32Time rtc;
extern const char* ntpServer1;
extern const char* ntpServer2;
extern const int  gmtOffset_sec;
extern const int  daylightOffset_sec;

extern unsigned int sys_epoch;
extern unsigned short sys_millis;

// WiFi Parameters
extern const uint16_t port;

extern uint8_t bootcnt_local;
extern bool windowCleared;
extern Preferences prefs;

extern WiFiUDP udp;
extern IPAddress g_udpTargetIP;
extern volatile bool g_wifiGotIP;


extern bool working_flag;

extern float BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z;
extern float BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z;
extern float BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z;
extern bool g_calibrationEnabled;
extern bool g_calibrationModeActive;
bool calibrationApply(float rawMv, uint16_t sensorIndex, float& outNorm);
bool calibrationLoadFromSpiffs();
bool calibrationSaveToSpiffs();
void calibrationResetSession();
void rebuildCalibrationIndexMap();


//Function Declaration
bool runOnlineOtaUpdateIfAvailable(const char* currentVersion);

void neopixelWrite(uint8_t red_val, uint8_t green_val, uint8_t blue_val);
void neopixelBlink(uint8_t red_val, uint8_t green_val, uint8_t blue_val, uint8_t cycle_times, uint16_t delayms);
void neopixelRainbow(uint8_t delayms);

inline void ledShowColor(const LedColor& color) {
  neopixelWrite(color.r, color.g, color.b);
}

inline void ledBlinkColor(const LedColor& color, uint8_t cycle_times, uint16_t delayms) {
  neopixelBlink(color.r, color.g, color.b, cycle_times, delayms);
}

bool init_RTC_from_net();
// (BQ32002 RTC helpers removed)
void RTC_error();


// (BQ32002 conversion helpers removed)


void normalizedCalibrationInit(unsigned char method);
void dataReceive();
void on_timer();
void configureMatrixPinsForScan();
void resetSensorFilterState();
bool applyFilterConfig(const FilterConfig& cfg, bool persist, bool enforceLicense);
void loadFilterConfigFromNvs();
void serviceUdpControlChannel();
void setLicenseWarning(bool on);
bool licenseWarningActive();
void logLoadConfig();
void logBegin();
void logService();
bool logSetEnabled(bool enabled, LogLevel level);
bool logIsEnabled();
LogLevel logConfiguredLevel();
const char* logLevelName(LogLevel level);
String logStatusJson();
Print& logErrorPrint();
bool logReadOrderedBytes(std::vector<uint8_t>& ordered, uint32_t& offset);
bool logReadOrderedBase64(String& outB64, size_t& rawSize, uint32_t& offset);

void SysProvEvent(arduino_event_t *sys_event);
bool waitForWiFiOrReboot(uint32_t timeout_ms, bool blinkDuringProvision);
bool hasSavedWiFiCredentials();

bool wasButtonLongPressedOnBoot(unsigned char BUTTON_PIN, unsigned long pressThresholdMs);

bool checkMultiPowerCycleForceProvision(unsigned int POWER_CYCLE_THRESHOLD);
bool checkMultiPowerCycleFactoryReset(unsigned int FACTORY_RESET_THRESHOLD);
void ClearBootCounter();
// Clear boot counter once when a window elapses while waiting
void clearBootCounterIfTimeout(uint32_t window_ms);


#endif // __GCU_H__
