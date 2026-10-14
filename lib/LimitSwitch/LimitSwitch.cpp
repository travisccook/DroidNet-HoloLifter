/**
 * LimitSwitch Library Implementation
 */

#include "LimitSwitch.h"
#include "config.h"

/**
 * Constructor
 */
LimitSwitch::LimitSwitch(uint8_t pin, uint32_t debounceMs)
    : pin(pin),
      debounceMs(debounceMs),
      lastChangeTime(0),
      currentRawState(false),
      debouncedState(false),
      initialized(false) {
}

/**
 * Initialize limit switch pin
 */
void LimitSwitch::begin() {
    // Configure pin as input with internal pull-up
    // This is essential for NC (normally closed) switch operation
    pinMode(pin, INPUT_PULLUP);

    // Read initial state
    currentRawState = (digitalRead(pin) == LIMIT_TRIGGERED);
    debouncedState = currentRawState;
    lastChangeTime = millis();

    initialized = true;

#if DEBUG_LIMITS
    DEBUG_PRINT(F("Limit switch initialized on pin "));
    DEBUG_PRINT(pin);
    DEBUG_PRINT(F(", initial state: "));
    DEBUG_PRINTLN(debouncedState ? F("TRIGGERED") : F("NOT TRIGGERED"));
#endif
}

/**
 * Update debouncing logic
 */
void LimitSwitch::update() {
    if (!initialized) {
        return;
    }

    // Read current raw state
    bool rawState = (digitalRead(pin) == LIMIT_TRIGGERED);

    // Check if state has changed
    if (rawState != currentRawState) {
        // State changed - reset debounce timer
        currentRawState = rawState;
        lastChangeTime = millis();

#if DEBUG_LIMITS
        DEBUG_PRINT(F("Limit switch "));
        DEBUG_PRINT(pin);
        DEBUG_PRINT(F(" raw state changed to: "));
        DEBUG_PRINTLN(rawState ? F("TRIGGERED") : F("NOT TRIGGERED"));
#endif
    } else {
        // State stable - check if debounce time has elapsed
        if ((millis() - lastChangeTime) >= debounceMs) {
            // Debounce time elapsed - update debounced state
            if (debouncedState != currentRawState) {
                debouncedState = currentRawState;

#if DEBUG_LIMITS
                DEBUG_PRINT(F("Limit switch "));
                DEBUG_PRINT(pin);
                DEBUG_PRINT(F(" debounced state: "));
                DEBUG_PRINTLN(debouncedState ? F("TRIGGERED") : F("NOT TRIGGERED"));
#endif
            }
        }
    }
}

/**
 * Check if limit switch is triggered (debounced)
 */
bool LimitSwitch::isTriggered() const {
    return debouncedState;
}

/**
 * Get raw (non-debounced) switch state
 */
bool LimitSwitch::getRawState() const {
    if (!initialized) {
        return false;
    }

    return (digitalRead(pin) == LIMIT_TRIGGERED);
}

/**
 * Get pin number
 */
uint8_t LimitSwitch::getPin() const {
    return pin;
}
