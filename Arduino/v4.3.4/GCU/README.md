# LED Status Reference

| Indicator    | Color / Pattern                              | Trigger (Code Reference) | Meaning                                                                 |
| ------------- | -------------------------------------------- | ------------------------ | ----------------------------------------------------------------------- |
| LED1          | N/A                                          | N/A                      | Not controlled by firmware; reflects only the power rail state.         |
| LED2          | N/A                                          | N/A                      | Not driven in firmware; any battery or charger indicator is hardware.   |
| RGB LED (U6)  | Rainbow cycle                                | `GCU.ino:113`            | Power-on animation confirming MCU and RGB driver startup.               |
| RGB LED (U6)  | Off                                          | `GCU.ino:111`, `WiFi.cpp:60`, `OTA.cpp:54` | Normal idle/connected state; LED turns off when Wi-Fi or OTA finishes. |
| RGB LED (U6)  | Solid white                                  | `GCU.ino:125`            | IMU initialization failed; firmware halts for user intervention.        |
| RGB LED (U6)  | Solid red                                    | `GCU.ino:149`            | BLE/SoftAP provisioning in progress, waiting for credentials.          |
| RGB LED (U6)  | Red blink (1 Hz)                             | `WiFi.cpp:53-56`         | Still waiting for Wi-Fi connection during the provisioning window.      |
| RGB LED (U6)  | Amber rapid blink -> Amber solid             | `WiFi.cpp:66-70`         | Wi-Fi connection timed out; device is about to reboot.                 |
| RGB LED (U6)  | Cyan blink (3x pattern)                      | `GCU.ino:203-205`        | SNTP RTC sync retry loop (time not yet available).                      |
| RGB LED (U6)  | Amber solid                                  | `GCU.cpp:194`, `GCU.ino:216` | Sensor matrix configured and timers armed; system ready/idle.       |
| RGB LED (U6)  | Solid green                                  | `GCU.cpp:231-234`        | `dataReceive()` is actively streaming UDP sensor frames.                |
| RGB LED (U6)  | Calibration palette (purple / magenta / blink) | `GCU.cpp:81-189`       | Only when `normalized_calibration_flag` is ON: purple=prep, magenta=sampling, blinking between magenta/purple indicates failure. |
| RGB LED (U6)  | OTA palette (blue / purple / gradient / cyan / magenta) | `OTA.cpp:294-457` | OTA lifecycle: blue=manifest, purple=prepping, gradient=download progress, cyan=success, magenta=error. |
| RGB LED (U6)  | Reserved pink (`LedPalette::StreamSocketError`) | `GCU.h:62`            | Reserved for future UDP/TCP error indication (currently unused).        |

## 硬件型号编译

- 推荐用编译宏选择型号：`GCU_MODEL_V21` / `GCU_MODEL_V22C` / `GCU_MODEL_V23D`，由 `ModelConfig.h` 统一设置 `GCU_EXPECTED_MODEL` 与硬件能力。
- `v2.3.d` 默认启用 `BQ25180` 电源管理，`v2.1` / `v2.2.c` 默认关闭。
- `BQ25180` 访问前会临时把 `I2C` 切到 `400kHz`，完成后恢复到 `GCU_I2C_HZ`；可用 `GCU_BQ25180_I2C_HZ` 覆盖。
- 电池默认参数写在 `BatteryConfig.h`，`BQ25180_CONFIG_*` 由该文件提供默认值，可用编译宏覆盖。
- 批量编译可运行 `build_models.ps1`（需 `arduino-cli`），默认分区为 `min_spiffs`；脚本会把 `PartitionScheme` 写进 `--fqbn`，避免仍用默认 1.2MB。更大 APP 可改为 `no_ota` / `huge_app`：`.\build_models.ps1 -Partitions no_ota`。脚本默认结束后暂停，想自动退出可加 `-Pause:$false`。
- 编译产物会自动重命名为 `gcu-<model>-<kFirmwareVersion>.bin` 与 `gcu-<model>-<kFirmwareVersion>.merged.bin`。

## UDP Frame Layout

| Section            | Bytes  | Description |
| ------------------ | ------ | ----------- |
| START (optional)   | 2      | `0x5A 0x5A` when `start_flag` is enabled. |
| Device Number (DN) | 6      | ESP32 MAC address (LSB first) when `device_num_flag` is enabled. |
| Sensor Count (SN)  | 1      | `sensors_rows_num * sensors_columns_num` when `sensors_num_flag` is enabled. |
| Epoch time         | 4      | Little-endian seconds from UTC epoch when `timestamp_flag` is enabled. |
| Millis offset      | 2      | Little-endian milliseconds from `ESP32Time::getMillis()` when `timestamp_flag` is enabled. |
| Sensor payload     | `N*4`  | Each sensor uses 4 bytes: raw `uint32_t` millivolts when normalization is OFF, or `float` when ON. |
| IMU payload        | 36     | When `IMU_flag` is enabled: magnetometer XYZ, gyroscope XYZ, accelerometer XYZ, each as 4-byte IEEE-754 floats. |
| END (optional)     | 2      | `0xA5 0xA5` when `end_flag` is enabled. |

All multi-byte fields use little-endian ordering. The frame buffer is assembled in `dataReceive()` and sent via UDP without extra checksums.

### Example Frame Layout

<table>
  <tr>
    <th colspan="2">START<br>(optional)</th>
    <th colspan="6">Device Number (MAC)</th>
    <th>SN</th>
    <th colspan="4">Epoch Time</th>
    <th colspan="2">Millis</th>
  </tr>
  <tr>
    <td>0x5A</td><td>0x5A</td>
    <td>MAC[0]</td><td>MAC[1]</td><td>MAC[2]</td><td>MAC[3]</td><td>MAC[4]</td><td>MAC[5]</td>
    <td>#rows×#cols</td>
    <td>T[0]</td><td>T[1]</td><td>T[2]</td><td>T[3]</td>
    <td>ms[0]</td><td>ms[1]</td>
  </tr>
  <tr>
    <th colspan="4">Sensor #1</th>
    <th colspan="4">Sensor #2</th>
    <th colspan="4">…</th>
    <th colspan="2"></th>
  </tr>
  <tr>
    <td colspan="4" align="center">S1[0..3]</td>
    <td colspan="4" align="center">S2[0..3]</td>
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
    <td>0xA5</td><td>0xA5</td>
    <td colspan="12"></td>
  </tr>
</table>

## TCP Sensor Config JSON Example

Send a single JSON line (terminated by CRLF) to `tcp://<board_ip>:22345`. Example for hardware model `v2.3.d` (adjust to your firmware):

```json
{
  "model": "v2.3.d",
  "analog": [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15],
  "select": [16, 17, 18, 19, 20, 21, 35, 36, 37, 39, 40, 41, 42, 45, 46]
}
```

Rules:

1. `model` must match the firmware `kExpectedModel`.
2. `analog` and `select` arrays may only contain pins from that model's whitelist (max 15 analog pins, 15 select pins).
3. The device replies with `{"status":"ok"}` on success or `{"status":"error","msg":"reason"}` on failure; valid payloads are saved to NVS.

## Filter (Median + IIR)

- Default off; requires Advanced tier or higher for effect.
- Parameters: `median` supports 1/3/5 (1 = no median), `alpha` range 0.05~0.6 (default 0.25).
- Configure (TCP 22345, one JSON line): e.g. `{"filter":{"enabled":true,"alpha":0.25,"median":3}}`.
- Query: `{"filter":"?"}` returns status such as:
  ```json
  {"filter":{"enabled":false,"alpha":0.250,"median":3},"license_ok":true,"effective":false}
  ```
  `license_ok` indicates license tier sufficiency; `effective` indicates whether filtering is actually applied.

## Calibration (PRO tier)

- Available only with PRO tier or higher, off by default. Calibration data lives in SPIFFS `<level>.csv`; filename is the calibration level (e.g. `0.0.csv`, `0.5.csv`, `1.0.csv`), rows=AnalogPin, cols=SelectPin; valid but inactive sensors still reserve slots.
- Enable/disable: `{"calibration":{"status":"enabled"}}` (requires complete data for all active sensors) / `{"calibration":{"status":"disabled"}}`.
- Applied in pipeline: sample → filter → calibration interpolation (piecewise linear, clamped).
- Commands (TCP 22345, one JSON line):
  - Start single-sensor calibration: `{"calibration":{"status":"start","analogpin":<A>,"selectpin":<S>,"level":0.5,"start_time":1000,"calibration_time":5000}}`
    - Requires PRO; enters calibration mode, LED shows CalibPrep then CalibSampling after `start_time` ms, averages `calibration_time` ms of data, writes to `<level>.csv`, responds `{"status":"ok","avg":<mv>}`.
  - End calibration and save: `{"calibration":{"status":"end"}}` (save CSV, exit calibration mode; if complete you may call `enabled`).
  - Query calibration status: `{"calibration":"?"}` returns `enabled/mode_active/levels/complete`.
  - Dump a level: `{"calibration":{"status":"dump","level":0.5}}` returns the matrix (rows=AnalogPin, cols=SelectPin, missing entries are null).
  - Delete a level: `{"calibration":{"status":"delete","level":0.5}}`.
- During calibration mode: streaming is paused; only calibration commands are served. Missing data makes `enabled` fail with `calibration_incomplete`.
