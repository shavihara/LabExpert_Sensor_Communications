#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <Wire.h>

#include "../include/sensor_communication.h"
#include "../include/config_handler.h"
#include "../include/experiment_manager.h"
#include "../include/mqtt_handler.h"

#include "nvs_wifi_credentials.h"
#include "nvs_mqtt_credentials.h"

// Hardware Config
#define RESTART_TRIGGER_PIN 32 // Keep consistent with TOF
// ONE_WIRE_BUS is 23, defined in sensor_communication.h

// Network
char ssid[33];
char password[65];

// MQTT Config
char mqttBroker[40] = "";
uint16_t mqttPort = 1883;
char backendMAC[18] = "";

// Function Prototypes
bool connectWithDHCP();

void cleanFirmwareAndBootOTA();

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== THR Firmware (DS18B20) ===");
    
    // Init I2C for EEPROM
    Wire.begin(EEPROM_SDA, EEPROM_SCL);
    
    // Init status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, HIGH); // Booting
    
    // 1. Load Credentials
    if (!loadWiFiCredentialsFromNVS(ssid, sizeof(ssid), password, sizeof(password))) {
        Serial.println("❌ No WiFi credentials!");
        cleanFirmwareAndBootOTA();
        return;
    }
    
    if (!loadMQTTCredentialsFromNVS(mqttBroker, sizeof(mqttBroker), &mqttPort, backendMAC, sizeof(backendMAC))) {
        Serial.println("❌ No MQTT credentials!");
        cleanFirmwareAndBootOTA();
        return;
    }
    
    // 2. Initialize DS18B20 Sensor
    if (!initializeTHRSensor()) {
        Serial.println("Warning: DS18B20 not found on boot.");
    }
    
    // 3. Connect WiFi
    WiFi.mode(WIFI_STA);
    if (connectWithDHCP()) {
        Serial.printf("WiFi Connected: %s\n", WiFi.localIP().toString().c_str());
        
        // 4. Check EEPROM for ID/Type
        bool sensorDetected = detectSensorFromEEPROM();
        if (!sensorDetected) {
            Serial.println("❌ EEPROM ID detection failed (Failsafe)");
            // Similar failsafe to TOF: Switch to OTA_0
            cleanFirmwareAndBootOTA();
            return;
        }
        
        // Get ID
        sensorID = getDeviceIDFromMAC();
        
        // 5. Setup MQTT
        setupMQTT();
        
    } else {
        Serial.println("WiFi Connection Failed");
    }
    
    // 6. Setup Web Server
    server.on("/status", HTTP_GET, handleStatus);
    server.on("/config", HTTP_GET, handleConfigure); // GET for read
    server.on("/config", HTTP_POST, handleConfigure); // POST for set
    server.on("/start", HTTP_POST, handleStart);
    server.on("/stop", HTTP_POST, handleStop);
    server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
    server.begin();
    
    digitalWrite(STATUS_LED, LOW); // Boot complete
}

void loop() {
    mqttLoop();
    manageExperimentLoop();
    
    // Check restart pin
    pinMode(RESTART_TRIGGER_PIN, INPUT_PULLUP);
    if (digitalRead(RESTART_TRIGGER_PIN) == LOW) {
        cleanFirmwareAndBootOTA();
    }
    
    yield();
}

bool connectWithDHCP() {
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    return (WiFi.status() == WL_CONNECTED);
}
