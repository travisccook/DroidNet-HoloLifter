/**
 * MotionController Library
 *
 * High-level motion control with state machine, homing, and motion profiles.
 * Orchestrates motor, encoder, and limit switches.
 *
 * State Machine:
 *   - IDLE: No movement, waiting for commands
 *   - HOMING: Running homing sequence
 *   - MOVING_TO_TARGET: Closed-loop position control
 *   - MANUAL_UP: Manual upward movement
 *   - MANUAL_DOWN: Manual downward movement
 *   - STOPPED: Emergency stop engaged
 *   - ERROR: Fault condition
 *
 * Features:
 *   - Requires manual homing before movement
 *   - Closed-loop position control with proportional gain
 *   - Deceleration before soft limits and goto targets (lib/MotionProfile)
 *   - Stall detection
 *   - Software and hardware limit enforcement
 */

#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include <Arduino.h>
#ifdef ESP32_THING_PLUS
  #include <ESP32Encoder.h>
  // ESP32Encoder uses different method names - create compatibility macros
  #define ENCODER_READ(enc) (enc).getCount()
  #define ENCODER_WRITE(enc, val) (enc).setCount(val)
#else
  #include <Encoder.h>
  #define ENCODER_READ(enc) (enc).read()
  #define ENCODER_WRITE(enc, val) (enc).write(val)
#endif
#include "MotorControl.h"
#include "LimitSwitch.h"
#include "config.h"

// Motion states
enum MotionState {
    IDLE,
    HOMING,
    MOVING_TO_TARGET,
    MANUAL_UP,
    MANUAL_DOWN,
    STOPPED,
    ERROR
};

/**
 * Register a hook invoked when the controller cuts holofan power as a fail-safe
 * (emergency stop / fault). The hook (setHolofanPower in main.cpp) is the single
 * place that drives the relay.
 * If no hook is registered, the controller falls back to a direct relay write.
 */
void setHolofanPowerHook(void (*hook)(bool on));

class MotionController {
public:
    /**
     * Constructor
     * @param motor Motor control object
     * @param encoder Encoder object (Paul Stoffregen's library)
     * @param upperLimit Upper limit switch object
     * @param lowerLimit Lower limit switch object
     */
    #ifdef ESP32_THING_PLUS
    MotionController(MotorControl& motor, ESP32Encoder& encoder,
                     LimitSwitch& upperLimit, LimitSwitch& lowerLimit);
    #else
    MotionController(MotorControl& motor, Encoder& encoder,
                     LimitSwitch& upperLimit, LimitSwitch& lowerLimit);
    #endif

    /**
     * Initialize motion controller
     */
    void begin();

    /**
     * Update motion controller (call in loop())
     * Executes state machine and safety checks
     */
    void update();

    // ===== Movement Commands =====

    /**
     * Move platform up (manual mode or relative move)
     * @param distance Optional distance to move (0 = continuous until stop/limit)
     * @return false if refused (ERROR state, or a relative move before homing)
     */
    bool moveUp(int32_t distance = 0);

    /**
     * Move platform down (manual mode or relative move)
     * @param distance Optional distance to move (0 = continuous until stop/limit)
     * @return false if refused (ERROR state, or a relative move before homing)
     */
    bool moveDown(int32_t distance = 0);

    /**
     * Move to absolute position (closed-loop)
     * Requires explicit homing first (returns false if not homed)
     * Validates target against soft limits
     * @param target Target position in encoder counts
     * @return true if command accepted, false if invalid target or not homed
     */
    bool gotoPosition(int32_t target);

    /**
     * Stop all movement (emergency stop)
     */
    void stop();

    /**
     * Run homing sequence (manual trigger)
     */
    void home();

    /**
     * Clear error state and return to IDLE
     */
    void clearError();

    // ===== Status Queries =====

    /**
     * Get current motion state
     * @return Current state
     */
    MotionState getState() const;

    /**
     * Get current position
     * @return Position in encoder counts
     */
    int32_t getPosition() const;

    /**
     * Get target position
     * @return Target position in encoder counts
     */
    int32_t getTarget() const;

    /**
     * Check if system is homed
     * @return true if homed
     */
    bool isHomed() const;

    /**
     * Check if system is moving
     * @return true if moving
     */
    bool isMoving() const;

    /**
     * Get upper limit switch state
     * @return true if triggered
     */
    bool getUpperLimit() const;

    /**
     * Get lower limit switch state
     * @return true if triggered
     */
    bool getLowerLimit() const;

    /**
     * Get current speed setting
     * @return Speed (0-255)
     */
    uint8_t getSpeed() const;

    /**
     * Get error message (if in ERROR state)
     * @return Pointer to error message string
     */
    const char* getErrorMessage() const;

    // ===== Configuration =====

    /**
     * Set movement speed
     * @param speed Speed (0-255)
     */
    void setSpeed(uint8_t speed);

    /**
     * Set soft limit margin
     * @param margin Margin from hard limits in encoder counts
     */
    bool setSoftLimitMargin(int32_t margin);  // false (errorMessage set) if out of range

    /**
     * Get soft limit margin
     * @return Margin from hard limits in encoder counts
     */
    int32_t getSoftLimitMargin() const;

    // ===== EEPROM Configuration =====

    /**
     * Save configuration to EEPROM
     */
    void saveConfig();

    /**
     * Load configuration from EEPROM
     */
    void loadConfig();

    /**
     * Reset configuration to defaults
     */
    void resetConfig();

private:
    // Hardware references
    MotorControl& motor;
    #ifdef ESP32_THING_PLUS
    ESP32Encoder& encoder;
    #else
    Encoder& encoder;
    #endif
    LimitSwitch& upperLimit;
    LimitSwitch& lowerLimit;

    // State machine
    MotionState state;
    MotionState previousState;
    uint32_t stateStartTime;
    uint8_t homingStage;  // Track homing sub-states: 0=down, 1=up, 2=return
    bool homingNeedBackoff;  // Stage 0 started on the upper switch: back off first
    bool homingBackedOff;    // Stage 0 back-off done, now re-finding the switch
    int32_t homingBackoffStart;  // Encoder position where the back-off started
    bool homingBackoffUnfinished;  // A back-off has not yet seen the switch release (kept across stop/re-home)

    // Movement parameters
    int32_t targetPosition;
    int8_t moveDir;  // Goto direction at start (+1 up, -1 down)
    int8_t lastDriveDir;     // Direction of the last non-zero motor drive (0 = none yet)
    uint32_t lastDriveMs;    // millis() of that drive (for the reversal pause)
    bool upAfterHoming;  // `up` before homing: home first, then continue up
    bool gotoAfterHoming;   // goto before homing: home first, then go to pendingTarget
    int32_t pendingTarget;  // absolute target of that goto
    uint8_t targetSpeed;
    uint8_t currentSpeed;
    bool homed;
    bool upperLimitCalibrated;  // Track if upper limit has been found
    bool lowerLimitCalibrated;  // Track if lower limit has been found

    // Software limits
    int32_t softLimitLower;   // Lowest allowed position (lower switch + margin)
    int32_t softLimitUpper;   // Highest allowed position (upper switch - margin)
    int32_t softLimitMargin;  // Configurable margin from hard limits
    int32_t homedTravel;      // Switch-to-switch travel measured by homing (0 = not homed)

    // Rest supervision (IDLE/STOPPED/ERROR): re-send stop, see superviseRest()
    uint32_t restStopMs;
    int32_t restPos;
    bool restUpper;
    bool restLower;

    // Stall detection
    uint32_t lastPositionTime;
    int32_t lastPosition;

    // Homing calibration
    int32_t homingUpperLimitPos;  // Encoder position at upper limit during homing

    // Error tracking
    char errorMessage[64];

    // ===== State Machine Handlers =====

    void handleIdle();
    void handleHoming();
    void handleMovingToTarget();
    void handleManualUp();
    void handleManualDown();
    void handleStopped();
    void handleError();
    void superviseRest();  // At rest: keep the motor stopped (lost I2C writes)
    void finishHoming();  // Homing done: IDLE, or continue a pending `up`

    // ===== Safety Checks =====

    bool checkLimits(int8_t dir);  // Stop if a limit switch blocks commanded direction dir
    bool checkStall();            // Detect stalled motor
    bool checkSoftLimits(int8_t direction);  // Check software position limits for given direction (+1=up, -1=down, 0=both)
    void enterErrorState(const char* message);
    bool checkWiring();  // Both switches triggered -> ERROR; true if faulted
    void cancelPendingMoves();  // Drop moves queued behind homing (stop/error/home)
    void applyMargin(int32_t margin);  // Set margin; if homed, move the soft limits with it

    // ===== Helper Functions =====

    void setState(MotionState newState);
    void setMotorSpeed(int16_t speed);
    const char* stateToString(MotionState state) const;
};

#endif // MOTION_CONTROLLER_H
