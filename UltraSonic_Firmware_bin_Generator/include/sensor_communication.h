#ifndef SENSOR_COMMUNICATION_H
#define SENSOR_COMMUNICATION_H

#include <Arduino.h>
#include <Wire.h>

// External declarations
// No external sensor object needed for HC-SR04

// I2C Configuration for EEPROM only
#define EEPROM_SDA 18
#define EEPROM_SCL 19

// HC-SR04 Ultrasonic Sensor Pins
#define TRIG_PIN 21
#define ECHO_PIN 22

// EEPROM configuration
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 1000

// Frequency configuration
#define DEFAULT_FREQUENCY 30
#define MAX_FREQUENCY 50
#define MIN_FREQUENCY 10

// HC-SR04 specific configuration
#define MAX_DISTANCE_MM 4000  // Maximum reliable distance for HC-SR04 (400cm = 4000mm)
#define SOUND_SPEED 0.0343  // Speed of sound in cm/μs
#define TIMEOUT_MICROS 30000 // Timeout for pulseIn (30ms)

// Sensor calibration structure
struct SensorCalibration {
    float offsetCM = 0.0;
    float scaleFactor = 1.0;
    uint16_t minValidReading = 2;   // Minimum valid distance in cm
    uint16_t maxValidReading = 400;  // Maximum valid distance in cm
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
bool initializeUltrasonicSensor();
bool detectSensorFromEEPROM();
String getDeviceIDFromMAC();
uint16_t readUltrasonicDistanceRaw();
float readUltrasonicDistance();
uint16_t readUltrasonicDistanceCM();
bool configureSensorForFrequency(int frequency);
bool setSensorTiming(int frequency);

// External variables
extern SensorCalibration calibration;
extern DiagnosticStats diagnostics;
extern String sensorType;
extern String sensorID;

#endif