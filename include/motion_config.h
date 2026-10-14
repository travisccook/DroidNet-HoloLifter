// Motion constants shared by the firmware (via config.h) and the native
// tests of lib/MotionProfile. Plain #defines only: no Arduino headers.
#ifndef MOTION_CONFIG_H
#define MOTION_CONFIG_H

// Speeds (0-255)
#define MOTOR_MAX_SPEED      255         // Maximum motor speed
#define MOTOR_HOMING_SPEED   80          // Homing speed upward (Pololu 5172: slower for control)
#define MOTOR_HOMING_DOWN_SPEED 60       // Homing speed downward (gravity-assisted)

// Deceleration profile (lib/MotionProfile) for gotos and for manual moves
// toward a soft limit. The motor cannot brake (SCMD has no brake mode), so
// the lift slows down before the stop point instead of coasting into it.
#define MOTOR_APPROACH_SPEED 50          // Final-approach speed up; uphill stalls below ~25 at 5V
#define MOTOR_APPROACH_SPEED_DOWN 30     // Final-approach speed down (gravity-assisted; 50 overshot by up to 16)
#define DECEL_RAMP_COUNTS    300         // Distance over which speed ramps from 255 down to MOTOR_APPROACH_SPEED
#define POSITION_DEADBAND    10          // Counts within target = "at position"

// Soft limits
#define SOFT_LIMIT_MARGIN    40          // Stop this many counts short of each limit switch (applied once since config v2)
#define SOFT_LIMIT_MARGIN_MIN 40         // Smallest margin accepted (#HLM, flash); largest is travel/2
#define SOFT_LIMIT_MARGIN_MAX_UNHOMED 500  // Largest margin accepted before homing (travel unknown)

// Motor supervision: SCMD I2C writes are not checked, so re-send them
#define MOTOR_REFRESH_MS     150         // Re-send the drive (moving) or the stop (at rest) this often
#define REST_DRIFT_COUNTS    5           // At rest, re-send stop at once if the encoder moves more than this

// Direction reversal: stop and wait this long after the last opposite drive
#define REVERSAL_PAUSE_MS    100

// Homing
#define HOMING_BACKOFF_MAX_COUNTS 150    // Max travel down to get off the upper switch before homing calls it a fault

#endif
