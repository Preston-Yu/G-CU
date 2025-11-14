# G-CU
## Contents
 - [Working Status](#working-status)
 - [Data Format](#data-format)
 - [Attention](#attention)

## Working Status

| Indicator    | Status                         | Meaning                                            |
| ------------ | ------------------------------ | -------------------------------------------------- |
| mini LED1    | Red                            | System is powered on     |
|              | Off                            | System is powered off    |
| mini LED2    | Green                          | Battery is charging      |
|              | Off                            | Battery is fully charged / Battery not installed   |
| RGB LED (U6)   | Rainbow cycle                                  | Power-on animation that confirms the MCU and RGB driver are alive.                                        |
| RGB LED (U6)   | Off                                            | Idle/connected state; turns off after Wi-Fi connects or OTA finishes successfully.                        |
| RGB LED (U6)   | Solid White                                    | IMU initialization failed (BMI270/BMM150); firmware halts for manual intervention.                        |
| RGB LED (U6)   | Solid Red                                      |  In BLE/SoftAP provisioning mode, waiting for credentials.                                                |
| RGB LED (U6)   | Red blink (1 Hz)                               |  Still waiting for Wi-Fi credentials/connection during provisioning window.                               |
| RGB LED (U6)   | Amber rapid blink → Amber solid                |  Wi-Fi connection timed out; device is about to reboot.                                                   |
| RGB LED (U6)   | Cyan (blink 3 times per retry)                 | SNTP RTC sync retry in progress (no valid time yet).                                                      |
| RGB LED (U6)   | Solid Amber                                             |  Sensor matrix configured, timers armed; system is ready/idle.                                            |
| RGB LED (U6)   | Solid Hreen                                             |  `dataReceive()` has started streaming sensor data over UDP.                                              |
| RGB LED (U6)   | Calibration palette (purple / magenta / blink)          |  Active only when `normalized_calibration_flag` is ON: purple = prep, magenta = sampling, blink = failure. |
| RGB LED (U6)   | OTA palette (blue / purple / gradient / cyan / magenta) |  OTA lifecycle: blue = manifest, purple = preparing, gradient = download progress, cyan = success, magenta = error. |
| RGB LED (U6)   | Reserved pink (`LedPalette::StreamSocketError`)         |  Reserved for future UDP/TCP error indications (not currently used).                                       |



## Data Format

<table>
  <tr>
    <th colspan="2">START<br>(optional)</th>
    <th colspan="6">Device Number (MAC)</th>
    <th>SN</th>
    <th colspan="4">Epoch Time</th>
    <th colspan="2">Millis</th>
  </tr>
  <tr>
    <td>0x5A</td>
    <td>0x5A</td>
    <td>MAC[0]</td>
    <td>MAC[1]</td>
    <td>MAC[2]</td>
    <td>MAC[3]</td>
    <td>MAC[4]</td>
    <td>MAC[5]</td>
    <td>SN[0]</td>
    <td>T[0]</td>
    <td>T[1]</td>
    <td>T[2]</td>
    <td>T[3]</td>
    <td>ms[0]</td>
    <td>ms[1]</td>
  </tr>
  <tr>
    <th colspan="4">Sensor #1</th>
    <th colspan="4">Sensor #2</th>
    <th colspan="4">…</th>
    <th colspan="2"></th>
  </tr>
  <tr>
    <td colspan="4" align="center">S₁[0..3]</td>
    <td colspan="4" align="center">S₂[0..3]</td>
    <td colspan="4" align="center">…</td>
    <td colspan="2"></td>
  </tr>
  <tr>
    <th colspan="4">Magnetometer X</th>
    <th colspan="4">Magnetometer Y</th>
    <th colspan="4">Magnetometer Z</th>
    <th colspan="2"></th>
  </tr>
  <tr>
    <td>Mx[0]</td><td>Mx[1]</td><td>Mx[2]</td><td>Mx[3]</td>
    <td>My[0]</td><td>My[1]</td><td>My[2]</td><td>My[3]</td>
    <td>Mz[0]</td><td>Mz[1]</td><td>Mz[2]</td><td>Mz[3]</td>
    <td colspan="2"></td>
  </tr>
  <tr>
    <th colspan="4">Gyroscope X</th>
    <th colspan="4">Gyroscope Y</th>
    <th colspan="4">Gyroscope Z</th>
    <th colspan="2"></th>
  </tr>
  <tr>
    <td>Gx[0]</td><td>Gx[1]</td><td>Gx[2]</td><td>Gx[3]</td>
    <td>Gy[0]</td><td>Gy[1]</td><td>Gy[2]</td><td>Gy[3]</td>
    <td>Gz[0]</td><td>Gz[1]</td><td>Gz[2]</td><td>Gz[3]</td>
    <td colspan="2"></td>
  </tr>
  <tr>
    <th colspan="4">Accelerometer X</th>
    <th colspan="4">Accelerometer Y</th>
    <th colspan="4">Accelerometer Z</th>
    <th colspan="2"></th>
  </tr>
  <tr>
    <td>Ax[0]</td><td>Ax[1]</td><td>Ax[2]</td><td>Ax[3]</td>
    <td>Ay[0]</td><td>Ay[1]</td><td>Ay[2]</td><td>Ay[3]</td>
    <td>Az[0]</td><td>Az[1]</td><td>Az[2]</td><td>Az[3]</td>
    <td colspan="2"></td>
  </tr>
  <tr>
    <th colspan="2">END<br>(optional)</th>
    <th colspan="12"></th>
  </tr>
  <tr>
    <td>0xA5</td>
    <td>0xA5</td>
    <td colspan="12"></td>
  </tr>
</table>


### Fields of the Data Packet
| Field Name        | Size (Bytes) | Description                                         |
| ----------------- | ------------ | -------------------------------------------------- |
| START             | 2            | Two 0x5a identifying the packet    |
| DN                | 6            | Device NO.    |
| SN                | 1            | Total number of the Pressure Sensors      |
| TIME              | 4            | Unix Time   |
| TIMEMS            | 2            | Million Seconds     |
| S_***x***         | 4            | Value of the Pressure Sensor NO. ***x***    |
| Magnetometer_xyz  | 12           | Value of the Magnetometer_xyz(Float)  |
| Gyroscope_xyz     | 12           | Value of the Gyroscope_xyz(Float)  |
| Accelerometer_xyz | 12           | Value of the Accelerometer_xyz(Float)  |
| END               | 2            | Two 0xa5 ending the packet   |

All multi-byte fields use little-endian ordering.
\* The packet format can be customized by [changing the value of the function flag](Arduino/README.md#function-flag).

### IO port(Chip on top)
-(board v2.2.C)
| Left       | Right      |
| ----------------- | ----------------- |
| GPIO18      | GPIO17      |
| GPIO29      | GPIO11(GIN10)            |
| GPIO20       | GPIO10(GIN9)            |
| GPIO21       | GPIO9(GIN8)            |
| GPIO35       | GPIO8(GIN7)           |
| GPIO36      | GPIO7(GIN6)            |
| GPIO37      | GPIO6(GIN5)            |
| GPIO39      | GPIO5(GIN4)            |
| GPIO40      | GPIO4(GIN3)            |
| GPIO41      | GPIO3(GIN2)            |
| GPIO42      | GPIO2(GIN1)            |
| GPIO45      | GPIO1(GIN0)            |

-(board v2.0.B)
| Left(2 pin)       | Right(2 pin)      |
| ----------------- | ----------------- |
| GPIO1(GIN0)       | GPIO7(GIN6)       |
| GPIO2(GIN1)       | GPIO19            |
| GPIO3(GIN2)       | GPIO20            |
| GPIO4(GIN3)       | GPIO21            |
| GPIO5(GIN4)       | GPIO35            |
| GPIO6(GIN5)       | GPIO36            |

\* Please change the value of analogReadIO and SelectIO in [IO Control TCP JSON](Arduino/README.md#conector-io).



## Attention
### For code v2.0
Due to changes in IMU sensor, the following matters need to be noted.
 - New Library in Arduino must be installed, please check [Arduino Library](Arduino/README.md#arduino-library).

### For board v2.0.B
Due to changes in IMU sensors and the removal of the RTC chip, the following matters need to be noted.
- When using [code v2.0](Arduino/v2.0/README.md), the value of IMU_chip must set to GCU_BMI270_BMM150.
- When using [code v2.0](Arduino/v2.0/README.md), the value of RTC_chip must set to GCU_FLAG_OFF.
- IF you need 2 bytes sensors data, please set the value of sensors_dataformat_define to Two_Bytes_Sensors_Data in [code v2.0](Arduino/v2.0/README.md).
- It is not recommended to use versions of the code earlier than [code v2.0](Arduino/v2.0/README.md).


### For board v1.0
Due to I2C Address Conflict, the following matters need to be noted.
 - If you want to use IMU Sensor, board v1.0 must update to [board v1.1](PCB%20Design/README.md) （Contact YU） or board v1.0.R.
 - If you choose to remove the RTC chip, you need to disable the RTC Function from [code v1.1](Arduino/v1.1/README.md).
 - If you want to use the board v1.0(Don't use IMU Sensor), you need to set the IMU_flag to 0 using [code v1.1](Arduino/v1.1/README.md).

### For code v1.0
Due to data structure, the following matters need to be noted.
 - Now all data transmission changed to Little-endian.

### For board v0.0 or v0.1
Due to issues such as hardware design, the following matters need to be noted.
 - GPIO38 (In the middle of U11) cannot be used.
 - Switch status is negetive.


