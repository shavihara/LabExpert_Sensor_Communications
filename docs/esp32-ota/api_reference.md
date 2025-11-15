# ESP32 OTA API Reference

Version: 1.0

Base URL: `http://<device-ip>/`

## Endpoints — Bootloader (`WebServer`)

### GET `/`
- Returns an HTML page with sensor info and a file upload form for OTA.
- Success: `200 text/html`

### GET `/info`
- Returns JSON with sensor metadata.
- Example response:
```
{
  "sensor_type": "<type>",
  "sensor_id": "<id>"
}
```
- Source: `ESP_32_OTA/src/main.cpp:244-251`

### GET `/ping`
- Health check endpoint.
- Response: `200 text/plain` with body `"pong"`.
- Source: `ESP_32_OTA/src/main.cpp:253-256`

### GET `/id`
- Returns JSON with `id` used for firmware selection.
- Source: `ESP_32_OTA/src/main.cpp:257-263`

### POST `/update` (multipart/form-data)
- Form field: `update` (firmware binary file).
- Behavior: Streams chunks to flash using `Update.write`.
- Completion: Responds `OK` or `FAIL` based on `Update.hasError()`, then restarts on success.
- Source: `ESP_32_OTA/src/main.cpp:211-242`

### POST `/ota/begin`
- Body: JSON
```
{
  "size": <integer-bytes>
}
```
- Behavior: Initializes OTA write with `Update.begin(size)`; sets internal progress state.
- Responses:
  - `200 {"success":true}` on init success
  - `400 {"success":false,"error":"bad_json"}` on parse failure
  - `500 {"success":false}` on init/write errors
- Source: `ESP_32_OTA/src/main.cpp:265-287`

### POST `/ota/write`
- Body: JSON
```
{
  "offset": <integer>,
  "size": <integer>,
  "data": "<hex-encoded-chunk>"
}
```
- Behavior: Validates hex and size; streams bytes to flash via `Update.write`.
- Responses:
  - `200 {"success":true}` on write success
  - `400` with `error` one of: `bad_json`, `bad_hex`, `size_mismatch`, `not_in_progress`
  - `500 {"success":false}` on write failure
- Source: `ESP_32_OTA/src/main.cpp:288-321`

### POST `/ota/end`
- Body: none
- Behavior: Finalizes OTA with `Update.end(true)`; on success responds and restarts.
- Responses:
  - `200 {"success":true}` then device restarts
  - `400 {"success":false,"error":"not_in_progress"}` if no active session
  - `500 {"success":false}` on finalize failure
- Source: `ESP_32_OTA/src/main.cpp:322-339`

## Endpoints — Sensor Firmwares (`ESPAsyncWebServer`)

### POST `/update`
- Upload handler streams with `Update.write(data)`; finalizes with `Update.end(true)` and restarts.
- Responses: `OK` or `FAIL` (text/plain) based on `Update.hasError()`.
- Source: `TOF_Firmware_bin_Generator/src/config_handler.cpp:212-241`

## UDP Discovery
- Request: UDP packet with ASCII `"LABEXPERT_DISCOVERY"` to port `8888`.
- Response: JSON to sender on port `8889`.
- Bootloader fields include: `device_id`, `ip_address`, `firmware_version = "OTA_BOOTLOADER"`, `sensor_type`, `availability`, `magic`.
- Source: `ESP_32_OTA/src/main.cpp:341-389`

## Errors & Status Codes
- `400 Bad Request`: JSON parse errors, invalid hex, size mismatch, or wrong state.
- `500 Internal Server Error`: Flash write or finalize failures.
- `200 OK`: Successful write or initialization.

## Notes
- Authentication is not enforced; endpoints are accessible over HTTP.
- Ensure firmware size fits target partition; typical `ota_0`/`ota_1` capacity ≈ 1.8 MB.
