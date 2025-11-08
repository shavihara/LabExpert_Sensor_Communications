#include "config_handler.h"
#include "sensor_communication.h"
#include "experiment_manager.h"
#include <ArduinoJson.h>
#include <Update.h>

// Global variables
ExperimentConfig config;
AsyncWebServer server(80);

// Handle status request
void handleStatus(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    doc["connected"] = true;
    doc["sensor_type"] = sensorType + "_I2C_HS";
    doc["sensor_id"] = sensorType;
    doc["experiment_running"] = experimentRunning;
    doc["ready"] = dataReady;
    doc["samples"] = sampleCount;
    doc["max_samples"] = MAX_SAMPLES;
    doc["configured"] = config.configured;
    
    JsonObject diag = doc["diagnostics"].to<JsonObject>();
    diag["total_readings"] = diagnostics.totalReadings;
    diag["successful"] = diagnostics.successfulReadings;
    diag["crc_errors"] = diagnostics.readErrors;
    diag["timeouts"] = diagnostics.timeouts;
    diag["out_of_range"] = diagnostics.outOfRange;
    
    if (diagnostics.totalReadings > 0) {
        diag["success_rate"] = (float)diagnostics.successfulReadings / diagnostics.totalReadings * 100.0;
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Handle configuration request
void handleConfigure(AsyncWebServerRequest *request) {
    Serial.println("=== DEBUG: Configuration Request Received ===");
    
    if (request->hasParam("plain", true)) {
        String body = request->getParam("plain", true)->value();
        Serial.printf("Raw body: %s\n", body.c_str());
        
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);
        
        if (error) {
            Serial.printf("JSON parse error: %s\n", error.c_str());
            request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
            return;
        }
        
        Serial.println("Parsed JSON:");
        if (doc.containsKey("frequency")) Serial.printf("frequency: %d\n", doc["frequency"].as<int>());
        if (doc.containsKey("duration")) Serial.printf("duration: %d\n", doc["duration"].as<int>());
        if (doc.containsKey("mode")) Serial.printf("mode: %s\n", doc["mode"].as<const char*>());
        if (doc.containsKey("averagingSamples")) Serial.printf("averagingSamples: %d\n", doc["averagingSamples"].as<int>());
        
        // Process configuration with validation
        int requestedFreq = doc["frequency"] | 30;  // Default 30Hz
        int requestedDuration = doc["duration"] | 10;  // Default 10s
        
        // Validate frequency range (10-50Hz)
        if (requestedFreq < 10 || requestedFreq > 50) {
            Serial.printf("Invalid frequency: %d Hz (must be 10-50Hz)\n", requestedFreq);
            request->send(400, "application/json", "{\"error\":\"Frequency must be 10-50Hz\"}");
            return;
        }
        
        // Validate duration (1-300s)
        if (requestedDuration < 1 || requestedDuration > 300) {
            Serial.printf("Invalid duration: %d s (must be 1-300s)\n", requestedDuration);
            request->send(400, "application/json", "{\"error\":\"Duration must be 1-300s\"}");
            return;
        }
        
        config.frequency = requestedFreq;
        config.duration = requestedDuration;
        config.mode = doc["mode"] | "medium";  // Default medium for 30Hz
        config.averagingSamples = doc["averagingSamples"] | 1;
        
        if (doc.containsKey("calibration")) {
            calibration.offsetMM = doc["calibration"]["offset"] | 0.0;
            calibration.scaleFactor = doc["calibration"]["scale"] | 1.0;
        }
        
        config.configured = true;
        sampleInterval = 1000 / config.frequency;
        diagnostics = DiagnosticStats();
        
        Serial.printf("Configured: freq=%dHz, dur=%ds, interval=%dms, avg=%d\n", 
            config.frequency, config.duration, sampleInterval, config.averagingSamples);
        
        // Configure sensor for the requested frequency
        if (!configureSensorForFrequency(config.frequency)) {
            Serial.println("WARNING: Sensor configuration failed");
        }
        
        // Update hardware timer with new frequency
        updateTimerFrequency(config.frequency);
        
        // Send proper JSON response
        String response;
        DynamicJsonDocument respDoc(200);
        respDoc["success"] = true;
        respDoc["frequency"] = config.frequency;
        respDoc["duration"] = config.duration;
        respDoc["interval"] = sampleInterval;
        serializeJson(respDoc, response);
        
        request->send(200, "application/json", response);
        
    } else {
        Serial.println("ERROR: No plain text parameter found");
        request->send(400, "application/json", "{\"error\":\"No body\"}");
    }
}

// Handle calibration request
void handleCalibrate(AsyncWebServerRequest *request) {
    if (request->hasParam("plain", true)) {
        String body = request->getParam("plain", true)->value();
        
        DynamicJsonDocument doc(256);
        if (deserializeJson(doc, body) == DeserializationError::Ok) {
            calibration.offsetMM = doc["offset"] | 0.0;
            calibration.scaleFactor = doc["scale"] | 1.0;
            
            Serial.printf("Calibration updated: offset=%.2fmm, scale=%.4f\n", 
                calibration.offsetMM, calibration.scaleFactor);
                
            request->send(200, "application/json", "{\"success\":true}");
            return;
        }
    }
    request->send(400, "application/json", "{\"error\":\"Invalid request\"}");
}

// Handle start experiment request
void handleStart(AsyncWebServerRequest *request) {
    if (!config.configured) {
        request->send(400, "application/json", "{\"error\":\"Not configured\"}");
        return;
    }
    
    // Prevent restart if already running
    if (experimentRunning) {
        request->send(400, "application/json", "{\"error\":\"Experiment already running\"}");
        return;
    }
    
    // Add cooldown period after experiment completion
    const unsigned long COOLDOWN_MS = 3000; // 3 second cooldown
    
    if (lastExperimentEnd > 0 && (millis() - lastExperimentEnd) < COOLDOWN_MS) {
        request->send(400, "application/json", "{\"error\":\"Please wait before starting new experiment\"}");
        return;
    }
    
    experimentRunning = true;
    dataReady = false;
    sampleCount = 0;
    experimentStartTime = millis();
    lastSampleTime = millis();
    lastExperimentEnd = 0; // Reset cooldown timer
    
    Serial.println("Experiment started");
    request->send(200, "application/json", "{\"success\":true}");
}

// Handle stop experiment request
void handleStop(AsyncWebServerRequest *request) {
    experimentRunning = false;
    dataReady = true;
    
    Serial.println("Experiment stopped");
    request->send(200, "application/json", "{\"success\":true}");
}

// Handle data retrieval request
void handleData(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    JsonArray distArray = doc["distances"].to<JsonArray>();
    JsonArray timeArray = doc["timestamps"].to<JsonArray>();
    
    for (int i = 0; i < sampleCount; i++) {
        distArray.add(distances[i]);
        timeArray.add(timestamps[i]);
    }
    
    doc["count"] = sampleCount;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Handle OPTIONS request (CORS)
void handleOptions(AsyncWebServerRequest *request) {
    request->send(200);
}

// Handle ID request
void handleId(AsyncWebServerRequest *request) {
    String response = "{\"id\":\"" + sensorType + "\"}";
    request->send(200, "application/json", response);
}

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