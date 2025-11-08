#ifndef SENSOR_COMMUNICATION_H
#define SENSOR_COMMUNICATION_H

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

// External declarations
extern VL53L1X tofSensor;

// I2C Configuration
#define EEPROM_SDA 18
#define EEPROM_SCL 19
#define TOF_SDA 21
#define TOF_SCL 22

// EEPROM configuration
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 1000

// Frequency configuration
#define DEFAULT_FREQUENCY 30
#define MAX_FREQUENCY 50
#define MIN_FREQUENCY 10

// Sensor calibration structure
struct SensorCalibration {
    float offsetMM = 0.0;
    float scaleFactor = 1.0;
    uint16_t minValidReading = 10;
    uint16_t maxValidReading = 8500;
};

// Diagnostic statistics
struct DiagnosticStats {
    uint32_t totalReadings = 0;
    uint32_t successfulReadings = 0;
    uint32_t readErrors = 0;
    uint32_t timeouts = 0;
    uint32_t outOfRange = 0;
};

// Function declarations
bool initializeTOFSensor();
bool detectSensorFromEEPROM();
String getDeviceIDFromMAC();
uint16_t readTOFDistanceRaw();
float readTOFDistance();
uint16_t readTOFDistanceMM();
bool configureSensorForFrequency(int frequency);
bool setSensorTiming(int frequency);

// External variables
extern VL53L1X tofSensor;
extern SensorCalibration calibration;
extern DiagnosticStats diagnostics;
extern String sensorType;
extern String sensorID;

#endif