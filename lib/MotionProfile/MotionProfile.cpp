#include "MotionProfile.h"
#include "motion_config.h"

namespace motionprofile {

uint8_t approachSpeed(int32_t remaining, uint8_t cruise, uint8_t floor, int32_t rampCounts) {
  if (remaining <= 0) return 0;
  if (rampCounts <= 0 || remaining >= rampCounts) return cruise;
  // The ramp depends on distance only (how hard the lift can slow down);
  // cruise just caps it, so a slow move enters the ramp later.
  int32_t s = floor + (int32_t)(255 - floor) * remaining / rampCounts;
  return (uint8_t)(s < cruise ? s : cruise);
}

int16_t gotoSpeed(int32_t error, int8_t dir, uint8_t cruise, uint8_t floor,
                  int32_t rampCounts, int32_t deadband) {
  int32_t ahead = (dir < 0) ? -error : error;   // counts still to go in the move's direction
  if (ahead <= deadband) return 0;              // arrived, or crossed the target: never hunt back
  int16_t s = approachSpeed(ahead, cruise, floor, rampCounts);
  return (dir < 0) ? -s : s;
}

uint8_t manualSpeed(bool homed, int32_t remaining, uint8_t cruise, uint8_t floor,
                    int32_t rampCounts, uint8_t unhomedSpeed) {
  if (!homed) return (cruise < unhomedSpeed) ? cruise : unhomedSpeed;
  return approachSpeed(remaining, cruise, floor, rampCounts);
}

int32_t marginFromStored(uint8_t version, int32_t margin) {
  return (version == 1) ? margin * 2 : margin;
}

bool homeBeforeManual(int8_t dir, bool homed) {
  return dir > 0 && !homed;
}

bool targetInRange(int32_t target, int32_t lower, int32_t upper) {
  return lower <= target && target <= upper;
}

bool wiringFault(bool upperTriggered, bool lowerTriggered) {
  return upperTriggered && lowerTriggered;
}

Backoff backoffStep(bool upperTriggered, bool lowerTriggered, int32_t travelled, int32_t maxCounts) {
  if (lowerTriggered) return Backoff::FAULT_LOWER;   // never drive into the lower end stop
  if (!upperTriggered) return Backoff::DONE;
  if (travelled < 0) travelled = -travelled;          // distance, whatever the encoder's sign
  if (travelled >= maxCounts) return Backoff::FAULT_DISTANCE;
  return Backoff::CONTINUE;
}

bool driveWriteDue(int16_t lastWritten, int16_t next, uint32_t sinceWriteMs, uint32_t refreshMs) {
  return next != lastWritten || sinceWriteMs >= refreshMs;
}

bool restStopDue(uint32_t sinceStopMs, int32_t drift, bool limitRose, uint32_t refreshMs, int32_t driftCounts) {
  if (limitRose) return true;
  if (drift > driftCounts || drift < -driftCounts) return true;
  return sinceStopMs >= refreshMs;
}

// Largest margin: travel/2 once homed, a fixed bound before
static int32_t marginMax(int32_t travel) {
  return (travel > 0) ? travel / 2 : SOFT_LIMIT_MARGIN_MAX_UNHOMED;
}

bool marginInRange(int32_t margin, int32_t travel) {
  return margin >= SOFT_LIMIT_MARGIN_MIN && margin <= marginMax(travel);
}

int32_t clampMargin(int32_t margin, int32_t travel) {
  if (margin < SOFT_LIMIT_MARGIN_MIN) return SOFT_LIMIT_MARGIN_MIN;
  if (travel > 0 && margin > travel / 2) return travel / 2;  // no fixed cap: homing clamps
  return margin;
}

bool limitBlocks(int8_t dir, bool upperTriggered, bool lowerTriggered) {
  return (dir > 0 && upperTriggered) || (dir < 0 && lowerTriggered);
}

uint32_t reversalPauseMs(int8_t lastDir, uint32_t sinceLastDriveMs, int16_t next, uint32_t pauseMs) {
  if (next == 0 || lastDir == 0) return 0;
  int8_t nextDir = (next > 0) ? 1 : -1;
  if (nextDir == lastDir) return 0;
  return (sinceLastDriveMs >= pauseMs) ? 0 : pauseMs - sinceLastDriveMs;
}

uint8_t clampSpeed(int32_t speed) {
  if (speed < MOTOR_APPROACH_SPEED) return MOTOR_APPROACH_SPEED;
  if (speed > MOTOR_MAX_SPEED) return MOTOR_MAX_SPEED;
  return (uint8_t)speed;
}

int32_t backoffStart(bool unfinished, int32_t storedStart, int32_t position) {
  return unfinished ? storedStart : position;
}

bool travelPlausible(int32_t travel) {
  return travel >= 2 * SOFT_LIMIT_MARGIN_MIN;
}

}  // namespace motionprofile
