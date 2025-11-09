#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "config_handler.h"

// MQTT Topics
#define MQTT_CONFIG_TOPIC "sensors/%s/config"
#define MQTT_COMMAND_TOPIC "sensors/%s/command"
#define MQTT_DATA_TOPIC "sensors/%s/data"
#define MQTT_STATUS_TOPIC "sensors/%s/status"

// Global variables - DECLARE as extern
extern WiFiClient wifiClient;
extern PubSubClient mqttClient;
extern bool mqttConnected;

// Function declarations
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void mqttLoop();
void publishOscillationData(int oscCount, unsigned long disconnectTime, unsigned long reconnectTime);
void publishExperimentSummary();
void publishStatus(const char *status, const char *message = nullptr);
void publishSensorIdentification();
String formatTime(unsigned long milliseconds);

#endif