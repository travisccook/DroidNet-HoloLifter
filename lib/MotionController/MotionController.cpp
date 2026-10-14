/**
 * MotionController Library Implementation
 */

#include "MotionController.h"
#include <EEPROM.h>
#include "MotionProfile.h"

// Optional hook for fail-safe holofan power cut (see MotionController.h).
static void (*s_holofanPowerHook)(bool) = nullptr;
void setHolofanPowerHook(void (*hook)(bool)) { s_holofanPowerHook = hook; }

// Cut holofan power on fault: use the hook (the relay helper) if set,
// otherwise fall back to a direct relay write.
static inline void failsafeHolofanOff() {
    if (s_holofanPowerHook) s_holofanPowerHook(false);
    else digitalWrite(RELAY_PIN, HOLOFAN_OFF);
}

/**
 * Constructor
 */
#ifdef ESP32_THING_PLUS
MotionController::MotionController(MotorControl& motor, ESP32Encoder& encoder,
                                   LimitSwitch& upperLimit, LimitSwitch& lowerLimit)
#else
MotionController::MotionController(MotorControl& motor, Encoder& encoder,
                                   LimitSwitch& upperLimit, LimitSwitch& lowerLimit)
#endif
    : motor(motor), encoder(encoder), upperLimit(upperLimit), lowerLimit(lowerLimit),
      state(IDLE), previousState(IDLE), stateStartTime(0), homingStage(0),
      homingNeedBackoff(false), homingBackedOff(false), homingBackoffStart(0), homingBackoffUnfinished(false),
      targetPosition(0), moveDir(0), lastDriveDir(0), lastDriveMs(0), upAfterHoming(false), gotoAfterHoming(false), pendingTarget(0), targetSpeed(MOTOR_DEFAULT_SPEED), currentSpeed(0),
      homed(false),
      upperLimitCalibrated(false), lowerLimitCalibrated(false),
      softLimitLower(0), softLimitUpper(10000), softLimitMargin(SOFT_LIMIT_MARGIN), homedTravel(0),
      restStopMs(0), restPos(0), restUpper(false), restLower(false),
      lastPositionTime(0), lastPosition(0) {

    errorMessage[0] = '\0';  // Initialize empty error message
}

/**
 * Initialize motion controller
 */
void MotionController::begin() {
    setState(IDLE);
    currentSpeed = 0;
    motor.stop();

    DEBUG_PRINTLN(F("MotionController initialized"));
}

/**
 * Update motion controller (call in loop())
 */
void MotionController::update() {
    // Both switches triggered while moving: wiring fault, stop now
    if (isMoving() && checkWiring()) {
        return;
    }

    // Execute state machine
    switch (state) {
        case IDLE:
            handleIdle();
            break;
        case HOMING:
            handleHoming();
            break;
        case MOVING_TO_TARGET:
            handleMovingToTarget();
            break;
        case MANUAL_UP:
            handleManualUp();
            break;
        case MANUAL_DOWN:
            handleManualDown();
            break;
        case STOPPED:
            handleStopped();
            break;
        case ERROR:
            handleError();
            break;
    }
}

// ===== Movement Commands =====

bool MotionController::moveUp(int32_t distance) {
    if (state == ERROR || checkWiring()) {
        return false;
    }

    // Reset stall detection to avoid false positive at start
    lastPosition = ENCODER_READ(encoder);
    lastPositionTime = millis();

    // If distance specified, use gotoPosition for relative move
    if (distance > 0) {
        // Before homing the position means nothing: refuse, don't guess
        if (!homed) {
            snprintf(errorMessage, sizeof(errorMessage), "Not homed - relative move refused, home first");
            return false;
        }
        int32_t target = ENCODER_READ(encoder) + distance;
        // Clamp to upper soft limit
        if (SOFT_LIMIT_ENABLED && target > softLimitUpper) {
            target = softLimitUpper;
        }
        return gotoPosition(target);
    }

    // Not homed: run the homing sequence first, then continue up
    if (motionprofile::homeBeforeManual(1, homed)) {
        DEBUG_PRINTLN(F("UP before homing - homing first"));
        home();
        upAfterHoming = (state == HOMING);
        return true;
    }

    setState(MANUAL_UP);
    return true;
}

bool MotionController::moveDown(int32_t distance) {
    if (state == ERROR || checkWiring()) {
        return false;
    }

    // Reset stall detection to avoid false positive at start
    lastPosition = ENCODER_READ(encoder);
    lastPositionTime = millis();

    // If distance specified, use gotoPosition for relative move
    if (distance > 0) {
        // Before homing the position means nothing: refuse, don't guess
        if (!homed) {
            snprintf(errorMessage, sizeof(errorMessage), "Not homed - relative move refused, home first");
            return false;
        }
        int32_t target = ENCODER_READ(encoder) - distance;
        // Clamp to lower soft limit
        if (SOFT_LIMIT_ENABLED && target < softLimitLower) {
            target = softLimitLower;
        }
        return gotoPosition(target);
    }

    // Not homed: no auto-home; handleManualDown runs at the homing-down
    // speed until the lower switch stops it (and zeroes the position there)
    if (!homed) {
        DEBUG_PRINTLN(F("DOWN before homing - slow speed to lower switch"));
    }

    setState(MANUAL_DOWN);
    return true;
}

bool MotionController::gotoPosition(int32_t target) {
    if (state == ERROR || checkWiring()) {
        return false;
    }

    // Check if homed - auto-home if enabled
    if (!homed) {
        #if AUTO_HOME_ON_FIRST_MOVE
        home();  // Auto-home on first movement
        // finishHoming() goes on to the target, checked against the new soft limits
        gotoAfterHoming = (state == HOMING);
        pendingTarget = target;
        return true;
        #else
        enterErrorState("Not homed - send 'home' command first");
        return false;
        #endif
    }

    // Validate target position against soft limits
    // Use static margin to ensure target is safely within limits
    if (SOFT_LIMIT_ENABLED && !motionprofile::targetInRange(target, softLimitLower, softLimitUpper)) {
        if (target < softLimitLower) {
            // Target below safe lower limit - return error without entering ERROR state
            DEBUG_PRINT(F("Invalid target position: "));
            DEBUG_PRINT(target);
            DEBUG_PRINT(F(" < lower limit ("));
            DEBUG_PRINT(softLimitLower);
            DEBUG_PRINTLN(F(")"));

            // Set error message but don't change state
            snprintf(errorMessage, sizeof(errorMessage),
                     "Target %ld below lower limit %ld",
                     target, softLimitLower);
            return false;
        }

        if (target > softLimitUpper) {
            // Target above safe upper limit - return error without entering ERROR state
            DEBUG_PRINT(F("Invalid target position: "));
            DEBUG_PRINT(target);
            DEBUG_PRINT(F(" > upper limit ("));
            DEBUG_PRINT(softLimitUpper);
            DEBUG_PRINTLN(F(")"));

            // Set error message but don't change state
            snprintf(errorMessage, sizeof(errorMessage),
                     "Target %ld above upper limit %ld",
                     target, softLimitUpper);
            return false;
        }
    }

    targetPosition = target;
    moveDir = (target >= ENCODER_READ(encoder)) ? 1 : -1;

    // Reset stall detection to avoid false positive at start
    lastPosition = ENCODER_READ(encoder);
    lastPositionTime = millis();

    setState(MOVING_TO_TARGET);
    return true;
}

void MotionController::stop() {
    cancelPendingMoves();  // a stop also cancels a move queued behind homing
    homingNeedBackoff = false;
    homingBackedOff = false;
    setState(STOPPED);
    motor.stop();
    currentSpeed = 0;

    // FAIL-SAFE: Turn off relay (holofan) on emergency stop
    failsafeHolofanOff();
}

void MotionController::home() {
    if (state == ERROR || checkWiring()) {
        return;
    }

    DEBUG_PRINTLN(F("Starting comprehensive homing sequence"));
    setState(HOMING);
    stateStartTime = millis();  // restart the homing timeout even if already HOMING
    cancelPendingMoves();  // moveUp/gotoPosition queue theirs after this
    // Decide the stage-0 back-off once, here: only if homing starts on the
    // upper switch. (Checking every loop also fired when stage 0 reached the
    // switch from below, so each homing hit it twice with a +80 -> -60 flip.)
    homingNeedBackoff = upperLimit.isTriggered();
    homingBackedOff = false;
    if (homingNeedBackoff) {
        // A repeated home (or stop + home) mid back-off keeps the first start,
        // so the HOMING_BACKOFF_MAX_COUNTS cap covers the whole back-off
        homingBackoffStart = motionprofile::backoffStart(homingBackoffUnfinished, homingBackoffStart,
                                                         ENCODER_READ(encoder));
        homingBackoffUnfinished = true;
    } else {
        homingBackoffUnfinished = false;  // the switch has released since
    }
    homingStage = 0;  // Start with stage 0: move UP to find upper limit
    homed = false;
    homedTravel = 0;
    lowerLimitCalibrated = false;
    upperLimitCalibrated = false;

    // Reset stall detection to avoid false positive at start
    lastPosition = ENCODER_READ(encoder);
    lastPositionTime = millis();
}

void MotionController::clearError() {
    if (state == ERROR || state == STOPPED) {
        motor.stop();  // Use stop() not brake() - allow platform to coast
        currentSpeed = 0;
        errorMessage[0] = '\0';  // Clear error message
        setState(IDLE);
        DEBUG_PRINTLN(F("Error cleared, returning to IDLE"));
    }
}

// ===== Status Queries =====

MotionState MotionController::getState() const {
    return state;
}

int32_t MotionController::getPosition() const {
    return ENCODER_READ(encoder);
}

int32_t MotionController::getTarget() const {
    return targetPosition;
}

bool MotionController::isHomed() const {
    return homed;
}

bool MotionController::isMoving() const {
    return (state == MOVING_TO_TARGET || state == MANUAL_UP ||
            state == MANUAL_DOWN || state == HOMING);
}

bool MotionController::getUpperLimit() const {
    return upperLimit.isTriggered();
}

bool MotionController::getLowerLimit() const {
    return lowerLimit.isTriggered();
}

uint8_t MotionController::getSpeed() const {
    return targetSpeed;
}

const char* MotionController::getErrorMessage() const {
    return errorMessage;
}

// ===== Configuration =====

void MotionController::setSpeed(uint8_t speed) {
    targetSpeed = motionprofile::clampSpeed(speed);
    saveConfig();  // Auto-save to EEPROM
}

bool MotionController::setSoftLimitMargin(int32_t margin) {
    int32_t travel = homed ? homedTravel : 0;
    if (!motionprofile::marginInRange(margin, travel)) {
        if (travel > 0) {
            snprintf(errorMessage, sizeof(errorMessage), "Margin %ld out of range %d-%ld",
                     (long)margin, SOFT_LIMIT_MARGIN_MIN, (long)(travel / 2));
        } else {
            snprintf(errorMessage, sizeof(errorMessage), "Margin %ld out of range %d-%d (not homed)",
                     (long)margin, SOFT_LIMIT_MARGIN_MIN, SOFT_LIMIT_MARGIN_MAX_UNHOMED);
        }
        return false;
    }
    applyMargin(margin);
    saveConfig();  // Auto-save to EEPROM
    return true;
}

void MotionController::applyMargin(int32_t margin) {
    // Never at or past the switches: [SOFT_LIMIT_MARGIN_MIN, travel/2]
    // (flash values too: an old, corrupt or v1-doubled margin is clamped;
    // before homing only to the minimum, homing then clamps to travel/2)
    margin = motionprofile::clampMargin(margin, homed ? homedTravel : 0);

    // Soft limits are switch position -/+ margin; move them with the margin
    // so a new margin takes effect without re-homing
    if (homed) {
        softLimitLower += margin - softLimitMargin;
        softLimitUpper -= margin - softLimitMargin;
    }
    softLimitMargin = margin;
}

int32_t MotionController::getSoftLimitMargin() const {
    return softLimitMargin;
}

// ===== EEPROM Configuration =====

void MotionController::saveConfig() {
    #ifdef ESP32_THING_PLUS
    EEPROM.begin(512);  // ESP32 requires EEPROM.begin() with size
    #endif

    // Write magic byte
    EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);

    // Write system config
    SystemConfig config;
    config.magicByte = EEPROM_MAGIC_VALUE;
    config.version = CONFIG_VERSION;
    config.defaultSpeed = targetSpeed;
    config.homingSpeed = MOTOR_HOMING_SPEED;
    config.softLimitLower = softLimitLower;
    config.softLimitUpper = softLimitUpper;
    config.softLimitMargin = softLimitMargin;
    config.autoHome = AUTO_HOME_ON_FIRST_MOVE;

    EEPROM.put(EEPROM_CONFIG_ADDR, config);

    #ifdef ESP32_THING_PLUS
    EEPROM.commit();  // ESP32 requires commit() to write to flash
    #endif

    DEBUG_PRINTLN(F("Configuration saved to EEPROM"));
}

void MotionController::loadConfig() {
    #ifdef ESP32_THING_PLUS
    EEPROM.begin(512);  // ESP32 requires EEPROM.begin() with size
    #endif

    // Check magic byte
    uint8_t magic = EEPROM.read(EEPROM_MAGIC_ADDR);
    if (magic != EEPROM_MAGIC_VALUE) {
        // No valid config found, use defaults (silent - main.cpp handles messaging)
        resetConfig();
        return;
    }

    // Load system config
    SystemConfig config;
    EEPROM.get(EEPROM_CONFIG_ADDR, config);

    if (config.magicByte == EEPROM_MAGIC_VALUE) {
        targetSpeed = motionprofile::clampSpeed(config.defaultSpeed);
        // Soft limits come from homing; stored ones only stand in until then
        if (!homed) {
            softLimitLower = config.softLimitLower;
            softLimitUpper = config.softLimitUpper;
        }
        applyMargin(motionprofile::marginFromStored(config.version, config.softLimitMargin));

        DEBUG_PRINTLN(F("Configuration loaded from EEPROM"));
    }
}

void MotionController::resetConfig() {
    // Reset to default values
    targetSpeed = MOTOR_DEFAULT_SPEED;
    // Keep the homed soft limits (resetting them to 0/10000 would let the
    // lift run onto the switches); they only move with the margin
    if (!homed) {
        softLimitLower = 0;
        softLimitUpper = 10000;
    }
    applyMargin(SOFT_LIMIT_MARGIN);

    DEBUG_PRINTLN(F("Configuration reset to defaults"));
}

// ===== State Machine Handlers =====

void MotionController::handleIdle() {
    // Idle state - waiting for commands; keep the motor stopped
    superviseRest();
}

void MotionController::superviseRest() {
    // A stop sent over I2C can be lost (SCMD::writeRegister does not check
    // endTransmission), so re-send it every MOTOR_REFRESH_MS, and at once if
    // the lift moves or a limit switch triggers while it should be at rest
    int32_t pos = ENCODER_READ(encoder);
    bool upper = upperLimit.isTriggered();
    bool lower = lowerLimit.isTriggered();
    bool limitRose = (upper && !restUpper) || (lower && !restLower);
    restUpper = upper;
    restLower = lower;
    if (motionprofile::restStopDue(millis() - restStopMs, pos - restPos, limitRose,
                                   MOTOR_REFRESH_MS, REST_DRIFT_COUNTS)) {
        motor.stop();
        currentSpeed = 0;
        restStopMs = millis();
        restPos = pos;
    }
}

void MotionController::handleHoming() {
    // Multi-stage homing sequence:
    // Stage 0: Move UP to find upper limit
    // Stage 1: Move DOWN to find lower limit (settles with gravity)

    // Check for timeout
    if (millis() - stateStartTime > HOMING_TIMEOUT_MS) {
        enterErrorState("Homing timeout");
        return;
    }

    // Check stall
    if (checkStall()) {
        enterErrorState("Motor stalled during homing");
        return;
    }

    // === Stage 0: Find Upper Limit ===
    if (homingStage == 0) {
        // Started on the upper switch (decided in home()): move down off it
        // first, then come back up to find its edge
        // (flags are reset by home(), stop() and enterErrorState())
        // If we need to move down, do so until we clear the limit, but never
        // onto the lower switch or further than HOMING_BACKOFF_MAX_COUNTS: a
        // broken upper wire reads "triggered" forever
        if (homingNeedBackoff && !homingBackedOff) {
            int32_t travelled = abs(homingBackoffStart - ENCODER_READ(encoder));
            switch (motionprofile::backoffStep(upperLimit.isTriggered(), lowerLimit.isTriggered(),
                                               travelled, HOMING_BACKOFF_MAX_COUNTS)) {
                case motionprofile::Backoff::FAULT_LOWER:
                    enterErrorState("Homing back-off reached lower switch - check upper switch wiring");
                    return;
                case motionprofile::Backoff::FAULT_DISTANCE:
                    enterErrorState("Homing back-off too long - check upper switch wiring");
                    return;
                case motionprofile::Backoff::DONE:
                    // We've cleared the upper limit, stop and mark that we moved down
                    motor.stop();
                    currentSpeed = 0;
                    homingBackedOff = true;
                    homingBackoffUnfinished = false;
                    delay(100);
                    DEBUG_PRINT(F("=== HOMING Stage 0: Cleared upper limit after "));
                    DEBUG_PRINT(travelled);
                    DEBUG_PRINTLN(F(" counts, now moving up to re-find it ==="));
                    lastPosition = ENCODER_READ(encoder);
                    lastPositionTime = millis();
                    return;
                case motionprofile::Backoff::CONTINUE:
                    // Still on upper limit, keep moving down
                    if (currentSpeed == 0) {
                        DEBUG_PRINTLN(F("=== HOMING Stage 0: Already at upper limit, moving down first ==="));
                    }
                    setMotorSpeed(-MOTOR_HOMING_DOWN_SPEED);
                    return;
            }
            return;
        }

        // Now do the normal upper limit finding
        if (upperLimit.isTriggered()) {
            motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
            currentSpeed = 0;

            // Store encoder position at upper limit (needed to calculate travel distance in Stage 1)
            homingUpperLimitPos = ENCODER_READ(encoder);
            upperLimitCalibrated = true;

            DEBUG_PRINTLN(F("=== HOMING Stage 0 Complete ==="));
            DEBUG_PRINT(F("  Encoder at upper limit: "));
            DEBUG_PRINTLN(homingUpperLimitPos);

            // Small delay to ensure motor fully stops before reversing direction
            delay(100);

            // Advance to stage 1
            homingStage = 1;
            homingNeedBackoff = false;
            homingBackedOff = false;
            lastPosition = ENCODER_READ(encoder);
            lastPositionTime = millis();
            return;
        }

        // Move up at homing speed
        setMotorSpeed(MOTOR_HOMING_SPEED);
        return;
    }

    // === Stage 1: Find Lower Limit (Final Home Position) ===
    if (homingStage == 1) {
        if (lowerLimit.isTriggered()) {
            motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
            currentSpeed = 0;

            // Read encoder position at lower limit (before reset)
            int32_t lowerLimitEncoderPos = ENCODER_READ(encoder);

            // Calculate travel distance from Stage 0 upper limit to current lower limit
            // NOTE: Motor direction may be inverted, so use absolute value
            int32_t travelDistance = abs(homingUpperLimitPos - lowerLimitEncoderPos);

            DEBUG_PRINTLN(F("=== HOMING Stage 1 Complete ==="));
            DEBUG_PRINT(F("  Encoder at lower limit (before reset): "));
            DEBUG_PRINTLN(lowerLimitEncoderPos);
            DEBUG_PRINT(F("  Upper limit was at: "));
            DEBUG_PRINTLN(homingUpperLimitPos);
            DEBUG_PRINT(F("  Travel distance: "));
            DEBUG_PRINTLN(travelDistance);

            // A broken lower wire reads "triggered" at once: stage 1 would
            // "find" the lower switch right below the top
            if (!motionprofile::travelPlausible(travelDistance)) {
                char msg[64];
                snprintf(msg, sizeof(msg), "Homing travel %ld too short - check lower switch wiring",
                         (long)travelDistance);
                enterErrorState(msg);
                return;
            }

            // Reset encoder to home position
            // Any encoder re-zero must also clear homingBackoffUnfinished (its
            // start is in the old frame); here the back-off already completed
            ENCODER_WRITE(encoder, HOME_OFFSET_COUNTS);

            // Verify encoder reset
            DEBUG_PRINT(F("  Encoder after reset: "));
            DEBUG_PRINTLN(ENCODER_READ(encoder));

            // Calculate soft limits based on travel distance
            // Lower limit = HOME_OFFSET_COUNTS + margin
            // Upper limit = HOME_OFFSET_COUNTS + travel distance - margin
            // (margin clamped to [SOFT_LIMIT_MARGIN_MIN, travel/2] for this travel)
            homedTravel = travelDistance;
            softLimitMargin = motionprofile::clampMargin(softLimitMargin, travelDistance);
            softLimitLower = HOME_OFFSET_COUNTS + softLimitMargin;
            softLimitUpper = HOME_OFFSET_COUNTS + travelDistance - softLimitMargin;
            lowerLimitCalibrated = true;
            homed = true;  // Fully homed!

            DEBUG_PRINT(F("  HOME_OFFSET_COUNTS: "));
            DEBUG_PRINTLN(HOME_OFFSET_COUNTS);
            DEBUG_PRINT(F("  softLimitMargin: "));
            DEBUG_PRINTLN(softLimitMargin);
            DEBUG_PRINT(F("  softLimitLower: "));
            DEBUG_PRINTLN(softLimitLower);
            DEBUG_PRINT(F("  softLimitUpper: "));
            DEBUG_PRINTLN(softLimitUpper);
            DEBUG_PRINT(F("  lowerLimitCalibrated: "));
            DEBUG_PRINTLN(lowerLimitCalibrated ? F("true") : F("false"));
            DEBUG_PRINT(F("  upperLimitCalibrated: "));
            DEBUG_PRINTLN(upperLimitCalibrated ? F("true") : F("false"));
            DEBUG_PRINT(F("  homed: "));
            DEBUG_PRINTLN(homed ? F("true") : F("false"));

            // Small delay to ensure motor fully stops before reversing direction
            delay(100);

            // Advance to stage 2: move up to display position (lower soft limit = just above bottom)
            homingStage = 2;
            targetPosition = softLimitLower;  // Move to lower soft limit (margin above bottom)
            lastPosition = ENCODER_READ(encoder);
            lastPositionTime = millis();

            DEBUG_PRINTLN(F("=== HOMING Stage 2: Moving to display position (lower margin) ==="));
            DEBUG_PRINT(F("  Target position: "));
            DEBUG_PRINTLN(targetPosition);
            return;
        }

        // Move down at slower homing speed (gravity-assisted, final approach)
        setMotorSpeed(-MOTOR_HOMING_DOWN_SPEED);
        return;
    }

    // === Stage 2: Move to Display Position (lower soft limit = margin above bottom) ===
    if (homingStage == 2) {
        int32_t currentPosition = ENCODER_READ(encoder);

        // Check if we've reached the target position
        if (currentPosition >= targetPosition) {
            motor.stop();
            currentSpeed = 0;

            DEBUG_PRINTLN(F("=== HOMING Stage 2 Complete ==="));
            DEBUG_PRINT(F("  Final position: "));
            DEBUG_PRINTLN(currentPosition);
            DEBUG_PRINTLN(F("=== HOMING COMPLETE ==="));

            finishHoming();
            return;
        }

        // Check for hardware upper limit (safety check)
        if (upperLimit.isTriggered()) {
            motor.stop();
            currentSpeed = 0;
            DEBUG_PRINTLN(F("Hardware upper limit reached during Stage 2"));
            finishHoming();
            return;
        }

        // Check stall
        if (checkStall()) {
            enterErrorState("Motor stalled during final positioning");
            return;
        }

        // Move up, slowing down to the approach speed near the target
        setMotorSpeed(motionprofile::approachSpeed(targetPosition - currentPosition, targetSpeed,
                                                   MOTOR_APPROACH_SPEED, DECEL_RAMP_COUNTS));
        return;
    }
}

void MotionController::cancelPendingMoves() {
    upAfterHoming = false;
    gotoAfterHoming = false;
}

void MotionController::finishHoming() {
    // Take the queued move before leaving HOMING (setState cancels it)
    bool continueUp = upAfterHoming;
    bool continueGoto = gotoAfterHoming;
    setState(IDLE);
    if (continueUp) {
        DEBUG_PRINTLN(F("Homed - continuing UP"));
        lastPosition = ENCODER_READ(encoder);
        lastPositionTime = millis();
        setState(MANUAL_UP);
    } else if (continueGoto) {
        DEBUG_PRINT(F("Homed - continuing GOTO "));
        DEBUG_PRINTLN(pendingTarget);
        if (!gotoPosition(pendingTarget)) {  // refused: stays IDLE, no move
            // Always printed (not a debug line): the goto's own reply said OK
            Console.print(F("ERROR "));
            Console.println(errorMessage);
        }
    }
}

void MotionController::handleMovingToTarget() {
    // Closed-loop position control

    // Get current position
    int32_t currentPosition = ENCODER_READ(encoder);
    int32_t error = targetPosition - currentPosition;

    // Check software limits based on movement direction
    int8_t direction = (error > 0) ? 1 : -1;
    if (homed && checkSoftLimits(direction)) {
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;
        setState(IDLE);
        return;
    }

    // Check limits
    if (checkLimits(moveDir)) {
        setState(IDLE);
        return;
    }

    // Check stall
    if (checkStall()) {
        enterErrorState("Motor stalled");
        return;
    }

    // Decelerate toward the target; 0 = within deadband of it, or past it
    uint8_t approach = (moveDir > 0) ? MOTOR_APPROACH_SPEED : MOTOR_APPROACH_SPEED_DOWN;
    int16_t motorSpeed = motionprofile::gotoSpeed(error, moveDir, targetSpeed, approach,
                                                  DECEL_RAMP_COUNTS, POSITION_DEADBAND);
    if (motorSpeed == 0) {
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;
        setState(IDLE);
        DEBUG_PRINTLN(F("Target reached"));
        return;
    }
    setMotorSpeed(motorSpeed);
}

void MotionController::handleManualUp() {
    // Manual upward movement

    // Check upper limit
    if (upperLimit.isTriggered()) {
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;
        // (No calibration here: homed is only ever set by the homing sequence)
        setState(IDLE);
        DEBUG_PRINTLN(F("Upper limit reached"));
        return;
    }

    // Check software limits (direction = +1 for UP)
    if (checkSoftLimits(1)) {  // only once homed
        DEBUG_PRINT(F("[MANUAL_UP] Soft limit triggered. Pos="));
        DEBUG_PRINT(ENCODER_READ(encoder));
        DEBUG_PRINT(F(" softLimitUpper="));
        DEBUG_PRINT(softLimitUpper);
        DEBUG_PRINT(F(" margin="));
        DEBUG_PRINTLN(softLimitMargin);
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;
        setState(IDLE);
        return;
    }

    // Check stall (skip if lower limit is pressed - moving away from limit)
    if (!lowerLimit.isTriggered() && checkStall()) {
        enterErrorState("Motor stalled");
        return;
    }

    // Move up at target speed, slowing down before the upper soft limit;
    // homing speed until homed (no soft limits yet)
    int32_t remaining = softLimitUpper - ENCODER_READ(encoder);
    setMotorSpeed(motionprofile::manualSpeed(SOFT_LIMIT_ENABLED && homed, remaining, targetSpeed,
                                             MOTOR_APPROACH_SPEED, DECEL_RAMP_COUNTS, MOTOR_HOMING_SPEED));
}

void MotionController::handleManualDown() {
    // Manual downward movement

    // Check lower limit
    if (lowerLimit.isTriggered()) {
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;

        // Not homed: zero the position at the switch (homing's reference)
        // but stay un-homed; only the full homing sequence measures the
        // travel and sets the upper soft limit
        if (!homed) {
            // Any encoder re-zero must also clear homingBackoffUnfinished (its
            // start is in the old frame); here an unfinished back-off means the
            // upper switch still reads triggered, so checkWiring() stops us first
            ENCODER_WRITE(encoder, HOME_OFFSET_COUNTS);  // Reset encoder to home position
            softLimitLower = HOME_OFFSET_COUNTS + softLimitMargin;
            lowerLimitCalibrated = true;
            DEBUG_PRINTLN(F("Lower limit: position zeroed, not homed"));
        }

        setState(IDLE);
        DEBUG_PRINTLN(F("Lower limit reached"));
        return;
    }

    // Check software limits (direction = -1 for DOWN)
    if (lowerLimitCalibrated && checkSoftLimits(-1)) {
        motor.stop();  // Stop motor (298:1 gearing provides natural holding resistance)
        currentSpeed = 0;
        setState(IDLE);
        return;
    }

    // Check stall
    if (checkStall()) {
        enterErrorState("Motor stalled");
        return;
    }

    // Move down at target speed, slowing down before the lower soft limit;
    // homing speed until homed (no soft limits yet)
    int32_t remaining = ENCODER_READ(encoder) - softLimitLower;
    setMotorSpeed(-motionprofile::manualSpeed(SOFT_LIMIT_ENABLED && homed, remaining, targetSpeed,
                                              MOTOR_APPROACH_SPEED_DOWN, DECEL_RAMP_COUNTS,
                                              MOTOR_HOMING_DOWN_SPEED));
}

void MotionController::handleStopped() {
    // Emergency stop state: stay stopped until the user resumes
    superviseRest();
}

void MotionController::handleError() {
    // Error state: stay stopped until reset
    superviseRest();
}

// ===== Safety Checks =====

bool MotionController::checkLimits(int8_t dir) {
    // Check hardware limits against the commanded direction, not the motor's
    // last speed (0 on a move's first tick, which would drive into the switch)
    if (motionprofile::limitBlocks(dir, upperLimit.isTriggered(), lowerLimit.isTriggered())) {
        DEBUG_PRINTLN(dir > 0 ? F("Upper limit triggered") : F("Lower limit triggered"));
        motor.stop();
        currentSpeed = 0;
        return true;
    }
    return false;
}

bool MotionController::checkStall() {
    // Detect stalled motor (no position change)
    int32_t currentPosition = ENCODER_READ(encoder);
    int32_t positionDelta = abs(currentPosition - lastPosition);
    uint32_t timeDelta = millis() - lastPositionTime;

    if (positionDelta < STALL_MIN_COUNTS) {
        if (timeDelta > STALL_THRESHOLD_MS) {
            DEBUG_PRINTLN(F("[STALL] Motor stalled!"));
            DEBUG_PRINT(F("  Current pos: "));
            DEBUG_PRINTLN(currentPosition);
            DEBUG_PRINT(F("  Last pos: "));
            DEBUG_PRINTLN(lastPosition);
            DEBUG_PRINT(F("  Position delta: "));
            DEBUG_PRINTLN(positionDelta);
            DEBUG_PRINT(F("  Time since last movement: "));
            DEBUG_PRINT(timeDelta);
            DEBUG_PRINTLN(F("ms"));
            DEBUG_PRINT(F("  STALL_MIN_COUNTS: "));
            DEBUG_PRINTLN(STALL_MIN_COUNTS);
            DEBUG_PRINT(F("  STALL_THRESHOLD_MS: "));
            DEBUG_PRINTLN(STALL_THRESHOLD_MS);
            return true;
        }
    } else {
        lastPosition = currentPosition;
        lastPositionTime = millis();
    }

    return false;
}

bool MotionController::checkSoftLimits(int8_t direction) {
    if (!SOFT_LIMIT_ENABLED || !homed) {
        return false;
    }

    int32_t currentPosition = ENCODER_READ(encoder);

    // The soft limits already include the margin (set at homing)

    // Only check lower limit when moving down (direction < 0) or both (direction == 0)
    if (direction <= 0) {
        if (currentPosition <= softLimitLower) {
            DEBUG_PRINTLN(F("Software lower limit reached"));
            return true;
        }
    }

    // Only check upper limit when moving up (direction > 0) or both (direction == 0)
    if (direction >= 0) {
        if (currentPosition >= softLimitUpper) {
            DEBUG_PRINTLN(F("Software upper limit reached"));
            return true;
        }
    }

    return false;
}

bool MotionController::checkWiring() {
    if (!motionprofile::wiringFault(upperLimit.isTriggered(), lowerLimit.isTriggered())) {
        return false;
    }
    enterErrorState("Both limit switches triggered - check switch wiring/GND");
    return true;
}

void MotionController::enterErrorState(const char* message) {
    cancelPendingMoves();
    homingNeedBackoff = false;
    homingBackedOff = false;
    strncpy(errorMessage, message, sizeof(errorMessage) - 1);
    errorMessage[sizeof(errorMessage) - 1] = '\0';
    motor.stop();
    currentSpeed = 0;

    // FAIL-SAFE: Turn off relay (holofan) on error
    failsafeHolofanOff();

    setState(ERROR);

    DEBUG_PRINT(F("ERROR: "));
    DEBUG_PRINTLN(message);
}

// ===== Helper Functions =====

void MotionController::setState(MotionState newState) {
    if (state != newState) {
        // Leaving HOMING any way but finishHoming() (a new move, stop, error)
        // drops the move queued behind it
        if (state == HOMING) {
            cancelPendingMoves();
        }
        previousState = state;
        state = newState;
        stateStartTime = millis();

        if (newState == IDLE || newState == STOPPED || newState == ERROR) {
            // Rest supervision: first stop re-send on the next tick
            restStopMs = millis() - MOTOR_REFRESH_MS;
            restPos = ENCODER_READ(encoder);
            restUpper = upperLimit.isTriggered();
            restLower = lowerLimit.isTriggered();
        }

        DEBUG_PRINT(F("State: "));
        DEBUG_PRINT(stateToString(previousState));
        DEBUG_PRINT(F(" -> "));
        DEBUG_PRINTLN(stateToString(state));
    }
}

void MotionController::setMotorSpeed(int16_t speed) {
    // Reversing (a new opposite goto/down during a move, homing started while
    // moving down, ...): stop and let the motor settle first
    uint32_t pause = motionprofile::reversalPauseMs(lastDriveDir, millis() - lastDriveMs, speed,
                                                    REVERSAL_PAUSE_MS);
    if (pause > 0) {
        motor.stop();
        currentSpeed = 0;
        DEBUG_PRINTLN(F("Direction reversal - pausing"));
        delay(pause);
        lastPosition = ENCODER_READ(encoder);  // the pause is not a stall
        lastPositionTime = millis();
        // Don't drive yet: the next tick re-checks the stop flag (mesh #HLS),
        // the limits and the state before driving the other way
        return;
    }
    motor.setSpeed(speed);
    currentSpeed = abs(speed);
    if (speed != 0) {
        lastDriveDir = (speed > 0) ? 1 : -1;
        lastDriveMs = millis();
    }
}

const char* MotionController::stateToString(MotionState state) const {
    switch (state) {
        case IDLE: return "IDLE";
        case HOMING: return "HOMING";
        case MOVING_TO_TARGET: return "MOVING_TO_TARGET";
        case MANUAL_UP: return "MANUAL_UP";
        case MANUAL_DOWN: return "MANUAL_DOWN";
        case STOPPED: return "STOPPED";
        case ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}
