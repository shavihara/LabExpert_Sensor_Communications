#ifndef MOTOR_CONTROLLER_H
#define MOTOR_CONTROLLER_H

#include <Arduino.h>

class MotorController {
public:
    enum State {
        IDLE,
        MOVING_OUT,
        HOLDING,
        MOVING_BACK,
        FINISHED,
        CALIBRATING_FIND_MAX,
        CALIBRATING_FIND_MIN,
        RETURNING_TO_MIN_FOR_SHUTDOWN,
        SHUTDOWN_COMPLETE
    };

    MotorController();

    void begin();
    void update();
    
    // Commands
    void setConfiguration(float angle, unsigned long durationMillis);
    void startSequence(); // Moves OUT to target - called on Config
    void holdPosition(); // Called when Experiment Starts
    void returnHome();    // Called when Experiment Ends
    
    // Safety & Calibration
    void executeSafeShutdown(); // Drives to MIN then signals readiness for reboot
    bool isShutdownComplete() const { return state == SHUTDOWN_COMPLETE; }
    void checkLimits();
    
    // Status
    bool isIdle() const { return state == IDLE; }
    bool isCalibrating() const { return state == CALIBRATING_FIND_MAX || state == CALIBRATING_FIND_MIN; }
    State getState() const { return state; }
    long getPulseCount() const { return pulseCount; }

private:
    // Pin Definitions
    const int RPWM_PIN = 33;
    const int LPWM_PIN = 26;
    const int LDR_PIN = 23;
    const int LIMIT_PIN = 35; // Shared Limit Switch (OR Logic)

    // Constants
    float ROTS_PER_DEGREE = 0.88; // Now variable, calculated during calibration
    const int MAX_PWM = 50;
    const unsigned long DEBOUNCE_DELAY = 200;
    
    // Angles
    const float ANGLE_MIN = 15.0;
    const float ANGLE_MAX = 40.0;

    // State
    State state;
    bool directionForward; // true = CW, false = CCW
    
    // Configuration (Active)
    long targetRotations;
    unsigned long holdDuration;
    
    // Encoder
    long pulseCount;
    long maxPositionPulses; // Pulses at MAX limit (relative to MIN=0)
    int lastSensorState;
    unsigned long lastCutTime;
    bool waitingForFirstCut;

    // Helpers
    void stopMotor();
    void setMotorSpeed(int speed, bool forward);
    void processEncoder();
    void checkLimits();
};

extern MotorController motor;

#endif
