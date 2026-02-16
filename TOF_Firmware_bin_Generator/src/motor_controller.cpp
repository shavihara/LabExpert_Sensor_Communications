#include "../include/motor_controller.h"
#include "../include/mqtt_handler.h" // For publishing status

MotorController motor;

MotorController::MotorController() {
    state = IDLE;
    pulseCount = 0;
    maxPositionPulses = 0;
    lastSensorState = -1;
    lastCutTime = 0;
    waitingForFirstCut = false;
    directionForward = true;
    targetRotations = 0;
    holdDuration = 0;
    ROTS_PER_DEGREE = 0.88; // Default fallback
}

void MotorController::begin() {
    pinMode(RPWM_PIN, OUTPUT);
    pinMode(LPWM_PIN, OUTPUT);
    // EN pins are VCC hardwired
    
    pinMode(LDR_PIN, INPUT);
    lastSensorState = digitalRead(LDR_PIN);
    
    // Shared Limit Switch (Active HIGH from OR gate assumed)
    pinMode(LIMIT_PIN, INPUT);
    
    stopMotor();
    Serial.println("Motor Controller Initialized");
    
    // BOOT LOGIC:
    // Check if limit is already triggered
    if (digitalRead(LIMIT_PIN) == HIGH) {
        Serial.println("Boot: Limit Triggered. Assuming MIN Position (15 deg).");
        state = IDLE; // Ready
        pulseCount = 0; // 0 = MIN (15 deg)
    } else {
        Serial.println("Boot: Limit NOT Triggered. Starting Calibration...");
        Serial.println("Calib: Finding MAX (40 deg)...");
        state = CALIBRATING_FIND_MAX;
        pulseCount = 0; // Reset
        setMotorSpeed(MAX_PWM, true); // Move OUT (UP)
    }
}

void MotorController::setConfiguration(float angle, unsigned long durationMillis) {
    if (angle < ANGLE_MIN) {
        Serial.printf("Error: Angle must be >= %.1f\n", ANGLE_MIN);
        return;
    }
    if (angle > ANGLE_MAX) {
        Serial.printf("Warning: Angle clamped to %.1f\n", ANGLE_MAX);
        angle = ANGLE_MAX;
    }
    
    // Calculate pulses relative to MIN (15 deg)
    float relativeAngle = angle - ANGLE_MIN;
    targetRotations = (long)(relativeAngle * ROTS_PER_DEGREE);
    
    holdDuration = durationMillis;
    
    Serial.printf("Motor Configured: Target=%ld pulses (%.1f deg), Duration=%lu ms\n", targetRotations, angle, holdDuration);
}

void MotorController::startSequence() {
    if (state != IDLE) {
        Serial.println("Error: Motor not IDLE. Cannot start.");
        return;
    }
    
    Serial.println("Motor: Starting Move OUT sequence");
    if (mqttConnected) publishStatus("motor_status", "wait");
    
    pulseCount = 0;
    waitingForFirstCut = false; // Calibration establishes sync, so we trust pulses
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
    
    // pulseCount has the current absolute position
    // We want to go to 0
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
            
            if (state == MOVING_OUT || state == CALIBRATING_FIND_MAX) {
                // Moving OUT/UP -> Count UP
                pulseCount++;
            } else if (state == MOVING_BACK || state == CALIBRATING_FIND_MIN || state == RETURNING_TO_MIN_FOR_SHUTDOWN) {
                // Moving BACK/DOWN -> Count DOWN
                pulseCount--;
            }
            Serial.printf("Motor Pulse: %ld\n", pulseCount);
        }
    }
    lastSensorState = currentState;
}

void MotorController::update() {
    processEncoder();
    checkLimits(); // Safety Check

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
            if (pulseCount <= 0) { // 0 is Home
                stopMotor();
                state = IDLE;
                Serial.println("Motor: Returned Home");
                if (mqttConnected) publishStatus("motor_status", "finished");
            }
            break;
            
        case CALIBRATING_FIND_MAX:
             // Logic handled in checkLimits()
             // Pulse counting happens in processEncoder()
             break;
             
        case CALIBRATING_FIND_MIN:
             // Logic handled in checkLimits()
             break;
             
        case RETURNING_TO_MIN_FOR_SHUTDOWN:
             // Logic handled in checkLimits()
             break;
            
        default:
            break;
    }
}

void MotorController::checkLimits() {
    // Check Shared Limit Pin (Active HIGH assumed)
    bool limitTriggered = (digitalRead(LIMIT_PIN) == HIGH);
    
    if (!limitTriggered) return;
    
    // CAUTION: LIMIT TRIGGERED
    
    if (state == MOVING_OUT) {
         Serial.println("🛑 Limit (MAX) Reached during Operation. Stopping.");
         stopMotor();
         pulseCount = maxPositionPulses > 0 ? maxPositionPulses : targetRotations; 
         state = FINISHED; // Treat as reached target? Or Error? Treating as reached for safety.
    } 
    else if (state == MOVING_BACK) {
         Serial.println("🛑 Limit (MIN) Reached during Operation. Stopping.");
         stopMotor();
         pulseCount = 0; 
         state = IDLE;
    }
    else if (state == CALIBRATING_FIND_MAX) {
         Serial.println("Calib: MAX Limit Hit. Saving Count & Reversing...");
         stopMotor();
         
         // We moved from unknown to MAX. We don't know the pulses yet.
         // Just reset count, move back to find MIN.
         // Effectively, just reverse now.
         pulseCount = 0; // Temporary reference
         state = CALIBRATING_FIND_MIN;
         delay(500);
         Serial.println("Calib: Finding MIN (15 deg)...");
         setMotorSpeed(MAX_PWM, false); // Move BACK (DOWN)
    }
    else if (state == CALIBRATING_FIND_MIN) {
         Serial.println("Calib: MIN Limit Hit. Calibration Complete.");
         stopMotor();
         
         // pulseCount decremented from 0 while moving back, so it should be negative
         long totalRangePulses = abs(pulseCount); 
         
         if (totalRangePulses > 10) { // Valid range check
             maxPositionPulses = totalRangePulses;
             float angleRange = ANGLE_MAX - ANGLE_MIN; // 40 - 15 = 25
             
             ROTS_PER_DEGREE = (float)totalRangePulses / angleRange;
             
             Serial.printf("Calib Result: Range=%ld pulses over %.1f deg.\n", totalRangePulses, angleRange);
             Serial.printf("Calib Result: ROTS_PER_DEGREE = %.2f\n", ROTS_PER_DEGREE);
         } else {
             Serial.println("Calib Failed: Movement too short. Keeping default.");
         }
         
         pulseCount = 0; // We are at MIN
         state = IDLE;
    }
    else if (state == RETURNING_TO_MIN_FOR_SHUTDOWN) {
         Serial.println("Safety: MIN Limit Reached. Shutdown Safety Complete.");
         stopMotor();
         pulseCount = 0;
         state = SHUTDOWN_COMPLETE;
    }
}

void MotorController::executeSafeShutdown() {
    if (state == IDLE && pulseCount == 0) {
        state = SHUTDOWN_COMPLETE;
        return;
    }
    
    Serial.println("Safety: Executing Safe Shutdown (Returning to MIN)...");
    stopMotor();
    state = RETURNING_TO_MIN_FOR_SHUTDOWN;
    setMotorSpeed(MAX_PWM, false); // Move BACK
}
