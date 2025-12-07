#ifndef EXPERIMENT_MANAGER_H
#define EXPERIMENT_MANAGER_H

#include <Arduino.h>

// Experiment state variables
extern bool experimentRunning;
extern bool dataReady;
extern unsigned long experimentStartTime;
extern int readingCount; // Count of readings in current session

// Sensor detection variables
extern unsigned long lastSensorCheck;
extern const unsigned long SENSOR_CHECK_INTERVAL;
extern bool sensorWasPresent;
extern bool backendCleanupRequested;

// Experiment management functions
void manageExperimentLoop();
void checkSensorStatus();
void handleBackendCleanup();

// Helper to get expected time for resolutions
unsigned long getExpectedTime(int resolution);

#endif
