
---
title: ESP32 OTA (ESP_32_OTA) Mechanism
version: '1.0'
commit: '51e257c695009957ec1183a0f769b73908d4013b'
branch: 'DT'
date: '2025-11-15'
---

# ESP32 OTA (ESP_32_OTA) Mechanism â€” Comprehensive Documentation

Version: 1.0

Repository: `d:\LabExpert\LabExpert_Sensor_Communications`

Build Metadata:
- Commit: `<to-be-populated>`
- Branch: `<to-be-populated>`
- Build date: `<to-be-populated>`

## Overview
- Purpose: Provide a robust OTA process for ESP32-based devices, featuring a dedicated OTA bootloader (`ota_0`) and application firmware (`ota_1`).
- Components: ESP32 OTA bootloader (`ESP_32_OTA`), sensor firmwares (`TOF_*`, `UltraSonic_*`, `OSI_*`), backend service, browser client, WiFi network, MQTT broker, UDP discovery.
- High-level modes:
  - Bootloader mode (`ota_0`): Accepts firmware via HTTP upload and a backend push API; performs partition erase and streaming writes, then restarts.
  - Application mode (`ota_1`): Runs sensor/UI firmware; can trigger rollback to bootloader upon sensor/EEPROM failure or manual input.

## Workflow Analysis

### Initialization
- Partition status: Logs current partition at boot and treats `ota_0` as OTA bootloader mode (`ESP_32_OTA/src/main.cpp:391-399`).
- Sensor detection: Reads EEPROM over I2C (`Wire`) with retries; sets `sensorType` or marks `UNKNOWN` and handles fallback (`ESP_32_OTA/src/main.cpp:94-156`).
- WiFi setup: STA mode, DHCP, connect to `ssid`/`password`, then start `WebServer` and UDP discovery (`ESP_32_OTA/src/main.cpp:454-483`).
- Inactive partition erase: At startup, erases the non-running OTA partition to ensure a clean target (`ESP_32_OTA/src/main.cpp:66-92`, called at `450-452`).
- Route setup: Registers HTTP routes including OTA web upload and backend push endpoints (`ESP_32_OTA/src/main.cpp:196-339`).

### Firmware Download Sequence
- Web upload (multipart form):
  - `POST /update` writes file chunks: `Update.begin(UPDATE_SIZE_UNKNOWN)` â†’ `Update.write(upload.buf)` â†’ `Update.end(true)`, then restart (`ESP_32_OTA/src/main.cpp:211-242`).
- Backend push API (JSON hex streaming):
  - `POST /ota/begin { size }` initializes `Update.begin(size)` and sets `otaInProgress` (`ESP_32_OTA/src/main.cpp:265-287`).
  - `POST /ota/write { offset, size, data(hex) }` decodes hex â†’ bytes and calls `Update.write(bytes)` with strict size checks (`ESP_32_OTA/src/main.cpp:288-321`).
  - `POST /ota/end` finalizes with `Update.end(true)` and restarts (`ESP_32_OTA/src/main.cpp:322-339`).
- Sensor firmwares (`ESPAsyncWebServer`):
  - `POST /update` handler streams `Update.write(data)` and finalizes `Update.end(true)` then restart (`TOF_Firmware_bin_Generator/src/config_handler.cpp:212-241`).

### Verification Steps
- Write verification:
  - Checks `Update.begin` and compares bytes written to expected size; errors via `Update.printError(Serial)`.
- Completion status:
  - Uses `Update.end(true)` and `Update.hasError()` to determine `OK` vs `FAIL` (`ESP_32_OTA/src/main.cpp:213`, `236-242`).
- Input validation for push API:
  - JSON parse checks with error codes; hex decoding validation; size mismatch guards (`ESP_32_OTA/src/main.cpp:270-307`).
- Cryptographic verification:
  - Not implemented; no SHA256/CRC signature checks; relies on `Update` success.

### Update Procedure
- Target partition selection:
  - Bootloader reports running and next update partitions; streams to `esp_ota_get_next_update_partition(NULL)` (`ESP_32_OTA/src/main.cpp:217-221, 227-229`).
- Partition erase:
  - Erases inactive partition at boot for a clean slate (`ESP_32_OTA/src/main.cpp:66-92`).
- Restart:
  - On success, calls `ESP.restart()` after brief delay (`ESP_32_OTA/src/main.cpp:333-334`, `221`).

### Rollback Mechanism
- Manual rollback:
  - Application firmwares can disconnect MQTT/WiFi and set boot partition to the opposite OTA slot via `esp_ota_set_boot_partition`, then restart (`OSI_Firmware_bin_Generator/src/main.cpp:205-271`).
- Sensor-triggered rollback:
  - On EEPROM/sensor failure, erase inactive partition and reboot to bootloader (`ESP_32_OTA/src/main.cpp:510-519`).
- Automatic runtime rollback framework:
  - Not used (e.g., `esp_ota_mark_app_valid_cancel_rollback` absent). Rollback is explicit/manual.

## Visual Documentation

### Architecture Diagram
- See `architecture.drawio` and `architecture.png` (to be generated). Components and connections:
  - ESP32 OTA Bootloader (`ota_0`) â†” Browser (HTTP form upload)
  - ESP32 OTA Bootloader â†” Backend (HTTP push API)
  - ESP32 OTA Bootloader â†” UDP discovery (ports 8888/8889)
  - Application firmware (`ota_1`) â†” MQTT broker (status/control)
  - WiFi AP/Router between all networked components

### Sequence Diagrams
- `sequence_web_upload.mmd`: Browser-based upload to `/update` with streaming and restart.
- `sequence_backend_push.mmd`: Backend push flow with begin/write/end and error cases.

### Flowchart
- `flowchart.mmd`: End-to-end OTA process with decision points (WiFi, sensor presence, begin/write/end success, restart).

### State Diagram
- `state_diagram.mmd`: Bootloader â†” Main firmware â†” OTA In Progress â†” Error/Recovery states.

## Technical Specifications

### Communication Protocols
- HTTP:
  - Bootloader: `WebServer` on port 80. Endpoints: `/`, `/update`, `/info`, `/ping`, `/id`, `/ota/begin`, `/ota/write`, `/ota/end` (`ESP_32_OTA/src/main.cpp:196-339`).
  - Sensor firmwares: `ESPAsyncWebServer` with `POST /update` (`TOF_Firmware_bin_Generator/src/config_handler.cpp:212-241`).
- UDP:
  - Discovery port `8888`, response `8889` (`ESP_32_OTA/src/main.cpp:59-65, 341-389`, `OSI_Firmware_bin_Generator/src/main.cpp:24-31`).
- MQTT:
  - Used in application firmwares for telemetry/control; not used for OTA data transfer (`OSI_Firmware_bin_Generator/src/main.cpp:88-91`).

### Security Measures
- Transport:
  - OTA occurs over HTTP without TLS; no `esp_https_ota`/`WiFiClientSecure` in OTA path.
- Access control:
  - OTA endpoints have no authentication; accessible on local network.
- Integrity:
  - Relies on `Update` success and internal write checks; no cryptographic signature or checksum verification.
- Recommendations:
  - Use HTTPS with certificate pinning for OTA push.
  - Add authentication (HMAC tokens or mutual TLS).
  - Implement firmware SHA256 verification and optional signing (ECDSA/RSA).
  - Rate-limit and CSRF-protect web upload.

### Memory Requirements and Constraints
- Partition layout (typical 4MB flash, per `ESPAsyncWebServer` reference table):
  - `ota_0` app: ~1856 KB; `ota_1` app: ~1856 KB; NVS/otadata/spiffs/coredump reserved (`TOF_Firmware_bin_Generator/.pio/libdeps/esp32dev/ESPAsyncWebServer/partitions-4MB.csv:1-7`).
- OTA write strategy:
  - Streaming writes consume minimal RAM; buffers are per-chunk from HTTP upload or push.
- Constraints:
  - Firmware binary must fit available app partition size.
  - Erase of inactive partition ensures maximal contiguous space.

### Performance Characteristics
- LED intervals: WiFi/Sensor LEDs toggle every 3s in bootloader (`ESP_32_OTA/src/main.cpp:47-51, 159-175, 178-194`).
- UDP discovery check: Every 1s in bootloader; every 5s in OSI firmware (`ESP_32_OTA/src/main.cpp:63-65, 341-389`; `OSI_Firmware_bin_Generator/src/main.cpp:29-31, 128-183`).
- Sensor check interval: 2s (`ESP_32_OTA/src/main.cpp:40-42, 501-521`).
- WiFi reconnection: On loss, attempts reconnect with 5s delay (`ESP_32_OTA/src/main.cpp:493-499`).
- OTA throughput: Bound by HTTP chunk size and WiFi RSSI; typical ESP32 SPI flash write speeds support multi-hundred KB/s in good conditions.

## API Reference Summary
- Full details in `api_reference.md`. Key endpoints:
  - Web upload: `POST /update` (multipart form-data `update`).
  - Backend push: `POST /ota/begin`, `POST /ota/write`, `POST /ota/end`.
  - Informational: `GET /`, `GET /info`, `GET /ping`, `GET /id`.

## Troubleshooting
- WiFi loss:
  - Device logs reconnection attempts; ensure SSID/password and AP stability (`ESP_32_OTA/src/main.cpp:493-499`).
- EEPROM/sensor not detected:
  - Bootloader may restart or remain in OTA mode; reconnect sensor and retry; check I2C wiring (`ESP_32_OTA/src/main.cpp:404-425`, `510-519`).
- OTA write failures:
  - Inspect serial logs for `Update.printError`; verify chunk sizes and JSON/hex validity for push API (`ESP_32_OTA/src/main.cpp:270-307, 313-318`).
- Partition operations fail:
  - If `esp_ota_set_boot_partition` or `esp_partition_erase_range` fails, device falls back to restart; check partition table compatibility and flash health (`ESP_32_OTA/src/main.cpp:77-86`; `OSI_Firmware_bin_Generator/src/main.cpp:254-271`).

## Version Control Information
- `firmware_version` reported by bootloader in UDP discovery: `OTA_BOOTLOADER` (`ESP_32_OTA/src/main.cpp:371`).
- OSI firmware UDP discovery reports version `1.0` (`OSI_Firmware_bin_Generator/src/main.cpp:158-159`).
- Include commit, branch, and build date in PDF header during generation.

## Appendices
- File references used in this document follow `file_path:line_number` format for traceability.

