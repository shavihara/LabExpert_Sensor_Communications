#pragma once
/**
 * @file nvs_mqtt_credentials.h
 * @brief NVS MQTT credential reader/writer for ESP32 firmwares
 * 
 * MQTT credentials are discovered by ESP_32_OTA via UDP broadcast
 * and saved to NVS for use by all firmwares.
 * 
 * Usage in ESP_32_OTA:
 * @code
 * #include "nvs_mqtt_credentials.h"
 * 
 * // After UDP discovery receives MQTT broker info
 * saveMQTTCredentialsToNVS("192.168.1.100", 1883, "aa:bb:cc:dd:ee:ff");
 * @endcode
 * 
 * Usage in Firmware Generators (TOF/OSI/UltraSonic):
 * @code
 * #include "nvs_mqtt_credentials.h"
 * 
 * char mqttBroker[40];
 * uint16_t mqttPort;
 * char backendMAC[18];
 * 
 * if (loadMQTTCredentialsFromNVS(mqttBroker, sizeof(mqttBroker), &mqttPort, 
 *                                 backendMAC, sizeof(backendMAC))) {
 *   // Use mqttBroker and mqttPort
 * } else {
 *   // No credentials - reboot to OTA
 * }
 * @endcode
 */

#include <nvs_flash.h>
#include <nvs.h>

/**
 * @brief Save MQTT credentials to NVS (used by ESP_32_OTA)
 * 
 * @param broker MQTT broker IP address (e.g., "192.168.1.100")
 * @param port MQTT broker port (typically 1883)
 * @param backendMAC Backend MAC address (optional, for future use)
 * @return true if successfully saved
 * @return false if save failed
 */
bool saveMQTTCredentialsToNVS(const char* broker, uint16_t port, const char* backendMAC = nullptr) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("mqtt", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        Serial.println("❌ Failed to open NVS for MQTT write");
        return false;
    }

    // Save broker IP
    err = nvs_set_str(handle, "broker", broker);
    if (err != ESP_OK) {
        nvs_close(handle);
        Serial.println("❌ Failed to save MQTT broker to NVS");
        return false;
    }

    // Save port
    err = nvs_set_u16(handle, "port", port);
    if (err != ESP_OK) {
        nvs_close(handle);
        Serial.println("❌ Failed to save MQTT port to NVS");
        return false;
    }

    // Save backend MAC address (optional)
    if (backendMAC && strlen(backendMAC) > 0) {
        err = nvs_set_str(handle, "backend_mac", backendMAC);
        // Don't fail if MAC save fails (it's optional for future use)
        if (err != ESP_OK) {
            Serial.println("⚠️ Warning: Failed to save backend MAC to NVS");
        }
    }

    // Commit changes
    err = nvs_commit(handle);
    nvs_close(handle);

    if (err == ESP_OK) {
        Serial.printf("✅ MQTT credentials saved to NVS: %s:%d\n", broker, port);
        if (backendMAC && strlen(backendMAC) > 0) {
            Serial.printf("   Backend MAC: %s\n", backendMAC);
        }
        return true;
    }
    
    Serial.println("❌ Failed to commit MQTT credentials to NVS");
    return false;
}

/**
 * @brief Load MQTT credentials from NVS (used by firmwares)
 * 
 * @param brokerBuf Buffer to store broker IP (should be at least 40 bytes)
 * @param brokerSize Size of broker buffer
 * @param port Pointer to store port number
 * @param macBuf Optional buffer to store backend MAC (at least 18 bytes)
 * @param macSize Size of MAC buffer
 * @return true if successfully loaded
 * @return false if credentials not found or invalid
 */
bool loadMQTTCredentialsFromNVS(char* brokerBuf, size_t brokerSize, uint16_t* port, 
                                 char* macBuf = nullptr, size_t macSize = 0) {
    // Initialize NVS if needed
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        Serial.println("❌ Failed to initialize NVS");
        return false;
    }

    // Open NVS handle
    nvs_handle_t handle;
    err = nvs_open("mqtt", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        Serial.println("❌ No MQTT credentials found in NVS");
        return false;
    }

    // Read broker IP
    size_t brokerLen = brokerSize;
    err = nvs_get_str(handle, "broker", brokerBuf, &brokerLen);
    if (err != ESP_OK) {
        nvs_close(handle);
        Serial.println("❌ Failed to read MQTT broker from NVS");
        return false;
    }

    // Read port
    err = nvs_get_u16(handle, "port", port);
    if (err != ESP_OK) {
        nvs_close(handle);
        Serial.println("❌ Failed to read MQTT port from NVS");
        return false;
    }

    // Read backend MAC (optional)
    if (macBuf && macSize > 0) {
        size_t macLen = macSize;
        err = nvs_get_str(handle, "backend_mac", macBuf, &macLen);
        if (err != ESP_OK) {
            macBuf[0] = '\0'; // Clear if not found
        }
    }

    nvs_close(handle);
    
    Serial.println("✅ MQTT credentials loaded from NVS");
    Serial.printf("   Broker: %s:%d\n", brokerBuf, *port);
    
    return true;
}

/**
 * @brief Check if MQTT credentials exist in NVS
 * 
 * @return true if credentials exist
 * @return false if credentials not found
 */
bool hasMQTTCredentialsInNVS() {
    char broker[40];
    uint16_t port;
    return loadMQTTCredentialsFromNVS(broker, sizeof(broker), &port);
}
