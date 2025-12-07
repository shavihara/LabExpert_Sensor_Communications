#include "mqtt_handler.h"
#include "sensor_communication.h"
#include "experiment_manager.h"
#include "../../shared/nvs_mqtt_credentials.h"

// MQTT client
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// MQTT configuration - Loaded from NVS by main_sensor.cpp
extern char mqttBroker[40];
extern uint16_t mqttPort;

// MQTT status
extern void cleanFirmwareAndBootOTA();
bool mqttConnected = false;

String formatTime(unsigned long milliseconds)
{
    unsigned long totalSeconds = milliseconds / 1000;
    unsigned long hours = totalSeconds / 3600;
    unsigned long minutes = (totalSeconds % 3600) / 60;
    unsigned long seconds = totalSeconds % 60;

    char buffer[12];
    snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buffer);
}

void setupMQTT()
{
    mqttClient.setServer(mqttBroker, mqttPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);

    Serial.println("MQTT client configured");
}

void reconnectMQTT()
{
    Serial.print("Attempting MQTT connection...");

    String clientId = "ESP32_OscCounter_" + sensorID;

    if (mqttClient.connect(clientId.c_str()))
    {
        Serial.println("connected");
        mqttConnected = true;

        // Subscribe to config and command topics
        char configTopic[50];
        snprintf(configTopic, sizeof(configTopic), MQTT_CONFIG_TOPIC, sensorID.c_str());
        mqttClient.subscribe(configTopic);
        
        char commandTopic[50];
        snprintf(commandTopic, sizeof(commandTopic), MQTT_COMMAND_TOPIC, sensorID.c_str());
        mqttClient.subscribe(commandTopic);

        Serial.printf("Subscribed to: %s and %s\n", configTopic, commandTopic);

        // Publish sensor identification
        publishSensorIdentification();
    }
    else
    {
        Serial.print("failed, rc=");
        Serial.print(mqttClient.state());
        Serial.println(" try again in 5 seconds");
        // Don't delay here - the timing is handled by mqttLoop()
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("Message arrived [%s]\n", topic);

    // Create a buffer for the payload
    char message[length + 1];
    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.printf("Received MQTT message: %s\n", message);

    // Parse JSON message
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.printf("JSON parse error: %s\n", error.c_str());
        return;
    }

    // Handle command messages
    const char *command = doc["command"];

    if (strcmp(command, "start_experiment") == 0)
    {
        int count = doc["count"] | 20;

        if (!experimentRunning)
        {
            startExperiment(count);
            // experimentStartTime is set inside startExperiment
            publishStatus("experiment_started");
        }
    }
    else if (strcmp(command, "stop_experiment") == 0)
    {
        stopExperiment();
        publishStatus("experiment_stopped");
    }
    else if (strcmp(command, "disconnect_device") == 0)
    {
        Serial.println("Disconnect command received - cleaning firmware and booting to OTA");
        publishStatus("disconnecting", "Device disconnecting and booting to OTA");
        
        // Clean up and prepare for OTA boot
        delay(1000); // Give time for status message to be sent
        
        // Clean firmware partition and boot to OTA
        cleanFirmwareAndBootOTA();
    }
    else if (strcmp(command, "status") == 0)
    {
        publishSensorIdentification();
    }
}

void publishOscillationData(int oscCount, unsigned long disconnectTime, unsigned long reconnectTime)
{
    if (!mqttClient.connected())
    {
        return;
    }

    // Create JSON payload
    DynamicJsonDocument doc(128);
    doc["count"] = oscCount;
    doc["disconnect_time"] = formatTime(disconnectTime);
    doc["reconnect_time"] = formatTime(reconnectTime);
    doc["sensor_id"] = sensorID;

    String payload;
    serializeJson(doc, payload);

    // Publish to data topic
    char dataTopic[50];
    snprintf(dataTopic, sizeof(dataTopic), MQTT_DATA_TOPIC, sensorID.c_str());

    mqttClient.publish(dataTopic, payload.c_str());
}

void publishExperimentSummary()
{
    if (currentOscillationCount == 0)
        return;

    if (!mqttClient.connected())
    {
        return;
    }

    // Calculate first and last oscillation times
    unsigned long firstOscTime = disconnectTimes[0] + (reconnectTimes[0] - disconnectTimes[0]);
    unsigned long lastOscTime = disconnectTimes[dataIndex - 1] + (reconnectTimes[dataIndex - 1] - disconnectTimes[dataIndex - 1]);

    DynamicJsonDocument doc(300);
    doc["total_count"] = currentOscillationCount;
    doc["first_oscillation_time"] = formatTime(firstOscTime);
    doc["last_oscillation_time"] = formatTime(lastOscTime);
    doc["status"] = "completed";
    doc["sensor_id"] = sensorID;
    doc["experiment_duration"] = formatTime(millis() - experimentStartTime);

    String payload;
    serializeJson(doc, payload);

    // Publish to summary topic
    char summaryTopic[50];
    snprintf(summaryTopic, sizeof(summaryTopic), "sensor/%s/summary", sensorID.c_str());

    mqttClient.publish(summaryTopic, payload.c_str());
}

void publishStatus(const char *status, const char *message)
{
    if (!mqttClient.connected())
    {
        return;
    }

    // Create JSON payload
    DynamicJsonDocument doc(256);
    doc["status"] = status;
    doc["sensor_id"] = sensorID;
    doc["sensor_type"] = sensorType;

    if (message != nullptr)
    {
        doc["message"] = message;
    }

    String payload;
    serializeJson(doc, payload);

    // Publish to status topic
    char statusTopic[50];
    snprintf(statusTopic, sizeof(statusTopic), MQTT_STATUS_TOPIC, sensorID.c_str());

    mqttClient.publish(statusTopic, payload.c_str());
}

void publishSensorIdentification()
{
    if (!mqttClient.connected())
    {
        return;
    }

    // Create JSON payload
    DynamicJsonDocument doc(256);
    doc["type"] = "sensor_identify";
    doc["sensor_id"] = sensorID;
    doc["sensor_type"] = sensorType;
    doc["ip_address"] = WiFi.localIP().toString();

    String payload;
    serializeJson(doc, payload);

    // Publish to status topic
    char statusTopic[50];
    snprintf(statusTopic, sizeof(statusTopic), MQTT_STATUS_TOPIC, sensorID.c_str());

    mqttClient.publish(statusTopic, payload.c_str());
    Serial.println("Published sensor identification via MQTT");
}

// MQTT loop function to be called in main loop
void mqttLoop()
{
    static unsigned long lastReconnectAttempt = 0;
    static unsigned long lastKeepalivePing = 0;
    const unsigned long reconnectInterval = 5000; // 5 seconds between reconnection attempts
    const unsigned long keepaliveInterval = 15000; // Send ping every 15 seconds to maintain connection
    
    if (!mqttClient.connected())
    {
        mqttConnected = false;
        unsigned long now = millis();
        
        // Only attempt reconnection every 5 seconds to avoid flooding
        if (now - lastReconnectAttempt > reconnectInterval)
        {
            lastReconnectAttempt = now;
            reconnectMQTT();
        }
    }
    else
    {
        mqttClient.loop();
        
        // Send periodic keepalive ping to maintain connection
        unsigned long now = millis();
        if (now - lastKeepalivePing > keepaliveInterval)
        {
            lastKeepalivePing = now;
            
            // Publish empty message to keep connection alive
            char statusTopic[50];
            snprintf(statusTopic, sizeof(statusTopic), MQTT_STATUS_TOPIC, sensorID.c_str());
            mqttClient.publish(statusTopic, ""); // Empty payload ping
        }
    }
}