#include "config_handler.h"
#include <Update.h>

// Global variables
ExperimentConfig config;
AsyncWebServer server(80);

// Handle OTA update upload
void handleUpdateUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    
    if (final) {
        if (Update.end(true)) {
            Serial.printf("Update Success: %u bytes\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
}

// Handle OTA update completion
void handleUpdate(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
    if (!Update.hasError()) {
        delay(1000);
        ESP.restart();
    }
}