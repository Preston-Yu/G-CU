# G-CU
## Contents
 - [Working Status](#working_status)
 - [Data Format](#data_format)

## Working Status

| Indicator    | Status                         | Meaning                                            |
| ------------ | ------------------------------ | -------------------------------------------------- |
| LED1         | Red                            | System is powered on     |
|              | Off                            | System is powered off    |
| LED2         | Green                          | Battery is charging      |
|              | Off                            | Battery is fully charged / Battery not installed   |
| RGB LED (U6) | Off                            | System is powered off / System is initializing     |
|              | Red-Green-Blue (Cycle)         | System is resetting / Low Battery     |
|              | Red                            | Connecting to WiFi / Unable to connect to WiFi  |
|              | Red (Blink)                    | Unable to set RTC     |
|              | Cyan                           | Setting RTC using NTP Server    |
|              | Cyan (Blink 3 Times)           | Unable to set RTC from NTP Server / Setting RTC using extern Chip(BQ32002)   |
|              | Yellow                         | Waiting for connect to TCP Server   |
|              | Green                          | Transferring data   |

## Data Format

<table>
 <tr>
  <th colspan="2">START</th>
  <th>DN</th>
  <th>SN</th>
  <th colspan="4">TIME</th>
  <th colspan="2">TIMEMS</th>
  <th colspan="2">S1</th>
  <th colspan="2">S2</th>
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
  <td>0x31</td>
  <td>0x66</td>
 </tr>
 <tr>
  <th colspan="10">...</th>
  <th colspan="2">DV</th>
  <th colspan="2">END</th>
 </tr>
 <tr>
  <td  colspan="10" style="text-align: center;">...</td>
  <td>0x0b</td>
  <td>0xb8</td>
  <td>0xa5</td>
  <td><0xa5</td>
 </tr>
</table>


