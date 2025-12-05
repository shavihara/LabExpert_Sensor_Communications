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

// New variables for specific oscillation logic
bool waitingForFirstCut = false;
int cutCount = 0;
unsigned long lastOscillationEndTime = 0;

// Sensor detection variables
bool sensorWasPresent = false;
unsigned long lastExperimentEnd = 0;
bool backendCleanupRequested = false;
float pendulumLengthCm = 0.0f;

void startExperiment(int count)
{
    if (experimentRunning)
        return;


    experimentRunning = true;
    dataReady = false;
    
    // Reset specific logic variables
    waitingForFirstCut = true;
    cutCount = 0;
    lastOscillationEndTime = 0;
    
    targetOscillationCount = count;
    currentOscillationCount = 0;
    dataIndex = 0;
    
    // Initial state check
    lastSensorState = digitalRead(SENSOR_PIN);

    Serial.printf("Experiment started - Waiting for 1st Cut to start timer. Target: %d oscillations\n", count);
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
    
    // We are looking for Rising Edge (LOW -> HIGH) which represents a "Cut" (Beam Broken)
    // assuming INPUT_PULLUP and LDR (Light = LOW, Dark/Cut = HIGH)
    
    if (currentState == HIGH && lastSensorState == LOW)
    {
        // Rising Edge Detected (Cut)
        unsigned long currentTime = millis();
        static unsigned long lastCutTime = 0;

        // Debounce / Noise Filter
        // Ignore cuts that happen too close to the previous one (< 200ms)
        if (currentTime - lastCutTime < 200 && !waitingForFirstCut) 
        {
             // Serial.println("⚠️ Ignored rapid cut (debounce)");
             lastSensorState = currentState;
             return;
        }

        lastCutTime = currentTime;

        if (waitingForFirstCut)
        {
            // This is the 1st Cut - Start the Timer
            experimentStartTime = currentTime;
            waitingForFirstCut = false;
            cutCount = 1;
            lastOscillationEndTime = 0; // Start time relative to experiment start is 0
            
            Serial.println("✂️ First Cut Detected - Timer Started (0 ms)");
        }
        else
        {
            // Subsequent Cuts
            cutCount++;
            unsigned long totalElapsedTime = currentTime - experimentStartTime;
            
            Serial.printf("✂️ Cut %d detected at %lu ms\n", cutCount, totalElapsedTime);
            
            // Check if this cut completes an oscillation
            // Oscillation 1 ends at Cut 3
            // Oscillation 2 ends at Cut 5
            // Oscillation N ends at Cut 2N + 1
            
            if (cutCount >= 3 && (cutCount % 2 != 0))
            {
                // Oscillation Complete
                currentOscillationCount = (cutCount - 1) / 2;
                
                // Publish Data
                // We send:
                // disconnect_time: Start of this oscillation (End of previous one)
                // reconnect_time: End of this oscillation (Current time)
                // Both are cumulative times from the start
                
                publishOscillationData(currentOscillationCount, 
                                     lastOscillationEndTime, 
                                     totalElapsedTime);
                                     
                Serial.printf("✅ Oscillation %d Completed. Time: %lu ms\n", currentOscillationCount, totalElapsedTime);
                
                // Update last end time for next oscillation
                lastOscillationEndTime = totalElapsedTime;
                
                // Blink LED
                digitalWrite(SENSOR_LED, HIGH);
                delay(50);
                digitalWrite(SENSOR_LED, LOW);
                
                // Check Target
                if (currentOscillationCount >= targetOscillationCount)
                {
                    Serial.println("🏁 Target reached - Experiment Completed");
                    experimentRunning = false;
                    dataReady = true;
                    publishStatus("experiment_completed");
                    // We can also publish summary if needed, but the arrays might not be populated correctly
                    // with this new logic if we don't update them. 
                    // Given the user's specific logic, the summary might be less relevant or needs update.
                    // For now, I'll skip complex array storage updates to avoid confusion/bugs 
                    // since the user relies on the immediate backend data.
                }
            }
        }
    }
    
    lastSensorState = currentState;
}

// Main experiment loop
void manageExperimentLoop()
{
    processSensorState();

    if (experimentRunning && !waitingForFirstCut)
    {
        // Optional: Timeout logic
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
