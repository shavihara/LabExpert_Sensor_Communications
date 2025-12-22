#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

class LedController {
public:
    enum State {
        OFF,
        ON,
        BLINK_SLOW,  // 1000ms
        BLINK_FAST,  // 200ms
        BLINK_PULSE, // 3000ms period, short ON
        BLINK_CUSTOM
    };

    LedController(int pin, bool activeLow = true);
    
    void begin();
    void update();
    
    void set(State state);
    void setBlink(unsigned long onTime, unsigned long offTime);
    void turnOn();
    void turnOff();

    State getState() const { return _currentState; }

private:
    int _pin;
    bool _activeLow;
    State _currentState;
    
    unsigned long _lastUpdate;
    bool _ledState; // Physical state (HIGH/LOW) specific to active logic
    bool _logicState; // Logical state (true=ON, false=OFF)
    
    // Blink timing
    unsigned long _onDuration;
    unsigned long _offDuration;
};

#endif
