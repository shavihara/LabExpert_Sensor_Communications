#include "../include/motor_controller.h"
#include "../include/mqtt_handler.h" // For publishing status

MotorController motor;

MotorController::MotorController() {
    state = IDLE;
    pulseCount = 0;
    lastSensorState = -1;
    lastCutTime = 0;
    waitingForFirstCut = false;
    directionForward = true;
    targetRotations = 0;
    holdDuration = 0;
}

void MotorController::begin() {
    pinMode(RPWM_PIN, OUTPUT);
    pinMode(LPWM_PIN, OUTPUT);
    // EN pins are VCC hardwired
    
    pinMode(LDR_PIN, INPUT_PULLUP);
    lastSensorState = digitalRead(LDR_PIN);
    
    stopMotor();
    Serial.println("Motor Controller Initialized");
}

void MotorController::setConfiguration(float angle, unsigned long durationMillis) {
    if (angle < 15) {
        Serial.println("Error: Angle must be >= 15");
        return;
    }
    if (angle > 40) {
        Serial.println("Warning: Angle clamped to 40 degrees");
        angle = 40;
    }
    float relativeAngle = angle - 15;
    targetRotations = (long)(relativeAngle * ROTS_PER_DEGREE);
    holdDuration = durationMillis;
    
    Serial.printf("Motor Configured: Target=%ld rots, Duration=%lu ms\n", targetRotations, holdDuration);
}

void MotorController::startSequence() {
    if (state != IDLE) return;
    
    Serial.println("Motor: Starting Move OUT sequence");
    if (mqttConnected) publishStatus("motor_status", "wait");
    
    pulseCount = 0;
    waitingForFirstCut = true;
    state = MOVING_OUT;
    setMotorSpeed(MAX_PWM, true); // CW
}

void MotorController::holdPosition() {
    // This is called when Experiment STARTS (after motor reached target)
    // In this specific workflow, the "HOLDING" state is virtually just 'stopped' 
    // waiting for the experiment timer (managed by experiment_manager) to finish.
    // The motor logic itself doesn't need to count time here, it just waits for 'returnHome'
    
    if (state == FINISHED) {
         state = HOLDING;
         // Status update managed by experiment manager if needed, but 'holding' is implicit
    }
}

void MotorController::returnHome() {
    if (state == IDLE || state == MOVING_BACK) return;
    
    Serial.println("Motor: Returning HOME");
    if (mqttConnected) publishStatus("motor_status", "wait");
    
    pulseCount = 0; // Reset for return distance measurement
    state = MOVING_BACK;
    setMotorSpeed(MAX_PWM, false); // CCW
}

void MotorController::stopMotor() {
    analogWrite(RPWM_PIN, 0);
    analogWrite(LPWM_PIN, 0);
}

void MotorController::setMotorSpeed(int speed, bool forward) {
    directionForward = forward;
    if (forward) {
        analogWrite(RPWM_PIN, speed);
        analogWrite(LPWM_PIN, 0);
    } else {
        analogWrite(RPWM_PIN, 0);
        analogWrite(LPWM_PIN, speed);
    }
}

void MotorController::processEncoder() {
    int currentState = digitalRead(LDR_PIN);
    
    if (currentState == HIGH && lastSensorState == LOW) { // Rising Edge
        unsigned long currentTime = millis();
        if (currentTime - lastCutTime > DEBOUNCE_DELAY) {
            lastCutTime = currentTime;
            
            if (state == MOVING_OUT || state == MOVING_BACK) {
                if (waitingForFirstCut) {
                    pulseCount = 0;
                    waitingForFirstCut = false;
                    Serial.println("Motor: Sync (0)");
                } else {
                    pulseCount++;
                    Serial.printf("Motor: %ld / %ld\n", pulseCount, targetRotations);
                }
            }
        }
    }
    lastSensorState = currentState;
}

void MotorController::update() {
    processEncoder();

    switch (state) {
        case MOVING_OUT:
            if (pulseCount >= targetRotations) {
                stopMotor();
                state = FINISHED; // Interim state before Experiment Start
                 Serial.println("Motor: Target Reached (Out)");
                 if (mqttConnected) publishStatus("motor_status", "finished");
            }
            break;
            
        case MOVING_BACK:
            if (pulseCount >= targetRotations) {
                stopMotor();
                state = IDLE;
                Serial.println("Motor: Returned Home");
                if (mqttConnected) publishStatus("motor_status", "finished");
            }
            break;
            
        default:
            break;
    }
}
