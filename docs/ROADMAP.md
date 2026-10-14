# HoloLifter Roadmap

The HoloLifter was built in phases, each finished and tested before the next
one started. This page is the short record of what each phase delivered and
what is still open.

## Status

    PHASE     FOCUS                                  STATUS
    -------   ------------------------------------   -----------
    Phase 1   Project setup, docs, structure         Complete
    Phase 2   Basic motor control                    Complete
    Phase 3   Encoder feedback, position control     Complete
    Phase 4   Command interface (#HL)                Complete
    Phase 5   Homing, motion profiles, storage       Complete
    Phase 6   Node on the WCB ESP-NOW mesh           Complete, with open items below

## Phase 1: Project setup

PlatformIO project, pin plan, wiring and parts documentation (WIRING.md,
BOM.md, docs/COMPONENT_REFERENCES.md).

## Phase 2: Basic motor control

SparkFun Qwiic Motor Driver (SCMD) over I2C at 0x5D, two normally closed limit
switches with fail-safe logic (lib/MotorControl, lib/LimitSwitch).

## Phase 3: Encoder feedback

Quadrature encoder on GPIO 32/33 through the ESP32's hardware pulse counter
(madhephaestus/ESP32Encoder), position tracking and closed-loop moves.

## Phase 4: Command interface

The #HL command set with text replies (lib/CommandParser, lib/LiftCommands);
see docs/SERIAL_COMMANDS.md.

## Phase 5: Advanced features

Homing (never on power-up: on request, or before the first move), trapezoidal
motion profiles (lib/MotionProfile), soft-limit margin, configuration saved in
flash, error handling and the emergency stop (lib/MotionController).

## Phase 6: Node on the WCB ESP-NOW mesh

The lift is node 15 "HoloLifter" on the droid's WCB mesh: it takes lift
commands and switches the holofan's power relay. Only queries reply, as a mesh
broadcast. It has a maintenance mode (Wi-Fi plus an HTTP API, see
docs/MAINTENANCE_API.md) for over-the-air updates after its one wired flash,
and a new image rolls back on its own unless it reaches the mesh within 30 s
of booting. The platform is pioarduino (Arduino-ESP32 3.3.12 on ESP-IDF 5.5),
needed for ESP-NOW v2.

The holofan's own replacement firmware (node 16 "HoloFan") lives in its own
repository, DroidNet-HoloFan, which shares the mesh libraries (lib/WcbCore,
lib/NodeConfig, lib/MaintCore, lib/LiftCommands) with this one.

## Open items

- At full speed the lift can coast onto the limit switches instead of stopping
  at the soft limit, and a goto can stall short of its target. Test slowly
  first (README, "First setup").

## Possible later work (not planned)

- Battery voltage monitoring
- Current sensing for motor diagnostics
- Status LED

---

See [COMPONENT_REFERENCES.md](COMPONENT_REFERENCES.md) for datasheets and
libraries, [WIRING.md](../WIRING.md) for connections and [BOM.md](../BOM.md)
for parts.
