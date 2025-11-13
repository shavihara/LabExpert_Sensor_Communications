#include "sensor_communication.h"
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include "config_handler.h"

// Global variables
SensorCalibration calibration;
DiagnosticStats diagnostics;
String sensorType = "ULTRASONIC";
String sensorID = "UNKNOWN";
bool wifiLedState = false;
bool sensorLedState = false;

// Initialize HC-SR04 Ultrasonic Sensor
bool initializeUltrasonicSensor()
{
    Serial.println("Initializing HC-SR04 Ultrasonic Sensor...");

    // Configure GPIO pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // Set initial state
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    Serial.println("✅ HC-SR04 Ultrasonic Sensor initialized");
    Serial.printf("  - TRIG Pin: %d\n", TRIG_PIN);
    Serial.printf("  - ECHO Pin: %d\n", ECHO_PIN);
    Serial.printf("  - Max Distance: %d mm\n", MAX_DISTANCE_MM);

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

                    if (eepromData == "ULT")
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

    // Calculate minimum delay between readings based on maximum distance
    int minDelayBetweenReadings = (MAX_DISTANCE_MM * 58) / 10000; // Convert to ms (mm to cm then to ms)
    minDelayBetweenReadings = max(minDelayBetweenReadings, 60);   // Minimum 60ms for HC-SR04

    Serial.printf("Sensor configured for %dHz:\n", frequency);
    Serial.printf("  - Sampling rate: %d Hz\n", frequency);
    Serial.printf("  - Max reliable distance: %d mm\n", MAX_DISTANCE_MM);
    Serial.printf("  - Minimum delay between readings: %d ms\n", minDelayBetweenReadings);

    return true;
}

// CRITICAL: Corrected millimeter reading for HC-SR04
uint16_t readUltrasonicDistanceCM()
{
    static unsigned long lastReadTime = 0;
    static uint16_t lastValidDistance = 1000; // Default 1000mm (100cm)
    static int consecutiveFailures = 0;

    // Ensure minimum 60ms delay between readings for HC-SR04 reliability
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < 60)
    {
        delay(60 - (currentTime - lastReadTime));
    }

    // Generate clean trigger pulse
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10); // 10μs trigger pulse
    digitalWrite(TRIG_PIN, LOW);

    // Measure echo pulse duration with timeout
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout

    // Update diagnostics
    diagnostics.totalReadings++;

    // Validate reading
    if (duration == 0)
    {
        // Timeout occurred
        diagnostics.timeouts++;
        consecutiveFailures++;

        if (consecutiveFailures > 5)
        {
            return 65535; // Error after multiple failures
        }
        return lastValidDistance; // Return last known good value
    }

    // Calculate distance in mm: duration (μs) * 0.343 / 2 = distance (mm)
    // Sound travels 0.343 mm/μs, but round trip is 2x so divide by 2
    float distance_mm = duration * 0.343 / 2.0;

    // Validate distance range (convert cm ranges to mm)
    if (distance_mm >= calibration.minValidReading * 10 &&
        distance_mm <= calibration.maxValidReading * 10)
    {

        lastValidDistance = (uint16_t)distance_mm;
        lastReadTime = millis();
        consecutiveFailures = 0;
        diagnostics.successfulReadings++;

        // Apply calibration (convert offset from cm to mm)
        float calibrated = distance_mm + (calibration.offsetCM * 10);
        calibrated = calibrated * calibration.scaleFactor;

        // Clamp to valid range (convert cm ranges to mm)
        if (calibrated < 20)
            calibrated = 20; // 2cm min
        if (calibrated > MAX_DISTANCE_MM)
            calibrated = MAX_DISTANCE_MM;

        return (uint16_t)calibrated;
    }
    else
    {
        // Out of range
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
float readUltrasonicDistance()
{
    uint16_t raw_mm = readUltrasonicDistanceCM();

    if (raw_mm == 65535)
    {
        static float lastValid = 1000.0f; // 1000mm (100cm)
        return lastValid;
    }

    // Apply calibration (convert offset from cm to mm)
    float calibrated = (raw_mm * calibration.scaleFactor) + (calibration.offsetCM * 10);

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
uint16_t readUltrasonicDistanceRaw()
{
    // Generate trigger pulse
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Measure echo pulse duration
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000); // 30ms timeout

    if (duration == 0)
    {
        return 65535; // Timeout or error
    }

    // Calculate and return raw distance in mm
    return (uint16_t)(duration * 0.343 / 2.0);
}

// Configure sensor for specific frequency
bool configureSensorForFrequency(int frequency)
{
    return setSensorTiming(frequency);
}

// Additional utility function for sensor diagnostics
void printSensorDiagnostics()
{
    Serial.println("=== Sensor Diagnostics ===");
    Serial.printf("Total Readings: %lu\n", diagnostics.totalReadings);
    Serial.printf("Successful: %lu\n", diagnostics.successfulReadings);
    Serial.printf("Timeouts: %lu\n", diagnostics.timeouts);
    Serial.printf("Out of Range: %lu\n", diagnostics.outOfRange);

    if (diagnostics.totalReadings > 0)
    {
        float successRate = (float)diagnostics.successfulReadings / diagnostics.totalReadings * 100.0;
        Serial.printf("Success Rate: %.1f%%\n", successRate);
    }

    Serial.printf("Calibration - Offset: %.2fcm (%.1fmm), Scale: %.4f\n",
                  calibration.offsetCM, calibration.offsetCM * 10, calibration.scaleFactor);
    Serial.println("==========================");
}