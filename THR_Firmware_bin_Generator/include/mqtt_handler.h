#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// MQTT configuration
extern PubSubClient mqttClient;
extern char mqttBroker[40];
extern uint16_t mqttPort;

// MQTT topics
#define MQTT_DATA_TOPIC "sensors/%s/data"
#define MQTT_STATUS_TOPIC "sensors/%s/status"
#define MQTT_CONFIG_TOPIC "sensors/%s/config"
#define MQTT_COMMAND_TOPIC "sensors/%s/command"

// Defines for data
struct SensorDataPacket {
    float celsius;
    float fahrenheit;
    float kelvin;
    unsigned long timestamp;
    uint32_t sampleCount;
    unsigned long processTimeMs; // Actual process time
};

// MQTT functions
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishSensorData(const SensorDataPacket* data);
void publishStatus(const char* status, const char* message = nullptr);
void publishSensorIdentification();
void handleMQTTCommands(char* topic, byte* payload, unsigned int length);
void mqttLoop();

// Firmware cleanup and OTA boot
void cleanFirmwareAndBootOTA();

// MQTT status
extern bool mqttConnected;

#endif
