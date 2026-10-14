# Bill of Materials - HoloLifter

## Project Overview

Control system for astromech droid holographic platform lifter with motorized position control and holofan power control (relay). The holofan's
playback runs over the WCB mesh (DroidNet-HoloFan).

**Primary Power Source:** 24V LiFePO4 Battery (in droid)

---

## Electronics Components

### Microcontroller

```
    QTY  COMPONENT                  PART NUMBER  SUPPLIER  NOTES
    ---  -------------------------  -----------  --------  ------------------------------------
    1    SparkFun ESP32 Thing Plus  WRL-15663    SparkFun  ESP32 dual-core @ 240MHz, 3.3V logic
```

**Specifications:**

- ESP32-WROOM module: Dual-core Xtensa LX6 @ 240MHz
- 4MB Flash, 520KB RAM
- 21 GPIO pins (all interrupt-capable)
- WiFi (2.4GHz) + Bluetooth 5.0
- I2C, SPI, 3x UART
- Operating Voltage: 3.3V logic (ALL pins - NOT 5V tolerant!)
- Input Voltage (VIN): 4.2-6V (5V from Pololu 2858)
- Built-in Qwiic connector (GPIO 22=SCL, 23=SDA)
- USB: Micro-B with CP2102 USB-to-serial

---

### Motor Control

```
    QTY  COMPONENT                                     PART NUMBER  SUPPLIER  NOTES
    ---  --------------------------------------------  -----------  --------  --------------------------------
    1    SparkFun Qwiic Motor Driver                   ROB-15451    SparkFun  Dual channel, 1.2A continuous/ch
    1    298:1 Micro Metal Gearmotor HP 6V w/ Encoder  5172         Pololu    ~80 RPM @ 5V, Back Connector
    1    JST SH-Style Cable for Encoder                4763         Pololu    6-Pin, Single-Ended Female, 30cm
```

**Motor Driver Specifications:**

- Operating Voltage: 3-11V (12V absolute max)
- Logic Level: 3.3V compatible (Qwiic)
- Continuous Current: 1.2A per channel
- Peak Current: 1.5A per channel
- I2C Address: 0x5D (default)
- **NOTE:** Does NOT have encoder inputs - encoders connect directly to ESP32 GPIO

**Gearmotor Specifications (Pololu 5172):**

- Rated Voltage: 6V (operating at 5V for quieter operation)
- No-Load Current: ~80mA @ 5V
- Stall Current: ~1.3A @ 5V
- No-Load Speed: ~80 RPM @ 5V (100 RPM @ 6V)
- Stall Torque: 4.0 kg-cm (55 oz-in)
- Gear Ratio: 297.92:1 (nominal 298:1)
- Encoder: 12 CPR (motor shaft) = ~14,300 CPR (output shaft, quadrature 4x)
- Encoder Output: Quadrature (A/B channels), 2.7-18V compatible

---

### Limit Switches

```
    QTY  COMPONENT                                  PART NUMBER  SUPPLIER  NOTES
    ---  -----------------------------------------  -----------  --------  -----------------
    2+   SPDT Limit Micro Switch, Long Hinge Lever  CYT1073      Amazon    AC 1A 125V, 3-pin
```

**Specifications:**

- Configuration: SPDT (Single Pole, Double Throw)
- Pins: COM, NC (Normally Closed), NO (Normally Open)
- Required: Use NC configuration for fail-safe operation
- Package: 25 pieces (extras for spares)

---

### Holofan Control

```
    QTY  COMPONENT                      PART NUMBER  SUPPLIER  NOTES
    ---  -----------------------------  -----------  --------  -------------------------------
    1    3.3V Relay Module (1-Channel)  B0BG2F7349   Amazon    JESSINIE brand, holofan power
```

**3.3V Relay Module Specifications (JESSINIE):**

- Operating Voltage: 3.3V DC (ESP32 compatible!)
- Trigger Current: <5mA (compatible with ESP32 GPIO)
- Contact Rating: 10A @ 250VAC, 10A @ 30VDC
- Control: Active HIGH (optocoupler isolated)
- Indicator LED: Shows relay state
- **Purpose:** Holofan power on/off control
- **Connection:** Pin A5 (GPIO 4)

**Control Architecture:**

- **Relay (Pin A5 / GPIO 4):** Power on/off - Simple GPIO control
- **Holofan playback:** over the WCB mesh, not wired to this board
- **Note:** A4 (GPIO 36) is input-only and cannot be used for output!

---

## Power Supply Components

### Single Buck Converter Design

**Input:** 24V LiFePO4 Battery (droid main power)

**Power Rail:** ONE Pololu 2858 provides 5V for the entire system.

```
    QTY  COMPONENT                   PART NUMBER      VOLTAGE   CURRENT               NOTES
    ---  --------------------------  ---------------  --------  --------------------  --------------------
    1    Pololu Step-Down Regulator  2858 (D24V22F5)  24V → 5V  1A cont. (2.2A peak)  Powers entire system
```

**Power Distribution:**

```
24V LiFePO4 Battery
        │
   [2A Fuse recommended]
        │
        ▼
   Pololu 2858 (24V → 5V @ 1A)
        │
        ├─→ Motor Driver VIN (motor runs at 5V)
        └─→ ESP32 Thing Plus VIN
                    │
                    └─→ Internal AP2112K → 3.3V rail
                                │
                                ├─→ Encoder Vcc
                                └─→ Relay VCC
```

### Power Budget Analysis

**5V Rail (from Pololu 2858):**

```
    Load                  Current
    --------------------  -------------------------------
    Motor (no-load @ 5V)  ~80mA
    Motor (stall @ 5V)    ~1.3A
    ESP32 + peripherals   ~200mA
    Peak total            ~1.5A (within 2.2A peak rating)
```

**3.3V Rail (from ESP32 AP2112K, 600mA max):**

```
    Load                Current
    ------------------  -------------------------
    Encoder             ~10mA
    Relay module        <5mA
    Total               ~15mA (well within 600mA)
```

---

## Wiring & Connectors

```
    QTY  COMPONENT             NOTES
    ---  --------------------  ------------------------------
    1    SparkFun Qwiic Cable  Any length, for I2C connection
    -    Wire 20-22 AWG        Power connections
    -    Wire 24-26 AWG        Signal connections
    -    JST connectors        Power distribution
    -    Heat shrink tubing    Wire protection
    -    Dupont connectors     Prototyping/testing
```

---

## Sourcing Summary

**SparkFun Electronics:**

- WRL-15663: ESP32 Thing Plus
- ROB-15451: Qwiic Motor Driver

**Pololu:**

- 5172: 298:1 Micro Metal Gearmotor HP 6V with Encoder
- 4763: JST SH-Style Encoder Cable
- 2858: D24V22F5 Step-Down Regulator (24V→5V)

**Amazon:**

- CYT1073: Limit Switches (25-pack)
- B0BG2F7349: JESSINIE 3.3V Relay Module

---

## Total Estimated Cost

```
    Component                         Est. Cost
    --------------------------------  ---------
    ESP32 Thing Plus (WRL-15663)      ~$25
    Qwiic Motor Driver (ROB-15451)    ~$20
    Gearmotor with Encoder (5172)     ~$30
    Encoder Cable (4763)              ~$3
    Pololu 2858 Buck Converter        ~$10
    Limit Switches (25-pack)          ~$10
    3.3V Relay Module                 ~$7
    Qwiic Cable                       ~$2
    Miscellaneous (wire, connectors)  ~$15
    Total                             ~$121
```

---

## Notes

1. ESP32 Thing Plus has built-in Qwiic connector - no level shifters needed (native 3.3V I2C)
2. Motor driver does NOT have encoder inputs - encoders connect directly to ESP32 GPIO pins
3. Single Pololu 2858 buck converter powers entire system (motor at 5V, not 6V)
4. Motor runs at reduced voltage (5V vs 6V) for quieter, more controlled operation
5. All ESP32 GPIO are 3.3V - NOT 5V tolerant!

---

**Document Version:** 2.2
**Last Updated:** 2026-10-14
