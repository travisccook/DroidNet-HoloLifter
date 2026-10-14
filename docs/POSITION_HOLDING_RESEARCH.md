# Position Holding Research

**Date:** 2025-11-19
**Issue:** Platform sinks when motor stops after adding weight
**Status:** Pending hardware test with different geared motor

## Problem Statement

After adding weight to the holographic platform, the system exhibits platform sinking when the motor stops. Initial motor speed (80) was also insufficient for upward movement under load.

## Changes Made

### 1. Increased Homing Speed
**File:** `include/config.h`
```cpp
#define MOTOR_HOMING_SPEED   120         // Increased from 80 to 120
```

**Rationale:** Original speed of 80 was insufficient to lift platform with added weight.

### 2. Implemented brake() Function
**Files:** `lib/MotorControl/MotorControl.h`, `lib/MotorControl/MotorControl.cpp`

**Initial Implementation (DANGEROUS - FIXED):**
```cpp
void MotorControl::brake() {
    currentSpeed = 0;
    motorDriver.setDrive(0, 0, 255);  // WRONG - drives motor DOWN at full speed!
}
```

**Critical Failure:** This caused motor to slam platform downward at full speed, resulting in:
- Motor overheating
- Platform damage risk
- Uncontrolled descent
- Emergency stop commands ignored during initial failure

**Root Cause Analysis:**
- `setDrive(channel, direction, level)` parameters misunderstood
- `setDrive(0, 0, 255)` = Channel 0, Direction 0 (DOWN), Level 255 (FULL POWER)
- Assumed level=255 would create braking, but it actually drove motor at full speed downward

**Current Implementation (SAFE):**
```cpp
void MotorControl::brake() {
    if (!initialized) {
        return;
    }

    currentSpeed = 0;
    // For now, brake is same as stop - SCMD doesn't have active brake
    // To hold position against gravity, we'd need to apply controlled power
    motorDriver.setDrive(0, 0, 0);  // Stop motor (coast)

    #if DEBUG_MOTOR
    DEBUG_PRINTLN(F("Motor brake engaged (coast)"));
    #endif
}
```

**Current Behavior:** `brake()` performs identical function to `stop()` - both result in coast/freewheel mode.

### 3. Added Homing Stage Delay
**File:** `lib/MotionController/MotionController.cpp`

```cpp
// === Stage 0: Find Upper Limit ===
if (homingStage == 0) {
    if (upperLimit.isTriggered()) {
        motor.brake();
        currentSpeed = 0;

        // Calibrate upper limit
        softLimitUpper = encoder.read() - SOFT_LIMIT_MARGIN;
        upperLimitCalibrated = true;

        DEBUG_PRINTLN(F("Stage 0 complete: Upper limit found"));

        // Small delay to ensure motor fully stops before reversing direction
        delay(100);  // ADDED - prevents immediate reversal

        homingStage = 1;
        return;
    }

    setMotorSpeed(MOTOR_HOMING_SPEED);
}
```

**Rationale:** Without delay, motor could attempt direction reversal before fully stopping, causing mechanical stress.

## Research: R2UppitySpinner Analysis

**Repository:** https://github.com/reeltwo/R2UppitySpinner
**Source:** https://github.com/reeltwo/R2UppitySpinner

### Key Findings

#### Hardware Differences
```
    Component           UppitySpinner                          HoloLifter
    ------------------  -------------------------------------  ----------------------------
    Motor Driver        TB6612FNG (GPIO control)               SCMD w/ TB6612FNG (I2C)
    Control Interface   Direct PWM (analogWrite())             I2C commands (setDrive())
    Pin Control         3 pins per motor (EN, D2, PWM1, PWM2)  I2C bus only
    Current Sensing     Yes (analog feedback)                  No (SCMD limitation)
    Encoder Resolution  Custom ISR (1x, single-edge)           Encoder.h library (4x, quad)
```

#### Position Holding Mechanism

**UppitySpinner's `lifterMotorStop()` function:**
```cpp
static void lifterMotorStop()
{
    enableMotors();              // Keep driver enabled
    analogWrite(PIN_M1PWM1, 0);  // Both PWM outputs LOW
    analogWrite(PIN_M1PWM2, 0);
    digitalWrite(PIN_M1D2, HIGH); // Normal mode
    fLifterThrottle = 0;
}
```

**What this does:**
1. Motor driver remains enabled
2. Both PWM signals set to 0 (LOW)
3. Direction pin set HIGH (normal mode)
4. **Result:** TB6612FNG enters **short-brake mode**

**Short-Brake Mode Explained:**
- Both motor terminals connected to GND through low-side FETs
- Motor acts as generator when trying to rotate
- Generated current flows through H-bridge, creating electromagnetic resistance
- Very low power consumption (~mA, not continuous motor current)
- Provides position holding for lightweight mechanisms

**TB6612FNG Truth Table (from datasheet):**
```
    IN1  IN2  PWM  OUT1  OUT2  Mode
    ---  ---  ---  ----  ----  ------------
    H    L    H    H     L     CW
    L    H    H    L     H     CCW
    L    L    H    L     L     Short brake
    H/L  H/L  L    OFF   OFF   Stop (coast)
```

**UppitySpinner Pin Mapping:**
- `PIN_M1D2` = IN1/IN2 direction control
- `PIN_M1PWM1`, `PIN_M1PWM2` = Dual PWM inputs
- With both PWM=0 and driver enabled → Short brake mode

### Critical Question: Does SCMD Support Short-Brake?

**TB6612FNG chip capabilities:**
- ✅ Supports short-brake mode (built into H-bridge IC)
- ✅ Low power consumption in brake mode
- ✅ Sufficient for lightweight periscope mechanisms

**SCMD library limitations:**
- ❌ I2C interface may not expose short-brake mode
- ❌ `setDrive(channel, direction, 0)` likely maps to coast, not brake
- ❓ Unknown if SCMD firmware supports brake command via I2C

**Research needed:**
1. Review SCMD library source code for brake mode support
2. Check SCMD firmware documentation for I2C brake command
3. Test if specific `setDrive()` parameter combination enables brake

## Motor Driver Comparison

### Direct GPIO Control (UppitySpinner)
**Advantages:**
- Full control over H-bridge states
- Can access all TB6612FNG modes (CW, CCW, brake, coast)
- Instant response (no I2C latency)
- Current sensing via analog pin
- Can implement custom PWM frequencies

**Disadvantages:**
- Requires 3+ GPIO pins per motor
- More complex firmware
- Must handle PWM generation in code
- More wiring complexity

### I2C SCMD (HoloLifter)
**Advantages:**
- Only 2 pins needed (I2C SDA/SCL via Qwiic)
- Clean firmware interface
- SparkFun library handles low-level details
- Qwiic connector = plug-and-play
- Isolated control logic from power circuit

**Disadvantages:**
- Limited to commands exposed by SCMD firmware
- May not support all TB6612FNG modes
- I2C latency (typically <1ms, but not instant)
- No direct current sensing
- Cannot customize PWM frequency

## Position Holding Solutions

### Option 1: Hardware Brake Mode (Preferred if Available)
**Implementation:** Use SCMD brake command if available
```cpp
void MotorControl::brake() {
    if (!initialized) return;

    currentSpeed = 0;
    // TODO: Research if SCMD supports brake mode via I2C
    // Potential commands to try:
    // - motorDriver.brake(0);  // If method exists
    // - Special setDrive() parameter combination
    // - Direct I2C register write (if documented)
    motorDriver.setDrive(0, 0, 0);  // Current: coast mode
}
```

**Pros:**
- Low power consumption
- Efficient electromagnetic resistance
- No software overhead
- Matches UppitySpinner approach

**Cons:**
- Unknown if SCMD supports this
- May require SCMD firmware modification
- Might not be strong enough for heavier loads

### Option 2: Active Power Holding (Software Solution)
**Implementation:** Apply continuous low-level upward power
```cpp
void MotorControl::brake() {
    if (!initialized) return;

    currentSpeed = 0;
    // Apply small upward force to counter gravity
    motorDriver.setDrive(0, 1, HOLDING_POWER);  // e.g., 20-40 PWM
}

// In config.h:
#define HOLDING_POWER 30  // Enough to counter gravity, not move platform
```

**Pros:**
- Guaranteed to work with SCMD
- Power level tunable for different loads
- Can compensate for varying weight

**Cons:**
- Motor runs continuously (wastes power)
- Motor heating over time
- Must calibrate holding power for load
- Higher power consumption than electromagnetic brake

### Option 3: Position Feedback Hold (Smart Software Solution)
**Implementation:** Monitor encoder, apply correction when sinking detected
```cpp
void MotionController::handleIdle() {
    if (!homed) return;

    // Check if platform has sunk below target
    int32_t currentPos = encoder.read();
    int32_t drift = targetPosition - currentPos;

    if (abs(drift) > POSITION_HOLD_DEADBAND) {
        // Platform sinking - apply brief correction
        int16_t correctionSpeed = constrain(drift * HOLD_KP, -50, 50);
        setMotorSpeed(correctionSpeed);
    } else {
        motor.stop();  // Within deadband - coast
    }
}

// In config.h:
#define POSITION_HOLD_DEADBAND 20    // Encoder counts tolerance
#define HOLD_KP 0.5                  // Proportional gain for corrections
```

**Pros:**
- Energy efficient (motor only runs when needed)
- Self-adjusting for different loads
- Prevents motor overheating
- More precise than constant power

**Cons:**
- More complex implementation
- Requires tuning (deadband, gain)
- Platform may "hunt" around target
- Small continuous oscillations possible

### Option 4: Mechanical Solution (Hardware Modification)
**Implementation:** Use self-locking lead screw or add counterweight

**Self-Locking Lead Screw Options:**
- Worm gear drive (typically self-locking)
- ACME thread with high pitch angle
- Trapezoidal thread with fine pitch

**Counterweight/Spring Options:**
- Gas spring to offset platform weight
- Coil spring pre-loaded for balance
- Counterweight pulley system

**Pros:**
- No power consumption for holding
- Most reliable solution
- Eliminates software complexity
- Best for heavy loads

**Cons:**
- Requires hardware purchase/modification
- May change mechanical dynamics
- Installation complexity
- May affect speed/acceleration

## Decision Path

### User's Current Plan (2025-11-19)
**Action:** Test with differently geared motor tomorrow
- Different gear ratio may provide more holding friction
- Higher gear reduction = more back-drive resistance
- May eliminate sinking without code changes

### If Platform Still Sinks After Motor Change

**Recommended approach:**
1. **First:** Research SCMD brake mode support
   - Review SparkFun SCMD library source
   - Check for undocumented I2C commands
   - Test parameter combinations

2. **If brake unavailable:** Implement Option 3 (Position Feedback Hold)
   - Most efficient software solution
   - Better than constant power
   - Adjusts to load automatically

3. **If still insufficient:** Consider mechanical solution (Option 4)
   - Self-locking lead screw
   - Or counterweight system

## Lessons Learned

### Critical Safety Lesson
**NEVER guess motor control parameters without documentation verification.**

The dangerous `setDrive(0, 0, 255)` implementation demonstrated the critical importance of:
1. Reading hardware documentation BEFORE implementing
2. Understanding parameter meanings precisely
3. Testing motor control changes in safe conditions
4. Having emergency stop mechanisms in place

**User feedback:** "This was a dangerous thing to guess on!"
**Response:** Absolutely correct. Motor control commands must ALWAYS be verified against documentation first.

### Development Process Improvements
1. Always verify I2C command parameters against library documentation
2. Test motor commands at low speeds before full operation
3. Implement emergency stop as first feature, not last
4. Add timeout limits to all motor operations
5. Document assumptions clearly for future reference

## References

- **R2UppitySpinner:** https://github.com/reeltwo/R2UppitySpinner
- **Source:** https://github.com/reeltwo/R2UppitySpinner
- **SparkFun SCMD Library:** https://github.com/sparkfun/SparkFun_Serial_Controlled_Motor_Driver_Arduino_Library
- **TB6612FNG Datasheet:** [Toshiba TB6612FNG Dual Motor Driver]
- **SCMD Hookup Guide:** https://learn.sparkfun.com/tutorials/serial-controlled-motor-driver-hookup-guide

## Next Steps

1. ✅ Document research findings (this file)
2. ⏳ Test with new geared motor (user's plan for tomorrow)
3. ⏳ Evaluate if sinking issue persists after motor change
4. ⏳ If needed, research SCMD brake mode support
5. ⏳ If needed, implement position feedback hold (Option 3)

## Test Results (To Be Updated)

### Test 1: Different Geared Motor
**Date:** [Pending]
**Motor Specs:** [To be documented]
**Gear Ratio:** [To be documented]
**Results:** [To be documented]

---

**Document Status:** Research complete, awaiting hardware test results.
