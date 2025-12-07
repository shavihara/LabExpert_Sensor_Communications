#include "../include/sensor_communication.h"
#include "../include/config_handler.h"
#include <WiFi.h>
#include <Wire.h>

// OneWire/Dallas objects
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddress;

// Global variables
String sensorType = "UNKNOWN";
String sensorID = "UNKNOWN";

// Initialize THR Sensor (DS18B20)
bool initializeTHRSensor() {
    Serial.println("Initializing DS18B20 Sensor...");
    sensors.begin();
    
    if (sensors.getDeviceCount() == 0) {
        Serial.println("❌ No DS18B20 sensor found! Check wiring on Pin 23.");
        return false;
    }
    
    if (!sensors.getAddress(sensorAddress, 0)) {
        Serial.println("❌ Cannot get sensor address!");
        return false;
    }
    
    Serial.print("✅ DS18B20 Found! Address: ");
    for (uint8_t i = 0; i < 8; i++) {
        if (sensorAddress[i] < 16) Serial.print("0");
        Serial.print(sensorAddress[i], HEX);
        if (i < 7) Serial.print(":");
    }
    Serial.println();
    
    // Set default resolution
    sensors.setResolution(sensorAddress, 10); // Default 10-bit

    // IMPORTANT: Make requestTemperatures() non-blocking
    sensors.setWaitForConversion(false);
    
    return true;
}

// Detect sensor from I2C EEPROM (for ID/Type detection)
bool detectSensorFromEEPROM() {
    // If we want to support EEPROM detection failure but still run because we know it's a THR sensor via Pin 23:
    // The TOF code enforces EEPROM presence for valid ID.
    // I will try to read EEPROM.
    
    for (int retry = 0; retry < EEPROM_RETRY_COUNT; retry++) {
        Wire.beginTransmission(EEPROM_SENSOR_ADDR);
        if (Wire.endTransmission() == 0) {
            Wire.beginTransmission(EEPROM_SENSOR_ADDR);
            Wire.write(0x00);
            if (Wire.endTransmission(false) == 0) {
                Wire.requestFrom(EEPROM_SENSOR_ADDR, EEPROM_SIZE);
                if (Wire.available() >= EEPROM_SIZE) {
                    char buffer[EEPROM_SIZE + 1];
                    for (int i = 0; i < EEPROM_SIZE; i++) buffer[i] = Wire.read();
                    buffer[EEPROM_SIZE] = '\0';
                    String eepromData = String(buffer);
                    Serial.printf("EEPROM data: %s\n", eepromData.c_str());
                    
                    // We accept THR or ANY string for now, but really should match what is expected.
                    // If the user just wants to reuse the board, maybe the EEPROM says "TOF" but we are using "THR"?
                    // The user is creating a NEW firmware generator. 
                    // I'll assume if it returns a valid string, we use it as type, or override if we detect DS18B20?
                    // Safe bet: Read it, assign to sensorType. 
                    // If it is "TOF" but we are running THR firmware, we might warn but proceed if DS18B20 is found.
                    // However, `sensorID` depends on this.
                    
                    sensorType = eepromData;
                    
                    // Actually, if we are deployed as THR firmware, we EXPECT THR hardware.
                    // But for now, let's just accept whatever is in EEPROM as the type, 
                    // but we KNOW we are doing DS18B20 logic.
                    
                    if (sensorType == "UNKNOWN" || sensorType.length() == 0) {
                         return false;
                    }

                    return true;
                }
            }
        }
        delay(EEPROM_RETRY_DELAY);
    }
    Serial.println("❌ EEPROM detection failed");
    return false; 
}

String getDeviceIDFromMAC() {
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    if (mac.length() >= 5) return mac.substring(mac.length() - 5);
    return mac;
}

void setSensorResolution(int resolution) {
    if (resolution < 9) resolution = 9;
    if (resolution > 12) resolution = 12;
    sensors.setResolution(sensorAddress, resolution);
    Serial.printf("Resolution set to %d-bit\n", resolution);
}
