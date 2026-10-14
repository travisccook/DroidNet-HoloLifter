# Wiring Guide - HoloLifter

Complete wiring reference for the ESP32-based motorized platform lifter.

---

## Power Architecture

**Single Buck Converter Design** - One Pololu 2858 powers the entire system.

```
24V LiFePO4 Battery (Droid Main Power)
              │
       [2A Fuse recommended]
              │
              ▼
      ┌───────────────┐
      │  Pololu 2858  │
      │   24V → 5V    │
      │   @ 1A cont.  │
      │  (2.2A peak)  │
      └───────┬───────┘
              │ 5V Rail
              │
      ┌───────┴───────┐
      │               │
      ▼               ▼
Motor Driver    ESP32 Thing Plus
    VIN              VIN
(Motor @ 5V)         │
                     │ Internal AP2112K LDO
                     ▼
                  3.3V Rail
                     │
         ┌───────────┴───────────┐
         │                       │
         ▼                       ▼
    Encoder Vcc             Relay VCC
    (Blue wire)             (3.3V module)
```

**Common Ground:** All GND connections tied together (star topology recommended).

**Critical Notes:**
- Motor operates at 5V (not rated 6V) - quieter, more controlled operation
- ESP32 Thing Plus: ALL GPIO are 3.3V - NOT 5V tolerant!
- Relay module must be 3.3V compatible (powered from ESP32 3V3 pin)

---

## Component List

```
    #  Component                    Part Number  Purpose
    -  ---------------------------  -----------  ---------------------
    1  SparkFun ESP32 Thing Plus    WRL-15663    Microcontroller
    2  Pololu 2858 Buck Converter   D24V22F5     24V → 5V power
    3  SparkFun Qwiic Motor Driver  ROB-15451    I2C motor control
    4  Pololu Gearmotor w/ Encoder  5172         298:1 HP 6V motor
    5  Pololu Encoder Cable         4763         6-pin JST-SH 30cm
    6  Limit Switches (x2)          CYT1073      SPDT micro switches
    7  3.3V Relay Module            JESSINIE     Holofan power control
```

---

## 1. ESP32 Thing Plus

**Board:** SparkFun ESP32 Thing Plus (WRL-15663)
**MCU:** ESP32-WROOM (Dual-core 240MHz, 3.3V logic)
**USB:** Micro-B with CP2102 USB-to-serial

### Board Label to GPIO Mapping

The silkscreen labels on the board don't always match GPIO numbers. Here's the mapping:

```
BOARD LABEL    GPIO NUMBER    ACTIVE USE
───────────    ───────────    ─────────────────────
23/SDA         GPIO 23        I2C SDA (Qwiic)
22/SCL         GPIO 22        I2C SCL (Qwiic)
14             GPIO 14        Upper Limit Switch
32             GPIO 32        Encoder Channel A
33             GPIO 33        Encoder Channel B
27             GPIO 27        Lower Limit Switch
12             GPIO 12        (boot pin - avoid)
13             GPIO 13        Available (LED_BUILTIN)
21             GPIO 21        Available
A5             GPIO 4         Relay Module
A4             GPIO 36        Input only (NOT usable for output!)
A3             GPIO 39        Input only
A2             GPIO 34        Input only
A1             GPIO 25        Available (DAC1)
A0             GPIO 26        Available (DAC2)
```

### Complete Pin Assignment Table

```
GPIO    BOARD LABEL    DIR     CONNECTED TO               NOTES
──────  ───────────    ──────  ─────────────────────────  ─────────────────────────
VIN     VIN            INPUT   Pololu 2858 VOUT (5V)      Accepts 4.2-6V
GND     GND            -       Common ground              Multiple GND pins available
3V3     3V3            OUTPUT  Encoder Vcc, Relay VCC     600mA max from AP2112K

22      22/SCL         I2C     Motor Driver Qwiic         Hardware I2C0
23      23/SDA         I2C     Motor Driver Qwiic         Hardware I2C0

32      32             INPUT   Encoder Channel A          Yellow wire, INPUT_PULLUP
33      33             INPUT   Encoder Channel B          White wire, INPUT_PULLUP

14      14             INPUT   Upper Limit Switch COM     Safe boot pin, INPUT_PULLUP
27      27             INPUT   Lower Limit Switch COM     Safe pin, INPUT_PULLUP

4       A5             OUTPUT  Relay Module IN            Holofan power on/off

-       USB Micro-B    I/O     Computer                   Programming & serial monitor
```

---

## 2. Pololu 2858 Buck Converter

**Board:** Pololu D24V22F5 Step-Down Voltage Regulator
**Input:** 4.5-38V (24V LiFePO4 nominal)
**Output:** 5V fixed @ 1A continuous (2.2A peak)

### Connections

```
FROM                          TO POLOLU 2858
────────────────────────────  ──────────────
24V Battery (+)          →    VIN
24V Battery (-)          →    GND

FROM POLOLU 2858              TO DEVICE
────────────────────────────  ──────────────
VOUT                     →    Motor Driver VIN
VOUT                     →    ESP32 Thing Plus VIN
GND                      →    Common ground bus
```

### Power Budget

```
LOAD                          CURRENT DRAW
────────────────────────────  ────────────────
Motor (no-load @ 5V)          ~80mA
Motor (stall @ 5V)            ~1.3A
ESP32 + peripherals           ~200mA
────────────────────────────  ────────────────
Peak total                    ~1.5A (within 2.2A peak)
```

---

## 3. SparkFun Qwiic Motor Driver

**Board:** SparkFun Qwiic Motor Driver (ROB-15451)
**Driver IC:** SCMD (Serial Controlled Motor Driver)
**I2C Address:** 0x5D (default)

### Connections

```
FROM                          TO MOTOR DRIVER
────────────────────────────  ───────────────
Pololu 2858 VOUT (5V)    →    VIN (screw terminal)
Common GND               →    GND (screw terminal)
ESP32 Qwiic connector    ↔    Motor Driver Qwiic connector
```

### Motor Output

```
MOTOR DRIVER TERMINAL         TO MOTOR CABLE
────────────────────────────  ───────────────
A1                       →    Red wire (Pin 1)
A2                       →    Black wire (Pin 2)
B1                       -    Not connected
B2                       -    Not connected
```

**Notes:**
- VIN range: 3-11V (5V is ideal for quieter operation)
- VIN powers motor outputs; Qwiic provides 3.3V logic power
- Motor polarity determines direction (swap A1/A2 if inverted)
- Current: 1.2A continuous, 1.5A peak per channel

---

## 4. Pololu Gearmotor with Encoder

**Motor:** Pololu 5172 (298:1 Micro Metal Gearmotor HP 6V)
**Cable:** Pololu 4763 (6-pin JST-SH, 30cm)

### 6-Pin Encoder Cable Pinout

**CRITICAL:** The 6-pin cable carries BOTH motor power AND encoder signals!

```
           6-Pin JST-SH Connector
       (looking into connector opening)

    Pin 1  Pin 2  Pin 3  Pin 4  Pin 5  Pin 6
     ┌─┐   ┌─┐   ┌─┐    ┌─┐    ┌─┐    ┌─┐
     │▲│   │ │   │ │    │ │    │ │    │ │     ◄── Pin 1 marked
     └┬┘   └┬┘   └┬┘    └┬┘    └┬┘    └┬┘
      │     │     │      │      │      │
     RED  BLACK BLUE  YELLOW WHITE  GREEN
      │     │     │      │      │      │
      │     │     │      │      │      │
      ▼     ▼     │      │      │      │
   ┌─────────┐   │      │      │      │
   │  Motor  │   └──────┴──────┴──────┘
   │  Driver │          │
   │ A1   A2 │     ESP32 Thing Plus
   └─────────┘     3V3  GPIO32 GPIO33  GND
```

### Wire-by-Wire Connection Table

```
PIN  COLOR    FUNCTION       DESTINATION                    WARNING
───  ───────  ─────────────  ─────────────────────────────  ─────────────────────────
1    RED      Motor M1+      Motor Driver A1 terminal       NOT ESP32! Motor power!
2    BLACK    Motor M2-      Motor Driver A2 terminal       NOT ESP32! Motor power!
3    BLUE     Encoder Vcc    ESP32 Thing Plus 3V3 pin       Encoder accepts 2.7-18V
4    YELLOW   Encoder Ch A   ESP32 Thing Plus GPIO 32       INPUT_PULLUP in code
5    WHITE    Encoder Ch B   ESP32 Thing Plus GPIO 33       INPUT_PULLUP in code
6    GREEN    Encoder GND    ESP32 Thing Plus GND           Common ground
```

**WARNING:** Pins 1-2 (Red/Black) carry MOTOR POWER at 5V. Connecting these to ESP32 GPIO WILL DAMAGE the board! Always verify Pin 1 orientation before connecting.

### Encoder Specifications

```
PARAMETER                     VALUE
────────────────────────────  ─────────────────────────
Motor gear ratio              297.92:1 (nominal 298:1)
CPR at motor shaft            12
CPR at output shaft           ~3,575 (12 x 297.92)
CPR with quadrature (4x)      ~14,300
Encoder voltage range         2.7V - 18V
Internal pull-ups             Yes (10k ohm)
```

---

## 5. Limit Switches

**Type:** SPDT Micro Limit Switch with Long Hinge Lever
**Part:** CYT1073 (Amazon)
**Configuration:** Normally Closed (NC) for fail-safe operation
**Quantity:** 2 (upper and lower limits)

### Switch Terminal Identification

```
Looking at switch with lever pointing UP:

                 Lever
                   /
                  *  <-- Hinge
                 /
   +-------------*--------------+
   |    "1A 125V AC"            |  <-- Switch body
   |    "C    NO    NC"         |      (labels visible)
   +------+------+------+-------+
          |      |      |
         [*]    [*]    [*]   <-- Pins extend downward
          |      |      |
         COM    NO     NC

   LEFT pin    = COM (C)  --> Connect to ESP32 GPIO
   MIDDLE pin  = NO       --> Leave unconnected
   RIGHT pin   = NC       --> Connect to GND
```

### Upper Limit Switch

```
FROM LIMIT SWITCH             TO ESP32 THING PLUS
────────────────────────────  ───────────────────────
COM (left pin)           →    GPIO 14 (INPUT_PULLUP)
NC (right pin)           →    GND
NO (middle pin)               Not connected
```

### Lower Limit Switch

```
FROM LIMIT SWITCH             TO ESP32 THING PLUS
────────────────────────────  ───────────────────────
COM (left pin)           →    GPIO 27 (INPUT_PULLUP)
NC (right pin)           →    GND
NO (middle pin)               Not connected
```

### Electrical Behavior (NC Fail-Safe Configuration)

```
CONDITION                     GPIO READS    WHY
────────────────────────────  ────────────  ──────────────────────────────────
Lever NOT pressed (normal)    LOW           NC contact closed, GPIO pulled to GND
Lever PRESSED (at limit)      HIGH          NC contact open, internal pull-up active
Wire broken (fault)           HIGH          Open circuit, pull-up active (FAIL-SAFE)
```

**Why NC is Fail-Safe:** Any failure (broken wire, loose connection, damaged switch) results in HIGH, which the code interprets as "limit triggered" and stops the motor.

---

## 6. Relay Module (Holofan Power Control)

**Type:** 1-Channel 3.3V Relay Module
**Part:** JESSINIE B0BG2F7349 (Amazon)
**Purpose:** Holofan power on/off control

### Control Side (Low-Voltage)

```
FROM ESP32 THING PLUS         TO RELAY MODULE
────────────────────────────  ───────────────
3V3                      →    VCC
GND                      →    GND
Pin A5 (GPIO 4)          →    IN
```

**Notes:**

- Pin A5 = GPIO 4 (see board label mapping)
- Must be 3.3V compatible relay (not 5V!)
- Trigger current <5mA (safe for ESP32 GPIO)
- Active HIGH (Pin A5 HIGH = relay energizes)
- Optocoupler isolated

### Switching Side (High-Voltage)

```
FROM                          TO RELAY TERMINAL
────────────────────────────  ─────────────────
Holofan Power Source (+) →    COM
Holofan Device (+)       ←    NO (Normally Open)
NC (Normally Closed)          Not connected

Holofan Power Source (-) →    Holofan Device (-) directly (bypass relay)
```

### Control Logic

```
PIN A5 STATE     RELAY STATE    HOLOFAN
─────────────    ───────────    ────────────────────────
LOW              De-energized   OFF (COM disconnected)
HIGH             Energized      ON (COM connected to NO)
```

**Why NO Terminal:** Using Normally Open means holofan is OFF by default. If ESP32 crashes, loses power, or resets, the holofan turns off safely.

---

## Complete System Wiring Diagram

```
                                    24V LiFePO4 Battery
                                           |
                                     [2A Fuse]
                                           |
                                           v
                                   +---------------+
                                   | Pololu 2858   |
                                   | 24V -> 5V     |
                                   +-------+-------+
                                           | 5V
                          +----------------+----------------+
                          |                                 |
                          v                                 v
                  +---------------+                 +---------------+
                  | Motor Driver  |                 | ESP32 Thing+  |
                  | VIN       GND |                 | VIN       GND |
                  +-------+-------+                 +-------+-------+
                          |                                 |
                   Qwiic  |  I2C                     3.3V   |  From AP2112K
                   Cable  |                                 |
                          +----------------+----------------+
                                           |
                          +----------------+----------------+
                          |                |                |
                          v                v                v
                   Motor Driver      Encoder Vcc       Relay VCC
                   Qwiic (logic)     (Blue wire)       (module)


ESP32 Pin Connections (using board silkscreen labels):
──────────────────────────────────────────────────────────────────────────────

22/SCL + 23/SDA (Qwiic) ───────────────► Motor Driver Qwiic (I2C @ 0x5D)

Pin 32 ◄───────────────────────────────── Encoder Ch A (Yellow)
Pin 33 ◄───────────────────────────────── Encoder Ch B (White)
3V3 ───────────────────────────────────► Encoder Vcc (Blue)
GND ◄──────────────────────────────────── Encoder GND (Green)

Pin 14 ◄───────────────────────────────── Upper Limit COM
Pin 27 ◄───────────────────────────────── Lower Limit COM
GND ───────────────────────────────────► Both Limit NC terminals

Pin A5 (GPIO 4) ───────────────────────► Relay Module IN
3V3 ───────────────────────────────────► Relay Module VCC
GND ───────────────────────────────────► Relay Module GND

Motor Driver Screw Terminals:
──────────────────────────────────────────────────────────────────────────────

A1 ◄──────────────────────────────────── Motor Red (Cable Pin 1)
A2 ◄──────────────────────────────────── Motor Black (Cable Pin 2)
B1, B2: Not connected
```

---

## Pre-Power Checklist

Before applying power, verify:

- [ ] All connections secure, no exposed wire strands
- [ ] Polarity correct on 24V input to Pololu 2858
- [ ] Motor Red/Black to Motor Driver A1/A2 (NOT to ESP32!)
- [ ] Encoder Blue wire to ESP32 3V3 (NOT motor driver!)
- [ ] Encoder Yellow/White/Green to GPIO 32/33/GND
- [ ] Limit switches using NC terminals (not NO)
- [ ] Relay module is 3.3V compatible
- [ ] No shorts between power rails (check with multimeter)
- [ ] GND continuity between all boards

---

## Power-On Test Sequence

### Step 1: Buck Converter Only
- [ ] Connect 24V to Pololu 2858 (nothing else connected)
- [ ] Measure output: 5.0V +/- 0.1V
- [ ] Disconnect 24V

### Step 2: ESP32 Only
- [ ] Connect 5V to ESP32 VIN and GND
- [ ] Connect USB to computer
- [ ] Upload blink test
- [ ] Verify boot and LED blink

### Step 3: I2C Motor Driver
- [ ] Connect Qwiic cable
- [ ] Upload I2C scanner sketch
- [ ] Verify device at 0x5D

### Step 4: Motor
- [ ] Connect motor wires to A1/A2
- [ ] Upload low-speed motor test
- [ ] Verify rotation both directions

### Step 5: Encoder
- [ ] Connect encoder cable (verify Pin 1!)
- [ ] Upload encoder test sketch
- [ ] Manually rotate motor, verify counts

### Step 6: Limit Switches
- [ ] Connect both switches (COM to GPIO, NC to GND)
- [ ] Upload limit switch test
- [ ] Verify: LOW normal, HIGH when pressed

### Step 7: Full System
- [ ] Upload HoloLifter firmware
- [ ] Test commands: status, up, down, stop
- [ ] Verify limits halt movement
- [ ] Test homing sequence

---

## Troubleshooting

**Motor doesn't move:**
- Check 5V at motor driver VIN with multimeter
- Run I2C scanner - device should appear at 0x5D
- Swap A1/A2 wires if direction is inverted

**Encoder doesn't count:**
- Verify cable Pin 1 orientation (red wire)
- Check 3.3V on blue wire (encoder Vcc)
- Verify yellow->GPIO32, white->GPIO33, green->GND
- Confirm INPUT_PULLUP enabled in code

**Limit switches always show triggered:**
- Verify using NC terminal (right pin), not NO (middle)
- Check INPUT_PULLUP enabled in code
- Test with multimeter: should read LOW when not pressed

**ESP32 won't boot:**
- Check 5V at VIN pin (4.2-6V valid range)
- Try different USB cable (data-capable, not charge-only)
- CP2102 drivers usually pre-installed on modern systems

**Relay doesn't click:**

- Verify 3.3V at relay VCC (not 5V module!)
- Check Pin A5 (GPIO 4) toggles (use LED test or multimeter)
- Confirm active-HIGH module

---

**Document Version:** 3.3
**Last Updated:** 2026-10-14
**Hardware:** ESP32 Thing Plus (WRL-15663) + Pololu 5172 (298:1) @ 5V
