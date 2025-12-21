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
        FINISHED
    };

    MotorController();

    void begin();
    void update();
    
    // Commands
    void setConfiguration(float angle, unsigned long durationMillis);
    void startSequence(); // Moves OUT to target - called on Config
    void holdPosition(); // Called when Experiment Starts
    void returnHome();    // Called when Experiment Ends
    
    // Status
    bool isIdle() const { return state == IDLE; }
    State getState() const { return state; }
    long getPulseCount() const { return pulseCount; }

private:
    // Pin Definitions
    const int RPWM_PIN = 33;
    const int LPWM_PIN = 26;
    const int LDR_PIN = 23;

    // Constants
    const float ROTS_PER_DEGREE = 0.88;
    const int MAX_PWM = 50;
    const unsigned long DEBOUNCE_DELAY = 200;

    // State
    State state;
    bool directionForward; // true = CW, false = CCW
    
    // Configuration (Active)
    long targetRotations;
    unsigned long holdDuration;
    
    // Encoder
    long pulseCount;
    int lastSensorState;
    unsigned long lastCutTime;
    bool waitingForFirstCut;

    // Helpers
    void stopMotor();
    void setMotorSpeed(int speed, bool forward);
    void processEncoder();
};

extern MotorController motor;

#endif
