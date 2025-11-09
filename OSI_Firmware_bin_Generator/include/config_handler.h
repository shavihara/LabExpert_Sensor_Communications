#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Pin definitions
#define SENSOR_PIN 21
#define WIFI_LED 14
#define SENSOR_LED 13

// EEPROM config
#define EEPROM_SENSOR_ADDR 0x50
#define EEPROM_SIZE 3
#define EEPROM_RETRY_COUNT 3
#define EEPROM_RETRY_DELAY 100

// Experiment configuration structure
struct ExperimentConfig
{
    int frequency = 30;       // Default 30Hz
    int duration = 10;        // Default 10s
    String mode = "medium";   // Default mode
    int averagingSamples = 1; // Default 1
    bool configured = false;
    int maxRange = 4000; // Default 4000mm
    bool userPaired = false;
    String pairedUserID = "";
};

// Global variables
extern ExperimentConfig config;

#endif