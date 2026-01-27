#ifndef FILTER_H
#define FILTER_H

#include "SensorConfig.h"

struct FilterConfig {
  bool enabled;
  uint8_t medianWindow;
  float alpha;
};

extern FilterConfig g_filterConfig;
extern const float kFilterAlphaMin;
extern const float kFilterAlphaMax;
extern const uint8_t kFilterDefaultMedian;
extern const float kFilterDefaultAlpha;

void filterInitFromNvs();
bool applyFilterConfig(const FilterConfig& cfg, bool persist, bool enforceLicense);
void resetSensorFilterState();
float filterProcessSample(uint16_t sampleMv, uint16_t sensorIndex, bool licenseOk);

#endif // FILTER_H
