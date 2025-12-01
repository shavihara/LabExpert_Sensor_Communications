#pragma once
/**
 * @file wifi_credentials.h
 * @brief Bluetooth-based WiFi credential manager for ESP32 OTA systems.
 *
 * Memory Requirements:
 * - Total runtime usage during provisioning under 30KB using NimBLE (BLE)
 * - Fixed-size buffers only; no Arduino String or heap allocations after init
 * - Task stack sizes limited to ≤ 2048 bytes each
 *
 * Usage Example:
 * @code
 * #include "wifi_credentials.h"
 * WiFiCredentialManager mgr;
 * void setup() {
 *   Serial.begin(115200);
 *   mgr.begin();
 *   if (mgr.connectWiFi()) {
 *     // Continue startup: start web server/OTA
 *   }
 * }
 * void loop() {
 *   // Existing application logic
 * }
 * @endcode
 */

#include <Arduino.h>

/**
 * @class WiFiCredentialManager
 * @brief Manages WiFi credentials using NVS and BLE provisioning with minimal memory footprint.
 */
class WiFiCredentialManager {
public:
  /**
   * @brief Initialize NVS, button handling and internal state.
   * @return true if initialization succeeded
   */
  bool begin();

  /**
   * @brief Connect to WiFi using saved credentials, waiting up to 15s.
   * @return true if connected and got IP
   */
  bool connectWiFi();

  /**
   * @brief Check if valid WiFi credentials are stored in NVS
   * @return true if valid credentials exist
   */
  bool checkSavedCredentials();

  /**
   * @brief Start Bluetooth LE provisioning mode
   */
  void handleBluetoothProvisioning();

  /**
   * @brief Get the stored SSID
   * @return const char* pointer to internal SSID buffer
   */
  const char* getSSID() { return ssidBuf; }

  /**
   * @brief Get the stored Password
   * @return const char* pointer to internal Password buffer
   */
  const char* getPassword() { return passBuf; }

  /**
   * @brief Clear stored WiFi credentials from NVS and internal buffers
   */
  void clearCredentials();

  private:
  friend void provTaskThunk(void* arg);
#if defined(WCM_USE_NIMBLE) && WCM_USE_NIMBLE
  friend class WcmSsidCallbacks;
  friend class WcmPassCallbacks;
#endif

  // Internal fixed buffers (null-terminated)
  char ssidBuf[33];
  char passBuf[65];

  // Internal state
  void startButtonTask();
  void startProvisioningTask();
  unsigned long lastCommitWindowStart = 0;
  int commitAttempts = 0;
  bool isRateLimited();
};