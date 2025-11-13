#include "esp_ota_ops.h"
#include "esp_partition.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h>


// Include modular headers
#include "config_handler.h"
#include "experiment_manager.h"
#include "mqtt_handler.h"
#include "sensor_communication.h"


// Network configuration
const char *ssid = "LabExpert_1.0";
const char *password = "11111111";
IPAddress local_IP(192, 168, 137, 15);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

// UDP Discovery configuration
WiFiUDP udp;
const int UDP_DISCOVERY_PORT = 8888;
const int UDP_RESPONSE_PORT = 8889;
const char *UDP_DISCOVERY_MAGIC = "LABEXPERT_DISCOVERY";
const char *UDP_RESPONSE_MAGIC = "LABEXPERT_RESPONSE";
unsigned long lastUDPCheck = 0;
const unsigned long UDP_CHECK_INTERVAL = 5000; // Check every 5 seconds

// Initialize I2C buses
#if !defined(EEPROM_SDA)
#define EEPROM_SDA 18
#endif
#if !defined(EEPROM_SCL)
#define EEPROM_SCL 19
#endif

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Oscillation Counter System ===");

  // Initialize I2C buses
  Wire.begin(EEPROM_SDA, EEPROM_SCL);

  Serial.printf("I2C Initialized: SDA=%d, SCL=%d\n", EEPROM_SDA, EEPROM_SCL);

  // Initialize pins
  pinMode(SENSOR_LED, OUTPUT);
  pinMode(WIFI_LED, OUTPUT);
  pinMode(SENSOR_PIN, INPUT);
  digitalWrite(SENSOR_LED, LOW);
  digitalWrite(WIFI_LED, HIGH);

  // Network setup
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected. IP: %s\n",
                  WiFi.localIP().toString().c_str());

    // Detect sensor from EEPROM
    bool sensorDetected = detectSensorFromEEPROM();

    if (!sensorDetected) {
      Serial.println("❌ EEPROM not detected!");
      delay(3000);
      ESP.restart();
    }

    // Get device ID (this will set sensorID variable)
    getDeviceIDFromMAC();

    Serial.printf("Detected sensor type: %s\n", sensorType.c_str());
    Serial.printf("Device ID: %s\n", sensorID.c_str());

    // Initialize MQTT connection
    setupMQTT();
    Serial.println("MQTT configured");

    // Initialize UDP for discovery
    if (udp.begin(UDP_DISCOVERY_PORT)) {
      Serial.printf("UDP discovery listening on port %d\n", UDP_DISCOVERY_PORT);
    } else {
      Serial.println("Failed to start UDP discovery");
    }

    // Blink WiFi LED once to indicate successful connection
    digitalWrite(WIFI_LED, LOW);
    delay(100);
    digitalWrite(WIFI_LED, HIGH);
    delay(100);
    digitalWrite(WIFI_LED, LOW);
  } else {
    Serial.println("\nWiFi connection failed!");
    digitalWrite(WIFI_LED, HIGH);
  }

  Serial.println("System initialized - Waiting for start command");
}

void updateWiFiLED() {
  static unsigned long lastWiFiBlink = 0;

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(WIFI_LED, HIGH);
  } else {
    if (millis() - lastWiFiBlink >= 5000) {
      digitalWrite(WIFI_LED, HIGH);
      delay(50);
      digitalWrite(WIFI_LED, LOW);
      lastWiFiBlink = millis();
    }
  }
}

void handleUDPDiscovery() {
  // Check for incoming UDP packets
  int packetSize = udp.parsePacket();
  if (packetSize) {
    char packetBuffer[255];
    int len = udp.read(packetBuffer, sizeof(packetBuffer));
    if (len > 0) {
      packetBuffer[len] = '\0';

      // Check if this is a discovery packet
      if (strcmp(packetBuffer, UDP_DISCOVERY_MAGIC) == 0) {
        IPAddress remoteIP = udp.remoteIP();

        // Network segmentation: Only respond to devices on our network segment
        // This prevents interference between team members on the same physical
        // network
        IPAddress ourNetwork = WiFi.localIP();
        ourNetwork[3] = 0; // Get network address (e.g., 192.168.137.0)

        IPAddress remoteNetwork = remoteIP;
        remoteNetwork[3] = 0; // Get remote network address

        if (ourNetwork == remoteNetwork) {
          Serial.println(
              "Received UDP discovery request from our network segment");

          // Create response JSON
          DynamicJsonDocument doc(256);
          doc["device_id"] = sensorID;
          doc["ip_address"] = WiFi.localIP().toString();
          doc["firmware_version"] = "1.0";
          doc["sensor_type"] = sensorType;
          doc["magic"] = UDP_RESPONSE_MAGIC;
          doc["ssid"] = ssid; // Include SSID for backend filtering

          String response;
          serializeJson(doc, response);

          // Send response back to the sender's IP but to the response port
          // (8889)
          udp.beginPacket(remoteIP, UDP_RESPONSE_PORT);
          udp.write((const uint8_t *)response.c_str(), response.length());
          udp.endPacket();

          Serial.printf("Sent UDP discovery response to %s:%d\n",
                        remoteIP.toString().c_str(), UDP_RESPONSE_PORT);
          Serial.printf("Response content: %s\n", response.c_str());
        } else {
          Serial.printf(
              "Ignoring UDP discovery from different network segment: %s\n",
              remoteIP.toString().c_str());
        }
      }
    }
  }
}

void loop() {
  updateWiFiLED();
  checkSensorStatus();
  handleBackendCleanup();
  mqttLoop();
  manageExperimentLoop();

  // Handle UDP discovery periodically
  handleUDPDiscovery();

  // Check restart trigger pin
  if (digitalRead(RESTART_TRIGGER_PIN) == LOW) {
    Serial.println(
        "⚠️ Restart trigger pin activated (LOW) - initiating OTA restart...");
    cleanFirmwareAndBootOTA();
  }

  delay(1);
}

void cleanFirmwareAndBootOTA() {
  Serial.println("Cleaning firmware and booting to OTA partition...");
  experimentRunning = false;
  dataReady = false;

  if (mqttClient.connected()) {
    mqttClient.disconnect();
    Serial.println("MQTT disconnected");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi disconnected");

  delay(1000);

  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == NULL) {
    Serial.println("Current running partition unknown");
    Serial.println("Restarting ESP32 as fallback...");
    delay(1000);
    ESP.restart();
    return;
  }
  Serial.printf("Current running partition: %s\n", running->label);

  const esp_partition_t *target = NULL;
  if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0) {
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
  } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) {
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  } else if (running->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!target) {
      target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    }
  } else {
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                      ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (!target) {
      target = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                        ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    }
  }

  if (target) {
    Serial.printf("Selected target partition: %s\n", target->label);
    if (esp_ota_set_boot_partition(target) == ESP_OK) {
      Serial.println("Boot partition updated successfully");
      delay(1000);
      ESP.restart();
      return;
    } else {
      Serial.println("Failed to set boot partition");
    }
  } else {
    Serial.println("No suitable OTA partition found");
  }

  Serial.println("Restarting ESP32 as fallback...");
  delay(1000);
  ESP.restart();
}
