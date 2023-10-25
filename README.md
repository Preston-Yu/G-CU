# G-CU Version 2
## Contents
 - [Working Status](#working_status)
 - [Data Format](#data_format)
 - [Attention](#attention)

## Working Status

| Indicator    | Status                         | Meaning                                            |
| ------------ | ------------------------------ | -------------------------------------------------- |
| LED1         | Red                            | System is powered on     |
|              | Off                            | System is powered off    |
| LED2         | Green                          | Battery is charging      |
|              | Off                            | Battery is fully charged / Battery not installed   |
| RGB LED (U6) | Off                            | System is powered off / System is initializing     |
|              | White                          | Unable to Init the IMU Sensor |
|              | Red-Green-Blue (Cycle)         | System is starting / Low Battery     |
|              | Red                            | Connecting to WiFi / Unable to connect to WiFi  |
|              | Red (Blink)                    | Unable to set RTC     |
|              | Cyan                           | Setting RTC using NTP Server    |
|              | Cyan (Blink 3 Times)           | Unable to set RTC from NTP Server / Setting RTC using extern Chip(BQ32002)   |
|              | Yellow                         | Waitting for transfer data / [TCP Mode]Waiting for connect to TCP Server |
|              | Green                          | Transferring data   |

## Data Format


<table>
 <tr>
  <th colspan="2">START</th>
  <th>DN</th>
  <th>SN</th>
  <th colspan="4">TIME</th>
  <th colspan="2">TIMEMS</th>
  <th colspan="2">S 1</th>
 </tr>
 <tr>
  <td>0x5a</td>
  <td>0x5a</td>
  <td>0x00</td>
  <td>0x04</td>
  <td>0x64</td>
  <td>0xac</td>
  <td>0xd6</td>
  <td>0xe1</td>
  <td>0x01</td>
  <td>0xf4</td>
  <td>0x30</td>
  <td>0x8e</td>
 </tr>
 <tr>
  <th colspan="2">S 2</th>
  <th colspan="2">S ...</th>
  <th colspan="4">Magnetometer_x</th>
  <th colspan="4">Magnetometer_y</th>
 <tr>
  <td>0x31</td>
  <td>0x66</td>
  <td colspan="2" align="center">...</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  
  
 </tr>
 <tr>
  <th colspan="4">Magnetometer_z</th>
  <th colspan="4">Gyroscope_x</th>
  <th colspan="4">Gyroscope_y</th>
 </tr>
 <tr>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
 </tr>
 <tr>
  <th colspan="4">Gyroscope_z</th>
  <th colspan="4">Accelerometer_x</th>
  <th colspan="4">Accelerometer_y</th>
 </tr>
 <tr>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
 </tr>
 <tr>
  <th colspan="4">Accelerometer_z</th>
  <th colspan="2">DV</th>
  <th colspan="2">END</th>
 </tr>
 <tr>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td>0xa5</td>
 </tr>
</table>

### Fields of the Data Packet
| Field Name        | Size (Bytes) | Description                                         |
| ----------------- | ------------ | -------------------------------------------------- |
| START             | 2            | Two 0x5a identifying the packet    |
| DN                | 1            | Device NO.    |
| SN                | 1            | Total number of sensors      |
| TIME              | 4            | Unix Time   |
| TIMEMS            | 2            | Million Seconds     |
| S ***x***         | 2            | Value of the Pressure Sensor ***x***    |
| Magnetometer_xyz  | 12           | Value of the Magnetometer_xyz(Float)  |
| Gyroscope_xyz     | 12           | Value of the Gyroscope_xyz(Float)  |
| Accelerometer_xyz | 12           | Value of the Accelerometer_xyz(Float)  |
| DV                | 2            | Data Validation of the Pressure Sensor (CheckSum)  |
| END               | 2            | Two 0xa5 ending the packet   |

\* The packet format can be customized by [changing the value of the flag](Arduino/README.md#flag).

## Attention
Due to issues such as hardware design, the following matters need to be noted.
 - GPIO38 (In the middle of U11) cannot be used.
 - Switch status is negetive.


