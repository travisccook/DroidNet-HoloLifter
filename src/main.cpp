/**
 * HoloLifter Main Application
 *
 * Motorized platform lifter control system for astromech droid holographic platform.
 *
 * Hardware:
 *   - SparkFun ESP32 Thing Plus (Micro-B USB) - Dual-core ESP32 @ 240MHz, 3.3V logic
 *   - SparkFun Qwiic Motor Driver (SCMD) with Qwiic I2C
 *   - Pololu 5172 Micro Metal Gearmotor 298:1 with Encoder
 *   - NC limit switches (upper and lower)
 *   - 3.3V relay module for holofan power control
 *
 * Features:
 *   - Closed-loop position control with encoder feedback
 *   - Manual homing required before movement
 *   - Safety limit switches with fail-safe design
 *   - Serial command interface with simple text responses
 *   - Deceleration before soft limits and goto targets
 *
 * Author: HoloLifter Project
 * Version: see HOLOLIFTER_VERSION in include/config.h
 */

#include <Arduino.h>
#include <Wire.h>
#include <ESP32Encoder.h>
#include "config.h"
#include "MotorControl.h"
#include "LimitSwitch.h"
#include "CommandParser.h"
#include "MotionController.h"
#include "MotionProfile.h"
#include "HoloMesh.h"

// ===== Global Objects =====

MotorControl motor(MOTOR_I2C_ADDR);
LimitSwitch upperLimit(UPPER_LIMIT_PIN, LIMIT_DEBOUNCE_MS);
LimitSwitch lowerLimit(LOWER_LIMIT_PIN, LIMIT_DEBOUNCE_MS);
ESP32Encoder encoder;  // ESP32-optimized encoder library
CommandParser cmdParser;
MotionController motion(motor, encoder, upperLimit, lowerLimit);

// ===== Function Prototypes =====

void processCommand();
void printHelp();
void sendStatus();
void sendLimits();
void sendEncoderInfo();
void printBanner();
void setHolofanPower(bool on);

// ===== Setup =====

void setup() {
    // Stop the motor first: after an ESP32-only reset (watchdog, OTA, USB
    // open) the SCMD may still be driving it
    motor.earlyStop();

    // Initialize serial
    Serial.begin(SERIAL_BAUD);
    delay(1000);  // Give time for serial to initialize

    // Print banner
    printBanner();

    // Initialize I2C
    Wire.begin();

    // Initialize motor driver
    Console.print(F("Initializing motor driver... "));
    if (!motor.begin()) {
        Console.println(F("FAILED!"));
        Console.println(F("ERROR: Motor driver not found at address 0x5D"));
        Console.println(F("Check I2C connection and power supply."));
        while (1) {
            delay(1000);  // Halt execution
        }
    }
    Console.println(F("OK"));

    // Initialize limit switches
    Console.print(F("Initializing limit switches... "));
    upperLimit.begin();
    lowerLimit.begin();
    Console.println(F("OK"));

    // Initialize ESP32 encoder (hardware pulse counter)
    Console.print(F("Initializing encoder... "));
    ESP32Encoder::useInternalWeakPullResistors = puType::up;  // Enable pull-ups
    encoder.attachHalfQuad(ENCODER_PIN_A, ENCODER_PIN_B);  // Attach to GPIO 32, 33
    encoder.setCount(0);  // Reset count to zero
    Console.println(F("OK"));

    // Initialize motion controller
    Console.print(F("Initializing motion controller... "));
    motion.begin();
    Console.println(F("OK"));

    // Load configuration from EEPROM
    Console.print(F("Loading configuration... "));
    motion.loadConfig();
    Console.println(F("OK"));

    // Initialize command parser (Serial already initialized above)
    // cmdParser.begin(SERIAL_BAUD);  // SKIP - Serial already initialized

    // Initialize relay (holofan control) - FAIL-SAFE: Start in OFF state
    Console.print(F("Initializing relay... "));
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HOLOFAN_OFF);  // Ensure OFF on startup
    Console.println(F("OK (OFF)"));

    // Route the motion controller's fail-safe holofan cut through the
    // single relay helper.
    setHolofanPowerHook(setHolofanPower);

    // Join the WCB ESP-NOW mesh (replaces the Serial2 droid bus). Does
    // nothing until a mesh password is configured over USB ("mesh pw ...").
    holoMeshSetup();

    // Print ready message
    Console.println();
    Console.println(F("========================================"));
    Console.println(F("System ready!"));
    Console.println(F("Type 'help' or '?' for available commands"));
    Console.println(F("========================================"));
    Console.println();
    Console.println(F("[DEBUG] setup() complete, entering loop()..."));
    Serial.flush();  // Ensure message is sent before entering loop
}

// ===== Main Loop =====

void loop() {
#ifdef HOLOFAN_LOOP_TIMING
    // Safety instrumentation: report max main-loop period once per second.
    static uint32_t lastLoopUs = 0, maxLoopUs = 0, lastReportMs = 0;
    uint32_t nowUs = micros();
    if (lastLoopUs) { uint32_t dt = nowUs - lastLoopUs; if (dt > maxLoopUs) maxLoopUs = dt; }
    lastLoopUs = nowUs;
    if (millis() - lastReportMs > 1000) {
        Console.printf("[timing] loop max_us=%u\n", maxLoopUs);
        maxLoopUs = 0; lastReportMs = millis();
    }
#endif

    // A mesh #HLS (seen on the radio task) stops the lift before anything else.
    if (holoMeshTakeStop()) motion.stop();

    // Update limit switch debouncing
    upperLimit.update();
    lowerLimit.update();

    // Update command parser
    cmdParser.update();

    // WCB mesh: receive, reply, retries, heartbeats, maintenance mode
    holoMeshLoop();

    // Execute motion controller state machine
    motion.update();

    // Process commands
    if (cmdParser.available()) {
        Console.println(F("[DEBUG] Command received!"));
        processCommand();
        cmdParser.clear();
    }

    // Small delay to prevent overwhelming the system
    delay(LOOP_RATE_MS);
}

// ===== Command Processing =====

void processCommand() {
    const char* cmd = cmdParser.getCommand();
    if (cmd == nullptr) return;

    // ===== Help Commands =====

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "?") == 0) {
        printHelp();
    }

    // ===== Status Commands =====

    else if (strcmp(cmd, "status") == 0) {
        sendStatus();
    }

    else if (strcmp(cmd, "limits") == 0) {
        sendLimits();
    }

    else if (strcmp(cmd, "encoder") == 0) {
        sendEncoderInfo();
    }

    else if (strcmp(cmd, "position") == 0 || strcmp(cmd, "pos") == 0) {
        // If argument provided, move to that position (alias for goto)
        if (cmdParser.getArgCount() > 0) {
            int32_t target = cmdParser.getArgInt(0);
            if (motion.gotoPosition(target)) {
                cmdParser.sendOK("Moving to target position");
            } else {
                cmdParser.sendError(motion.getErrorMessage());
            }
        } else {
            // No argument - just report current position
            Console.print(F("position="));
            Console.print(motion.getPosition());
            Console.print(F(" homed="));
            Console.println(motion.isHomed());
        }
    }

    else if (strcmp(cmd, "version") == 0) {
        Console.print(F("firmware="));
        Console.print(F(HOLOLIFTER_VERSION));
        Console.print(F(" date="));
        Console.println(F(HOLOLIFTER_DATE));
    }

    // ===== Movement Commands =====

    else if (strcmp(cmd, "home") == 0) {
        motion.home();
        cmdParser.sendOK("Homing sequence started");
    }

    else if (strcmp(cmd, "up") == 0) {
        int32_t distance = 0;
        if (cmdParser.getArgCount() > 0) {
            distance = cmdParser.getArgInt(0);
        }
        if (!motion.moveUp(distance)) {
            cmdParser.sendError(motion.getErrorMessage());
        } else if (distance > 0) {
            cmdParser.sendOK("Moving up by relative distance");
        } else {
            cmdParser.sendOK("Moving up");
        }
    }

    else if (strcmp(cmd, "down") == 0) {
        int32_t distance = 0;
        if (cmdParser.getArgCount() > 0) {
            distance = cmdParser.getArgInt(0);
        }
        if (!motion.moveDown(distance)) {
            cmdParser.sendError(motion.getErrorMessage());
        } else if (distance > 0) {
            cmdParser.sendOK("Moving down by relative distance");
        } else {
            cmdParser.sendOK("Moving down");
        }
    }

    else if (strcmp(cmd, "stop") == 0) {
        motion.stop();
        cmdParser.sendOK("Emergency stop");
    }

    else if (strcmp(cmd, "reset") == 0 || strcmp(cmd, "clear") == 0) {
        motion.clearError();
        cmdParser.sendOK("Error cleared");
    }

    else if (strcmp(cmd, "goto") == 0) {
        if (cmdParser.getArgCount() > 0) {
            int32_t target = cmdParser.getArgInt(0);
            if (motion.gotoPosition(target)) {
                cmdParser.sendOK("Moving to target position");
            } else {
                // gotoPosition failed - send error message
                cmdParser.sendError(motion.getErrorMessage());
            }
        } else {
            cmdParser.sendError("Usage: goto <position>");
        }
    }

    // ===== Configuration Commands =====

    else if (strcmp(cmd, "speed") == 0) {
        if (cmdParser.getArgCount() > 0) {
            uint8_t speed = motionprofile::clampSpeed(cmdParser.getArgInt(0, MOTOR_DEFAULT_SPEED));
            motion.setSpeed(speed);

            Console.print(F("OK speed="));
            Console.println(speed);
        } else {
            Console.print(F("speed="));
            Console.println(motion.getSpeed());
        }
    }

    else if (strcmp(cmd, "enable") == 0) {
        motor.enable();
        cmdParser.sendOK("Motor driver enabled");
    }

    else if (strcmp(cmd, "disable") == 0) {
        motor.disable();
        cmdParser.sendOK("Motor driver disabled");
    }

    // ===== Holofan Control (Relay) =====

    else if (strcmp(cmd, "relay") == 0) {
        if (cmdParser.getArgCount() > 0) {
            const char* arg = cmdParser.getArg(0);
            if (strcmp(arg, "on") == 0) {
                setHolofanPower(true);
                cmdParser.sendOK("Relay ON (holofan powered)");
            } else if (strcmp(arg, "off") == 0) {
                setHolofanPower(false);
                cmdParser.sendOK("Relay OFF (holofan off)");
            } else {
                cmdParser.sendError("Usage: relay <on|off>");
            }
        } else {
            // Query current state
            Console.print(F("relay="));
            Console.println(digitalRead(RELAY_PIN) == HOLOFAN_ON ? F("ON") : F("OFF"));
        }
    }

    else if (strcmp(cmd, "holofan") == 0) {
        // Alias for relay command (more intuitive name)
        if (cmdParser.getArgCount() > 0) {
            const char* arg = cmdParser.getArg(0);
            if (strcmp(arg, "on") == 0) {
                setHolofanPower(true);
                cmdParser.sendOK("Holofan powered ON");
            } else if (strcmp(arg, "off") == 0) {
                setHolofanPower(false);
                cmdParser.sendOK("Holofan powered OFF");
            } else {
                cmdParser.sendError("Usage: holofan <on|off>");
            }
        } else {
            // Query current state
            Console.print(F("holofan="));
            Console.println(digitalRead(RELAY_PIN) == HOLOFAN_ON ? F("ON") : F("OFF"));
        }
    }

    // ===== Droid Bus Bench Command (USB) =====

    else if (strcmp(cmd, "hl") == 0) {
        // Send a raw #HL command over USB and print the reply (bench testing)
        if (cmdParser.getArgCount() > 0) {
            // CommandParser lowercases the whole line; #HL commands are uppercase.
            char line[CMD_MAX_TOKEN_LEN + 1];
            strncpy(line, cmdParser.getArg(0), sizeof(line) - 1);
            line[sizeof(line) - 1] = '\0';
            for (char* p = line; *p; ++p) *p = (char)toupper((unsigned char)*p);
            holoMeshBench(line);
            Console.println();
        } else {
            cmdParser.sendError("Usage: hl <#HLxxx>  (e.g. hl #HLF1, hl #HLF0)");
        }
    }

    // ===== Mesh Node Configuration (USB) =====

    else if (strcmp(cmd, "mesh") == 0) {
        holoMeshUsb(cmdParser.getRawLine());   // case-preserving: passwords, SSIDs
    }

    // ===== Configuration Storage =====

    else if (strcmp(cmd, "saveconfig") == 0) {
        motion.saveConfig();
        cmdParser.sendOK("Configuration saved to EEPROM");
    }

    else if (strcmp(cmd, "loadconfig") == 0) {
        motion.loadConfig();
        cmdParser.sendOK("Configuration loaded from EEPROM");
    }

    else if (strcmp(cmd, "resetconfig") == 0) {
        motion.resetConfig();
        cmdParser.sendOK("Configuration reset to defaults");
    }

    // ===== Calibration Commands =====

    else if (strcmp(cmd, "margin") == 0) {
        if (cmdParser.getArgCount() > 0) {
            int32_t margin = cmdParser.getArgInt(0);
            if (motion.setSoftLimitMargin(margin)) {
                Console.print(F("OK margin="));
                Console.println(margin);
            } else {
                cmdParser.sendError(motion.getErrorMessage());
            }
        } else {
            Console.print(F("margin="));
            Console.println(motion.getSoftLimitMargin());
        }
    }

    // ===== Diagnostic Commands =====

    else if (strcmp(cmd, "motorid") == 0) {
        Console.print(F("motor_id=0x"));
        Console.print(motor.getDriverID(), HEX);
        Console.print(F(" expected=0xA9"));
        Console.println();
    }

    // ===== Unknown Command =====

    else {
        cmdParser.sendError("Unknown command. Type 'help' for available commands");
    }
}

// ===== Helper Functions =====

void printBanner() {
    Console.println();
    Console.println(F("╔════════════════════════════════════════╗"));
    Console.println(F("║         HoloLifter v" HOLOLIFTER_VERSION "         ║"));
    Console.println(F("║   Platform Lifter Control System      ║"));
    Console.println(F("║   SparkFun ESP32 Thing Plus (ESP32)   ║"));
    Console.println(F("╚════════════════════════════════════════╝"));
    Console.println();
}

void printHelp() {
    Console.println();
    Console.println(F("=== HoloLifter Command Reference ==="));
    Console.println();
    Console.println(F("Movement Commands:"));
    Console.println(F("  home              - Run homing sequence"));
    Console.println(F("  up [n]            - Move platform up (n counts: homed only)"));
    Console.println(F("  down [n]          - Move platform down (n counts: homed only)"));
    Console.println(F("  stop              - Emergency stop"));
    Console.println(F("  reset, clear      - Clear error state"));
    Console.println(F("  goto <pos>        - Move to absolute position"));
    Console.println();
    Console.println(F("Status Commands:"));
    Console.println(F("  status            - Full system status"));
    Console.println(F("  position, pos     - Current position"));
    Console.println(F("  limits            - Limit switch states"));
    Console.println(F("  encoder           - Encoder information"));
    Console.println(F("  version           - Firmware version"));
    Console.println();
    Console.println(F("Configuration:"));
    Console.println(F("  speed [value]     - Get/set movement speed (0-255)"));
    Console.println(F("  enable            - Enable motor driver"));
    Console.println(F("  disable           - Disable motor driver"));
    Console.println(F("  margin [val]      - Get/set soft limit margin (counts)"));
    Console.println();
    Console.println(F("Configuration Storage:"));
    Console.println(F("  saveconfig        - Save config to EEPROM"));
    Console.println(F("  loadconfig        - Load config from EEPROM"));
    Console.println(F("  resetconfig       - Reset config to defaults"));
    Console.println();
    Console.println(F("Holofan Control:"));
    Console.println(F("  relay <on|off>    - Control relay (holofan power)"));
    Console.println(F("  holofan <on|off>  - Power holofan (alias for relay)"));
    Console.println(F("  hl <#HLxxx>       - Run a mesh #HL command locally (e.g. hl #HLQ)"));
    Console.println();
    Console.println(F("Mesh Node (WCB ESP-NOW):"));
    Console.println(F("  mesh              - Show mesh config and counters"));
    Console.println(F("  mesh pw <pw>      - Mesh password (mesh is off until set)"));
    Console.println(F("  mesh oct <hh> <hh>, ch <1-11>, q <0-20>, crc on|off"));
    Console.println(F("  mesh id <1-19>, name <alias>, reply on|off (broadcast replies)"));
    Console.println(F("  mesh ssid <1|2> <name>, pass <1|2> [passphrase]"));
    Console.println(F("  mesh maint <1|2>  - Maintenance mode (Wi-Fi + OTA)"));
    Console.println(F("  mesh off          - Clear mesh password (mesh OFF after restart)"));
    Console.println(F("  mesh clear <1|2>  - Forget a Wi-Fi profile"));
    Console.println(F("  mesh restart      - Reboot to apply mesh settings"));
    Console.println();
    Console.println(F("Utility:"));
    Console.println(F("  help, ?           - Display this help"));
    Console.println(F("  motorid           - Display motor driver ID"));
    Console.println();
    Console.println(F("======================================"));
    Console.println();
}

void sendStatus() {
    // State
    MotionState state = motion.getState();
    Console.print(F("STATUS state="));
    switch (state) {
        case IDLE:               Console.print(F("IDLE")); break;
        case HOMING:             Console.print(F("HOMING")); break;
        case MOVING_TO_TARGET:   Console.print(F("MOVING")); break;
        case MANUAL_UP:          Console.print(F("MOVING_UP")); break;
        case MANUAL_DOWN:        Console.print(F("MOVING_DOWN")); break;
        case STOPPED:            Console.print(F("STOPPED")); break;
        case ERROR:              Console.print(F("ERROR")); break;
    }

    // Position and target
    Console.print(F(" position="));
    Console.print(motion.getPosition());
    Console.print(F(" target="));
    Console.print(motion.getTarget());
    Console.print(F(" homed="));
    Console.print(motion.isHomed());

    // Speed
    Console.print(F(" speed="));
    Console.print(motion.getSpeed());

    // Limit switches
    Console.print(F(" limits="));
    Console.print(motion.getUpperLimit());
    Console.print(F(","));
    Console.print(motion.getLowerLimit());

    // Error message (if any)
    if (state == ERROR) {
        Console.print(F(" error=\""));
        Console.print(motion.getErrorMessage());
        Console.print(F("\""));
    }

    Console.println();
}

void sendLimits() {
    Console.print(F("upper_limit="));
    Console.print(upperLimit.isTriggered());
    Console.print(F(" lower_limit="));
    Console.print(lowerLimit.isTriggered());
    Console.print(F(" upper_pin="));
    Console.print(UPPER_LIMIT_PIN);
    Console.print(F(" lower_pin="));
    Console.println(LOWER_LIMIT_PIN);
}

void sendEncoderInfo() {
    Console.print(F("position="));
    #ifdef ESP32_THING_PLUS
    Console.print(encoder.getCount());
    #else
    Console.print(encoder.read());
    #endif
    Console.print(F(" pin_a="));
    Console.print(ENCODER_PIN_A);
    Console.print(F(" pin_b="));
    Console.print(ENCODER_PIN_B);
    Console.print(F(" cpr_motor="));
    Console.print(ENCODER_CPR_MOTOR);
    Console.print(F(" cpr_output="));
    Console.println((int32_t)ENCODER_CPR_OUTPUT);
}

// ===== Holofan Power =====

// Single choke point for holofan power (the relay). Used by the USB and mesh
// handlers and by the MotionController's fail-safe hook.
void setHolofanPower(bool on) {
    digitalWrite(RELAY_PIN, on ? HOLOFAN_ON : HOLOFAN_OFF);
}

// Defer the new-image validity check to HoloMesh (docs/MAINTENANCE_API.md): the core must not
// mark an OTA image valid at startup. C linkage: the core's weak default lives
// in cores/esp32/esp32-hal-misc.c.
extern "C" bool verifyRollbackLater() { return true; }
