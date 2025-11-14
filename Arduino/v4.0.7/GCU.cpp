#include "GCU.h"
#include <cstring>

WiFiUDP udp;
const IPAddress kBroadcastIP(255, 255, 255, 255);
IPAddress g_udpTargetIP(kBroadcastIP);

namespace {
constexpr size_t kUdpControlMsgBufLen = 64;
constexpr uint32_t kUnicastLeaseMs = 20000;
constexpr char kCmdSubscribe[] = "GCU_SUBSCRIBE";
constexpr char kCmdBroadcast[] = "GCU_BROADCAST";
constexpr char kCmdAck[] = "GCU_ACK";

bool g_unicastModeActive = false;
unsigned long g_lastSubscribeMs = 0;

void revertToBroadcast(const char* reason) {
  const bool wasBroadcast = (g_udpTargetIP == kBroadcastIP) && !g_unicastModeActive;
  g_udpTargetIP = kBroadcastIP;
  g_unicastModeActive = false;
  if (!wasBroadcast) {
    if (reason && reason[0] != '\0') {
      Serial.printf("UDP: back to broadcast (%s)\n", reason);
    } else {
      Serial.println("UDP: back to broadcast");
    }
  }
}

void armUnicastForRemote(const IPAddress& remoteIp, uint16_t remotePort) {
  g_udpTargetIP = remoteIp;
  g_unicastModeActive = true;
  g_lastSubscribeMs = millis();
  String ipString = remoteIp.toString();
  Serial.printf("UDP: switch to unicast %s:%u\n", ipString.c_str(), port);

  udp.beginPacket(remoteIp, remotePort);
  udp.print(kCmdAck);
  udp.endPacket();
}

void uppercaseBuffer(char* buffer, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (buffer[i] >= 'a' && buffer[i] <= 'z') {
      buffer[i] = buffer[i] - ('a' - 'A');
    }
  }
}
} // namespace


/**
 * @brief Configure analog rows and select columns for faster scanning.
 */
void configureMatrixPinsForScan() {
  for (uint8_t i = 0; i < sensors_rows_num; ++i) {
    pinMode(analogReadIO[i], INPUT);
  }
  for (uint8_t j = 0; j < sensors_columns_num; ++j) {
    pinMode(SelectIO[j], OUTPUT_OPEN_DRAIN);
    digitalWrite(SelectIO[j], HIGH); // release column (high-Z)
  }
}


/**
 * @brief Run the on-boot sensor normalization calibration routine.
 * @param method Calibration method selector (peak or mean) defined in GCU.h.
 */
void normalizedCalibrationInit(unsigned char method)
{

  unsigned long calibration_num = (calibration_duration / 1000) * device_frequency;
  unsigned long calibration_delaytimes_ms = 1000 / device_frequency;
  Serial.print("calibration_num: ");Serial.print(calibration_num); Serial.println("  couts");
  Serial.print("calibration_delaytimes_ms: ");Serial.print(calibration_delaytimes_ms); Serial.println("  ms");

  if(method == normalized_calibration_method_peak){

    ledShowColor(LedPalette::CalibPrep);
    for(uint8_t i = 0; i < sensors_num; i++){
      maxMillVolts[i] = 0; // Initialize max values to 0
      minMillVolts[i] = 3300; // Initialize min values to maximum possible reading (assuming 10-bit ADC)
    }
    delay(5000);
    ledShowColor(LedPalette::CalibSampling);

    // Begin sampling for 5 seconds
    unsigned long startTime = millis();
    while(millis() - startTime < calibration_duration){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);

          uint16_t currentReading = analogReadMilliVolts(analogReadIO[i]); // Read the current value

          pinMode(SelectIO[j],INPUT);
          
          // Update max and min arrays
          uint16_t index = i * sensors_columns_num + j;
          if(currentReading > maxMillVolts[index]){
            maxMillVolts[index] = currentReading * (1 + normalized_calibration_max_factor);
          }
          if(currentReading < minMillVolts[index]){
            minMillVolts[index] = currentReading * (1 - normalized_calibration_min_factor);
          }
        }
      }
      delay(calibration_delaytimes_ms); // Delay between samples
    }

    // Print max and min values for debugging
    Serial.println("maxMillVolts(peak): ");
    for(uint8_t i = 0; i < sensors_num; i++){
      Serial.print(maxMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");
    Serial.println("minMillVolts(peak): ");
    for(uint8_t i = 0; i < sensors_num; i++){
      Serial.print(minMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");




  }
  else if(method == normalized_calibration_method_mean){
    ledShowColor(LedPalette::CalibPrep);
    delay(3000);
    ledShowColor(LedPalette::CalibSampling);

    for(uint16_t calibration_count = 0; calibration_count < calibration_num; calibration_count++){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);
          
          maxMillVolts[i * sensors_columns_num + j] += ((((float)analogReadMilliVolts(analogReadIO[i])) / ((float)calibration_num)) * (1 + normalized_calibration_max_factor));

          pinMode(SelectIO[j],INPUT);
        }
      }
      delay(calibration_delaytimes_ms);
      // Serial.print("calibration_counts: ");Serial.print(calibration_count); Serial.println("  times");
    }


    Serial.print("maxMillVolts: ");
    for(uint16_t i = 0; i < sensors_num; i++){
      Serial.print(maxMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");

    ledBlinkColor(LedPalette::CalibPrep, 5, 1000);
    ledShowColor(LedPalette::CalibSampling);

    for(uint16_t calibration_count = 0; calibration_count < calibration_num; calibration_count++){
      for(uint8_t i = 0; i < sensors_rows_num; i++){
        for(uint8_t j = 0; j < sensors_columns_num; j++){
          pinMode(SelectIO[j],OUTPUT);
          digitalWrite(SelectIO[j], LOW);
          
          minMillVolts[i * sensors_columns_num + j] += ((((float)analogReadMilliVolts(analogReadIO[i])) / ((float)calibration_num)) * (1 - normalized_calibration_min_factor));

          pinMode(SelectIO[j],INPUT);
        }
      }
      delay(calibration_delaytimes_ms);
      // Serial.print("calibration_counts: ");Serial.print(calibration_count); Serial.println("  times");
    }

    Serial.print("minMillVolts: ");
    for(uint16_t i = 0; i < sensors_num; i++){
      Serial.print(minMillVolts[i]); Serial.print("  ");
    }
    Serial.println("  ");
  }

  for(uint16_t i = 0; i < sensors_num; i++){
    if(maxMillVolts[i] <= minMillVolts[i]){
       while(1){
         ledShowColor(LedPalette::CalibError);
         delay(500);
         ledShowColor(LedPalette::CalibSampling);
         delay(500);
         Serial.println("normalizedCalibrationInit: Error");
       }
     }
  }

  ledShowColor(LedPalette::ReadyIdle);
  delay(3000);

  
}





/**
 * @brief Normalize one sensor sample using the stored calibration window.
 * @param milliVolts Raw ADC millivolt reading.
 * @param sensors_number Linearized sensor index inside the matrix.
 */
float normalizedCalibrationCalculate(float milliVolts, uint16_t sensors_number)
{
  return ((milliVolts - (minMillVolts[sensors_number])) / (maxMillVolts[sensors_number] - minMillVolts[sensors_number]));
}

/**
 * @brief Timer callback used by Ticker to flag that a frame should be sent.
 */
void on_timer() {
  data_ready = true;
}


/**
 * @brief Collect sensor/IMU data and push a UDP packet to the configured peer.
 */
void dataReceive(){
  unsigned long t_start = micros();
  unsigned int sys_epoch;
  unsigned short sys_millis;
  uint8_t* cursor = data + sensor_data_offset;

  if(!working_flag){
    ledShowColor(LedPalette::StreamActive);
    working_flag = 1;
  }

  if(timestamp_flag){
    sys_epoch = rtc.getEpoch();
    memcpy(cursor, &sys_epoch, sizeof(sys_epoch));
    cursor += sizeof(sys_epoch);

    sys_millis = rtc.getMillis();
    memcpy(cursor, &sys_millis, sizeof(sys_millis));
    cursor += sizeof(sys_millis);
  }
  
  uint32_t sampleCounter = 0;
  for(uint8_t i = 0; i < sensors_rows_num; i++){
    for(uint8_t j = 0; j < sensors_columns_num; j++){
      const uint8_t selPin = SelectIO[j];
      digitalWrite(selPin, LOW);
      uint16_t milliVolts = analogReadMilliVolts(analogReadIO[i]);
      digitalWrite(selPin, HIGH);
      
      if(normalized_calibration_flag){
        float sensorData = normalizedCalibrationCalculate(static_cast<float>(milliVolts), (i * sensors_columns_num + j));
        memcpy(cursor, &sensorData, sizeof(sensorData));
        cursor += sizeof(sensorData);
      }
      else{
        uint32_t sensorData = milliVolts;
        memcpy(cursor, &sensorData, sizeof(sensorData));
        cursor += sizeof(sensorData);
      }

      ++sampleCounter;
    }
  }

  if (IMU_flag){
    IMU.readGyroscope(BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z);
    IMU.readAcceleration(BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z);
    IMU.readMagneticField(BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z);
    float imuData[] = {
      BMI270_BMM150_magn_x, BMI270_BMM150_magn_y, BMI270_BMM150_magn_z,
      BMI270_BMM150_gyro_x, BMI270_BMM150_gyro_y, BMI270_BMM150_gyro_z,
      BMI270_BMM150_accel_x, BMI270_BMM150_accel_y, BMI270_BMM150_accel_z
    };
    memcpy(cursor, imuData, sizeof(imuData));
    cursor += sizeof(imuData);
  }
  data_p = data + sensor_data_offset;
  

  udp.beginPacket(g_udpTargetIP, port);
  udp.write(data,data_frame_len);
  udp.endPacket();


/*
  if (IMU_flag){
    // Display the magnetometer results (magn is magnetometer in uTesla)
    Serial.print("M ");
    Serial.print("X: "); Serial.print(BMI270_BMM150_magn_x); Serial.print("  ");
    Serial.print("Y: "); Serial.print(BMI270_BMM150_magn_y); Serial.print("  ");
    Serial.print("Z: "); Serial.print(BMI270_BMM150_magn_z); Serial.print("  ");
    Serial.println("uT");

    // Display the gyroscope results (gyroscope data is in degrees/second)
    Serial.print("G ");
    Serial.print("X: "); Serial.print(BMI270_BMM150_gyro_x); Serial.print("  ");
    Serial.print("Y: "); Serial.print(BMI270_BMM150_gyro_y); Serial.print("  ");
    Serial.print("Z: "); Serial.print(BMI270_BMM150_gyro_z); Serial.print("  ");
    Serial.println("degrees/second");
    
    // Display the accelerometer results (accelerometer data is in G's)
    Serial.print("A ");
    Serial.print("X: "); Serial.print(BMI270_BMM150_accel_x); Serial.print("  ");
    Serial.print("Y: "); Serial.print(BMI270_BMM150_accel_y); Serial.print("  ");
    Serial.print("Z: "); Serial.print(BMI270_BMM150_accel_z); Serial.print("  ");
    Serial.println("G's");

    Serial.println("");
  }
  */
  unsigned long t_end = micros();
Serial.print("dataReceive took: ");
Serial.print(t_end - t_start);
Serial.println(" us");
}

void serviceUdpControlChannel() {
  int packetSize = udp.parsePacket();
  while (packetSize > 0) {
    const int maxReadable = static_cast<int>(kUdpControlMsgBufLen) - 1;
    int bytesToRead = packetSize > maxReadable ? maxReadable : packetSize;
    char buffer[kUdpControlMsgBufLen] = {0};
    int bytesRead = udp.read(buffer, bytesToRead);
    if (bytesRead <= 0) {
      break;
    }
    buffer[bytesRead] = '\0';

    while (udp.available() > 0) {
      udp.read();
    }

    uppercaseBuffer(buffer, static_cast<size_t>(bytesRead));
    const IPAddress remoteIp = udp.remoteIP();
    const bool isCurrentSubscriber = g_unicastModeActive && (remoteIp == g_udpTargetIP);
    const bool allowSubscriberCommands = !g_unicastModeActive || isCurrentSubscriber;

    if (strncmp(buffer, kCmdSubscribe, strlen(kCmdSubscribe)) == 0) {
      if (allowSubscriberCommands) {
        armUnicastForRemote(remoteIp, udp.remotePort());
      } else {
        String ipString = remoteIp.toString();
        Serial.printf("UDP: ignored subscribe from %s (locked to %s)\n",
                      ipString.c_str(),
                      g_udpTargetIP.toString().c_str());
      }
    } else if (strncmp(buffer, kCmdBroadcast, strlen(kCmdBroadcast)) == 0) {
      if (allowSubscriberCommands) {
        revertToBroadcast("broadcast command");
      } else {
        String ipString = remoteIp.toString();
        Serial.printf("UDP: ignored broadcast request from %s (locked to %s)\n",
                      ipString.c_str(),
                      g_udpTargetIP.toString().c_str());
      }
    }

    packetSize = udp.parsePacket();
  }

  if (g_unicastModeActive && (millis() - g_lastSubscribeMs) > kUnicastLeaseMs) {
    revertToBroadcast("subscription timeout");
  }
}


