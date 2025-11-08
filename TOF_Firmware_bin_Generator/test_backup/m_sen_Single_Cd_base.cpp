// Fixed main_sensor.cpp - v4.4 WebSocket High-Speed Update (TIMING FIXED)
// Fixed sampling rate issue - now produces exact number of samples
// COMPILATION FIX: Removed duplicate variable definitions

#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_partition.h"
#include "esp_ota_ops.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <Update.h>
#include <Wire.h>
#include <WebSocketsClient.h>

#define STATUS_LED 2
#define TOF_RXD 16
#define TOF_TXD 17

// EEPROM config
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3 // Only read 3 bytes for sensor type

// Retry mechanism for EEPROM detection
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 1000 // 1 second between retries

const char *ssid = "LabExpert_1.0";
const char *password = "11111111";
IPAddress local_IP(192, 168, 137, 15);
IPAddress gateway(192, 168, 137, 1);
IPAddress subnet(255, 255, 255, 0);

// Backend WebSocket configuration
const char* backendHost = "192.168.137.1"; // Backend server IP (use actual backend IP)
const uint16_t backendPort = 5000;
WebSocketsClient backendWebSocket;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

HardwareSerial TOFSerial(2);
const uint8_t SLAVE_ADDR = 0x01;

// Diagnostic counters
struct DiagnosticStats {
  uint32_t totalReadings = 0;
  uint32_t successfulReadings = 0;
  uint32_t crcErrors = 0;
  uint32_t timeouts = 0;
  uint32_t outOfRange = 0;
} diagnostics;

struct SensorCalibration {
  float offsetMM = 0.0;
  float scaleFactor = 1.0;
  uint16_t minValidReading = 10;
  uint16_t maxValidReading = 8500;
} calibration;

struct ExperimentConfig {
  int frequency = 50;
  int duration = 60;
  String mode = "long";
  bool configured = false;
  int averagingSamples = 1;
} config;

bool experimentRunning = false;
bool dataReady = false;
unsigned long experimentStartTime = 0;
unsigned long lastSampleTime = 0;
int sampleInterval = 1000 / config.frequency;

// Sensor identification
String sensorType = "UNKNOWN";
String sensorID = "UNKNOWN";

const int MAX_SAMPLES = 1000;
float distances[MAX_SAMPLES];
unsigned long timestamps[MAX_SAMPLES];
int sampleCount = 0;

bool wsActive = false;
unsigned long lastWSPing = 0;

// Variables for sensor detection and bootloader return
unsigned long lastSensorCheck = 0;
const unsigned long SENSOR_CHECK_INTERVAL = 5000; // Check every 5 seconds
bool sensorWasPresent = false;
unsigned long lastExperimentEnd = 0; // Track experiment end time for cooldown

// Backend-initiated cleanup flag
bool backendCleanupRequested = false;

// Backend WebSocket Event Handler
void onBackendWsEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("✗ Backend WS disconnected");
      break;
    case WStype_CONNECTED: {
      Serial.println("✓ Backend WS connected");
      // Send sensor registration message
      String regMessage = "{\"type\":\"sensor_id\",\"sensor_id\":\"" + sensorType + "\",\"device_id\":\"" + sensorID + "\"}";
      backendWebSocket.sendTXT(regMessage);
      Serial.println("Sent sensor registration: " + regMessage);
      break;
    }
    case WStype_TEXT: {
      Serial.printf("Backend WS received: %s\n", payload);
      
      // Parse JSON message to check for backend commands
      DynamicJsonDocument cmdDoc(256);
      DeserializationError error = deserializeJson(cmdDoc, payload, length);
      if (!error) {
        const char* cmdType = cmdDoc["type"];
        if (cmdType && strcmp(cmdType, "disconnect_and_cleanup") == 0) {
          Serial.println("Received disconnect_and_cleanup command from backend");
          backendCleanupRequested = true;
          Serial.println("Cleanup flag set - will execute in main loop");
        }
      }
      break;
    }
    case WStype_ERROR:
      Serial.println("✗ Backend WS error");
      break;
  }
}

// Local WebSocket Event Handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS Client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    wsActive = true;
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS Client #%u disconnected\n", client->id());
    wsActive = (ws.count() > 0);
  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      String message = (char*)data;
      Serial.printf("WS Received: %s\n", message.c_str());

      DynamicJsonDocument doc(256);
      deserializeJson(doc, message);
      String cmd = doc["cmd"];
      if (cmd == "pause") {
        experimentRunning = false;
        Serial.println("Experiment paused via WS");
        ws.textAll("{\"status\":\"paused\"}");
      } else if (cmd == "resume") {
        experimentRunning = true;
        Serial.println("Experiment resumed via WS");
        ws.textAll("{\"status\":\"resumed\"}");
      }
    }
  } else if (type == WS_EVT_PONG) {
    Serial.printf("WS Pong received from client #%u\n", client->id());
  }
}

// ========== Detect sensor from EEPROM ==========
bool detectSensorFromEEPROM() {
  for (int retry = 0; retry < EEPROM_RETRY_COUNT; retry++) {
    Wire.beginTransmission(EEPROM_SENSOR_ADDR);
    int error = Wire.endTransmission();
    if (error == 0) {
      Wire.beginTransmission(EEPROM_SENSOR_ADDR);
      Wire.write(0x00);
      if (Wire.endTransmission(false) == 0) {
        Wire.requestFrom(EEPROM_SENSOR_ADDR, EEPROM_SIZE);
        if (Wire.available() >= EEPROM_SIZE) {
          char buffer[EEPROM_SIZE + 1];
          for (int i = 0; i < EEPROM_SIZE; i++) {
            buffer[i] = Wire.read();
          }
          buffer[EEPROM_SIZE] = '\0';
          String eepromData = String(buffer);
          Serial.printf("EEPROM data: %s\n", eepromData.c_str());

          if (eepromData == "OSI") {
            sensorType = "OSI";
          } else if (eepromData == "TOF") {
            sensorType = "TOF";
          } else {
            sensorType = "UNKNOWN";
          }
          Serial.printf("Sensor Type: %s, ID: %s\n", sensorType.c_str(), sensorID.c_str());
          return (sensorType != "UNKNOWN");
        } else {
          Serial.println("✘ Not enough data from EEPROM");
        }
      } else {
        Serial.println("✘ Failed to set EEPROM address");
      }
    } else {
      Serial.printf("✘ EEPROM sensor not found, I2C error: %d\n", error);
    }
    if (retry < EEPROM_RETRY_COUNT - 1) {
      Serial.printf("Retrying EEPROM detection (%d/%d)...\n", retry + 1, EEPROM_RETRY_COUNT);
      delay(EEPROM_RETRY_DELAY);
    }
  }
  return false;
}

// ========== Get device ID from MAC address ==========
String getDeviceIDFromMAC() {
  String mac = WiFi.macAddress(); // "AA:BB:CC:DD:EE:FF"
  mac.replace(":", "");
  if (mac.length() >= 5) return mac.substring(mac.length()-5);
  return mac;
}

uint16_t modbusCRC(uint8_t *buf, int len) {
  uint16_t crc = 0xFFFF;
  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];
    for (int i = 8; i != 0; i--) {
      if ((crc & 0x0001) != 0) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

uint16_t readTOFDistanceRaw() {
  const int MAX_RETRIES = 1;
  
  for (int retry = 0; retry < MAX_RETRIES; retry++) {
    diagnostics.totalReadings++;
    
    uint8_t cmd[8] = {SLAVE_ADDR, 0x03, 0x00, 0x10, 0x00, 0x01, 0x00, 0x00};
    uint16_t crc = modbusCRC(cmd, 6);
    cmd[6] = crc & 0xFF;
    cmd[7] = (crc >> 8) & 0xFF;

    while (TOFSerial.available()) TOFSerial.read();
    
    TOFSerial.write(cmd, 8);
    TOFSerial.flush();

    uint8_t response[7];
    int bytesRead = 0;
    unsigned long startTime = millis();
    
    while (bytesRead < 7 && millis() - startTime < 20) { 
      if (TOFSerial.available()) {
        response[bytesRead++] = TOFSerial.read();
      }
      yield();
    }

    if (bytesRead != 7) {
      diagnostics.timeouts++;
      delay(5);
      continue;
    }
    
    if (response[0] != SLAVE_ADDR || response[1] != 0x03 || response[2] != 0x02) {
      delay(5);
      continue;
    }
    
    uint16_t receivedCRC = response[5] | (response[6] << 8);
    uint16_t calculatedCRC = modbusCRC(response, 5);
    
    if (receivedCRC != calculatedCRC) {
      diagnostics.crcErrors++;
      delay(5);
      continue;
    }
    
    uint16_t dist = (response[3] << 8) | response[4];
    
    if (dist < calibration.minValidReading || dist > calibration.maxValidReading) {
      diagnostics.outOfRange++;
      return 65535;
    }
    
    diagnostics.successfulReadings++;
    return dist;
  }
  
  return 65535;
}

float readTOFDistance() {
  if (config.averagingSamples <= 1) {
    uint16_t raw = readTOFDistanceRaw();
    if (raw == 65535) return 65535.0;
    return (raw * calibration.scaleFactor) + calibration.offsetMM;
  }
  
  const int maxSamples = min(config.averagingSamples, 10);
  uint16_t samples[10];
  int validCount = 0;
  
  for (int i = 0; i < maxSamples; i++) {
    uint16_t raw = readTOFDistanceRaw();
    if (raw != 65535) {
      samples[validCount++] = raw;
    }
    if (i < maxSamples - 1) delay(10);
  }
  
  if (validCount == 0) return 65535.0;
  
  for (int i = 0; i < validCount - 1; i++) {
    for (int j = 0; j < validCount - i - 1; j++) {
      if (samples[j] > samples[j + 1]) {
        uint16_t temp = samples[j];
        samples[j] = samples[j + 1];
        samples[j + 1] = temp;
      }
    }
  }
  
  float result;
  if (validCount >= 3) {
    result = (validCount % 2 == 0) ? (samples[validCount/2 - 1] + samples[validCount/2]) / 2.0 : samples[validCount/2];
  } else {
    float sum = 0;
    for (int i = 0; i < validCount; i++) sum += samples[i];
    result = sum / validCount;
  }
  
  return (result * calibration.scaleFactor) + calibration.offsetMM;
}

bool setRangingMode(bool longDistance) {
  uint16_t value = longDistance ? 0x0001 : 0x0000;
  uint8_t cmd[8] = {SLAVE_ADDR, 0x06, 0x00, 0x04, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF), 0x00, 0x00};
  uint16_t crc = modbusCRC(cmd, 6);
  cmd[6] = crc & 0xFF;
  cmd[7] = (crc >> 8) & 0xFF;
  
  while (TOFSerial.available()) TOFSerial.read();
  
  TOFSerial.write(cmd, 8);
  TOFSerial.flush();
  
  uint8_t response[8];
  int bytesRead = 0;
  unsigned long startTime = millis();
  while (bytesRead < 8 && millis() - startTime < 300) {
    if (TOFSerial.available()) response[bytesRead++] = TOFSerial.read();
  }
  
  if (bytesRead == 8 && response[0] == SLAVE_ADDR && response[1] == 0x06) {
    uint16_t receivedCRC = response[6] | (response[7] << 8);
    uint16_t calculatedCRC = modbusCRC(response, 6);
    if (receivedCRC == calculatedCRC) {
      delay(150);
      Serial.printf("Range mode set to %s successfully\n", longDistance ? "LONG (8000mm)" : "SHORT (2000mm)");
      return true;
    }
  }
  
  Serial.println("ERROR: Range mode setting failed");
  return false;
}

bool configureSensorForMaxRange() {
  Serial.println("Configuring sensor for maximum range (8000mm)...");
  if (!setRangingMode(true)) {
    Serial.println("ERROR: Failed to set long range mode");
    return false;
  }
  Serial.println("Sensor configured successfully");
  return true;
}

void sendWS(const String& data) {
  if (wsActive && ws.count() > 0) {
    ws.textAll(data);
    lastWSPing = millis();
  }
}

// Send data in custom binary format for real-time processing
void sendBinaryData(float distance, unsigned long timestamp, uint16_t sampleNumber) {
  if (backendWebSocket.isConnected()) {
    // Custom binary format: [header:2B][timestamp:4B][distance:4B][sample:2B][checksum:2B]
    uint8_t binaryData[14];
    
    // Header (0xAA 0x55)
    binaryData[0] = 0xAA;
    binaryData[1] = 0x55;
    
    // Timestamp (4 bytes, little endian)
    binaryData[2] = timestamp & 0xFF;
    binaryData[3] = (timestamp >> 8) & 0xFF;
    binaryData[4] = (timestamp >> 16) & 0xFF;
    binaryData[5] = (timestamp >> 24) & 0xFF;
    
    // Distance (4 bytes, float)
    uint32_t distanceInt = *((uint32_t*)&distance);
    binaryData[6] = distanceInt & 0xFF;
    binaryData[7] = (distanceInt >> 8) & 0xFF;
    binaryData[8] = (distanceInt >> 16) & 0xFF;
    binaryData[9] = (distanceInt >> 24) & 0xFF;
    
    // Sample number (2 bytes, little endian)
    binaryData[10] = sampleNumber & 0xFF;
    binaryData[11] = (sampleNumber >> 8) & 0xFF;
    
    // Checksum (2 bytes, simple XOR checksum)
    uint16_t checksum = 0;
    for (int i = 0; i < 12; i++) {
      checksum ^= binaryData[i];
    }
    binaryData[12] = checksum & 0xFF;
    binaryData[13] = (checksum >> 8) & 0xFF;
    
    // Send binary data via backend WebSocket
    backendWebSocket.sendBIN(binaryData, sizeof(binaryData));
    
    // Also print to serial for debugging
    Serial.printf("BIN: ts=%lu, dist=%.1fmm, sample=%d\n", timestamp, distance, sampleNumber);
  }
}

// Simplified request handlers without complex response objects
void handleStatus(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  doc["connected"] = true;
  doc["sensor_type"] = sensorType + "_UART_HS";
  doc["sensor_id"] = sensorType;
  doc["experiment_running"] = experimentRunning;
  doc["ready"] = dataReady;
  doc["samples"] = sampleCount;
  doc["max_samples"] = MAX_SAMPLES;
  doc["configured"] = config.configured;
  
  JsonObject diag = doc["diagnostics"].to<JsonObject>();
  diag["total_readings"] = diagnostics.totalReadings;
  diag["successful"] = diagnostics.successfulReadings;
  diag["crc_errors"] = diagnostics.crcErrors;
  diag["timeouts"] = diagnostics.timeouts;
  diag["out_of_range"] = diagnostics.outOfRange;
  
  if (diagnostics.totalReadings > 0) {
    diag["success_rate"] = (float)diagnostics.successfulReadings / diagnostics.totalReadings * 100.0;
  }
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleConfigure(AsyncWebServerRequest *request) {
  Serial.println("=== DEBUG: Configuration Request Received ===");
  
  if (request->hasParam("plain", true)) {
    String body = request->getParam("plain", true)->value();
    Serial.printf("Raw body: %s\n", body.c_str());
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
      Serial.printf("JSON parse error: %s\n", error.c_str());
      request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
      return;
    }
    
    Serial.println("Parsed JSON:");
    if (doc.containsKey("frequency")) Serial.printf("frequency: %d\n", doc["frequency"].as<int>());
    if (doc.containsKey("duration")) Serial.printf("duration: %d\n", doc["duration"].as<int>());
    if (doc.containsKey("mode")) Serial.printf("mode: %s\n", doc["mode"].as<const char*>());
    if (doc.containsKey("averagingSamples")) Serial.printf("averagingSamples: %d\n", doc["averagingSamples"].as<int>());
    
    // Process configuration
    config.frequency = doc["frequency"] | 50;
    config.duration = doc["duration"] | 60;
    config.mode = doc["mode"] | "long";
    config.averagingSamples = doc["averagingSamples"] | 1;
    
    if (doc.containsKey("calibration")) {
      calibration.offsetMM = doc["calibration"]["offset"] | 0.0;
      calibration.scaleFactor = doc["calibration"]["scale"] | 1.0;
    }
    
    config.configured = true;
    sampleInterval = 1000 / config.frequency;
    diagnostics = DiagnosticStats();
    
    Serial.printf("Configured: freq=%dHz, dur=%ds, interval=%dms, avg=%d\n", 
      config.frequency, config.duration, sampleInterval, config.averagingSamples);
    
    // FIXED: Send proper JSON response
    String response;
    DynamicJsonDocument respDoc(200);
    respDoc["success"] = true;
    respDoc["frequency"] = config.frequency;
    respDoc["duration"] = config.duration;
    respDoc["interval"] = sampleInterval;
    serializeJson(respDoc, response);
    
    request->send(200, "application/json", response);
    
  } else {
    Serial.println("ERROR: No plain text parameter found");
    request->send(400, "application/json", "{\"error\":\"No body\"}");
  }
}

void handleCalibrate(AsyncWebServerRequest *request) {
  if (request->hasParam("plain", true)) {
    String body = request->getParam("plain", true)->value();
    
    DynamicJsonDocument doc(256);
    if (deserializeJson(doc, body) == DeserializationError::Ok) {
      calibration.offsetMM = doc["offset"] | 0.0;
      calibration.scaleFactor = doc["scale"] | 1.0;
      
      Serial.printf("Calibration updated: offset=%.2fmm, scale=%.4f\n", 
        calibration.offsetMM, calibration.scaleFactor);
        
      request->send(200, "application/json", "{\"success\":true}");
      return;
    }
  }
  request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

void handleStart(AsyncWebServerRequest *request) {
  if (!config.configured) {
    request->send(400, "application/json", "{\"error\":\"Not configured\"}");
    return;
  }
  
  // CRITICAL FIX: Prevent restart if already running
  if (experimentRunning) {
    request->send(400, "application/json", "{\"error\":\"Experiment already running\"}");
    return;
  }
  
  // CRITICAL FIX: Add cooldown period after experiment completion
  const unsigned long COOLDOWN_MS = 3000; // 3 second cooldown
  
  if (lastExperimentEnd > 0 && (millis() - lastExperimentEnd) < COOLDOWN_MS) {
    request->send(400, "application/json", "{\"error\":\"Please wait before starting new experiment\"}");
    return;
  }
  
  experimentRunning = true;
  dataReady = false;
  sampleCount = 0;
  experimentStartTime = millis();
  lastSampleTime = millis();
  lastExperimentEnd = 0; // Reset cooldown timer
  
  Serial.println("Experiment started");
  request->send(200, "application/json", "{\"success\":true}");
}

void handleStop(AsyncWebServerRequest *request) {
  experimentRunning = false;
  dataReady = true;
  
  Serial.println("Experiment stopped");
  request->send(200, "application/json", "{\"success\":true}");
}

void handleData(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(2048);
  JsonArray distArray = doc["distances"].to<JsonArray>();
  JsonArray timeArray = doc["timestamps"].to<JsonArray>();
  
  for (int i = 0; i < sampleCount; i++) {
    distArray.add(distances[i]);
    timeArray.add(timestamps[i]);
  }
  
  doc["count"] = sampleCount;
  
  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void handleOptions(AsyncWebServerRequest *request) {
  request->send(200);
}

void handleId(AsyncWebServerRequest *request) {
  String response = "{\"id\":\"" + sensorType + "\"}";
  request->send(200, "application/json", response);
}

void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.printf("Update Start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  }
  
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  
  if (final) {
    if (Update.end(true)) {
      Serial.printf("Update Success: %u bytes\n", index + len);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleUpdate(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  if (!Update.hasError()) {
    delay(1000);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== TOF400F Firmware v4.4 (WebSocket High-Speed - TIMING FIXED) ===");
  
  // Initialize I2C for EEPROM
  Wire.begin();
  
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, HIGH);
  
  TOFSerial.begin(115200, SERIAL_8N1, TOF_RXD, TOF_TXD);
  Serial.printf("UART on RX=%d, TX=%d @ 115200 baud\n", TOF_RXD, TOF_TXD);
  
  delay(300);
  
  if (configureSensorForMaxRange()) {
    Serial.println("Sensor initialization successful");
  } else {
    Serial.println("WARNING: Sensor init issues");
  }
  
  WiFi.mode(WIFI_STA);
  WiFi.config(local_IP, gateway, subnet);
  WiFi.begin(ssid, password);
  
  Serial.print("Connecting to WiFi");
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500); Serial.print("."); wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
    
    // Detect sensor from EEPROM
    bool sensorDetected = detectSensorFromEEPROM();
    sensorWasPresent = sensorDetected;
    Serial.printf("Detected sensor type: %s\n", sensorType.c_str());
    
    // Get device ID from MAC address
    sensorID = getDeviceIDFromMAC();
    Serial.printf("Device ID: %s\n", sensorID.c_str());
    
    // Initialize backend WebSocket connection
    backendWebSocket.begin(backendHost, backendPort, String("/ws/device?device_id=") + sensorID);
    backendWebSocket.onEvent(onBackendWsEvent);
    Serial.printf("Connecting to backend at %s:%d\n", backendHost, backendPort);
    
  } else {
    Serial.println("\nWiFi connection failed!");
  }
  
  // Setup WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  
  // Setup routes
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/start", HTTP_GET, handleStart);
  server.on("/stop", HTTP_GET, handleStop);
  server.on("/data", HTTP_GET, handleData);
  server.on("/id", HTTP_GET, handleId);
  
  // OTA update
  server.on("/update", HTTP_POST, handleUpdate, handleUpdateUpload);
  
  // CORS handling
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });
  
  server.begin();
  Serial.println("HTTP/WS server started");
  digitalWrite(STATUS_LED, LOW);
}

void loop() {
  ws.cleanupClients();
  backendWebSocket.loop();
  
  if (wsActive && (millis() - lastWSPing > 30000)) {
    ws.pingAll();
    lastWSPing = millis();
  }

  // Periodic sensor detection check
  if (millis() - lastSensorCheck > SENSOR_CHECK_INTERVAL) {
    lastSensorCheck = millis();
    
    bool sensorCurrentlyPresent = detectSensorFromEEPROM();
    
    // If sensor was previously present but is now missing
    if (sensorWasPresent && !sensorCurrentlyPresent) {
      Serial.println("⚠️  Sensor unplugged detected! Returning to bootloader mode...");
      
      // Send notification to backend if connected
      if (backendWebSocket.isConnected()) {
        backendWebSocket.sendTXT("{\"type\":\"sensor_status\",\"status\":\"unplugged\",\"action\":\"reboot_to_bootloader\"}");
      }
      
      // Wait a moment for messages to be sent
      delay(1000);
      
      // Reboot into bootloader mode
      Serial.println("🔄 Rebooting into bootloader mode...");
      
      // For ESP32 with OTA partitions, we need to set the boot partition to ota_0 (bootloader)
      const esp_partition_t* boot_partition = esp_partition_find_first(
          ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
      
      if (boot_partition != NULL) {
        esp_ota_set_boot_partition(boot_partition);
        Serial.println("✓ Boot partition set to ota_0 (bootloader)");
      }
      
      // Perform a clean reboot
      ESP.restart();
    }
    
    // Update sensor presence state
    sensorWasPresent = sensorCurrentlyPresent;
  }

  if (experimentRunning) {
    unsigned long now = millis();
    unsigned long elapsed = now - experimentStartTime;
    
    // Stop condition: time elapsed OR buffer full
    if (elapsed >= (config.duration * 1000UL) || sampleCount >= MAX_SAMPLES) {
      experimentRunning = false;
      dataReady = true;
      
      // Set cooldown period
      lastExperimentEnd = millis();
      
      Serial.printf("Experiment COMPLETED. Collected %d samples in %lu ms\n", sampleCount, elapsed);
      return;
    }
    
    // FIXED TIMING: Calculate expected sample count based on elapsed time
    int expectedSamples = (elapsed / sampleInterval) + 1;  // +1 to include sample at exact duration
    
    // Take samples if we're behind schedule
    if (sampleCount < expectedSamples && sampleCount < MAX_SAMPLES) {
      float distance = readTOFDistance();
      unsigned long sampleTime = experimentStartTime + (sampleCount * sampleInterval);
      
      timestamps[sampleCount] = sampleTime - experimentStartTime;
      distances[sampleCount] = distance;
      
      // Send data via binary format for real-time processing
      sendBinaryData(distance, timestamps[sampleCount], sampleCount + 1);
      
      // Also send via local WebSocket for compatibility (optional)
      DynamicJsonDocument doc(256);
      doc["distance"] = distance;
      doc["timestamp"] = timestamps[sampleCount];
      doc["sample"] = sampleCount + 1;
      doc["time"] = timestamps[sampleCount] / 1000.0;  // Convert to seconds
      
      String json;
      serializeJson(doc, json);
      sendWS(json);
      
      Serial.printf("Sample %d: %lu ms, distance: %.1f mm\n", 
        sampleCount + 1, timestamps[sampleCount], distance);
      
      digitalWrite(STATUS_LED, !digitalRead(STATUS_LED));
      sampleCount++;
    }
    
    // Update lastSampleTime to prevent multiple samples in same interval
    lastSampleTime = experimentStartTime + (sampleCount * sampleInterval);
    
  } else {
    digitalWrite(STATUS_LED, LOW);
  }
  
  // Check for backend-initiated cleanup request
  if (backendCleanupRequested) {
    Serial.println("Executing backend-initiated cleanup: rebooting to bootloader mode");
    backendCleanupRequested = false; // Reset flag
    
    // Send notification to backend if connected
    if (backendWebSocket.isConnected()) {
      backendWebSocket.sendTXT("{\"type\":\"sensor_status\",\"status\":\"disconnected\",\"action\":\"reboot_to_bootloader\"}");
    }
    
    // Wait a moment for messages to be sent
    delay(1000);
    
    // Reboot into bootloader mode
    Serial.println("🔄 Rebooting into bootloader mode...");
    
    // For ESP32 with OTA partitions, set the boot partition to ota_0 (bootloader)
    const esp_partition_t* boot_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    
    if (boot_partition != NULL) {
      esp_ota_set_boot_partition(boot_partition);
      Serial.println("✓ Boot partition set to ota_0 (bootloader)");
    }
    
    // Perform a clean reboot
    ESP.restart();
  }
  
  delay(1);
}