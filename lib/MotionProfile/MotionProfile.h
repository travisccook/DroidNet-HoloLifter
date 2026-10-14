// Speed profile for lift moves: decelerate before a stop point. Pure C++,
// tested in the native env; MotionController applies it to the motor.
#ifndef MOTION_PROFILE_H
#define MOTION_PROFILE_H
#include <stdint.h>

namespace motionprofile {

// Speed (0-255) with `remaining` counts left before a stop point: 0 at or
// past it; otherwise floor + (255 - floor) * remaining / rampCounts, never
// above cruise. floor must keep the lift moving (uphill) so the final
// approach never trips the stall detector. rampCounts <= 0: no ramp.
uint8_t approachSpeed(int32_t remaining, uint8_t cruise, uint8_t floor, int32_t rampCounts);
// Signed motor speed for a goto. error = target - position; dir = sign of
// the error when the move started. 0 once within deadband of the target or
// past it (the lift never reverses to hunt for it).
int16_t gotoSpeed(int32_t error, int8_t dir, uint8_t cruise, uint8_t floor,
                  int32_t rampCounts, int32_t deadband);

// Speed for a manual up/down move. Not homed (no soft limits yet): the
// safe speed, or cruise if lower. Homed: approachSpeed toward the soft limit.
uint8_t manualSpeed(bool homed, int32_t remaining, uint8_t cruise, uint8_t floor,
                    int32_t rampCounts, uint8_t unhomedSpeed);

// Soft-limit margin as stored in config `version`. Version 1 applied the
// margin twice, so its clearance was 2x the stored value; keep that clearance.
int32_t marginFromStored(uint8_t version, int32_t margin);

// Whether a manual move (dir +1 up, -1 down) must run homing first. Up
// before homing homes first (something may block the way up); down does not
// (it runs at the homing-down speed onto the lower switch).
bool homeBeforeManual(int8_t dir, bool homed);

// Whether a goto target lies within the soft limits (inclusive).
bool targetInRange(int32_t target, int32_t lower, int32_t upper);

// Both limit switches triggered at once: the NC switches read triggered when
// a wire breaks or the common GND is lost, so this is a wiring fault.
bool wiringFault(bool upperTriggered, bool lowerTriggered);

// Homing stage 0 back-off (started on the upper switch, driving down to get
// off it). travelled = counts moved since the back-off started (either sign).
enum class Backoff : uint8_t { CONTINUE, DONE, FAULT_LOWER, FAULT_DISTANCE };
Backoff backoffStep(bool upperTriggered, bool lowerTriggered, int32_t travelled, int32_t maxCounts);

// Whether to write the motor drive: on any change, or to refresh an
// unchanged one after refreshMs (a lost I2C write is otherwise never fixed).
bool driveWriteDue(int16_t lastWritten, int16_t next, uint32_t sinceWriteMs, uint32_t refreshMs);

// At rest (IDLE/STOPPED/ERROR): whether to re-send stop. Every refreshMs,
// and at once if the encoder drifted more than driftCounts since the last
// stop or a limit switch just triggered.
bool restStopDue(uint32_t sinceStopMs, int32_t drift, bool limitRose, uint32_t refreshMs, int32_t driftCounts);

// Soft-limit margin range for #HLM: [SOFT_LIMIT_MARGIN_MIN, travel/2];
// travel <= 0 (not homed yet): [SOFT_LIMIT_MARGIN_MIN, SOFT_LIMIT_MARGIN_MAX_UNHOMED].
bool marginInRange(int32_t margin, int32_t travel);
// Clamp a stored/applied margin: to the minimum, and to travel/2 when the
// travel is known (> 0). No fixed cap before homing: a homed #HLM700 must
// survive a reboot; homing then clamps it to the measured travel.
int32_t clampMargin(int32_t margin, int32_t travel);

// Whether a limit switch blocks a move in commanded direction dir (+1 up,
// -1 down): the upper switch blocks up, the lower switch blocks down.
bool limitBlocks(int8_t dir, bool upperTriggered, bool lowerTriggered);

// Pause (ms) to hold the motor stopped before driving at `next` when the
// last non-zero drive was in the other direction (lastDir +1/-1, 0 = none)
// sinceLastDriveMs ago: what is left of pauseMs, or 0 (no reversal).
uint32_t reversalPauseMs(int8_t lastDir, uint32_t sinceLastDriveMs, int16_t next, uint32_t pauseMs);

// Set speed range: [MOTOR_APPROACH_SPEED, MOTOR_MAX_SPEED] (lower stalls uphill).
uint8_t clampSpeed(int32_t speed);

// Where a new back-off is measured from: the earlier start while a
// back-off since power-up has not yet seen the upper switch release
// (so repeated homing cannot restart the distance cap), else here.
int32_t backoffStart(bool unfinished, int32_t storedStart, int32_t position);

// Whether a travel measured by homing is believable: at least room for
// the minimum margin at both ends (2 x SOFT_LIMIT_MARGIN_MIN).
bool travelPlausible(int32_t travel);

}  // namespace motionprofile

#endif
