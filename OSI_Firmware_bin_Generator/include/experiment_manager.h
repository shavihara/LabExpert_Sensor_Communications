#ifndef EXPERIMENT_MANAGER_H
#define EXPERIMENT_MANAGER_H

#include <Arduino.h>
#include "config_handler.h"

// Reduced maximum samples storage
#define MAX_SAMPLES 500 // Reduced from 1000

// Global variables - DECLARE as extern
extern bool experimentRunning;
extern bool dataReady;
extern unsigned long experimentStartTime;
extern unsigned long lastStateChangeTime;
extern int targetOscillationCount;
extern int currentOscillationCount;
extern int lastSensorState;
extern bool waitingForHigh;
extern unsigned long disconnectTimes[];
extern unsigned long reconnectTimes[];
extern int dataIndex;
extern bool sensorWasPresent;
extern unsigned long lastExperimentEnd;
extern bool backendCleanupRequested;
extern float pendulumLengthCm;

// Function declarations
void manageExperimentLoop();
void checkSensorStatus();
void handleBackendCleanup();
void processSensorState();
void startExperiment(int count);
void stopExperiment();

#endif
