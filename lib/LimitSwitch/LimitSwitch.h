/**
 * LimitSwitch Library
 *
 * Debounced limit switch input handler for Normally Closed (NC) switches.
 * Provides fail-safe operation: broken wire = emergency stop.
 *
 * Logic (NC configuration):
 *   - LOW  = not triggered (switch closed, COM connected to NC/GND)
 *   - HIGH = triggered (switch open, COM disconnected, pulled high) or wire broken (fail-safe)
 */

#ifndef LIMIT_SWITCH_H
#define LIMIT_SWITCH_H

#include <Arduino.h>

class LimitSwitch {
public:
    /**
     * Constructor
     * @param pin GPIO pin number
     * @param debounceMs Debounce time in milliseconds (default 20ms)
     */
    LimitSwitch(uint8_t pin, uint32_t debounceMs = 20);

    /**
     * Initialize limit switch pin
     * Configures pin as INPUT_PULLUP for NC switch operation
     */
    void begin();

    /**
     * Update debouncing logic
     * Must be called regularly in loop() (every 1-10ms recommended)
     */
    void update();

    /**
     * Check if limit switch is triggered (debounced)
     * @return true if triggered (or wire broken - fail-safe)
     */
    bool isTriggered() const;

    /**
     * Get raw (non-debounced) switch state
     * @return true if raw pin reads as triggered
     */
    bool getRawState() const;

    /**
     * Get pin number
     * @return GPIO pin number
     */
    uint8_t getPin() const;

private:
    uint8_t pin;                  // GPIO pin number
    uint32_t debounceMs;          // Debounce time in milliseconds
    uint32_t lastChangeTime;      // Timestamp of last state change
    bool currentRawState;         // Current raw pin reading
    bool debouncedState;          // Debounced state
    bool initialized;             // Initialization flag
};

#endif // LIMIT_SWITCH_H
