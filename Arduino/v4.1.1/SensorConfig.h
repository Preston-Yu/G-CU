#ifndef SENSOR_CONFIG_H
#define SENSOR_CONFIG_H

#include <Arduino.h>

constexpr size_t kMaxAnalogPins = 11;
constexpr size_t kMaxSelectPins = 13;
constexpr size_t kMaxSensors = kMaxAnalogPins * kMaxSelectPins;
constexpr size_t kSensorValueBytes = 4;
constexpr size_t kTimestampBytes = 6;
constexpr size_t kImuBytes = 4 * 3 * 3;
constexpr size_t kMaxDataFrameLen =
  2 + 6 + 1 + (kMaxSensors * kSensorValueBytes) + kTimestampBytes + kImuBytes + 2;

extern uint8_t analogReadIO[kMaxAnalogPins];
extern uint8_t SelectIO[kMaxSelectPins];
extern uint8_t sensors_rows_num;
extern uint8_t sensors_columns_num;
extern uint16_t sensors_num;

extern unsigned char data[kMaxDataFrameLen];
extern unsigned char* data_p;
extern size_t data_frame_len;
extern size_t sensor_data_offset;
extern size_t sensor_data_bytes;

extern float maxMillVolts[kMaxSensors];
extern float minMillVolts[kMaxSensors];

void initSensorConfig(uint64_t deviceMac);
bool sensorConfigApplyJson(const String& json);
bool sensorConfigSaveJson(const String& json);
bool sensorConfigLoadFromNvs();
const char* sensorConfigLastError();

#endif // SENSOR_CONFIG_H
