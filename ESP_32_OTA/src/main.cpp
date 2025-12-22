#include <WiFi.h>
#include "wifi_credentials.h"
WiFiCredentialManager wifiMgr;
#include <WebServer.h>
#include <Update.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <WiFiUdp.h>
#include "nvs_mqtt_credentials.h"
#include <vector>
// Forward declarations
static bool hexToBytes(const String &hex, std::vector<uint8_t> &out);
static String getDeviceIDFromMAC();
// GPIO pin setup
#define BLE_LED_PIN 12 
#define SENSOR_LED_PIN 13 
#define WIFI_LED_PIN 14 
#define OTA_LED_PIN 16
#define EEPROM_WP_PIN 25 

#include "LedController.h"

LedController wifiLed(WIFI_LED_PIN, true);   // Active LOW
LedController bleLed(BLE_LED_PIN, true);     // Active LOW
LedController sensorLed(SENSOR_LED_PIN, true); // Active LOW
LedController otaLed(OTA_LED_PIN, true);     // Active LOW

// EEPROM config
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3 // Only read 3 bytes for sensor type


// Wi-Fi credentials - Managed by WiFiCredentialManager
// const char* ssid = "LabExpert_Hotspot"; // REMOVED
// const char* password = "password123";     // REMOVED
// Network configuration - All settings obtained from DHCP

// Web server configuration
const int HTTP_PORT = 80;
WebServer server(HTTP_PORT);
String deviceID = "LabExpertModule";
// Backend discovery - Uses UDP broadcast discovery (see handleUDPDiscovery)

// OTA state
bool otaInProgress = false;
size_t otaExpectedSize = 0;
size_t otaWritten = 0;

// Sensor check state
unsigned long lastSensorCheck = 0;
const unsigned long sensorCheckInterval = 2000;
String lastSensorTypeReported = "UNKNOWN";
String sensorType = "UNKNOWN";
String sensorID = "N/A";

// LED blink state managed by LedController


// Retry mechanism for EEPROM detection
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 1000 // 1 second between retries

// UDP Discovery configuration
WiFiUDP udp;
const unsigned int UDP_DISCOVERY_PORT = 8888;
const unsigned int UDP_RESPONSE_PORT = 8889;
const char *UDP_DISCOVERY_MAGIC = "LABEXPERT_DISCOVERY";
const char *UDP_RESPONSE_MAGIC = "LABEXPERT_RESPONSE";
unsigned long lastUDPCheck = 0;
const unsigned long UDP_CHECK_INTERVAL = 1000; // Check for UDP packets every second

// ========== Erase inactive OTA partition ==========
void eraseInactivePartition()
{
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
  const esp_partition_t *ota_1 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);

  const esp_partition_t *inactive = (running == ota_0) ? ota_1 : ota_0;

  if (inactive)
  {
    Serial.printf("Erasing inactive partition: %s\n", inactive->label);
    size_t offset = 0;
    const size_t chunkSize = 4096 * 8;
    while (offset < inactive->size)
    {
      size_t eraseSize = (inactive->size - offset) < chunkSize ? (inactive->size - offset) : chunkSize;
      esp_err_t err = esp_partition_erase_range(inactive, offset, eraseSize);
      if (err != ESP_OK)
      {
        Serial.printf("✘ Failed to erase offset 0x%x. Error: %d\n", (unsigned)offset, err);
        break;
      }
      offset += eraseSize;
      Serial.print(".");
      yield();
    }
    Serial.println();
    Serial.println("✓ Inactive partition erase done.");
  }
  else
  {
    Serial.println("✘ Could not find inactive OTA partition.");
  }
}

// ========== Detect sensor from EEPROM ==========
bool detectSensor()
{
  for (int retry = 0; retry < EEPROM_RETRY_COUNT; retry++)
  {
    Wire.beginTransmission(EEPROM_SENSOR_ADDR);
    int error = Wire.endTransmission();
    if (error == 0)
    {
      Wire.beginTransmission(EEPROM_SENSOR_ADDR);
      Wire.write(0x00);
      if (Wire.endTransmission(false) == 0)
      {
        Wire.requestFrom(EEPROM_SENSOR_ADDR, EEPROM_SIZE);
        if (Wire.available() >= EEPROM_SIZE)
        {
          char buffer[EEPROM_SIZE + 1];
          for (int i = 0; i < EEPROM_SIZE; i++)
          {
            buffer[i] = Wire.read();
          }
          buffer[EEPROM_SIZE] = '\0';
          String eepromData = String(buffer);
          Serial.printf("EEPROM data: %s\n", eepromData.c_str());

          if (eepromData != "")
          {
            sensorType = eepromData;
            lastSensorTypeReported = sensorType;
          }
          else
          {
            sensorType = "UNKNOWN";
            Serial.printf("⚠️ WARNNING!(Sensor Type: %s, ID: %s unable to recognize!)\n", sensorType.c_str(), sensorID.c_str());
            digitalWrite(EEPROM_WP_PIN, LOW);
          }
          Serial.printf("Sensor Type: %s, ID: %s\n", sensorType.c_str(), sensorID.c_str());
          return (sensorType != "UNKNOWN");
        }
        else
        {
          Serial.println("✘ Not enough data from EEPROM");
        }
      }
      else
      {
        Serial.println("✘ Failed to set EEPROM address");
      }
    }
    else
    {
      // EEPROM not detected - this indicates sensor was unplugged
      Serial.printf("✘ EEPROM sensor not found, I2C error: %d\n", error);
      sensorType = "UNKNOWN";
      return false;
    }
    if (retry < EEPROM_RETRY_COUNT - 1)
    {
      Serial.printf("Retrying EEPROM detection (%d/%d)...\n", retry + 1, EEPROM_RETRY_COUNT);
      delay(EEPROM_RETRY_DELAY);
    }
  }
  sensorType = "UNKNOWN";
  return false;
}

// ========== Wi-Fi & BLE LED status ==========
void handleLeds()
{
  wifiLed.update();
  bleLed.update();
  sensorLed.update();
  otaLed.update();

  // WiFi LED Logic
  // Disconnected -> ON
  // Connected -> Blink
  // Bluetooth On -> OFF (Handled by checking if in Provisioning Mode)
  
  // Check if in Bluetooth provisioning mode (not connected and no saved credentials)
  bool bluetoothMode = (WiFi.status() != WL_CONNECTED) && !wifiMgr.checkSavedCredentials();
  
  if (bluetoothMode) {
      // Bluetooth ON
      wifiLed.set(LedController::OFF);
      
      // BLE LED Logic:
      // BLE On -> ON
      // BLE Connected -> Blink (We don't easily know connection state from here without callback, assuming ON for now)
      bleLed.set(LedController::ON); 
  } else {
      // Normal WiFi Mode
      bleLed.set(LedController::OFF);
      
      if (WiFi.status() == WL_CONNECTED) {
          // Connected -> Blink
          if (wifiLed.getState() != LedController::BLINK_SLOW) {
             wifiLed.set(LedController::BLINK_SLOW);
          }
      } else {
          // Disconnected -> ON
          wifiLed.set(LedController::ON);
      }
  }
}

// ========== Sensor LED status ==========
void updateSensorLed()
{
  // Pin 13 Sensor LED:
  // Connected -> OFF
  // Disconnected -> ON
  
  if (sensorType != "UNKNOWN") {
      sensorLed.set(LedController::OFF);
  } else {
      sensorLed.set(LedController::ON);
  }
}

// ========== Web server routes ==========
void setupRoutes()
{
  server.on("/", HTTP_GET, []()
            {
    String html =
      "<h1>LabExpert Module OTA Manager</h1>"
      "<p>Sensor: " + sensorType + " (ID: " + sensorID + ")</p>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='update'>"
      "<input type='submit' value='Upload Firmware'>"
      "</form>"
      "<hr><p><a href='/info'>Sensor Info (JSON)</a></p>";
    server.send(200, "text/html", html); });

  server.on("/update", HTTP_POST, []()
            {
    server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    delay(200);
    if (!Update.hasError()) {
      Serial.println("✓ Update successful. Rebooting...");
      
      // Fast blink OTA LED on success/restart
      otaLed.set(LedController::BLINK_FAST);
      unsigned long start = millis();
      while(millis() - start < 2000) otaLed.update(); // Block briefly to show blink
      
      const esp_partition_t *running = esp_ota_get_running_partition();
      const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
      Serial.printf("Running partition: %s\n", running->label);
      Serial.printf("Updated partition: %s\n", next->label);
      ESP.restart();
    } }, []()
            {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update Start: %s\n", upload.filename.c_str());
      const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
      Serial.printf("Writing to partition: %s\n", next->label);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    } });

  server.on("/info", HTTP_GET, []()
            {
    JsonDocument doc;  // NEW
    {
      String reportedType = (sensorType == "UNKNOWN" && lastSensorTypeReported != "UNKNOWN")
                            ? lastSensorTypeReported
                            : sensorType;
      doc["sensor_type"] = reportedType;
    }
    doc["sensor_id"] = sensorID;
    String jsonResp;
    serializeJson(doc, jsonResp);
    server.send(200, "application/json", jsonResp); });

  // Lightweight ping endpoint for device status checking
  server.on("/ping", HTTP_GET, []()
            { server.send(200, "text/plain", "pong"); });

  // Repair sensor EEPROM route
  server.on("/sensor/repair", HTTP_POST, []()
            {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"bad_json\"}");
      return;
    }
    String id = (const char*)(doc["id"] | "");
    id.trim();
    if (id.length() != 3) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"invalid_id\"}");
      return;
    }
    // Allow write: pull WP low
    digitalWrite(EEPROM_WP_PIN, LOW);
    delay(5);
    // Write 3 chars to 24C02 starting at 0x00
    Wire.beginTransmission(EEPROM_SENSOR_ADDR);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)id[0]);
    Wire.write((uint8_t)id[1]);
    Wire.write((uint8_t)id[2]);
    Wire.endTransmission();
    delay(10);
    // Set WP high again
    digitalWrite(EEPROM_WP_PIN, HIGH);
    // Re-read sensor
    bool ok = detectSensor();
    JsonDocument resp;
    resp["success"] = ok;
    resp["sensor_type"] = sensorType;
    String json;
    serializeJson(resp, json);
    server.send(ok ? 200 : 500, "application/json", json);
    delay(1000);
    ESP.restart();
  });

  server.on("/id", HTTP_GET, []()
            {
    JsonDocument doc;
    {
      String reportedType = (sensorType == "UNKNOWN" && lastSensorTypeReported != "UNKNOWN")
                            ? lastSensorTypeReported
                            : sensorType;
      doc["id"] = reportedType;
    }
    String json;
    serializeJson(doc, json);
    server.send(200, "application/json", json); });

  // OTA push endpoints for backend
  server.on("/ota/begin", HTTP_POST, []()
            {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"bad_json\"}");
      return;
    }
    size_t size = doc["size"] | 0;
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
    if (!Update.begin(size)) {
      String e; Update.printError(Serial);
      server.send(500, "application/json", "{\"success\":false}");
      return;
    }
    otaInProgress = true;
    otaExpectedSize = size;
    otaWritten = 0;
    Serial.printf("OTA begin: size=%u, partition=%s\n", (unsigned)size, next? next->label : "?");
    server.send(200, "application/json", "{\"success\":true}"); });

  server.on("/ota/write", HTTP_POST, []()
            {
    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"bad_json\"}");
      return;
    }
    size_t offset = doc["offset"] | 0;
    size_t size = doc["size"] | 0;
    String hex = doc["data"] | "";
    std::vector<uint8_t> bytes;
    if (!hexToBytes(hex, bytes)) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"bad_hex\"}");
      return;
    }
    if (bytes.size() != size) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"size_mismatch\"}");
      return;
    }
    if (!otaInProgress) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"not_in_progress\"}");
      return;
    }
    size_t written = Update.write(bytes.data(), bytes.size());
    if (written != bytes.size()) {
      Update.printError(Serial);
      server.send(500, "application/json", "{\"success\":false}");
      return;
    }
    otaWritten += written;
    server.send(200, "application/json", "{\"success\":true}"); });

  server.on("/ota/end", HTTP_POST, []()
            {
    if (!otaInProgress) {
      server.send(400, "application/json", "{\"success\":false,\"error\":\"not_in_progress\"}");
      return;
    }
    bool ok = Update.end(true);
    if (ok) {
      Serial.printf("OTA success: %u/%u bytes\n", (unsigned)otaWritten, (unsigned)otaExpectedSize);
      server.send(200, "application/json", "{\"success\":true}");
      delay(200);
      ESP.restart();
    } else {
      Update.printError(Serial);
      server.send(500, "application/json", "{\"success\":false}");
    }
    otaInProgress = false; });
}

// Global flag to track if MQTT config has been applied in this session
static bool sMqttConfigured = false;

// ========== UDP Discovery Functions ==========
void handleUDPDiscovery()
{
  unsigned long currentMillis = millis();
  if (currentMillis - lastUDPCheck >= UDP_CHECK_INTERVAL)
  {
    lastUDPCheck = currentMillis;

    // Check for incoming UDP packets
    int packetSize = udp.parsePacket();
    if (packetSize)
    {
      char packetBuffer[512]; // Increased for JSON packets
      int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      if (len > 0)
      {
        packetBuffer[len] = '\0';

        // Try to parse as JSON first (new format)
        JsonDocument discoveryDoc;
        DeserializationError error = deserializeJson(discoveryDoc, packetBuffer);
        
        bool isDiscovery = false;
        
        if (!error && discoveryDoc["magic"] == "LABEXPERT_DISCOVERY") {
          // New JSON format
          isDiscovery = true;
          
          // 1. Get Backend MAC from JSON (for security verification)
          const char* backendMAC = discoveryDoc["backend_mac"];


          
          // 2. Get Broker IP from UDP Packet Source (Dynamic & Robust)
          // We ignore any IP in the JSON and trust the network layer.
          String remoteIPStr = udp.remoteIP().toString();
          const char* mqttBroker = remoteIPStr.c_str();
          
          // Default port since we removed it from JSON
          uint16_t mqttPort = 1883; 


            
            // Inside handleUDPDiscovery()
            if (mqttBroker && strlen(mqttBroker) > 0) {
            if (sMqttConfigured) {
              // Already configured once this session, ignore subsequent packets
              Serial.println("ℹ️ MQTT Config Ignored: Already configured in this session.");
            } else {
              const char* storedHostMac = wifiMgr.getHostMac();
              bool shouldSave = false;
              
              // Verify Backend MAC against stored Host MAC
              if (storedHostMac && strlen(storedHostMac) > 0) {
                if (backendMAC && strcasecmp(storedHostMac, backendMAC) == 0) {
                  Serial.printf("✓ Backend MAC Verified: %s\n", backendMAC);
                  shouldSave = true;
                } else {
                  Serial.printf("❌ MQTT Config Rejected: Backend MAC mismatch!\n");
                  Serial.printf("   Expected: %s\n", storedHostMac);
                  Serial.printf("   Received: %s\n", backendMAC ? backendMAC : "(null)");
                }
              } else {
                // No stored Host MAC to verify against - log warning but decide whether to allow
                Serial.println("⚠️ MQTT Config Ignored: No Host MAC stored in NVS to verify against.");
              }

              if (shouldSave) {
                // Save MQTT credentials to NVS
                // Note: We are saving the UDP Source IP as the broker


                if (saveMQTTCredentialsToNVS(mqttBroker, mqttPort, backendMAC)) {
                  Serial.printf("📡 MQTT broker discovered and saved: %s:%d\n", mqttBroker, mqttPort);
                  sMqttConfigured = true; // Set flag to prevent future writes
                }
              }
            }
          }
        }else if (strcmp(packetBuffer, UDP_DISCOVERY_MAGIC) == 0) {
          // Legacy simple string format (backward compatibility)
          isDiscovery = true;
        }
        
        if (isDiscovery) {
          Serial.println("Received UDP discovery request");
          // Get device ID from MAC address (last 5 digits)
          String deviceID = getDeviceIDFromMAC();
          // Create response JSON
          JsonDocument doc;
          doc["device_id"] = deviceID;
          doc["ip_address"] = WiFi.localIP().toString();
          doc["firmware_version"] = "OTA_BOOTLOADER";
          doc["sensor_type"] = sensorType;
          doc["availability"] = 1; // Always available in OTA mode
          doc["magic"] = UDP_RESPONSE_MAGIC;
          
          // Add stored backend MAC for bidirectional verification
          const char* storedHostMac = wifiMgr.getHostMac();
          if (storedHostMac && strlen(storedHostMac) > 0) {
            doc["backend_mac"] = storedHostMac;
          }
          
          String response;
          serializeJson(doc, response);
          // Send response back to sender
          udp.beginPacket(udp.remoteIP(), UDP_RESPONSE_PORT);
          udp.write((const uint8_t *)response.c_str(), response.length());
          udp.endPacket();
          Serial.printf("Sent UDP discovery response: %s\n", response.c_str());
        }
      }
    }
  }
}



bool connectWithDHCP(const char* ssid, const char* password)
{
  Serial.println("🌐 Trying DHCP connection...");
  WiFi.begin(ssid, password);
  Serial.print("Connecting via DHCP");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30)
  {
    delay(500);
    Serial.print(".");
    attempts++;
    yield();
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("\n✅ DHCP connection successful. IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
  }
  Serial.println("\n❌ DHCP connection failed");
  return false;
}

bool connectWithDynamicIP()
{
  if (!wifiMgr.checkSavedCredentials()) {
    Serial.println("❌ No saved credentials found in WiFiCredentialManager");
    return false;
  }

  Serial.printf("🔧 Connecting to WiFi (saved credentials) using DHCP...\n");
  return wifiMgr.connectWiFi();
}

// ========== Setup ==========
void setup()
{
  Serial.begin(115200);
  delay(1000); // Wait for serial to stabilize
  const esp_partition_t *running = esp_ota_get_running_partition();
  Serial.printf("Booting from partition: %s\n", running->label);
  Serial.println("OTA Bootloader Starting...");

  wifiLed.begin();
  bleLed.begin();
  sensorLed.begin();
  otaLed.begin();
  
  pinMode(EEPROM_WP_PIN, OUTPUT);
  digitalWrite(EEPROM_WP_PIN, HIGH);
  
  // Initialize WiFi Manager (loads credentials, starts button task)
  wifiMgr.begin();

  
  // Initial state for sensor led
  updateSensorLed();

  Wire.begin(18, 19); // SDA, SCL

  if (!detectSensor())
  {
    Serial.println("✘ Sensor not detected. Continuing in bootloader mode; network services will remain active.");
  }

  // Check if we should run in bootloader mode (custom partition 0)
  if (running && strcmp(running->label, "ota_0") == 0)
  {
    Serial.println("✅ Running in bootloader mode (ESP_32_OTA on ota_0)");
    if (sensorType == "UNKNOWN")
    {
      Serial.println("🔄 Likely booted back from main firmware due to EEPROM failure");
      Serial.println("📡 Ready to receive new firmware via OTA when sensor is reconnected");
    }
  }
  else if (running && strcmp(running->label, "ota_1") == 0)
  {
    Serial.println("⚠️ Running in UI firmware mode (partition 1) - This should not happen in OTA bootloader!");
    // If running UI firmware but sensor is missing, erase and reboot to bootloader
    if (sensorType == "UNKNOWN")
    {
      Serial.println("❌ Sensor missing while running UI firmware - erasing and rebooting to bootloader");
      eraseInactivePartition();
      delay(1000);
      ESP.restart();
    }
  }

  // Erase inactive partition to allow clean OTA
  eraseInactivePartition();

  // Check if already connected via wifiMgr
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  // If not connected, try dynamic IP fallback
  if (!wifiConnected) {
    wifiConnected = connectWithDynamicIP();
  }

  if (wifiConnected)
  {
    Serial.printf("\n✓ Connected to WiFi, IP: %s\n", WiFi.localIP().toString().c_str());

    // Only set up network services if WiFi is connected
    setupRoutes();
    server.begin();

    // Initialize UDP for discovery
    if (udp.begin(UDP_DISCOVERY_PORT))
    {
      Serial.printf("✓ UDP discovery server started on port %d\n", UDP_DISCOVERY_PORT);
    }
    else
    {
      Serial.println("✘ Failed to start UDP discovery server");
    }

    Serial.println("✓ OTA Server ready.");
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("✘ WiFi not connected - skipping network services setup");
    
    // Still get device ID for identification purposes
    deviceID = getDeviceIDFromMAC();
    Serial.printf("DeviceID: %s\n", deviceID.c_str());
    
    Serial.println("✓ Bluetooth provisioning mode active.");
    // wifiMgr.begin(); // Already initialized at top of setup
  }
}

// ========== Main Loop ==========
void loop()
{
  // Only handle network services if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
    handleUDPDiscovery();
  }
  
  handleLeds();
  updateSensorLed();

  if (WiFi.status() != WL_CONNECTED)
  {
    // Only try to reconnect if we have saved credentials
    // Otherwise, rely on Bluetooth provisioning (already started in setup)
    if (wifiMgr.checkSavedCredentials())
    {
      Serial.println("✘ WiFi lost. Reconnecting...");
      
      // Stop UDP before disconnecting WiFi to prevent socket errors
      udp.stop();
      
      WiFi.disconnect();
      
      // Try to reconnect
      if (connectWithDynamicIP())
      {
        // Restart UDP after successful reconnection
        if (udp.begin(UDP_DISCOVERY_PORT))
        {
          Serial.printf("✓ UDP discovery server restarted on port %d\n", UDP_DISCOVERY_PORT);
        }
        else
        {
          Serial.println("✘ Failed to restart UDP discovery server");
        }
      }
    }
    else
    {
      // No credentials available - Bluetooth provisioning should be active
      // Just delay to avoid spamming the loop
      delay(1000);
    }
  }

  // Periodic sensor presence check - skip during Bluetooth provisioning to avoid interference
  if (WiFi.status() == WL_CONNECTED && millis() - lastSensorCheck >= sensorCheckInterval)
  {
    lastSensorCheck = millis();
    String prev = sensorType;
    bool ok = detectSensor();

    if (sensorType != prev)
    {
      Serial.printf("Sensor type changed: %s -> %s\n", prev.c_str(), sensorType.c_str());
      if (!detectSensor())
      {
        Serial.println("✘ Sensor not detected. Keeping network services active, waiting for reconnection...");
      }
    }
  }
}

// ========== Utils ==========
static bool hexToBytes(const String &hex, std::vector<uint8_t> &out)
{
  if (hex.length() % 2 != 0)
    return false;
  out.clear();
  out.reserve(hex.length() / 2);
  auto toNib = [](char c) -> int
  {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
      return 10 + (c - 'A');
    return -1;
  };
  for (size_t i = 0; i < hex.length(); i += 2)
  {
    int n1 = toNib(hex[i]);
    int n2 = toNib(hex[i + 1]);
    if (n1 < 0 || n2 < 0)
      return false;
    out.push_back((uint8_t)((n1 << 4) | n2));
  }
  return true;
}

static String getDeviceIDFromMAC()
{
  String mac = WiFi.macAddress(); // "AA:BB:CC:DD:EE:FF"
  mac.replace(":", "");
  if (mac.length() >= 5)
    return mac.substring(mac.length() - 5);
  return mac;
}
