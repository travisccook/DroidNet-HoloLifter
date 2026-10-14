# CLAUDE.md

Guidance for Claude Code (claude.ai/code) and other coding assistants working
in this repository.

## Project overview

DroidNet-HoloLifter is the firmware for a motorized holofan lift in an
astromech droid's dome: a SparkFun ESP32 Thing Plus (ESP32, 240 MHz, 3.3 V
logic) on PlatformIO drives a geared motor with encoder feedback between two
limit switches and switches the holofan's power with a relay. It is node 15
("HoloLifter") on the WCB ESP-NOW mesh. The holofan's own firmware is the
sibling repository DroidNet-HoloFan.

## Build and test

Platform: pioarduino `55.03.312-1` (Arduino-ESP32 3.3.12 on ESP-IDF 5.5.5),
pinned by URL in platformio.ini; it is needed for ESP-NOW v2 (252-byte WCB
packets). Board: `esp32thing_plus`.

```bash
pio run                     # build
pio run -t upload           # flash over USB
pio device monitor          # USB console, 115200 baud
pio test -e native          # unit tests on the host (Unity)
pio test -e native -f "test_wcb_*"   # one family of suites
```

`pio run` builds only the real firmware. The `esp32thing_plus_rollbacktest`
env is a bench image that never starts the mesh (it proves an OTA rollback);
never flash it by cable.

## Layout

    src/            main.cpp (setup/loop, USB console), HoloMesh (mesh node)
    include/        config.h: pins, speeds, limits, version
    lib/            CommandParser LimitSwitch MotionController MotionProfile
                    MotorControl WcbArduino (Arduino adapters)
                    WcbCore NodeConfig MaintCore LiftCommands (shared, pure C++)
    test/           native Unity suites; i2c_scanner is on-hardware only
    tools/          check_private.py (pre-commit leak scan), hlcon.py (USB helper)
    docs/           SERIAL_COMMANDS, MAINTENANCE_API, WCB_MESH_NOTES, ...

lib/WcbCore, lib/NodeConfig, lib/MaintCore and lib/LiftCommands are shared
with DroidNet-HoloFan: change both copies together and keep them identical
(`diff -r ../DroidNet-HoloFan/lib/WcbCore lib/WcbCore`). The same holds for
their test suites.

## Hardware constraints

Power:

- Input: the droid's 24 V LiFePO4 battery.
- One Pololu 2858 (24 V to 5 V, 1 A) feeds the motor driver's VIN (the motor
  runs at 5 V) and the Thing Plus's VIN (3.3 V from its AP2112K).
- GPIO logic is 3.3 V and not 5 V tolerant. Every peripheral must be 3.3 V.

Pins (fixed; see WIRING.md):

    GPIO 32 / 33    encoder A / B (PCNT, ESP32Encoder, INPUT_PULLUP)
    GPIO 14         upper limit switch (NC to GND, INPUT_PULLUP)
    GPIO 27         lower limit switch (NC to GND, INPUT_PULLUP)
    GPIO 4 (A5)     relay IN, holofan power
    GPIO 23 / 22    I2C SDA / SCL (Qwiic) to the motor driver at 0x5D
    Board labels:   A5 = GPIO 4, A4 = GPIO 36 (input only), A3 = 39, A2 = 34,
                    A1 = 25, A0 = 26
    Avoid:          GPIO 0, 2, 12, 15 (boot straps); 6-11 (flash);
                    34, 35, 36, 39 (input only, no pull-ups)
    Free:           GPIO 13, 16, 17, 18, 19, 21, 25, 26

Encoder cable (Pololu 4763, 6-pin JST-SH, pin 1 marked):

    1 RED    motor M1+  -> motor driver A1 (NOT the ESP32)
    2 BLACK  motor M2-  -> motor driver A2 (NOT the ESP32)
    3 BLUE   encoder Vcc -> 3V3
    4 YELLOW encoder A  -> GPIO 32
    5 WHITE  encoder B  -> GPIO 33
    6 GREEN  encoder GND -> GND

Red and black are motor power, not encoder power. Wrong wiring destroys parts.

## Safety rules that belong to the code

- Never home on power-up. Homing runs on `home` / `#HLH`, or automatically
  before the first `up`, `down` or `goto`. Homing state is not saved.
- Limit switches are normally closed: LOW = not triggered, HIGH = triggered
  or wire broken. Check them every loop before driving the motor; a move
  toward a triggered switch stops.
- Emergency stop (`stop` / `#HLS`) halts the motor at once (no ramp), is
  handled ahead of any queued command, and also cuts the holofan's power.
- Maintenance mode disables the motor driver; limits, e-stop and the USB
  console keep running.

## Command protocol

Commands are `#HL<COMMAND>[<PARAM>]` terminated by `\r`, sent through a WCB as
`;wHoloLifter,#HLU\r` or `;w15,#HLU\r`. Only queries reply, as a mesh
broadcast starting with `#HL:`; actions are silent even on error. The `#HL`
prefix avoids the prefixes of other droid systems (@ MarcDuino, ? and ; WCB,
<> Vocalizer, DT/S FlthyHPs, #LE JawaLite). The full reference is
docs/SERIAL_COMMANDS.md; keep it in step with lib/LiftCommands.

## Documentation rules

- No markdown tables. Use text tables inside code blocks so they read well in
  a plain text editor.
- Every fact is either tested on hardware or marked untested.
- Never assume a library API or command syntax; check its documentation.

## Private data

Never write home, droid or credential details into this repository: IP
addresses on a home network, host names, Wi-Fi or mesh passwords, MAC
addresses of real devices, personal paths. tools/check_private.py checks for
them and runs as the pre-commit hook:

    python3 tools/check_private.py            whole tree
    python3 tools/check_private.py --staged   what is staged
