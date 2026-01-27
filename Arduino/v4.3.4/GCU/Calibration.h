#ifndef CALIBRATION_H
#define CALIBRATION_H

#include <Arduino.h>

bool calibrationApply(float rawMv, uint16_t sensorIndex, float& outNorm);
bool calibrationLoadFromSpiffs();
bool calibrationSaveToSpiffs();
void calibrationResetSession();
bool calibrationSetPoint(float level, uint8_t analogPin, uint8_t selectPin, float value);
bool calibrationHasCompleteData();
bool calibrationRemoveLevel(float level);
void calibrationListLevels(String& outJson);
bool calibrationDumpLevel(float level, String& outJson);

#endif // CALIBRATION_H
