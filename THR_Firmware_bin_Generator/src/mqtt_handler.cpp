#include "../include/mqtt_handler.h"
#include "../include/config_handler.h"
#include "../include/experiment_manager.h"
#include "../include/sensor_communication.h"
#include <esp_ota_ops.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
bool mqttConnected = false;

// Credentials (should be loaded from NVS in main, but declared here/extern)
extern char mqttBroker[40];
extern uint16_t mqttPort;

void setupMQTT() {
    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setCallback(mqttCallback);
}

void reconnectMQTT() {
    if (!mqttClient.connected()) {
        String clientId = "ESP32_" + sensorID;
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("MQTT Connected");
            mqttConnected = true;
            
            char configTopic[50];
            snprintf(configTopic, sizeof(configTopic), MQTT_CONFIG_TOPIC, sensorID.c_str());
            mqttClient.subscribe(configTopic, 1);
            
            char commandTopic[50];
            snprintf(commandTopic, sizeof(commandTopic), MQTT_COMMAND_TOPIC, sensorID.c_str());
            mqttClient.subscribe(commandTopic, 1);
            
            publishSensorIdentification();
            
        } else {
            // Serial.print("failed, rc=");
            // Serial.print(mqttClient.state());
        }
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    handleMQTTCommands(topic, payload, length);
}

void handleMQTTCommands(char* topic, byte* payload, unsigned int length) {
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';
    
    DynamicJsonDocument doc(512);
    deserializeJson(doc, message);
    
    String topicStr = String(topic);
    
    if (topicStr.endsWith("/config")) {
        if (doc.containsKey("resolution")) {
            int res = doc["resolution"];
            if (res >= 9 && res <= 12) {
                config.resolution = res;
                setSensorResolution(res);
            }
        }
        if (doc.containsKey("duration")) {
            config.duration = doc["duration"];
        }
        publishStatus("config_updated");
    } else if (topicStr.endsWith("/command")) {
        const char* cmd = doc["command"];
        if (strcmp(cmd, "start_experiment") == 0) {
            experimentRunning = true;
            experimentStartTime = millis();
            readingCount = 0;
            publishStatus("experiment_started");
        } else if (strcmp(cmd, "stop_experiment") == 0) {
            experimentRunning = false;
            dataReady = true;
            publishStatus("experiment_stopped");
        } else if (strcmp(cmd, "disconnect_device") == 0) {
            cleanFirmwareAndBootOTA();
        }
    }
}

void publishSensorData(const SensorDataPacket* data) {
    if (!mqttClient.connected()) return;
    
    DynamicJsonDocument doc(256);
    doc["c"] = data->celsius;
    doc["f"] = data->fahrenheit;
    doc["k"] = data->kelvin;
    doc["ts"] = data->timestamp;
    doc["cnt"] = data->sampleCount;
    doc["pt"] = data->processTimeMs;
    
    String payload;
    serializeJson(doc, payload);
    
    char dataTopic[50];
    snprintf(dataTopic, sizeof(dataTopic), MQTT_DATA_TOPIC, sensorID.c_str());
    mqttClient.publish(dataTopic, payload.c_str());
}

void publishStatus(const char* status, const char* message) {
    if (!mqttClient.connected()) return;
    
    DynamicJsonDocument doc(256);
    doc["status"] = status;
    doc["sensor_id"] = sensorID;
    if (message) doc["message"] = message;
    
    String payload;
    serializeJson(doc, payload);
    
    char statusTopic[50];
    snprintf(statusTopic, sizeof(statusTopic), MQTT_STATUS_TOPIC, sensorID.c_str());
    mqttClient.publish(statusTopic, payload.c_str());
}

void publishSensorIdentification() {
    if (!mqttClient.connected()) return;
    
    DynamicJsonDocument doc(256);
    doc["type"] = "sensor_identify";
    doc["sensor_id"] = sensorID;
    doc["sensor_type"] = "THR"; // Force THR as type
    doc["paired"] = config.userPaired;
    doc["paired_user"] = config.pairedUserID;
    
    String payload;
    serializeJson(doc, payload);
    
    char statusTopic[50];
    snprintf(statusTopic, sizeof(statusTopic), MQTT_STATUS_TOPIC, sensorID.c_str());
    mqttClient.publish(statusTopic, payload.c_str());
}

void mqttLoop() {
    static unsigned long lastReconnect = 0;
    if (!mqttClient.connected()) {
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            reconnectMQTT();
        }
    } else {
        mqttClient.loop();
    }
}

void cleanFirmwareAndBootOTA() {
    // Similar logic to TOF to boot back to ota_0
    const esp_partition_t *ota_0 = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (ota_0) {
        esp_ota_set_boot_partition(ota_0);
        ESP.restart();
    }
}
