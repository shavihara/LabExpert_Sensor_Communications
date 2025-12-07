#include "../include/experiment_manager.h"
#include "../include/sensor_communication.h"
#include "../include/mqtt_handler.h"
#include "../include/config_handler.h"
#include <esp_partition.h>
#include <esp_ota_ops.h>

bool experimentRunning = false;
bool dataReady = false;
unsigned long experimentStartTime = 0;
int readingCount = 0;

unsigned long lastSensorCheck = 0;
const unsigned long SENSOR_CHECK_INTERVAL = 5000;
bool sensorWasPresent = false;
bool backendCleanupRequested = false;

// Timing state
enum MeasurementState {
    STATE_IDLE,
    STATE_WAITING_FOR_CONVERSION,
    STATE_COOLDOWN
};
MeasurementState measureState = STATE_IDLE;
unsigned long conversionStartTime = 0;
unsigned long expectedConversionTime = 0;
unsigned long lastReadingTime = 0;

unsigned long getExpectedTime(int resolution) {
    switch(resolution) {
        case 9:  return 94;
        case 10: return 188;
        case 11: return 375;
        case 12: return 750;
        default: return 750;
    }
}

void manageExperimentLoop() {
    // Handle Backend Cleanup
    handleBackendCleanup();
    
    // Check Sensor Status
    checkSensorStatus();
    
    if (!experimentRunning) {
        measureState = STATE_IDLE;
        digitalWrite(STATUS_LED, LOW);
        return;
    }
    
    // Check duration
    if (config.duration > 0 && (millis() - experimentStartTime >= config.duration * 1000UL)) {
        experimentRunning = false;
        dataReady = true;
        publishStatus("experiment_completed", "Duration reached");
        digitalWrite(STATUS_LED, LOW);
        return;
    }
    
    digitalWrite(STATUS_LED, HIGH); // LED On while running
    
    switch (measureState) {
        case STATE_IDLE:
             // Start Conversion
             sensors.requestTemperatures();
             conversionStartTime = millis();
             expectedConversionTime = getExpectedTime(config.resolution);
             measureState = STATE_WAITING_FOR_CONVERSION;
             break;
             
        case STATE_WAITING_FOR_CONVERSION:
             if (millis() - conversionStartTime >= expectedConversionTime) {
                 // Time to read
                 unsigned long processStart = millis();
                 
                 float celsius = sensors.getTempC(sensorAddress);
                 if (celsius == DEVICE_DISCONNECTED_C) {
                     Serial.println("Error: Sensor read failed");
                     // Maybe retry or just skip?
                     measureState = STATE_IDLE; // Try again
                     return;
                 }
                 
                 float fahrenheit = sensors.toFahrenheit(celsius);
                 float kelvin = celsius + 273.15;
                 
                 readingCount++;
                 
                 SensorDataPacket packet;
                 packet.celsius = celsius;
                 packet.fahrenheit = fahrenheit;
                 packet.kelvin = kelvin;
                 packet.timestamp = millis() - experimentStartTime;
                 packet.sampleCount = readingCount;
                 packet.processTimeMs = millis() - conversionStartTime;
                 
                 publishSensorData(&packet);
                 
                 // Debug output similar to user's code
                 Serial.printf("#%d | Time: %.3fs | Wait: %lums | T: %.2f C | %.2f F | %.2f K\n",
                     readingCount, 
                     packet.timestamp / 1000.0,
                     packet.processTimeMs,
                     celsius, fahrenheit, kelvin);
                 
                 lastReadingTime = millis();
                 measureState = STATE_COOLDOWN;
             }
             break;
             
        case STATE_COOLDOWN:
             // Small delay between readings? User code had delay(100).
             if (millis() - lastReadingTime >= 100) {
                 measureState = STATE_IDLE;
             }
             break;
    }
}

void checkSensorStatus() {
    if (millis() - lastSensorCheck > SENSOR_CHECK_INTERVAL) {
        lastSensorCheck = millis();
        // Here we might want to check connection integrity
        if (sensors.getDeviceCount() == 0) {
            // Sensor lost
             if (sensorWasPresent) {
                 Serial.println("❌ Sensor unplugged!");
                 publishStatus("sensor_unplugged");
                 // Handle failsafe/reboot logic if needed
             }
             sensorWasPresent = false;
        } else {
            sensorWasPresent = true;
        }
    }
}

void handleBackendCleanup() {
    if (backendCleanupRequested) {
        Serial.println("Backend cleanup requested, restarting...");
        delay(1000);
        ESP.restart();
    }
}
