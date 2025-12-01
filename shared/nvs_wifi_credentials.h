#pragma once
/**
 * @file nvs_wifi_credentials.h
 * @brief Lightweight NVS WiFi credential reader for sensor firmwares
 * 
 * This header-only utility reads WiFi credentials from NVS storage
 * that were saved by the OTA bootloader via Bluetooth provisioning.
 * 
 * Usage:
 * @code
 * #include "nvs_wifi_credentials.h"
 * 
 * char ssid[33];
 * char password[65];
 * if (loadWiFiCredentialsFromNVS(ssid, sizeof(ssid), password, sizeof(password))) {
 *   WiFi.begin(ssid, password);
 * } else {
 *   // No credentials found - boot back to OTA
 * }
 * @endcode
 */

#include <nvs_flash.h>
#include <nvs.h>

/**
 * @brief Load WiFi credentials from NVS storage
 * 
 * Reads SSID and password from the same NVS namespace/keys used by
 * the OTA bootloader's WiFiCredentialManager.
 * 
 * @param ssidBuf Buffer to store SSID (should be at least 33 bytes)
 * @param ssidSize Size of SSID buffer
 * @param passBuf Buffer to store password (should be at least 65 bytes)
 * @param passSize Size of password buffer
 * @return true if credentials were successfully loaded
 * @return false if credentials not found or invalid
 */
bool loadWiFiCredentialsFromNVS(char* ssidBuf, size_t ssidSize, char* passBuf, size_t passSize) {
  // Initialize NVS if not already done
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    Serial.println("Failed to initialize NVS");
    return false;
  }

  // Open NVS handle (same namespace as OTA bootloader)
  nvs_handle_t handle;
  err = nvs_open("wifi", NVS_READONLY, &handle);
  if (err != ESP_OK) {
    Serial.println("No WiFi credentials found in NVS");
    return false;
  }

  // Read SSID
  size_t ssidLen = ssidSize;
  err = nvs_get_str(handle, "ssid", ssidBuf, &ssidLen);
  if (err != ESP_OK) {
    nvs_close(handle);
    Serial.println("Failed to read SSID from NVS");
    return false;
  }

  // Read password
  size_t passLen = passSize;
  err = nvs_get_str(handle, "pass", passBuf, &passLen);
  if (err != ESP_OK) {
    nvs_close(handle);
    Serial.println("Failed to read password from NVS");
    return false;
  }

  nvs_close(handle);
  
  Serial.println("✓ WiFi credentials loaded from NVS");
  Serial.printf("  SSID: %s\n", ssidBuf);
  
  return true;
}
