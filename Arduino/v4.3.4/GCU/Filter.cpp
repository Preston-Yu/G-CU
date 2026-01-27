#include "Filter.h"
#include "GCU.h"
#include <Preferences.h>

const float kFilterAlphaMin = 0.05f;
const float kFilterAlphaMax = 0.6f;
const uint8_t kFilterDefaultMedian = 3;
const float kFilterDefaultAlpha = 0.25f;
constexpr uint8_t kFilterMedianMax = 5;

FilterConfig g_filterConfig{false, kFilterDefaultMedian, kFilterDefaultAlpha};

namespace {

struct SensorFilterState {
  float lowpass = 0.0f;
  bool lpInit = false;
  uint16_t medianBuf[kFilterMedianMax] = {0};
  uint8_t medianCount = 0;
  uint8_t medianCursor = 0;
};

SensorFilterState g_filterState[kMaxSensors];
Preferences g_filterPrefs;

uint16_t medianOfBuffer(const uint16_t* buf, uint8_t count) {
  uint16_t tmp[kFilterMedianMax];
  for (uint8_t i = 0; i < count; ++i) {
    tmp[i] = buf[i];
  }
  for (uint8_t i = 0; i < count; ++i) {
    for (uint8_t j = i + 1; j < count; ++j) {
      if (tmp[j] < tmp[i]) {
        uint16_t v = tmp[i];
        tmp[i] = tmp[j];
        tmp[j] = v;
      }
    }
  }
  return tmp[count / 2];
}

bool saveFilterConfigToNvs(const FilterConfig& cfg) {
  if (!g_filterPrefs.begin("filter", false)) {
    return false;
  }
  bool ok = g_filterPrefs.putBool("enabled", cfg.enabled);
  ok = g_filterPrefs.putUChar("median", cfg.medianWindow) && ok;
  ok = g_filterPrefs.putFloat("alpha", cfg.alpha) && ok;
  g_filterPrefs.end();
  return ok;
}

} // namespace

void filterInitFromNvs() {
  if (!g_filterPrefs.begin("filter", true)) {
    return;
  }
  bool enabled = g_filterPrefs.getBool("enabled", false);
  uint8_t median = g_filterPrefs.getUChar("median", kFilterDefaultMedian);
  float alpha = g_filterPrefs.getFloat("alpha", kFilterDefaultAlpha);
  g_filterPrefs.end();
  FilterConfig cfg{enabled, median, alpha};
  applyFilterConfig(cfg, false, false);
}

bool applyFilterConfig(const FilterConfig& cfg, bool persist, bool enforceLicense) {
  if (cfg.medianWindow != 1 && cfg.medianWindow != 3 && cfg.medianWindow != 5) {
    return false;
  }
  if (cfg.alpha < kFilterAlphaMin || cfg.alpha > kFilterAlphaMax) {
    return false;
  }
  if (enforceLicense && cfg.enabled && !licenseAtLeast(LICENSE_TIER_ADV)) {
    return false;
  }
  g_filterConfig = cfg;
  resetSensorFilterState();
  if (persist) {
    saveFilterConfigToNvs(cfg);
  }
  return true;
}

void resetSensorFilterState() {
  for (size_t i = 0; i < kMaxSensors; ++i) {
    g_filterState[i] = SensorFilterState{};
  }
}

float filterProcessSample(uint16_t sampleMv, uint16_t sensorIndex, bool licenseOk) {
  static bool warnedLicense = false;
  static bool ledWarned = false;
  if (!g_filterConfig.enabled) {
    return static_cast<float>(sampleMv);
  }
  if (!licenseOk) {
    if (!warnedLicense) {
      LOGW("FILTER", "inactive reason=license tier_required=Advanced");
      warnedLicense = true;
    }
    setLicenseWarning(true);
    ledWarned = true;
    return static_cast<float>(sampleMv);
  } else {
    warnedLicense = false;
    ledWarned = false;
    if (licenseWarningActive()) {
      setLicenseWarning(false);
    }
  }
  if (sensorIndex >= kMaxSensors) {
    return static_cast<float>(sampleMv);
  }
  SensorFilterState& st = g_filterState[sensorIndex];
  const uint8_t window = g_filterConfig.medianWindow;
  uint16_t median = sampleMv;
  if (window > 1) {
    st.medianBuf[st.medianCursor] = sampleMv;
    st.medianCursor = (st.medianCursor + 1) % window;
    if (st.medianCount < window) {
      ++st.medianCount;
    }
    median = medianOfBuffer(st.medianBuf, st.medianCount);
  }
  if (!st.lpInit) {
    st.lowpass = static_cast<float>(median);
    st.lpInit = true;
  } else {
    st.lowpass = g_filterConfig.alpha * static_cast<float>(median) +
                 (1.0f - g_filterConfig.alpha) * st.lowpass;
  }
  return st.lowpass;
}
