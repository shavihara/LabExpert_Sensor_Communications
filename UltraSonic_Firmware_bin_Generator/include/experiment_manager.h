#ifndef EXPERIMENT_MANAGER_H
#define EXPERIMENT_MANAGER_H

#include <Arduino.h>

// LED pin definitions
#define STATUS_LED 13
#define WIFI_LED 14
#define SENSOR_LED 13

// Experiment constants
const int MAX_SAMPLES = 1000;

// Experiment data structures
extern float distances[MAX_SAMPLES];
extern unsigned long timestamps[MAX_SAMPLES];
extern int sampleCount;

// Experiment state variables
extern bool experimentRunning;
extern bool dataReady;
extern unsigned long experimentStartTime;
extern unsigned long lastSampleTime;
extern int sampleInterval;

// Sensor detection variables
extern unsigned long lastSensorCheck;
extern const unsigned long SENSOR_CHECK_INTERVAL;
extern bool sensorWasPresent;
extern unsigned long lastExperimentEnd;

// Backend cleanup flag
extern bool backendCleanupRequested;

// Adaptive batching constants
#define BATCH_1_5HZ 2
#define BATCH_10_20HZ 5
#define BATCH_30_50HZ 10
#define BATCH_HIGH_FREQ 15

// Experiment management functions
void manageExperimentLoop();
void checkSensorStatus();
void handleBackendCleanup();

// Hardware timer functions
bool initHardwareTimer();
void updateTimerFrequency(int frequency);

#endif