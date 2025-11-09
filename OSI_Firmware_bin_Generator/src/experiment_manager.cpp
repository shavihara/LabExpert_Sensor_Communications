#include "experiment_manager.h"
#include "mqtt_handler.h"
#include "sensor_communication.h"
#include "config_handler.h"
#include <Arduino.h>

// Reduce array sizes to save memory
unsigned long disconnectTimes[20]; // Reduced from 40 to 20
unsigned long reconnectTimes[20];  // Reduced from 40 to 20
int dataIndex = 0;

// Experiment state variables
bool experimentRunning = false;
bool dataReady = false;
unsigned long experimentStartTime = 0;
unsigned long lastStateChangeTime = 0;
int targetOscillationCount = 0;
int currentOscillationCount = 0;
int lastSensorState = -1;
bool waitingForHigh = true;

// Sensor detection variables
bool sensorWasPresent = false;
unsigned long lastExperimentEnd = 0;
bool backendCleanupRequested = false;

void startExperiment(int count)
{
    if (experimentRunning)
        return;

    // Limit maximum count to fit in reduced arrays
    if (count > 10)
        count = 10; // Max 10 oscillations (20 data points)

    experimentRunning = true;
    dataReady = false;
    experimentStartTime = millis();
    lastStateChangeTime = experimentStartTime;
    targetOscillationCount = count;
    currentOscillationCount = 0;
    dataIndex = 0;
    waitingForHigh = true;
    lastSensorState = digitalRead(SENSOR_PIN);

    Serial.printf("Experiment started - Target: %d oscillations\n", count);
}

void stopExperiment()
{
    experimentRunning = false;
    dataReady = true;
    Serial.println("Experiment stopped");
}

void processSensorState()
{
    if (!experimentRunning)
        return;

    int currentState = digitalRead(SENSOR_PIN);
    unsigned long currentTime = millis();
    unsigned long elapsedTime = currentTime - experimentStartTime;

    // Only process if state changed
    if (currentState != lastSensorState)
    {
        lastSensorState = currentState;
        lastStateChangeTime = currentTime;

        if (currentState == HIGH && waitingForHigh)
        {
            // Beam cut - record disconnect time
            if (dataIndex < 20)
            { // Updated to match new array size
                disconnectTimes[dataIndex] = elapsedTime;
                waitingForHigh = false;
                Serial.printf("Beam CUT - Time: %lu ms\n", elapsedTime);
            }
        }
        else if (currentState == LOW && !waitingForHigh)
        {
            // Beam reconnected - record reconnect time and complete oscillation
            if (dataIndex < 20)
            { // Updated to match new array size
                reconnectTimes[dataIndex] = elapsedTime;

                // Publish this oscillation immediately
                publishOscillationData(currentOscillationCount + 1,
                                       disconnectTimes[dataIndex],
                                       reconnectTimes[dataIndex]);

                currentOscillationCount++;
                dataIndex++;
                waitingForHigh = true;

                // Blink sensor LED for each oscillation
                digitalWrite(SENSOR_LED, HIGH);
                delay(50);
                digitalWrite(SENSOR_LED, LOW);

                Serial.printf("Oscillation %d completed\n", currentOscillationCount);

                // Check if target reached
                if (currentOscillationCount >= targetOscillationCount)
                {
                    experimentRunning = false;
                    dataReady = true;
                    publishExperimentSummary();

                    Serial.println("Target oscillation count reached - Experiment completed");
                    publishStatus("experiment_completed");
                }
            }
        }
    }
}

// Main experiment loop
void manageExperimentLoop()
{
    processSensorState();

    if (experimentRunning)
    {
        unsigned long currentTime = millis();
        unsigned long elapsedTime = currentTime - experimentStartTime;

        // You can add timeout logic here if needed
    }
}

// Check sensor status
void checkSensorStatus()
{
    // Add periodic EEPROM check if needed
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 10000)
    { // Check every 10 seconds
        lastCheck = millis();
        bool currentStatus = detectSensorFromEEPROM();

        if (sensorWasPresent && !currentStatus)
        {
            Serial.println("❌ Sensor unplugged!");
            publishStatus("sensor_unplugged");
        }

        sensorWasPresent = currentStatus;
    }
}

// Handle backend cleanup
void handleBackendCleanup()
{
    if (backendCleanupRequested)
    {
        Serial.println("Backend cleanup requested");
        backendCleanupRequested = false;
        publishStatus("disconnected", "Rebooting to bootloader");
        delay(1000);
        ESP.restart();
    }
}