#ifndef SENSOR_COMMUNICATION_H
#define SENSOR_COMMUNICATION_H

#include <Arduino.h>
#include <Wire.h>
#include "config_handler.h"

// Global variables - DECLARE as extern (no initialization here)
extern String sensorType;
extern String sensorID;

// Function declarations
bool detectSensorFromEEPROM();
String getDeviceIDFromMAC();

#endif