#include "sensor_communication.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

// Global variables - DEFINE here (only once, at the top)
String sensorType = "LDR_OSCILLATION";
String sensorID = "UNKNOWN";
bool wifiLedState = false;
bool sensorLedState = false;

// Detect sensor from EEPROM
bool detectSensorFromEEPROM()
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

                    if (eepromData == "OSI")
                    {
                        sensorType = eepromData;
                        sensorLedState = true;
                        digitalWrite(SENSOR_LED, sensorLedState ? LOW : HIGH);
                    }
                    else
                    {
                        sensorType = eepromData;
                        Serial.printf("⚠️ WARNNING!(Sensor Type: %s, ID: %s not copatible with this firmware)\n ♻ REBOOTING OTA", sensorType.c_str(), sensorID.c_str());
                        return false;
                    }

                    Serial.printf("Sensor Type: %s\n", sensorType.c_str());
                    return true;
                }
            }
        }

        if (retry < EEPROM_RETRY_COUNT - 1)
        {
            delay(EEPROM_RETRY_DELAY);
        }
    }

    Serial.println("❌ EEPROM detection failed after all retries");
    sensorType = "UNKNOWN";
    return false;
}

// Get device ID from MAC address
String getDeviceIDFromMAC()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    if (mac.length() >= 5)
    {
        sensorID = mac.substring(mac.length() - 5);
        return sensorID;
    }
    sensorID = mac;
    return sensorID;
}