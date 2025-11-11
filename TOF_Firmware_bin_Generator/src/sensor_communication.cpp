#include "sensor_communication.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "config_handler.h"
// I2C Devices
VL53L1X tofSensor;

// Global variables
SensorCalibration calibration;
DiagnosticStats diagnostics;
String sensorType = "TOF400F_I2C";
String sensorID = "UNKNOWN";
bool wifiLedState = false;
bool sensorLedState = false;

// Initialize TOF sensor with I2C
bool initializeTOFSensor()
{
    Serial.println("Initializing TOF400F Sensor via I2C...");

    // Use Wire1 (already initialized in main_sensor.cpp)
    tofSensor.setBus(&Wire1);

    if (!tofSensor.init())
    {
        Serial.println("❌ Failed to detect TOF sensor on I2C!");
        return false;
    }

    // Configure for optimal performance
    tofSensor.setDistanceMode(VL53L1X::Long);
    tofSensor.setMeasurementTimingBudget(33000); // 33ms for long range
    tofSensor.startContinuous(50);               // Target 50Hz

    Serial.println("✅ TOF400F Sensor initialized via I2C");
    Serial.printf("  - Device ID: 0x%04X\n", tofSensor.readReg16Bit(0x010F));
    Serial.printf("  - Distance Mode: Long\n");
    Serial.printf("  - Timing Budget: 33ms\n");

    return true;
}

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

                    if (eepromData == "TOF")
                    {
                        sensorType = eepromData;
                        sensorLedState = true;
                        digitalWrite(SENSOR_LED, sensorLedState ? LOW : HIGH);
                    }
                    else
                    {
                        sensorType = eepromData;
                        Serial.printf("⚠️ WARNING!(Sensor Type: %s, ID: %s not compatible with this firmware)\n ♻ REBOOTING OTA", sensorType.c_str(), sensorID.c_str());
                        return false;
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
            Serial.printf("✘ EEPROM sensor not found, I2C error: %d\n", error);
        }
        if (retry < EEPROM_RETRY_COUNT - 1)
        {
            Serial.printf("Retrying EEPROM detection (%d/%d)...\n", retry + 1, EEPROM_RETRY_COUNT);
            delay(EEPROM_RETRY_DELAY);
        }
    }

    // CRITICAL: If EEPROM detection fails completely, return false
    // This triggers the failsafe mechanism to boot back to ESP_32_OTA
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
        return mac.substring(mac.length() - 5);
    return mac;
}

// Configure sensor timing based on frequency
bool setSensorTiming(int frequency)
{
    if (frequency < 10 || frequency > 50)
    {
        Serial.printf("Invalid frequency: %d Hz (must be 10-50Hz)\n", frequency);
        return false;
    }

    uint32_t timingBudget;

    // Adjust timing budget based on frequency
    if (frequency >= 40)
    {
        // High frequency - shorter range, faster timing
        timingBudget = 20000; // 20ms
        tofSensor.setDistanceMode(VL53L1X::Short);
    }
    else if (frequency >= 30)
    {
        // Medium frequency - balanced
        timingBudget = 25000; // 25ms
        tofSensor.setDistanceMode(VL53L1X::Medium);
    }
    else
    {
        // Lower frequency - longer range, more accurate
        timingBudget = 33000; // 33ms
        tofSensor.setDistanceMode(VL53L1X::Long);
    }

    tofSensor.stopContinuous();
    tofSensor.setMeasurementTimingBudget(timingBudget);

    // Calculate optimal inter-measurement period
    uint32_t interMeasurementPeriod = 1000 / frequency; // Convert to ms
    tofSensor.startContinuous(interMeasurementPeriod);

    Serial.printf("Sensor configured for %dHz:\n", frequency);
    Serial.printf("  - Timing Budget: %lu ms\n", timingBudget);
    Serial.printf("  - Inter-measurement: %lu ms\n", interMeasurementPeriod);

    return true;
}

// CRITICAL: Raw millimeter reading via I2C (non-blocking)
uint16_t readTOFDistanceMM()
{
    static unsigned long lastReadTime = 0;
    static uint16_t lastValidDistance = 1000; // Default 1000mm
    static int consecutiveFailures = 0;

    // Check if data is ready (non-blocking)
    if (!tofSensor.dataReady())
    {
        // Return last valid reading if recent
        if (millis() - lastReadTime < 100)
        {
            return lastValidDistance;
        }
        return 65535; // Error value
    }

    // Read distance (this clears the dataReady flag)
    uint16_t distance_mm = tofSensor.read();

    // Validate reading
    if (distance_mm >= calibration.minValidReading &&
        distance_mm <= calibration.maxValidReading)
    {

        lastValidDistance = distance_mm;
        lastReadTime = millis();
        consecutiveFailures = 0;
        diagnostics.successfulReadings++;

        // Apply calibration offset
        int32_t calibrated = (int32_t)distance_mm + (int32_t)calibration.offsetMM;
        if (calibrated < 10)
            calibrated = 10;
        if (calibrated > 8500)
            calibrated = 8500;

        return (uint16_t)calibrated;
    }
    else
    {
        diagnostics.outOfRange++;
        consecutiveFailures++;

        if (consecutiveFailures > 5)
        {
            return 65535; // Error after multiple failures
        }

        return lastValidDistance; // Return last known good value
    }
}

// Read with filtering
float readTOFDistance()
{
    uint16_t raw_mm = readTOFDistanceMM();

    if (raw_mm == 65535)
    {
        static float lastValid = 100.0f;
        return lastValid;
    }

    // Convert to cm with calibration
    float distance_cm = raw_mm / 10.0f;
    float calibrated = (distance_cm * calibration.scaleFactor) + (calibration.offsetMM / 10.0f);

    // Light smoothing
    static float smoothed = 0.0f;
    if (smoothed == 0.0f)
    {
        smoothed = calibrated;
    }
    else
    {
        smoothed = smoothed * 0.7f + calibrated * 0.3f;
    }

    return smoothed;
}

// Raw reading without calibration
uint16_t readTOFDistanceRaw()
{
    if (tofSensor.dataReady())
    {
        return tofSensor.read();
    }
    return 65535;
}

// Configure sensor for specific frequency
bool configureSensorForFrequency(int frequency)
{
    return setSensorTiming(frequency);
}