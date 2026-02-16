#include "LedController.h"

LedController::LedController(int pin, bool activeLow) {
    _pin = pin;
    _activeLow = activeLow;
    _currentState = OFF;
    _lastUpdate = 0;
    _onDuration = 500;
    _offDuration = 500;
}

void LedController::begin() {
    pinMode(_pin, OUTPUT);
    turnOff(); // Default to off
}

void LedController::turnOn() {
    digitalWrite(_pin, _activeLow ? LOW : HIGH);
    _logicState = true;
}

void LedController::turnOff() {
    digitalWrite(_pin, _activeLow ? HIGH : LOW);
    _logicState = false;
}

void LedController::set(State state) {
    _currentState = state;
    
    switch (state) {
        case OFF:
            turnOff();
            break;
        case ON:
            turnOn();
            break;
        case BLINK_SLOW:
            setBlink(1000, 1000);
            break;
        case BLINK_FAST:
            setBlink(200, 200);
            break;
        case BLINK_PULSE:
            setBlink(150, 2850);
            break;
        default:
            break;
    }
}

void LedController::setBlink(unsigned long onTime, unsigned long offTime) {
    _onDuration = onTime;
    _offDuration = offTime;
    _currentState = BLINK_CUSTOM;
}

void LedController::update() {
    if (_currentState == OFF || _currentState == ON) return;

    unsigned long now = millis();
    unsigned long cycleTime = _onDuration + _offDuration;
    unsigned long currentCyclePos = (now - _lastUpdate) % cycleTime;

    // Use modulo math to handle timing without resetting _lastUpdate constantly
    // But simplistic approach:
    
    if (_logicState) {
        // Currently ON
        if (now - _lastUpdate >= _onDuration) {
            turnOff();
            _lastUpdate = now;
        }
    } else {
        // Currently OFF
        if (now - _lastUpdate >= _offDuration) {
            turnOn();
            _lastUpdate = now;
        }
    }
}
