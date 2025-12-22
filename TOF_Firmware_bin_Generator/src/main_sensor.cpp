// TOF400F Firmware - I2C Version with Core-Based Processing
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Wire.h>
#include <VL53L1X.h>

// Include modular headers
#include "../include/sensor_communication.h"
#include "../include/config_handler.h"
#include "../include/experiment_manager.h"
#include "../include/mqtt_handler.h"
#include "../include/motor_controller.h"

// Include NVS WiFi credentials reader
#include "nvs_wifi_credentials.h"
#include "../../shared/nvs_mqtt_credentials.h"

// Hardware configuration - I2C Pins
#define RESTART_TRIGGER_PIN 32
#define BLE_LED_PIN 12 
#define SENSOR_LED_PIN 13 
#define WIFI_LED_PIN 14 
#define OTA_LED_PIN 16
#define EEPROM_SDA 18
#define EEPROM_SCL 19
#define TOF_SDA 21
#define TOF_SCL 22
#define SENSOR_ACTIVE_LED_PIN 27

#include "LedController.h"

LedController wifiLed(WIFI_LED_PIN, true);   // Active LOW
LedController bleLed(BLE_LED_PIN, true);     // Active LOW
LedController sensorLed(SENSOR_LED_PIN, true); // Active LOW
LedController sensorActiveLed(SENSOR_ACTIVE_LED_PIN, true); // Active LOW
LedController otaLed(OTA_LED_PIN, true);     // Active LOW

// Network configuration - WiFi credentials loaded from NVS at runtime
char ssid[33];      // Will be loaded from NVS
char password[65];  // Will be loaded from NVS
// Network configuration - All settings obtained from DHCP

// Function prototypes
bool connectWithDynamicIP();
bool connectWithDHCP();
void safeRestartSequence(); // FWD DECLARE

// MQTT configuration - Loaded from NVS (set by OTA bootloader via UDP discovery)
char mqttBroker[40] = "";
uint16_t mqttPort = 1883;
char backendMAC[18] = "";

void setup()
{
    Serial.begin(115200);
    Serial.println("\n=== TOF400F Firmware - I2C Version with Core-Based Processing ===");

    // Initialize I2C buses
    Wire.begin(EEPROM_SDA, EEPROM_SCL);
    Wire1.begin(TOF_SDA, TOF_SCL);

    Serial.printf("I2C Buses Initialized:\n");
    Serial.printf("  - EEPROM: SDA=%d, SCL=%d\n", EEPROM_SDA, EEPROM_SCL);
    Serial.printf("  - TOF Sensor: SDA=%d, SCL=%d\n", TOF_SDA, TOF_SCL);

    // Load WiFi credentials from NVS
    if (!loadWiFiCredentialsFromNVS(ssid, sizeof(ssid), password, sizeof(password))) {
        Serial.println("❌ No WiFi credentials found in NVS!");
        Serial.println("Booting back to OTA for credential provisioning...");
        delay(2000);
        safeRestartSequence();
        return; // safeRestartSequence calls cleanFirmwareAndBootOTA which restarts
    }

    // Load MQTT credentials from NVS
    Serial.println("Loading MQTT credentials from NVS...");
    if (!loadMQTTCredentialsFromNVS(mqttBroker, sizeof(mqttBroker), &mqttPort, 
                                     backendMAC, sizeof(backendMAC))) {
        Serial.println("❌ No MQTT credentials found in NVS");
        Serial.println("   Rebooting to OTA bootloader for initial setup...");
        delay(2000);
        safeRestartSequence();
        return;
    }
    
    Serial.printf("✅ MQTT broker loaded: %s:%d\n", mqttBroker, mqttPort);
    if (strlen(backendMAC) > 0) {
        Serial.printf("   Backend MAC: %s\n", backendMAC);
    }

    // Init LEDs
    wifiLed.begin();
    bleLed.begin();
    sensorLed.begin();
    sensorActiveLed.begin();
    otaLed.begin();
    
    wifiLed.set(LedController::ON); // ON = Disconnected
    
    pinMode(RESTART_TRIGGER_PIN, INPUT_PULLUP);

    delay(300);

    // Initialize TOF sensor
    if (initializeTOFSensor())
    {
        Serial.println("TOF Sensor initialization successful");
    }
    else
    {
        Serial.println("WARNING: TOF Sensor init issues - check wiring");
    }

    // Initialize Motor Controller
    motor.begin();

    // Initialize hardware timer for interrupt-driven sampling
    if (initHardwareTimer())
    {
        Serial.println("Hardware timer initialized successfully");
    }
    else
    {
        Serial.println("ERROR: Hardware timer initialization failed");
    }
    
    // Network setup with dynamic IP assignment
    WiFi.mode(WIFI_STA);
    
    Serial.println("Starting dynamic IP connection...");
    
    // Try dynamic IP assignment (multiple static IPs, then fallback to DHCP)
    bool wifiConnected = connectWithDynamicIP();
    
    if (wifiConnected) {
        Serial.printf("\n✅ WiFi connected successfully. IP: %s\n", WiFi.localIP().toString().c_str());
        wifiLed.set(LedController::BLINK_SLOW); // Connected
        
        // Detect sensor from EEPROM with failsafe mechanism
        bool sensorDetected = detectSensorFromEEPROM();

        // CRITICAL: EEPROM Detection Failsafe
        if (!sensorDetected)
        {
            Serial.println("❌ EEPROM not detected! Implementing failsafe mechanism...");

            // Check current running partition
            const esp_partition_t *running = esp_ota_get_running_partition();
            Serial.printf("Current running partition: %s\n", running->label);

            // If running on ota_1, switch back to ota_0 and erase ota_1
            if (running && strcmp(running->label, "ota_1") == 0)
            {
                Serial.println("Running on ota_1 with EEPROM failure - switching to ota_0...");

                // Set boot partition to ota_0 (ESP_32_OTA bootloader)
                const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
                if (ota_0 != NULL)
                {
                    esp_err_t err = esp_ota_set_boot_partition(ota_0);
                    if (err == ESP_OK)
                    {
                        Serial.println("✅ Boot partition set to ota_0 (ESP_32_OTA)");

                        // Erase current partition (ota_1) to clean up
                        Serial.println("🗑️ Erasing ota_1 partition...");
                        err = esp_partition_erase_range(running, 0, running->size);
                        if (err == ESP_OK)
                        {
                            Serial.println("✅ ota_1 partition erased successfully");
                        }
                        else
                        {
                            Serial.printf("❌ Failed to erase ota_1 partition: %s\n", esp_err_to_name(err));
                        }

                        Serial.println("🔄 Restarting to ESP_32_OTA bootloader...");
                        delay(2000);
                        ESP.restart();
                    }
                    else
                    {
                        Serial.printf("❌ Failed to set boot partition: %s\n", esp_err_to_name(err));
                    }
                }
                else
                {
                    Serial.println("❌ ota_0 partition not found!");
                }
            }
            else
            {
                Serial.println("Running on ota_0 - EEPROM failure handled by OTA bootloader");
            }

            // If we reach here, something went wrong - restart anyway
            Serial.println("⚠️ Failsafe mechanism completed - restarting...");
            delay(3000);
            ESP.restart();
        }

        sensorWasPresent = sensorDetected;
        Serial.printf("Detected sensor type: %s\n", sensorType.c_str());

        // Get device ID from MAC address
        sensorID = getDeviceIDFromMAC();
        Serial.printf("Device ID: %s\n", sensorID.c_str());

        // Initialize MQTT connection
        setupMQTT();
        Serial.printf("MQTT configured for broker at %s:%d\n", mqttBroker, mqttPort);
    }
    else
    {
        Serial.println("\nWiFi connection failed!");
    }

    // Setup HTTP routes
    server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);

    // CORS handling
    server.onNotFound([](AsyncWebServerRequest *request)
                      {
        if (request->method() == HTTP_OPTIONS) {
            request->send(200);
        } else {
            request->send(404);
        } });

    server.begin();
    Serial.println("HTTP server started");
    
     // Initial sensor LED state
    if (sensorID != "UNKNOWN") {
       sensorLed.set(LedController::OFF); // Connected
    } else {
       sensorLed.set(LedController::ON); // Disconnected
    }
}

void loop()
{
    // Update LEDs
    wifiLed.update();
    bleLed.update();
    sensorLed.update();
    sensorActiveLed.update();
    otaLed.update();
    
    // Sensor Active Logic:
    // If experiment running -> Blink Pin 27
    // Else -> OFF
    if (experimentRunning) {
        if (sensorActiveLed.getState() != LedController::BLINK_FAST) {
             sensorActiveLed.set(LedController::BLINK_FAST);
        }
    } else {
        sensorActiveLed.set(LedController::OFF);
    }
    // Check sensor status periodically
    checkSensorStatus();

    // Handle backend cleanup requests
    handleBackendCleanup();

    // MQTT maintenance
    mqttLoop();

    // Manage experiment execution
    manageExperimentLoop();
    
    // Motor State Machine
    motor.update();
    
    // Check restart trigger pin
    if (digitalRead(RESTART_TRIGGER_PIN) == LOW) {
        Serial.println("⚠️ Restart trigger pin activated (LOW) - initiating SAFE OTA restart...");
        safeRestartSequence();
    }

    delay(1);

    // Feed the watchdog to prevent resets when Serial Monitor is not open
    yield();
}

void safeRestartSequence() {
    Serial.println("🛑 INITIATING SAFE RESTART SEQUENCE 🛑");
    
    // 1. Move Plane to MIN (Home)
    experimentRunning = false; // Stop any active experiment logic
    motor.executeSafeShutdown();
    
    unsigned long shutdownStart = millis();
    while (!motor.isShutdownComplete()) {
        motor.update(); // Keep processing motor logic
        
        // Timeout safety (e.g. 30 seconds)
        if (millis() - shutdownStart > 30000) {
            Serial.println("Safety Timeout! Forcing restart...");
            break;
        }
        delay(10);
    }
    
    Serial.println("✅ Safe Position Reached (or Timeout). Cleaning up...");
    
    // 2. Perform standard cleanup and OTA boot
    cleanFirmwareAndBootOTA();
}
    
void cleanFirmwareAndBootOTA()
{
    Serial.println("Cleaning firmware and booting to OTA partition...");

    // Clean up any running processes
    experimentRunning = false;
    dataReady = false;

    // Disconnect MQTT gracefully
    if (mqttClient.connected())
    {
        mqttClient.disconnect();
        Serial.println("MQTT disconnected");
    }

    // Stop WiFi
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi disconnected");

    delay(1000);

    // Get the running partition
    const esp_partition_t *running = esp_ota_get_running_partition();
    Serial.printf("Current running partition: %s\n", running->label);

    // Find the OTA boot partition (partition 0)
    const esp_partition_t *ota_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);

    if (ota_partition != NULL)
    {
        Serial.printf("Found OTA partition: %s\n", ota_partition->label);

        // Set boot partition to OTA 0
        if (esp_ota_set_boot_partition(ota_partition) == ESP_OK)
        {
            Serial.println("Boot partition set to OTA_0 successfully");

            // Restart the ESP32 to boot into OTA partition
            Serial.println("Restarting ESP32 to boot into OTA partition...");
            delay(1000);
            ESP.restart();
        }
        else
        {
            Serial.println("Failed to set boot partition to OTA_0");
        }
    }
    else
    {
        Serial.println("OTA partition not found");
    }

    // If we get here, something went wrong - restart anyway
    Serial.println("Restarting ESP32 as fallback...");
    delay(1000);
    ESP.restart();
}

// ================= DYNAMIC IP IMPLEMENTATION =================

bool connectWithDynamicIP() {
    Serial.println("🔧 Connecting to WiFi using DHCP...");
    return connectWithDHCP();
}



bool connectWithDHCP() {
    Serial.println("🌐 Trying DHCP connection...");
    
    // Use DHCP (no static IP configuration)
    WiFi.begin(ssid, password);
    
    Serial.print("Connecting via DHCP");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
        yield(); // Feed watchdog
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✅ DHCP connection successful. IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    
    Serial.println("\n❌ DHCP connection failed");
    return false;
}
