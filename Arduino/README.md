# GCU-Code
## Update History
### Version 1.0 
- Add IMU Sensor Data Measurement (Must Use Board v1.1(including v1.0.R) or above)
### Version 0.2 ([Details](v0.2/README.md))
- Add OTA Function
- Optimized file structure
### Version 0.1 ([Details](v0.1/README.md))
- Add UDP Function

## Arduino Library
[ESP32Time](https://www.arduinolibraries.info/libraries/esp32-time) By fbiego v2.0.2

## System Configuration
All system configuration variables except for definitions are in the [GCU.ino](v0.1/GCU.ino) file.

### Device Setting
```cpp
// Device Parameters
const unsigned char device_number = 0x01;
const unsigned char device_frequency = 10;
// #define sample_delayms 100

// Sensor Numbers
const unsigned char sensors_rows_num = 1;
const unsigned char sensors_columns_num = 1;
```

### WiFi
```cpp
const char* SSID       = "GCU-wifi";
const char* password   = "12345678";
const char* SeverIP = "192.168.1.101";
const uint16_t port = 1337;
const bool TCP_UDP_Flag = UDP;
```

### Flag
```cpp
// Data Format Function
const bool start_flag = 1;
const bool device_num_flag = 1;
const bool sensors_num_flag = 1;
const bool timestamp_flag = 1;
const bool data_validation_flag = 1;
const bool end_flag = 1;
```

### Deifine

The definition defines many hardware addresses and port mappings. 

Without changing the PCB, there is no need to change the defined content.

```cpp
#define GCU_SDA 48
#define GCU_SCL 47
#define BQ32002_I2C_ADDRESS 0x68

#define BQ32002_SECONDS_Register 0x00
#define BQ32002_MINUTES_Register 0x01
#define BQ32002_CENT_HOURS_Register 0x02
#define BQ32002_DAY_Register 0x03
#define BQ32002_DATE_Register 0x04
#define BQ32002_MONTH_Register 0x05
#define BQ32002_YEARS_Register 0x06
```
