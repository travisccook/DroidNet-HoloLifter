/**
 * MotorControl Library
 *
 * Interface to SparkFun Qwiic SCMD (Serial Controlled Motor Driver)
 * Controls a single DC motor via I2C with speed and direction control.
 *
 * Hardware:
 *   - SparkFun Qwiic Motor Driver (ROB-15451)
 *   - I2C Address: 0x5D (default)
 *   - Motor on Channel A (Motor 0)
 */

#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <Arduino.h>
#include <SCMD.h>
#include <SCMD_config.h>

class MotorControl {
public:
    /**
     * Constructor
     * @param address I2C address of motor driver (default 0x5D)
     */
    MotorControl(uint8_t address = 0x5D);

    /**
     * Initialize motor driver
     * @return true if successful, false if driver not found
     */
    bool begin();

    /**
     * Write drive 0 to the driver as early as possible in setup(), before
     * begin(): after an ESP32-only reset the SCMD keeps its last drive level
     * (its outputs are still enabled) until something writes 0.
     * Best effort, unchecked; begin() writes 0 again before enabling.
     */
    void earlyStop();

    /**
     * Set motor speed and direction
     * @param speed Motor speed (-255 to +255)
     *              Positive = up, Negative = down, 0 = stop
     */
    void setSpeed(int16_t speed);

    /**
     * Stop motor immediately (coast/freewheel)
     */
    void stop();

    /**
     * Brake motor (coast to stop)
     * NOTE: SCMD doesn't support active braking. Platform relies on 298:1
     * gearbox natural resistance to hold position. This is identical to stop().
     */
    void brake();

    /**
     * Enable motor driver
     */
    void enable();

    /**
     * Disable motor driver
     */
    void disable();

    /**
     * Check if motor driver is ready (I2C communication OK)
     * @return true if driver responding
     */
    bool isReady();

    /**
     * Check for motor driver fault condition
     * @return true if fault detected
     */
    bool isFault();

    /**
     * Get current commanded speed
     * @return Current speed setting (-255 to +255)
     */
    int16_t getCurrentSpeed() const;

    /**
     * Get motor driver ID (for diagnostics)
     * @return Driver ID byte
     */
    uint8_t getDriverID();

private:
    SCMD motorDriver;           // SparkFun SCMD library object
    uint8_t i2cAddress;         // I2C address
    int16_t currentSpeed;       // Current speed setting (last written)
    uint32_t lastWriteMs;       // millis() of the last drive write (for the refresh)
    bool initialized;           // Initialization flag

    /**
     * Convert signed speed to SCMD drive level and direction
     * @param speed Signed speed value
     * @param level Output: unsigned drive level (0-255)
     * @param direction Output: direction (0=forward, 1=reverse)
     */
    void speedToLevelAndDirection(int16_t speed, uint8_t& level, uint8_t& direction);
};

#endif // MOTOR_CONTROL_H
