# GCU-Code

## Deifine

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

## Flag
```cpp
// Data Format Function
const bool start_flag = 1;
const bool device_num_flag = 1;
const bool sensors_num_flag = 1;
const bool timestamp_flag = 1;
const bool data_validation_flag = 1;
const bool end_flag = 1;
```
