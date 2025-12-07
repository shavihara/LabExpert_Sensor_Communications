#ifndef CONFIG_HANDLER_H
#define CONFIG_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

#define STATUS_LED 13

// Experiment configuration structure
struct ExperimentConfig {
    int resolution = 10;          // 9, 10, 11, 12 bits
    int duration = 0;             // seconds (0 = infinite/manual stop)
    String pairedUserID = "";     // User ID this sensor is paired with
    bool userPaired = false;      // Whether sensor is currently paired with a user
};

// HTTP request handlers
void handleStatus(AsyncWebServerRequest *request);
void handleConfigure(AsyncWebServerRequest *request);
void handleStart(AsyncWebServerRequest *request);
void handleStop(AsyncWebServerRequest *request);
void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void handleUpdate(AsyncWebServerRequest *request);

// External variables
extern ExperimentConfig config;
extern AsyncWebServer server;

#endif
