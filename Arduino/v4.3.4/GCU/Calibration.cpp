#include "GCU.h"
#include "SensorConfig.h"

#include <vector>
#include <map>
#include <cmath>

struct CalibTable {
  std::vector<float> values; // size kMaxAnalogPins * kMaxSelectPins, NAN when missing
};

static std::map<float, CalibTable> s_levels;
static size_t g_canonicalMap[kMaxSensors]; // sensorIndex -> canonical index, SIZE_MAX when invalid

// Map to full hardware-supported pin lists to avoid coordinate drift from custom ordering
namespace {
constexpr uint8_t kAnalogPinsV21[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
constexpr uint8_t kSelectPinsV21[] = {18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};
constexpr uint8_t kAnalogPinsV22C[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
constexpr uint8_t kSelectPinsV22C[] = {17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45};
constexpr uint8_t kAnalogPinsV23D[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
constexpr uint8_t kSelectPinsV23D[] = {16, 17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45, 46};

template <size_t N>
int indexOfPin(uint8_t pin, const uint8_t (&list)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (list[i] == pin) return static_cast<int>(i);
  }
  return -1;
}

bool pinToCanonicalIndex(uint8_t analogPin, uint8_t selectPin, size_t& rowIdx, size_t& colIdx) {
  // Choose the hardware whitelist based on model
  if (kExpectedModel && strcmp(kExpectedModel, "v2.3.d") == 0) {
    int r = indexOfPin(analogPin, kAnalogPinsV23D);
    int c = indexOfPin(selectPin, kSelectPinsV23D);
    if (r >= 0 && c >= 0) {
      rowIdx = static_cast<size_t>(r);
      colIdx = static_cast<size_t>(c);
      return true;
    }
  } else if (kExpectedModel && strcmp(kExpectedModel, "v2.2.c") == 0) {
    int r = indexOfPin(analogPin, kAnalogPinsV22C);
    int c = indexOfPin(selectPin, kSelectPinsV22C);
    if (r >= 0 && c >= 0) {
      rowIdx = static_cast<size_t>(r);
      colIdx = static_cast<size_t>(c);
      return true;
    }
  } else {
    int r = indexOfPin(analogPin, kAnalogPinsV21);
    int c = indexOfPin(selectPin, kSelectPinsV21);
    if (r >= 0 && c >= 0) {
      rowIdx = static_cast<size_t>(r);
      colIdx = static_cast<size_t>(c);
      return true;
    }
  }
  return false;
}
}  // namespace

static size_t sensorIndexForPins(uint8_t analogPin, uint8_t selectPin) {
  size_t rowIdx = SIZE_MAX;
  size_t colIdx = SIZE_MAX;
  if (!pinToCanonicalIndex(analogPin, selectPin, rowIdx, colIdx)) {
    return SIZE_MAX;
  }
  return rowIdx * kMaxSelectPins + colIdx;
}

void rebuildCalibrationIndexMap() {
  for (size_t i = 0; i < kMaxSensors; ++i) {
    g_canonicalMap[i] = SIZE_MAX;
  }
  for (size_t r = 0; r < sensors_rows_num; ++r) {
    for (size_t c = 0; c < sensors_columns_num; ++c) {
      size_t sensorIdx = r * sensors_columns_num + c;
      size_t cr = SIZE_MAX;
      size_t cc = SIZE_MAX;
      if (pinToCanonicalIndex(analogReadIO[r], SelectIO[c], cr, cc)) {
        g_canonicalMap[sensorIdx] = cr * kMaxSelectPins + cc;
      }
    }
  }
}

static bool readCsvLevel(const char* path, float levelVal) {
  File f = SPIFFS.open(path, "r");
  if (!f) return false;
  CalibTable table;
  table.values.assign(kMaxAnalogPins * kMaxSelectPins, NAN);
  String line;
  size_t row = 0;
  while (f.available() && row < kMaxAnalogPins) {
    line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) {
      ++row;
      continue;
    }
    int col = 0;
    int start = 0;
    for (int i = 0; i <= line.length(); ++i) {
      if (i == line.length() || line[i] == ',') {
        String token = line.substring(start, i);
        token.trim();
        if (token.length() > 0) {
          float val = token.toFloat();
          size_t idx = row * kMaxSelectPins + col;
          if (idx < table.values.size()) table.values[idx] = val;
        }
        start = i + 1;
        ++col;
      }
    }
    ++row;
  }
  s_levels[levelVal] = table;
  f.close();
  return true;
}

bool calibrationLoadFromSpiffs() {
  s_levels.clear();
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) {
    LOGE("CAL", "spiffs_root_unavailable");
    return false;
  }
  File file = root.openNextFile();
  while (file) {
    String name = file.name();              // May include or omit leading '/'
    String baseName = name;
    if (baseName.startsWith("/")) baseName = baseName.substring(1);
    if (baseName.startsWith("calib_") && baseName.endsWith(".csv")) {
      String levelStr = baseName.substring(String("calib_").length(), baseName.length() - 4);
      float level = levelStr.toFloat();
      String path = name.startsWith("/") ? name : ("/" + name);
      readCsvLevel(path.c_str(), level);
    }
    file = root.openNextFile();
  }
  bool any = !s_levels.empty();
  LOGI("CAL", "load status=%s levels=%u", any ? "ok" : "empty", static_cast<unsigned>(s_levels.size()));
  return any;
}

bool calibrationSaveToSpiffs() {
  bool ok = true;
  for (const auto& kv : s_levels) {
    float level = kv.first;
    const CalibTable& table = kv.second;
    char fname[32];
    dtostrf(level, 0, 2, fname);
    String path = "/calib_";
    path += String(fname);
    path.trim();
    path += ".csv";
    File f = SPIFFS.open(path, "w");
    if (!f) {
      LOGE("CAL", "save_open_failed path=%s", path.c_str());
      ok = false;
      continue;
    }
    for (size_t r = 0; r < kMaxAnalogPins; ++r) {
      for (size_t c = 0; c < kMaxSelectPins; ++c) {
        size_t idx = r * kMaxSelectPins + c;
        if (idx < table.values.size() && !isnan(table.values[idx])) {
          if (f.print(table.values[idx], 3) == 0) {
            ok = false;
          }
        }
        if (c + 1 < kMaxSelectPins && f.print(",") == 0) {
          ok = false;
        }
      }
      if (f.println() == 0) {
        ok = false;
      }
    }
    f.close();
    if (ok) {
      LOGI("CAL", "saved path=%s", path.c_str());
    }
  }
  return ok;
}

void calibrationResetSession() {
  g_calibrationEnabled = false;
  g_calibrationModeActive = false;
}

bool calibrationRemoveLevel(float level) {
  auto it = s_levels.find(level);
  if (it == s_levels.end()) return false;
  s_levels.erase(it);
  char fname[32];
  dtostrf(level, 0, 2, fname);
  String path = "/calib_";
  path += String(fname);
  path.trim();
  path += ".csv";
  if (SPIFFS.exists(path)) {
    SPIFFS.remove(path);
  }
  return true;
}

bool calibrationHasCompleteData() {
  if (s_levels.empty()) return false;
  // Only verify active matrix pins have data, mapped onto the canonical pin table
  for (const auto& kv : s_levels) {
    const CalibTable& t = kv.second;
    if (t.values.size() < kMaxAnalogPins * kMaxSelectPins) return false;
    for (size_t r = 0; r < sensors_rows_num; ++r) {
      for (size_t c = 0; c < sensors_columns_num; ++c) {
        size_t sensorIdx = r * sensors_columns_num + c;
        size_t idx = (sensorIdx < kMaxSensors) ? g_canonicalMap[sensorIdx] : SIZE_MAX;
        if (idx >= t.values.size() || isnan(t.values[idx])) {
          return false;
        }
      }
    }
  }
  return true;
}

void calibrationListLevels(String& outJson) {
  outJson = "[";
  bool first = true;
  for (const auto& kv : s_levels) {
    if (!first) outJson += ",";
    outJson += String(kv.first, 3);
    first = false;
  }
  outJson += "]";
}

bool calibrationDumpLevel(float level, String& outJson) {
  auto it = s_levels.find(level);
  if (it == s_levels.end()) {
    // Allow floating-point tolerance when matching the requested level
    float bestKey = NAN;
    for (const auto& kv : s_levels) {
      if (std::fabs(kv.first - level) < 0.001f) {
        bestKey = kv.first;
        break;
      }
    }
    if (std::isnan(bestKey)) return false;
    it = s_levels.find(bestKey);
    if (it == s_levels.end()) return false;
  }
  const CalibTable& t = it->second;
  outJson = "{\"level\":";
  outJson += String(level, 3);
  outJson += ",\"matrix\":[";
  // Emit full hardware matrix dimensions; unused IO slots are null
  for (size_t r = 0; r < kMaxAnalogPins; ++r) {
    if (r > 0) outJson += ",";
    outJson += "[";
    for (size_t c = 0; c < kMaxSelectPins; ++c) {
      size_t idx = r * kMaxSelectPins + c;
      if (c > 0) outJson += ",";
      if (idx < t.values.size() && !isnan(t.values[idx])) {
        outJson += String(t.values[idx], 3);
      } else {
        outJson += "null";
      }
    }
    outJson += "]";
  }
  outJson += "]}";
  return true;
}

bool calibrationSetPoint(float level, uint8_t analogPin, uint8_t selectPin, float value) {
  size_t idx = sensorIndexForPins(analogPin, selectPin);
  if (idx == SIZE_MAX) {
    return false;
  }
  CalibTable& table = s_levels[level];
  if (table.values.empty()) {
    table.values.assign(kMaxAnalogPins * kMaxSelectPins, NAN);
  }
  if (idx >= table.values.size()) {
    return false;
  }
  table.values[idx] = value;
  return true;
}

bool calibrationApply(float rawMv, uint16_t sensorIndex, float& outNorm) {
  if (!g_calibrationEnabled || s_levels.empty()) return false;
  if (sensorIndex >= kMaxSensors) return false;
  size_t canonicalIdx = g_canonicalMap[sensorIndex];
  if (canonicalIdx == SIZE_MAX || canonicalIdx >= kMaxSensors) return false;

  // Look up calibration points using canonical indices
  std::vector<std::pair<float, float>> pts;
  for (const auto& kv : s_levels) {
    if (canonicalIdx < kv.second.values.size()) {
      float v = kv.second.values[canonicalIdx];
      if (!isnan(v)) pts.push_back({kv.first, v});
    }
  }
  if (pts.size() < 1) return false;
  std::sort(pts.begin(), pts.end(), [](auto& a, auto& b){ return a.first < b.first; });
  float x = rawMv;
  if (x <= pts.front().second) {
    outNorm = pts.front().first;
    return true;
  }
  if (x >= pts.back().second) {
    outNorm = pts.back().first;
    return true;
  }
  for (size_t i = 1; i < pts.size(); ++i) {
    float v0 = pts[i-1].second;
    float v1 = pts[i].second;
    if (x >= v0 && x <= v1) {
      float l0 = pts[i-1].first;
      float l1 = pts[i].first;
      float t = (x - v0) / (v1 - v0 + 1e-6f);
      outNorm = l0 + (l1 - l0) * t;
      return true;
    }
  }
  return false;
}
