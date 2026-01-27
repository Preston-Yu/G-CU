# Repository Guidelines

## Response Requirements
- All assistant responses to this repository must be in Chinese.

## Project Structure and Modules
- `GCU.ino`: ESP32-S3 entry point that wires up setup/loop, logging, provisioning, RTC/NTP sync, OTA checks, ticker scheduling, and kicks off the sensor server.
- `GCU.h`: Shared header for constants, LED palette, globals, and forward declarations. Add new shared symbols here.
- `GCU.cpp`: Sensor matrix scanning, normalization routines, UDP frame packing, IMU/RTC data injection, and ticker callbacks such as `on_timer` and `dataReceive`.
- `SensorConfig.cpp/.h`: Hardware model descriptors (`v2.1`, `v2.2.c`), pin whitelists, TCP config server on port 22345, JSON parsing, NVS persistence, and rebuild of frame headers.
- `Provisioning.cpp/.h`: Builds the BLE provisioning context (service name + UUIDv1 derived from MAC) and wraps `WiFiProv.beginProvision` plus device name programming.
- `WiFi.cpp`: Handles Wi-Fi/BLE provisioning events, LED feedback, connection wait logic, and the helper `hasSavedWiFiCredentials`.
- `OTA.cpp`: HTTPS manifest-driven OTA with TLS timeouts, SHA256 verification, LED progress gradients, and graceful rollback on failure.
- `PowerReset.cpp`: Maintains the boot counter in NVS via `Preferences`, enabling multi-boot force provisioning and higher-threshold factory resets.
- `button.cpp`: Optional long-press detector during boot (active-low GPIO) for manual provisioning or reset hooks.
- `RTC.cpp`: SNTP-only RTC sync that programs the `ESP32Time` instance (BQ32002 support removed).
- `neopixel.cpp`: RGB LED driver; uses `rgbLedWriteOrdered` on modern cores and falls back to manual RMT when required.
- All sources live in this folder; assets are embedded and there is no separate test directory.

## Build, Test, and Development Commands
- Arduino IDE (ESP32S3 Dev Module / `esp32:esp32:esp32s3`):
  - Select a partition with >=1.9 MB APP (for example `No OTA (2MB APP/2MB SPIFFS)`) or uploads will fail with "Sketch too big".
  - Serial console runs at 115200 baud. Enable `USB CDC On Boot` when available for native USB-Serial.
- arduino-cli:
  - `arduino-cli core install esp32:esp32`
  - `arduino-cli board list`
  - `arduino-cli compile --fqbn esp32:esp32:esp32s3 .`
  - `arduino-cli upload -p <COMx> --fqbn esp32:esp32:esp32s3 .`
  - `arduino-cli monitor -p <COMx> -c baudrate=115200`
- Before flashing, confirm `kFirmwareVersion`, `kExpectedModel`, provisioning PoP (`pop`), and UDP peers (`SeverIP` / `port`) match the hardware you are about to run.

## Coding Style and Naming
- C++ with 2-space indentation, K&R braces, no tabs.
- Keep modules split into `.cpp/.h` pairs (for example `SensorConfig.cpp/.h`).
- Constants/macros are ALL_CAPS (for example `GCU_RGB_BRIGHTNESS`). Globals follow the existing descriptive pattern (`g_wifiGotIP`, `bootcnt_local`).
- Functions use lowerCamelCase or follow nearby naming (`waitForWiFiOrReboot`, `runOnlineOtaUpdateIfAvailable`).
- Files must stay UTF-8 without BOM.

## Testing Guidelines
- No unit-test harness; rely on serial logs and runtime behavior:
  - `Serial.begin(115200)` output should cover provisioning, IMU, RTC, OTA, and reboot counters.
  - Use the Espressif Provisioning mobile app (or equivalent) to enter the PoP and push BLE credentials.
  - `arduino-cli monitor -p COM3 -c baudrate=115200` is the default monitor command; update the COM port as needed.
  - Connect to `<board_ip>:22345` (for example with `nc`) and send JSON like `{"analog":[...],"select":[...]}` to test sensor-matrix overrides and confirm they persist in NVS after reboot.
  - Capture UDP on `port` (Wireshark/tcpdump) to verify frame headers, timestamps, and IMU payloads.
- Document reproduction steps in PRs: board, partition scheme, provisioning method, logs, and whether online OTA was attempted.

## Commit and PR Expectations
- Commits are short, imperative, and scoped (for example `wifi: tighten wait loop`, `sensor-config: persist custom matrix`).
- PR descriptions must call out:
  - Purpose and summary of the change.
  - Hardware board and partition scheme used for validation.
  - Before/after behavior with relevant serial logs or screenshots.
  - Linked issues (if any) and explicit test steps (OTA/provisioning/sensor-server/UDP checks).

## Security and Configuration Tips
- Update the manifest URL and any credentials in `OTA.cpp` before distributing builds.
- BLE/SoftAP provisioning exposes temporary credentials; wipe stored SSIDs/passwords before shipping units.
- Multi-boot factory reset wipes all NVS state; back up critical configuration before testing reset paths.
- The device depends on NTP; `RTC.cpp` will keep retrying when no time source is available, so ensure outbound UDP/123 or provide a local server.
- Normalization (`normalized_calibration_flag`) should be run in a stable environment; calibration data lives in RAM and must be regenerated after every reboot.
