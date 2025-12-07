#include <Update.h>
#include "../include/config_handler.h"
#include "../include/experiment_manager.h"
#include "../include/mqtt_handler.h"
#include "../include/sensor_communication.h"

ExperimentConfig config;
AsyncWebServer server(80);

void handleStatus(AsyncWebServerRequest *request) {
    if (mqttConnected) {
        request->send(200, "application/json", "{\"status\":\"connected\",\"mqtt\":true}");
    } else {
        request->send(200, "application/json", "{\"status\":\"disconnected\",\"mqtt\":false}");
    }
}

void handleConfigure(AsyncWebServerRequest *request) {
    if (request->hasParam("resolution")) {
        int res = request->getParam("resolution")->value().toInt();
        if (res >= 9 && res <= 12) {
            config.resolution = res;
            setSensorResolution(config.resolution);
        }
    }
    if (request->hasParam("duration")) {
        config.duration = request->getParam("duration")->value().toInt();
    }
    
    // Create response
    DynamicJsonDocument doc(256);
    doc["resolution"] = config.resolution;
    doc["duration"] = config.duration;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleStart(AsyncWebServerRequest *request) {
    experimentRunning = true;
    experimentStartTime = millis();
    readingCount = 0;
    
    request->send(200, "application/json", "{\"status\":\"started\"}");
    
    // Also publish via MQTT
    publishStatus("experiment_started", "Started via HTTP");
}

void handleStop(AsyncWebServerRequest *request) {
    experimentRunning = false;
    dataReady = true;
    
    request->send(200, "application/json", "{\"status\":\"stopped\"}");
    
    publishStatus("experiment_stopped", "Stopped via HTTP");
}

void handleUpdate(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", (Update.hasError()) ? "UPDATE FAILED" : "UPDATE SUCCESS");
    ESP.restart();
}

void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Update Params: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Update.printError(Serial);
        }
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %uB\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
}
