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
#include <vector>
// Forward declarations
static bool hexToBytes(const String &hex, std::vector<uint8_t> &out);
static String getDeviceIDFromMAC();
// GPIO pin setup
#define WIFI_LED 14
#define SENSOR_LED 13

// EEPROM config
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3 // Only read 3 bytes for sensor type

// Wi-Fi credentials - Connect to device hotspot
const char *ssid = "Connectify-1.0";
const char *password = "11111111";
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);
const int DYNAMIC_IP_BASE = 15;
const int DYNAMIC_IP_MAX_ATTEMPTS = 10;

// Web server
WebServer server(80);
String deviceID = "ESP32";
const char *backendHost = "192.168.1.198"; // Backend server IP - Use the IP of your device running the backend
// const char* backendHost = "192.168.137.1";  Backend server IP - Connect to hotspot interface
const uint16_t backendPort = 5000;

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

// LED blink state
unsigned long previousWifiLedMillis = 0;
unsigned long previousSensorLedMillis = 0;
const unsigned long ledInterval = 3000;
bool wifiLedState = false;
bool sensorLedState = false;

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
    esp_err_t err = esp_partition_erase_range(inactive, 0, inactive->size);
    if (err == ESP_OK)
    {
      Serial.println("✓ Inactive partition erased successfully.");
    }
    else
    {
      Serial.printf("✘ Failed to erase inactive partition. Error: %d\n", err);
    }
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
          }
          else
          {
            sensorType = "UNKNOWN";
            Serial.printf("⚠️ WARNNING!(Sensor Type: %s, ID: %s unable to recognize!)\n", sensorType.c_str(), sensorID.c_str());
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

// ========== Wi-Fi LED status ==========
void handleWifiLed()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    unsigned long now = millis();
    if (now - previousWifiLedMillis >= ledInterval)
    {
      previousWifiLedMillis = now;
      wifiLedState = !wifiLedState;
      digitalWrite(WIFI_LED, wifiLedState ? LOW : HIGH);
    }
  }
  else
  {
    digitalWrite(WIFI_LED, LOW); // solid ON when disconnected
  }
}

// ========== Sensor LED status ==========
void handleSensorLed()
{
  unsigned long now = millis();
  if (sensorType != "UNKNOWN")
  {
    if (now - previousSensorLedMillis >= ledInterval)
    {
      previousSensorLedMillis = now;
      sensorLedState = !sensorLedState;
      digitalWrite(SENSOR_LED, sensorLedState ? LOW : HIGH);
    }
  }
  else
  {
    digitalWrite(SENSOR_LED, LOW); // solid ON if no sensor
  }
}

// ========== Web server routes ==========
void setupRoutes()
{
  server.on("/", HTTP_GET, []()
            {
    String html =
      "<h1>ESP32 OTA Manager</h1>"
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
    doc["sensor_type"] = sensorType;
    doc["sensor_id"] = sensorID;
    String jsonResp;
    serializeJson(doc, jsonResp);
    server.send(200, "application/json", jsonResp); });

  // Lightweight ping endpoint for device status checking
  server.on("/ping", HTTP_GET, []()
            { server.send(200, "text/plain", "pong"); });

  server.on("/id", HTTP_GET, []()
            {
    JsonDocument doc;
    doc["id"] = sensorType; // Use sensorType as ID for firmware selection
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
      char packetBuffer[255];
      int len = udp.read(packetBuffer, sizeof(packetBuffer) - 1);
      if (len > 0)
      {
        packetBuffer[len] = '\0';

        // Check if this is a discovery packet
        if (strcmp(packetBuffer, UDP_DISCOVERY_MAGIC) == 0)
        {
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

bool tryStaticIP(int ipSuffix)
{
  IPAddress local_IP(192, 168, 137, ipSuffix);
  if (!WiFi.config(local_IP, gateway, subnet))
  {
    Serial.printf("❌ Failed to configure static IP 192.168.137.%d\n", ipSuffix);
    return false;
  }
  WiFi.begin(ssid, password);
  Serial.printf("Connecting with IP 192.168.137.%d", ipSuffix);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20)
  {
    delay(500);
    Serial.print(".");
    attempts++;
    yield();
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.printf("\n✅ Connected with IP 192.168.137.%d\n", ipSuffix);
    return true;
  }
  Serial.printf("\n❌ Failed to connect with IP 192.168.137.%d\n", ipSuffix);
  WiFi.disconnect();
  delay(500);
  return false;
}

bool connectWithDHCP()
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
  Serial.println("🔧 Starting dynamic IP assignment...");
  for (int i = 0; i < DYNAMIC_IP_MAX_ATTEMPTS; i++)
  {
    int ipSuffix = DYNAMIC_IP_BASE + i;
    Serial.printf("Trying static IP: 192.168.137.%d\n", ipSuffix);
    if (tryStaticIP(ipSuffix))
    {
      Serial.printf("✅ Successfully connected with static IP 192.168.137.%d\n", ipSuffix);
      return true;
    }
    delay(1000);
  }
  Serial.println("❌ All static IP attempts failed, falling back to DHCP...");
  return connectWithDHCP();
}

// ========== Setup ==========
void setup()
{
  Serial.begin(115200);
  delay(1000); // Wait for serial to stabilize
  
  wifiMgr.begin();
  
  // Check if we have stored WiFi credentials
  if (wifiMgr.checkSavedCredentials()) {
    Serial.println("✓ Found stored WiFi credentials - attempting to connect...");
    if (wifiMgr.connectWiFi()) {
      Serial.print("✓ Connected to WiFi, IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("✘ Failed to connect with stored credentials");
      // Bluetooth provisioning is already started by wifiMgr.begin() when no credentials exist
    }
  } else {
    Serial.println("✗ No stored WiFi credentials found");
    // Bluetooth provisioning is already started by wifiMgr.begin() when no credentials exist
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  Serial.printf("Booting from partition: %s\n", running->label);
  Serial.println("OTA Bootloader Starting...");

  pinMode(WIFI_LED, OUTPUT);
  pinMode(SENSOR_LED, OUTPUT);
  digitalWrite(WIFI_LED, HIGH);
  digitalWrite(SENSOR_LED, HIGH);

  Wire.begin(18, 19); // SDA, SCL

  if (!detectSensor())
  {
    Serial.println("✘ Sensor not detected. Waiting for reconnection...");
    for (int i = 0; i < 3; i++)
    {
      delay(5000); // Wait 5 seconds per attempt
      if (detectSensor())
      {
        Serial.println("✓ Sensor reconnected.");
        break;
      }
      Serial.printf("✘ Reconnection attempt %d/3 failed.\n", i + 1);
    }
    if (sensorType == "UNKNOWN")
    {
      Serial.println("✘ Giving up. Rebooting...");
      delay(2000);
      esp_restart();
    }
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

  WiFi.mode(WIFI_STA);
  bool wifiConnected = connectWithDynamicIP();
  if (wifiConnected)
  {
    Serial.printf("\n✓ Connected to WiFi, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else
  {
    Serial.println("\nWiFi connection failed!");
  }

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
    Serial.println("✘ WiFi not connected - skipping network services setup");
    
    // Still get device ID for identification purposes
    deviceID = getDeviceIDFromMAC();
    Serial.printf("DeviceID: %s\n", deviceID.c_str());
    
    Serial.println("✓ Bluetooth provisioning mode active.");
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
  
  handleWifiLed();
  handleSensorLed();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("✘ WiFi lost. Reconnecting...");
    WiFi.disconnect();
    connectWithDynamicIP();
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

      if (sensorType == "UNKNOWN")
      {
        Serial.println("Sensor unplug detected; erasing inactive OTA partition and rebooting to bootloader mode");
        eraseInactivePartition();
        // Force reboot to ensure we're in bootloader mode when sensor is unplugged
        delay(1000);
        ESP.restart();
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
