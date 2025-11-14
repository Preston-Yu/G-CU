# GCU-Code
## Update History
### Version 4.0.7（Latest）
- Added Bluetooth provisioning for Wi-Fi setup via the official ESP mobile app.
- Added reset function: power-cycling the device 5 times clears Wi-Fi settings.
- Added TCP-based configuration control. IO pins can now be toggled via TCP commands, and the configuration is saved to NVS until modified by another command or reset.
- Added factory reset functionality. Power-cycling the device 10 times will now clear all NVS-stored configurations.
- Reworked OTA update system. The device now automatically connects to the network, checks for new firmware, and updates itself.
- Added UDP unicast mode. Packet loss is significantly reduced by unicasting to subscribed devices, while falling back to broadcast when no subscriptions are active.
- Updated the device serial number (DN) field in data packets. DN is now fixed and derived from the internal chip ID, with its length changed from 1 byte to 6 bytes.
- Optimized sensor scanning algorithms, greatly increasing the maximum operating frequency (from 50 Hz to 80 Hz with all 143 sensors enabled).
- Removed support for legacy chips BQ32002 and BMI160.
- Removed the TCP data transmission feature.
- Removed the optional 2-byte sensor data format.
- Added sensor calibration functionality with two available calibration algorithms (BETA).
### Version 2.2
- Added new functions to support ESP32 official library version 3.0 and above.
### Version 2.1
- Critical bug fix: Resolved a major issue that caused watchdog timer (WDT) resets when actual sensor read frequency dropped below the set frequency. **This is a fatal bug fix and updating is strongly recommended for all users.**
- Optimized multi-sensor reading logic to significantly reduce latency.
- Fixed a data interpretation bug when using `Two_Bytes_Sensors_Data` format. **However, using `Four_Bytes_Sensors_Data` is still strongly recommended for best compatibility and precision.**
### Version 2.0
- Add data normalized calibration function.
- Add new IMU and MI sensor measurement function.(Board v2.0 or above)(In this version also used old IMU)
- Pressure sensor data format default set to Four_Bytes_Sensors_Data.(In this version also can set to Two_Bytes_Sensors_Data)
- RTC Chip Function default set to OFF.(In this version also can set to ON(except Board v2.0 or above))
### Version 1.0 
- Add IMU Sensor Data Measurement (Must using Board v1.1(including v1.0.R) or above)
- Data transmission now uses little-endian byte order.
- Remove DV from data format.
### Version 0.2 ([Details](v0.2/README.md))
- Add OTA Function
- Optimized file structure
### Version 0.1 ([Details](v0.1/README.md))
- Add UDP Function


## Arduino Library
[ESP32Time](https://www.arduinolibraries.info/libraries/esp32-time) By fbiego v2.0.6

[Arduino_BMI270_BMM150](https://github.com/arduino-libraries/Arduino_BMI270_BMM150) By Arduino v1.2.0

## System Configuration
All system configuration variables except for definitions are in the [GCU.ino](v0.1/GCU.ino) file.

### Device Setting
```cpp
// Device Parameters
const uint16_t device_frequency = 100;
const uint16_t calibration_duration = 10000;
constexpr uint32_t kSensorServerPollIntervalMs = 50;
constexpr uint32_t kUdpControlPollIntervalMs = 50;
```

### WiFi
```cpp
// Data Transfer Parameters
const uint16_t port = 13250;
```

### Function Flag
```cpp
// Data Format Function
const bool start_flag = GCU_FLAG_ON;
const bool device_num_flag = GCU_FLAG_ON;
const bool sensors_num_flag = GCU_FLAG_ON;
const bool timestamp_flag = GCU_FLAG_ON;
const bool IMU_flag = GCU_FLAG_ON;
const bool end_flag = GCU_FLAG_ON;


/*********Normalized calibration function flag**********/
/*        If normalized_calibration is ON              */
/*        Sensors Dataformat must be Four Bytes        */
const bool normalized_calibration_flag = GCU_FLAG_OFF;
const float normalized_calibration_max_factor = 0.2;
const float normalized_calibration_min_factor = 0.2;
```

### Conector IO
```json
{
  "model": "v2.2.c",
  "analog": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
  "select": [17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45]
}
```



### Deifine

The definition defines many hardware addresses and port mappings. 

Without changing the PCB, there is no need to change the defined content.

```cpp
//System Define
//! DONT NEED CHANGE
#define GCU_SDA 48
#define GCU_SCL 47
#define GCU_LED_PIN 38
#define GCU_RGB_BRIGHTNESS 20

#define GCU_FLAG_ON 1
#define GCU_FLAG_OFF 0

#define normalized_calibration_method_peak 0x00
#define normalized_calibration_method_mean 0x01

#define BOOTCNT_CLEAR_WINDOW_MS 5000
```
