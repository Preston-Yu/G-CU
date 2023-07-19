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

| 111 | DN | SN | TIME | TIMEMS | S1 | S2 | ... | DV | <th colspan="2">Connected</th> |
| ----- | -- | -- | ---- | ----- | -- | -- | --- | -- | --- |
| ----- | -- | -- | ---- | ----- | -- | -- | --- | -- | --- |


| Indicator         | Status      | Meaning                 |
| ----------------- | ----------- | ----------------------- |
| Power             | Green       | System is powered on    |
| Network           | <td colspan="2">Connected</td>  |
| Battery           | Green       | Battery level is high   |
| Error             | Red         | System error occurred   |

