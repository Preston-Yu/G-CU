#include "SensorConfig.h"
#include "GCU.h"

#include <Preferences.h>
#include <cstring>

static void rebuildDataHeader();

namespace {

struct HardwareVariant {
  const char* model;
  const uint8_t* analogPins;
  size_t analogCount;
  const uint8_t* selectPins;
  size_t selectCount;
};

constexpr uint8_t kAnalogPinsV21[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
constexpr uint8_t kSelectPinsV21[] = {18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};
constexpr uint8_t kAnalogPinsV22C[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kSelectPinsV22C[] = {17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};
constexpr uint8_t kAnalogPinsV23D[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint8_t kSelectPinsV23D[] = {16, 17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45, 46};

constexpr size_t kAnalogPinsV21Count = sizeof(kAnalogPinsV21) / sizeof(kAnalogPinsV21[0]);
constexpr size_t kSelectPinsV21Count = sizeof(kSelectPinsV21) / sizeof(kSelectPinsV21[0]);
constexpr size_t kAnalogPinsV22CCount = sizeof(kAnalogPinsV22C) / sizeof(kAnalogPinsV22C[0]);
constexpr size_t kSelectPinsV22CCount = sizeof(kSelectPinsV22C) / sizeof(kSelectPinsV22C[0]);
constexpr size_t kAnalogPinsV23DCount = sizeof(kAnalogPinsV23D) / sizeof(kAnalogPinsV23D[0]);
constexpr size_t kSelectPinsV23DCount = sizeof(kSelectPinsV23D) / sizeof(kSelectPinsV23D[0]);

constexpr HardwareVariant kHardwareVariants[] = {
  {"v2.1", kAnalogPinsV21, kAnalogPinsV21Count, kSelectPinsV21, kSelectPinsV21Count},
  {"v2.2.c", kAnalogPinsV22C, kAnalogPinsV22CCount, kSelectPinsV22C, kSelectPinsV22CCount},
  {"v2.3.d", kAnalogPinsV23D, kAnalogPinsV23DCount, kSelectPinsV23D, kSelectPinsV23DCount}
};

uint64_t g_deviceMac = 0;
size_t g_sensorsCountFieldOffset = SIZE_MAX;
Preferences g_sensorPrefs;

void applyDefaultSensorPins();
size_t computeDataFrameLength(uint16_t sensorCount);
bool parseUintArray(const String& source, const char* key, uint8_t* out, size_t& count, size_t maxCount);
bool parseStringValue(const String& source, const char* key, String& out);
bool applySensorPinLists(const uint8_t* rows, size_t rowCount, const uint8_t* cols, size_t colCount);
bool applySensorConfigJsonInternal(const String& json);
bool saveSensorPinConfigToNvs(const String& json);
bool loadSensorPinConfigFromNvs();
const HardwareVariant* findVariantByModel(const char* model);
bool pinsInWhitelist(const uint8_t* pins, size_t count, const uint8_t* whitelist, size_t whitelistCount);

/**
 * @brief Load the default pin matrix for the compiled hardware model.
 */
void applyDefaultSensorPins() {
  const HardwareVariant* variant = findVariantByModel(kExpectedModel);
  if (!variant) {
    LOGW("SENSOR", "model_whitelist_missing expected=%s", kExpectedModel ? kExpectedModel : "null");
    variant = &kHardwareVariants[0];
  }
  applySensorPinLists(variant->analogPins, variant->analogCount, variant->selectPins, variant->selectCount);
}

/**
 * @brief Compute encoded frame length for a given sensor count.
 * @param sensorCount Active sensors in the current matrix.
 * @return Total bytes reserved for a data frame.
 */
size_t computeDataFrameLength(uint16_t sensorCount) {
  size_t len = 0;
  if (start_flag) len += 2;
  if (device_num_flag) len += 6;
  if (sensors_num_flag) len += 1;
  len += static_cast<size_t>(sensorCount) * kSensorValueBytes;
  if (timestamp_flag) len += kTimestampBytes;
  if (IMU_flag) len += kImuBytes;
  if (end_flag) len += 2;
  return len;
}

/**
 * @brief Parse a JSON array of uint8_t from a raw string payload.
 * @param source Source JSON payload.
 * @param key Key to locate (e.g., "analog").
 * @param out Destination buffer for parsed values.
 * @param count Filled element count (output).
 * @param maxCount Maximum elements allowed.
 */
bool parseUintArray(const String& source, const char* key, uint8_t* out, size_t& count, size_t maxCount) {
  count = 0;
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) {
    return false;
  }
  int arrayStart = source.indexOf('[', keyIdx);
  int arrayEnd = source.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0 || arrayEnd <= arrayStart) {
    return false;
  }
  int cursor = arrayStart + 1;
  while (cursor < arrayEnd && count < maxCount) {
    int nextSep = source.indexOf(',', cursor);
    if (nextSep < 0 || nextSep > arrayEnd) {
      nextSep = arrayEnd;
    }
    String token = source.substring(cursor, nextSep);
    token.trim();
    if (token.length() > 0) {
      int value = token.toInt();
      if (value < 0 || value > 255) {
        return false;
      }
      out[count++] = static_cast<uint8_t>(value);
    }
    cursor = nextSep + 1;
  }
  return count > 0;
}

/**
 * @brief Extract a quoted string value for a key inside the JSON payload.
 * @param source Source JSON string.
 * @param key Key name to search.
 * @param out Output string (only valid when returning true).
 */
bool parseStringValue(const String& source, const char* key, String& out) {
  String keyPattern = "\"" + String(key) + "\"";
  int keyIdx = source.indexOf(keyPattern);
  if (keyIdx < 0) {
    return false;
  }
  int colonIdx = source.indexOf(':', keyIdx + keyPattern.length());
  if (colonIdx < 0) {
    return false;
  }
  int firstQuote = source.indexOf('"', colonIdx);
  if (firstQuote < 0) {
    return false;
  }
  int secondQuote = source.indexOf('"', firstQuote + 1);
  if (secondQuote < 0) {
    return false;
  }
  out = source.substring(firstQuote + 1, secondQuote);
  out.trim();
  return out.length() > 0;
}

/**
 * @brief Copy validated row/column pin arrays into global state.
 * @param rows Analog pin list.
 * @param rowCount Number of analog entries.
 * @param cols Select pin list.
 * @param colCount Number of select entries.
 */
bool applySensorPinLists(const uint8_t* rows, size_t rowCount, const uint8_t* cols, size_t colCount) {
  if (rowCount == 0 || rowCount > kMaxAnalogPins) {
    return false;
  }
  if (colCount == 0 || colCount > kMaxSelectPins) {
    return false;
  }
  size_t total = rowCount * colCount;
  if (total > kMaxSensors) {
    return false;
  }
  memcpy(analogReadIO, rows, rowCount);
  memcpy(SelectIO, cols, colCount);
  sensors_rows_num = rowCount;
  sensors_columns_num = colCount;
  sensors_num = sensors_rows_num * sensors_columns_num;
  rebuildCalibrationIndexMap();
  return true;
}

const char* g_sensorConfigError = nullptr;

/**
 * @brief Validate and apply one JSON configuration payload.
 * @param json Incoming JSON payload string.
 * @return True when configuration is accepted and applied.
 */
bool applySensorConfigJsonInternal(const String& json) {
  g_sensorConfigError = nullptr;
  String model;
  if (!parseStringValue(json, "model", model)) {
    LOGE("SENSOR", "config_missing field=model");
    g_sensorConfigError = "model_required";
    return false;
  }
  if (kExpectedModel && model != kExpectedModel) {
    LOGE("SENSOR", "model_mismatch expected=%s got=%s", kExpectedModel, model.c_str());
    g_sensorConfigError = "model_mismatch";
    return false;
  }
  const HardwareVariant* variant = findVariantByModel(model.c_str());
  if (!variant) {
    LOGE("SENSOR", "model_unknown name=%s", model.c_str());
    g_sensorConfigError = "model_unknown";
    return false;
  }

  uint8_t rows[kMaxAnalogPins];
  size_t rowCount = 0;
  uint8_t cols[kMaxSelectPins];
  size_t colCount = 0;
  if (!parseUintArray(json, "analog", rows, rowCount, kMaxAnalogPins)) {
    LOGE("SENSOR", "config_invalid field=analog");
    g_sensorConfigError = "analog_invalid";
    return false;
  }
  if (!parseUintArray(json, "select", cols, colCount, kMaxSelectPins)) {
    LOGE("SENSOR", "config_invalid field=select");
    g_sensorConfigError = "select_invalid";
    return false;
  }

  if (!pinsInWhitelist(rows, rowCount, variant->analogPins, variant->analogCount)) {
    LOGE("SENSOR", "analog_pin_unsupported");
    g_sensorConfigError = "analog_unsupported";
    return false;
  }
  if (!pinsInWhitelist(cols, colCount, variant->selectPins, variant->selectCount)) {
    LOGE("SENSOR", "select_pin_unsupported");
    g_sensorConfigError = "select_unsupported";
    return false;
  }

  if (!applySensorPinLists(rows, rowCount, cols, colCount)) {
    LOGE("SENSOR", "apply_layout_failed");
    g_sensorConfigError = "apply_failed";
    return false;
  }
  // After IO changes, re-evaluate calibration completeness
  g_calibrationEnabled = calibrationHasCompleteData();

  rebuildDataHeader();
  configureMatrixPinsForScan();
  resetSensorFilterState();
  return true;
}

/**
 * @brief Persist the latest JSON payload to NVS for reuse on reboot.
 * @param json Raw JSON string previously validated.
 */
bool saveSensorPinConfigToNvs(const String& json) {
  if (!g_sensorPrefs.begin("sensorio", false)) {
    return false;
  }
  bool ok = g_sensorPrefs.putString("pins", json) > 0;
  g_sensorPrefs.end();
  return ok;
}

/**
 * @brief Attempt to restore pin configuration from NVS.
 * @return True when a stored payload was valid and applied.
 */
bool loadSensorPinConfigFromNvs() {
  if (!g_sensorPrefs.begin("sensorio", true)) {
    return false;
  }
  String json = g_sensorPrefs.getString("pins", "");
  g_sensorPrefs.end();
  if (json.length() == 0) {
    return false;
  }
  bool ok = applySensorConfigJsonInternal(json);
  if (!ok) {
    applyDefaultSensorPins();
  }
  return ok;
}

/**
 * @brief Lookup the whitelist descriptor for a given hardware model tag.
 * @param model Model string from configuration/firmware.
 */
const HardwareVariant* findVariantByModel(const char* model) {
  if (!model) {
    return nullptr;
  }
  for (const auto& variant : kHardwareVariants) {
    if (strcmp(variant.model, model) == 0) {
      return &variant;
    }
  }
  return nullptr;
}

/**
 * @brief Ensure each provided pin value exists in the model whitelist.
 * @param pins Caller-provided pins (analog or select).
 * @param count Number of pins to verify.
 * @param whitelist Reference whitelist values for the active model.
 * @param whitelistCount Whitelist length.
 */
bool pinsInWhitelist(const uint8_t* pins, size_t count, const uint8_t* whitelist, size_t whitelistCount) {
  for (size_t i = 0; i < count; ++i) {
    bool found = false;
    for (size_t j = 0; j < whitelistCount; ++j) {
      if (pins[i] == whitelist[j]) {
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool sensorConfigApplyJson(const String& json) {
  return applySensorConfigJsonInternal(json);
}

bool sensorConfigSaveJson(const String& json) {
  return saveSensorPinConfigToNvs(json);
}

bool sensorConfigLoadFromNvs() {
  return loadSensorPinConfigFromNvs();
}

const char* sensorConfigLastError() {
  return g_sensorConfigError;
}

uint8_t analogReadIO[kMaxAnalogPins];
uint8_t SelectIO[kMaxSelectPins];
uint8_t sensors_rows_num = kMaxAnalogPins;
uint8_t sensors_columns_num = kMaxSelectPins;
uint16_t sensors_num = kMaxSensors;

unsigned char data[kMaxDataFrameLen];
unsigned char* data_p = data;
size_t data_frame_len = 0;
size_t sensor_data_offset = 0;
size_t sensor_data_bytes = 0;

float maxMillVolts[kMaxSensors];
float minMillVolts[kMaxSensors];

/**
 * @brief Initialize sensor IO state and attempt to restore stored config.
 * @param deviceMac Unique device MAC used when stamping frames.
 */
void initSensorConfig(uint64_t deviceMac) {
  g_deviceMac = deviceMac;
  applyDefaultSensorPins();
  bool restored = loadSensorPinConfigFromNvs();
  if (restored) {
    LOGI("SENSOR", "io_config source=nvs");
  } else {
    LOGI("SENSOR", "io_config source=default");
  }
  for (size_t i = 0; i < kMaxSensors; ++i) {
    maxMillVolts[i] = 0.0f;
    minMillVolts[i] = 0.0f;
  }
  if (!restored) {
    sensor_data_bytes = sensors_num * kSensorValueBytes;
    data_frame_len = computeDataFrameLength(sensors_num);
    rebuildDataHeader();
  }
  configureMatrixPinsForScan();
}

/**
 * @brief Recalculate common frame header/tail sections after IO changes.
 */
void rebuildDataHeader() {
  sensor_data_bytes = sensors_num * kSensorValueBytes;
  data_frame_len = computeDataFrameLength(sensors_num);
  if (data_frame_len > kMaxDataFrameLen) {
    data_frame_len = kMaxDataFrameLen;
  }
  memset(data, 0, kMaxDataFrameLen);
  unsigned char* cursor = data;
  if (start_flag) {
    *cursor++ = 0x5a;
    *cursor++ = 0x5a;
  }
  if (device_num_flag) {
    for (int i = 0; i < 6; ++i) {
      *cursor++ = (g_deviceMac >> (8 * i)) & 0xFF;
    }
  }
  if (sensors_num_flag) {
    g_sensorsCountFieldOffset = cursor - data;
    *cursor++ = sensors_num;
  } else {
    g_sensorsCountFieldOffset = SIZE_MAX;
  }
  size_t timestampBytes = timestamp_flag ? kTimestampBytes : 0;
  size_t imuBytes = IMU_flag ? kImuBytes : 0;
  sensor_data_offset = cursor - data;
  cursor += timestampBytes + sensor_data_bytes + imuBytes;
  if (end_flag) {
    *cursor++ = 0xa5;
    *cursor++ = 0xa5;
  }
  data_frame_len = cursor - data;
  data_p = data + sensor_data_offset;
  if (g_sensorsCountFieldOffset != SIZE_MAX) {
    data[g_sensorsCountFieldOffset] = sensors_num;
  }
  LOGI("SENSOR", "matrix rows=%u cols=%u total=%u", sensors_rows_num, sensors_columns_num, sensors_num);
}
