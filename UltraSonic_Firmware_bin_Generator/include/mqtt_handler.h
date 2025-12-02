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
#define MQTT_BINARY_DATA_TOPIC "sensors/%s/binary_data"

// Binary protocol definitions
#define BINARY_PROTOCOL_VERSION 1
#define BINARY_HEADER_SIZE 12  // Fixed: version(1) + sensor_type(1) + packet_id(2) + sample_count(2) + total_samples(2) + start_timestamp(4)
#define BINARY_SAMPLE_SIZE 8
#define BINARY_MAX_SAMPLES_PER_PACKET 10

// Binary protocol packet structure
#pragma pack(push, 1)
typedef struct {
    uint8_t version;          // Protocol version
    uint8_t sensor_type;      // Sensor type identifier
    uint16_t packet_id;       // Sequential packet ID
    uint16_t sample_count;    // Number of samples in this packet
    uint16_t total_samples;   // Total samples in experiment
    uint32_t start_timestamp; // Experiment start timestamp
    // Followed by sample_count * BINARY_SAMPLE_SIZE bytes of sample data
} BinaryPacketHeader;

typedef struct {
    uint32_t timestamp;       // Sample timestamp (ms since start)
    uint16_t distance;        // Distance measurement
    uint16_t sample_number;   // Sequential sample number
} BinarySample;
#pragma pack(pop)

// MQTT functions
void setupMQTT();
void reconnectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishBinarySensorData(const BinarySample* samples, uint16_t count, uint32_t start_time, uint16_t total_samples);
void publishStatus(const char* status, const char* message = nullptr);
void publishSensorIdentification();
void handleMQTTCommands(char* topic, byte* payload, unsigned int length);
void mqttLoop();

// Firmware cleanup and OTA boot
void cleanFirmwareAndBootOTA();

// MQTT status
extern bool mqttConnected;

#endif