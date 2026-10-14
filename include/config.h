/**
 * HoloLifter - Configuration Header
 *
 * Central configuration for all pin definitions, constants, and feature flags.
 * This file should be included by all modules that need hardware or timing constants.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include "motion_config.h"  // motion constants (also used by the native tests)

// ===== Firmware Version =====
#define HOLOLIFTER_VERSION "1.0.0"
#define HOLOLIFTER_DATE    "2025-11-17"

// ===== Hardware Pin Assignments =====

// I2C Motor Driver (SparkFun Qwiic SCMD)
#define MOTOR_I2C_ADDR       0x5D        // Motor driver I2C address
#define I2C_SDA_PIN          23          // GPIO 23 (Qwiic SDA, hardware I2C0)
#define I2C_SCL_PIN          22          // GPIO 22 (Qwiic SCL, hardware I2C0)

// Quadrature Encoder (safe ESP32 pins, interrupt-capable)
#define ENCODER_PIN_A        32          // GPIO 32 (Encoder Channel A)
#define ENCODER_PIN_B        33          // GPIO 33 (Encoder Channel B)

// Limit Switches (Normally Closed configuration)
#define UPPER_LIMIT_PIN      14          // GPIO 14 (Upper limit switch, safe pin)
#define LOWER_LIMIT_PIN      27          // GPIO 27 (Lower limit switch, safe pin)

// Holofan Control (Phase 6)
// Board labels: A5 = GPIO 4, A4 = GPIO 36 (input-only!)
#define RELAY_PIN            4           // GPIO 4 / A5 (3.3V Relay module IN)


// ===== Motor Control Constants =====

// Speed limits (0-255)
#define MOTOR_DEFAULT_SPEED  128         // Default speed for manual control

// Direction convention
#define MOTOR_DIR_UP         1           // Positive = upward movement
#define MOTOR_DIR_DOWN       -1          // Negative = downward movement
#define MOTOR_DIR_STOP       0           // Zero = stopped

// ===== Position Control Constants =====

// Encoder configuration (Pololu 5172: 298:1 Micro Metal Gearmotor HP 6V)
#define ENCODER_CPR_MOTOR    12          // Counts per revolution at motor shaft
#define GEARBOX_RATIO        297.92f     // Gearbox ratio (298:1 nominal, 297.92:1 actual)
#define ENCODER_CPR_OUTPUT   (ENCODER_CPR_MOTOR * GEARBOX_RATIO * 4)  // ~14,300 CPR (quadrature)

// Position control parameters

// Stall detection
#define STALL_THRESHOLD_MS   500         // Time without position change = stalled (0.5 seconds)
#define STALL_MIN_COUNTS     2           // Minimum counts to register movement

// ===== Limit Switch Configuration =====

// Debouncing
#define LIMIT_DEBOUNCE_MS    20          // Debounce time for limit switches

// Logic for NC (Normally Closed) fail-safe wiring:
// Wiring: COM → GPIO pin (with INPUT_PULLUP), NC → GND
// LOW  = not triggered (NC closed to GND)
// HIGH = triggered OR wire broken (pull-up active) - FAIL-SAFE!
#define LIMIT_TRIGGERED      HIGH
#define LIMIT_NOT_TRIGGERED  LOW

// ===== Serial Communication =====

// Baud rates
#define SERIAL_BAUD          115200      // USB serial baud rate (debug)

// Command buffer
#define CMD_BUFFER_SIZE      128         // Maximum console line (fits "mesh pass 1 <63-char passphrase>")
#define CMD_MAX_TOKENS       3           // Maximum command arguments (command + 2 args max)
#define CMD_MAX_TOKEN_LEN    16          // Maximum length of each token (longest: "resetconfig" = 11 chars)

// ===== Homing Configuration =====

// Homing behavior
#define AUTO_HOME_ON_FIRST_MOVE  true    // Auto-home on first movement command (not on power-up)
#define HOME_TO_LOWER_LIMIT      true    // Home to lower limit (vs upper)
#define HOME_OFFSET_COUNTS       0       // Offset from limit after homing

// Homing sequence parameters
#define HOME_BACKUP_COUNTS   100         // Counts to back off after hitting limit
#define HOME_BACKUP_SPEED    40          // Speed for backing off limit

// ===== EEPROM Configuration =====

// EEPROM/Flash addresses
#define EEPROM_MAGIC_ADDR    0x00        // Magic byte address
#define EEPROM_MAGIC_VALUE   0xA5        // Signature for valid config

#define EEPROM_CONFIG_ADDR   0x10        // Configuration structure address
#define CONFIG_VERSION       2           // v2: margin applied once (v1 applied it twice; loadConfig doubles a v1 margin)
#define EEPROM_CALIB_ADDR    0x20        // Calibration data address

// ===== Safety Parameters =====

// Software position limits (counts)
// These are set after homing and provide software bounds checking
#define SOFT_LIMIT_ENABLED   true        // Enable software limits

// Watchdog (optional - AVR watchdog available)
#define WATCHDOG_ENABLED     false       // Enable watchdog timer
#define WATCHDOG_TIMEOUT_MS  2000        // Watchdog timeout

// ===== Debug and Diagnostics =====

// Debug output
#define DEBUG_ENABLED        true        // Enable debug serial output
#define DEBUG_MOTOR          true        // Debug motor control
#define DEBUG_ENCODER        true        // Debug encoder counts
#define DEBUG_LIMITS         false       // Debug limit switches
#define DEBUG_COMMANDS       true        // Debug command parsing

// Status reporting interval
#define STATUS_REPORT_MS     0           // Auto-report status interval (0 = disabled)

// ===== Holofan Control (Phase 6) =====

// Relay Control (Power On/Off - PRIMARY)
#define RELAY_ACTIVE_HIGH    true        // Relay activates on HIGH signal
#define HOLOFAN_ON           (RELAY_ACTIVE_HIGH ? HIGH : LOW)
#define HOLOFAN_OFF          (RELAY_ACTIVE_HIGH ? LOW : HIGH)

// Holofan playback is controlled over the WCB mesh by the fan's own firmware.
// The old IR codes are kept for reference in docs/HOLOFAN_IR_CODES.md.

// ===== Timing Constants =====

// Loop timing
#define LOOP_RATE_MS         10          // Target loop rate (100 Hz)

// Timeout values
#define MOVEMENT_TIMEOUT_MS  30000       // Max time for any movement (30 sec)
#define HOMING_TIMEOUT_MS    15000       // Max time for homing sequence (15 sec)

// ===== Helper Macros =====

// Debug printing goes to Console: USB, mirrored to the mesh as RTERM lines
// (defined in src/HoloMesh.cpp).
class Print;
extern Print& Console;
#if DEBUG_ENABLED
  #define DEBUG_PRINT(x)     Console.print(x)
  #define DEBUG_PRINTLN(x)   Console.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Constrain and map helpers
#define CONSTRAIN_SPEED(x)   constrain((x), -MOTOR_MAX_SPEED, MOTOR_MAX_SPEED)
#define ABS_SPEED(x)         abs(x)
#define SIGN(x)              ((x) > 0 ? 1 : ((x) < 0 ? -1 : 0))

// ===== Configuration Structure (for EEPROM) =====

struct SystemConfig {
  uint8_t magicByte;                     // Signature byte (0xA5)
  uint8_t version;                       // Config version
  uint8_t defaultSpeed;                  // Default movement speed
  uint8_t homingSpeed;                   // Homing sequence speed
  int32_t softLimitLower;                // Software lower limit (counts)
  int32_t softLimitUpper;                // Software upper limit (counts)
  int32_t softLimitMargin;               // Soft limit margin from hard limits (counts)
  bool autoHome;                         // Auto-home on first movement
  uint8_t reserved[16];                  // Reserved for future use
};

#endif // CONFIG_H
