/**
 * MotorControl Library Implementation
 */

#include "MotorControl.h"
#include <Wire.h>
#include "config.h"
#include "MotionProfile.h"

/**
 * Constructor
 */
MotorControl::MotorControl(uint8_t address)
    : i2cAddress(address), currentSpeed(0), lastWriteMs(0), initialized(false) {
}

/**
 * Initialize motor driver
 */
bool MotorControl::begin() {
    // Configure motor driver settings
    motorDriver.settings.commInterface = I2C_MODE;
    motorDriver.settings.I2CAddress = i2cAddress;

    // Initialize motor driver
    // begin() returns ID_WORD (0xA9) if successful
    uint8_t result = motorDriver.begin();
    if (result != 0xA9) {
        DEBUG_PRINTLN(F("ERROR: Motor driver init failed!"));
        initialized = false;
        return false;
    }

    // Wait for enumeration (required by SCMD library)
    delay(50);

    // Check if motor driver is ready
    while (motorDriver.ready() == false) {
        delay(10);
    }

    // Drive 0 before enabling: the SCMD may still hold a drive level from
    // before an ESP32 reset (stop() below would be a no-op: not initialized yet)
    motorDriver.setDrive(0, 0, 0);

    // SCMD failsafe deliberately left OFF (FSAFE_TIME 0x76 = 0x00, its
    // default). Per the SCMD datasheet: FSAFE_TIME is the watchdog timeout
    // in 10 ms units (0 = off), any received I2C byte feeds it, and on
    // timeout FSAFE_CTRL (default 0x11) centres the drive (no drive) and
    // re-inits the expansion port, counting a hit in FSAFE_FAULTS. Before
    // enabling it (e.g. >= 500 ms; setSpeed and the rest supervision write at
    // least every MOTOR_REFRESH_MS), check the blocking paths against the
    // timeout (homing delay(100)s, EEPROM commits, maintenance mode, OTA)
    // and note that isFault() treats any FSAFE_FAULTS count as a fault.

    // Enable motor driver
    motorDriver.enable();

    initialized = true;
    currentSpeed = 0;
    stop();  // initialized now, so this really writes 0 (and starts the refresh clock)

    DEBUG_PRINTLN(F("Motor driver initialized successfully"));

    return true;
}

/**
 * Early drive-0 write (see header)
 */
void MotorControl::earlyStop() {
    motorDriver.settings.commInterface = I2C_MODE;
    motorDriver.settings.I2CAddress = i2cAddress;
    Wire.begin();                    // begin() calls it again; a second call is harmless
    motorDriver.setDrive(0, 0, 0);   // level 0 -> MA_DRIVE 0x80 (centre = no drive)
}

/**
 * Set motor speed and direction
 */
void MotorControl::setSpeed(int16_t speed) {
    if (!initialized) {
        DEBUG_PRINTLN(F("ERROR: Motor driver not initialized!"));
        return;
    }

    // Constrain speed to valid range
    speed = constrain(speed, -MOTOR_MAX_SPEED, MOTOR_MAX_SPEED);

    // Called every loop: write I2C when the speed/direction changes, and
    // re-send an unchanged one every MOTOR_REFRESH_MS (SCMD::writeRegister
    // ignores endTransmission(), so a lost write would otherwise stick).
    // Every path that zeroes currentSpeed (stop/brake/begin) writes 0 first.
    if (!motionprofile::driveWriteDue(currentSpeed, speed, millis() - lastWriteMs, MOTOR_REFRESH_MS)) {
        return;
    }

    // Store current speed
    currentSpeed = speed;

    // Convert to SCMD level and direction
    uint8_t level;
    uint8_t direction;
    speedToLevelAndDirection(speed, level, direction);

    // Set motor drive (Motor 0 = Channel A)
    motorDriver.setDrive(0, direction, level);
    lastWriteMs = millis();
}

/**
 * Stop motor immediately
 */
void MotorControl::stop() {
    if (!initialized) {
        return;
    }

    bool wasMoving = (currentSpeed != 0);
    currentSpeed = 0;
    motorDriver.setDrive(0, 0, 0);
    lastWriteMs = millis();

#if DEBUG_MOTOR
    // Quiet when re-sending a stop at rest (MotionController does it every MOTOR_REFRESH_MS)
    if (wasMoving) {
        DEBUG_PRINTLN(F("Motor stopped"));
    }
#endif
}

/**
 * Brake motor (coast to stop)
 * NOTE: SCMD doesn't support active braking. Pololu 5172 motor with 298:1
 * gearbox provides sufficient natural resistance to hold platform position.
 */
void MotorControl::brake() {
    if (!initialized) {
        return;
    }

    currentSpeed = 0;
    motorDriver.setDrive(0, 0, 0);  // Coast to stop (no active braking)

#if DEBUG_MOTOR
    DEBUG_PRINTLN(F("Motor stopped (coasting - natural gearbox resistance holds position)"));
#endif
}

/**
 * Enable motor driver
 */
void MotorControl::enable() {
    if (!initialized) {
        return;
    }

    motorDriver.enable();
    DEBUG_PRINTLN(F("Motor driver enabled"));
}

/**
 * Disable motor driver
 */
void MotorControl::disable() {
    if (!initialized) {
        return;
    }

    stop();
    motorDriver.disable();
    DEBUG_PRINTLN(F("Motor driver disabled"));
}

/**
 * Check if motor driver is ready
 */
bool MotorControl::isReady() {
    if (!initialized) {
        return false;
    }

    return motorDriver.ready();
}

/**
 * Check for motor driver fault
 */
bool MotorControl::isFault() {
    if (!initialized) {
        return true;  // Not initialized = fault condition
    }

    // Check for I2C communication failure
    if (!motorDriver.ready()) {
        return true;
    }

    // Check motor driver diagnostic register
    SCMDDiagnostics diagnostics;
    motorDriver.getDiagnostics(diagnostics);
    if (diagnostics.U_I2C_RD_ERR || diagnostics.U_I2C_WR_ERR ||
        diagnostics.U_BUF_DUMPED || diagnostics.E_I2C_RD_ERR ||
        diagnostics.E_I2C_WR_ERR || diagnostics.FSAFE_FAULTS) {
        return true;
    }

    return false;
}

/**
 * Get current commanded speed
 */
int16_t MotorControl::getCurrentSpeed() const {
    return currentSpeed;
}

/**
 * Get motor driver ID (returns begin() result, should be 0xA9)
 */
uint8_t MotorControl::getDriverID() {
    if (!initialized) {
        return 0;
    }

    // SCMD doesn't have a separate getID() method
    // The ID (0xA9) is returned by begin() during initialization
    // Return 0xA9 if initialized and ready
    return (motorDriver.ready() ? 0xA9 : 0x00);
}

/**
 * Convert signed speed to SCMD level and direction
 */
void MotorControl::speedToLevelAndDirection(int16_t speed, uint8_t& level, uint8_t& direction) {
    if (speed >= 0) {
        // Positive = forward (up)
        direction = 0;  // Motor wired backwards - use dir 0 for UP
        level = (uint8_t)speed;
    } else {
        // Negative = reverse (down)
        direction = 1;  // Motor wired backwards - use dir 1 for DOWN
        level = (uint8_t)(-speed);
    }
}
