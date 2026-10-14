# Component References & Datasheets

This document contains links to datasheets, documentation, and resources for all components used in the HoloLifter project.

---

## Microcontroller

### SparkFun ESP32 Thing Plus (WRL-15663)

**Product Page:** https://www.sparkfun.com/products/15663

**Documentation:**
- Schematic: https://cdn.sparkfun.com/assets/e/7/5/6/9/ESP32_Thing_Plus_v20_schematic.pdf
- Hookup Guide: https://learn.sparkfun.com/tutorials/esp32-thing-plus-hookup-guide/all
- GitHub Repository: https://github.com/sparkfun/ESP32_Thing_Plus
- ESP32 Datasheet: https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf

**Board Support Package:**
- Platform: Espressif 32 (ESP32)
- PlatformIO Platform: `espressif32`
- Board ID: `esp32thing_plus`
- ESP32 Arduino Core: https://docs.espressif.com/projects/arduino-esp32/

**Key Resources:**
- **CRITICAL:** 3.3V logic ONLY - NOT 5V tolerant!
- ESP32 GPIO Matrix: https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/io_mux.html
- CP2102 drivers (usually pre-installed on modern systems)
- Qwiic System: https://www.sparkfun.com/qwiic

**Key Specifications:**
- Processor: ESP32-WROOM (dual-core Xtensa LX6 @ 240MHz)
- Memory: 4MB Flash, 520KB RAM
- WiFi: 2.4GHz 802.11 b/g/n
- Bluetooth: 5.0 (classic + BLE)
- I2C (Qwiic): GPIO 22 (SCL), GPIO 23 (SDA)

---

## Motor Driver

### SparkFun Qwiic Motor Driver (ROB-15451)

**Product Page:** https://www.sparkfun.com/products/15451

**Documentation:**
- Schematic: https://cdn.sparkfun.com/assets/3/5/e/1/9/Qwiic_Motor_Driver_Schematic.pdf
- Hookup Guide: https://learn.sparkfun.com/tutorials/qwiic-motor-driver-hookup-guide
- User Manual: https://cdn.sparkfun.com/assets/4/8/8/6/5/SCMD_UserGuide.pdf

**Arduino Library:**
- Library Name: "Serial Controlled Motor Driver" (SCMD)
- GitHub: https://github.com/sparkfun/SparkFun_Serial_Controlled_Motor_Driver_Arduino_Library
- Install via Arduino Library Manager or PlatformIO
- PlatformIO: `sparkfun/SparkFun Serial Controlled Motor Driver@^1.0.3`

**Key Specifications:**
- I2C Default Address: 0x5D
- Operating Voltage: 3-11V (12V absolute max)
- Current per Channel: 1.2A continuous, 1.5A peak
- Logic Level: 3.3V compatible

**Example Code:**
```cpp
#include <SCMD.h>
#include <SCMD_config.h>

SCMD myMotorDriver;

void setup() {
  myMotorDriver.settings.commInterface = I2C_MODE;
  myMotorDriver.settings.I2CAddress = 0x5D;
  myMotorDriver.begin();
}
```

---

## Gearmotor & Encoder

### Pololu 298:1 Micro Metal Gearmotor HP 6V with Encoder (5172)

**Product Page:** https://www.pololu.com/product/5172

**Documentation:**
- Product Dimensions: https://www.pololu.com/product/5172#resources
- Encoder Information: https://www.pololu.com/product/5172#specifications
- Micro Metal Gearmotor Overview: https://www.pololu.com/category/60/micro-metal-gearmotors

**Key Specifications:**
- Rated Voltage: 6V (operating at 5V for quieter operation)
- Gear Ratio: 297.92:1 (nominal 298:1)
- No-Load Speed: ~80 RPM @ 5V (100 RPM @ 6V)
- Stall Current: ~1.3A @ 5V (1.6A @ 6V)
- Stall Torque: 4.0 kg-cm (55 oz-in)
- Encoder: 12 CPR (motor shaft), ~14,300 CPR (output shaft, quadrature 4x)

**Encoder Resources:**
- Encoder types explained: https://www.pololu.com/docs/0J16/all#4.b
- Reading quadrature encoders: https://www.pololu.com/docs/0J18

### JST SH-Style Encoder Cable (4763)

**Product Page:** https://www.pololu.com/product/4763

**6-Pin Cable Pinout:**
1. Motor M1+ (Red) - Connect to Motor Driver A1
2. Motor M2- (Black) - Connect to Motor Driver A2
3. Encoder Vcc (Blue) - 2.7V to 18V (connect to ESP32 3V3 pin)
4. Encoder Channel A (Yellow) - Connect to ESP32 Pin 32
5. Encoder Channel B (White) - Connect to ESP32 Pin 33
6. Encoder GND (Green) - Connect to ESP32 GND

**WARNING:** Pins 1-2 (Red/Black) are MOTOR POWER at 5V! Connecting to ESP32 GPIO WILL DAMAGE the board!

---

## Limit Switches

### SPDT Limit Micro Switch, Long Hinge Lever (CYT1073)

**Type:** Generic AC micro switch with SPDT configuration

**Specifications:**
- Rating: AC 1A 125V
- Terminals: COM, NC (Normally Closed), NO (Normally Open)
- Lever Type: Long hinge lever for easy mechanical triggering

**Similar Products:**
- Adafruit: https://www.adafruit.com/product/819
- SparkFun: https://www.sparkfun.com/products/13119
- General micro switch documentation: https://www.electronics-tutorials.ws/io/io_2.html

---

## Holofan Control Components

### 5V Relay Module (RECOMMENDED)

**Product Page:** https://www.amazon.com/dp/B096S15761

**Specifications:**
- Operating Voltage: 5V DC
- Trigger Current: 5mA
- Contact Rating: 10A @ 250VAC, 10A @ 30VDC
- Control: Active HIGH or LOW (jumper selectable)
- Indicator LED shows relay state

**Resources:**
- Relay Basics: https://learn.sparkfun.com/tutorials/button-and-switch-basics/relays
- Arduino Relay Control: https://www.arduino.cc/en/Tutorial/BuiltInExamples/Blink (same principle)

**NOTE:** The relay only switches the holofan's power; playback runs over the
WCB mesh (DroidNet-HoloFan).

---

## Power Supply Components

### Buck Converters (Generic)

**Recommended Modules:**

#### LM2596-based Buck Converter
- Input: 4-40V
- Output: 1.25-37V (adjustable)
- Current: 3A maximum
- Common on Amazon, eBay, AliExpress
- Search terms: "LM2596 DC-DC buck converter module"

**Datasheet:** https://www.ti.com/lit/ds/symlink/lm2596.pdf

#### XL4015 Buck Converter
- Input: 8-36V
- Output: 1.25-36V (adjustable)
- Current: 5A maximum
- Better for motor power rail
- Search terms: "XL4015 DC-DC step down module"

**Datasheet:** https://datasheetspdf.com/pdf-file/788970/XLSEMI/XL4015/1

#### MP1584 Buck Converter (Compact)
- Input: 4.5-28V
- Output: 0.8-26V (adjustable)
- Current: 3A maximum
- Very small form factor
- Search terms: "MP1584 mini DC-DC buck converter"

**Datasheet:** https://www.monolithicpower.com/en/mp1584.html

**Setup Guide:**
- How to adjust buck converter output: https://learn.sparkfun.com/tutorials/how-to-power-a-project
- Buck converter basics: https://www.electronics-tutorials.ws/power/buck-converter.html

---

## Arduino/PlatformIO Libraries

### Required Libraries

1. **SparkFun Serial Controlled Motor Driver**
   - GitHub: https://github.com/sparkfun/SparkFun_Serial_Controlled_Motor_Driver_Arduino_Library
   - Install: `pio lib install "sparkfun/SparkFun Serial Controlled Motor Driver"`

2. **ESP32 Encoder Library** (for quadrature decoding with hardware PCNT)
   - Recommended: madhephaestus/ESP32Encoder library
   - GitHub: https://github.com/madhephaestus/ESP32Encoder
   - Install: `pio lib install "madhephaestus/ESP32Encoder"`
   - **Uses ESP32 hardware pulse counter (PCNT) peripheral**
   - Much more accurate than software interrupts
   - All ESP32 GPIO pins support interrupts

3. **ArduinoJson** (for serial command responses)
   - GitHub: https://github.com/bblanchon/ArduinoJson
   - Install: `pio lib install "bblanchon/ArduinoJson"`
   - Documentation: https://arduinojson.org/

### Future Libraries

4. **IRremote** (for IR control - Phase 6)
   - GitHub: https://github.com/Arduino-IRremote/Arduino-IRremote
   - Install: `pio lib install "Arduino-IRremote"`
   - Supports sending and receiving IR signals

---

## Development Tools

### PlatformIO
- Homepage: https://platformio.org/
- Documentation: https://docs.platformio.org/
- VS Code Extension: https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide
- CLI Installation: https://docs.platformio.org/en/latest/core/installation.html

### Serial Monitoring
- PlatformIO Device Monitor: Built-in (`pio device monitor`)
- CoolTerm: https://freeware.the-meiers.org/ (GUI serial terminal)
- Arduino Serial Monitor: If using Arduino IDE
- Screen (macOS/Linux): `screen /dev/ttyUSB0 115200`

### I2C Tools (for debugging)
- I2C Scanner Sketch: https://playground.arduino.cc/Main/I2cScanner/
- Logic Analyzer Software (for debugging): Saleae Logic, PulseView

---

## Community Resources

### Astromech Droid Building
- R2 Builders Club: https://astromech.net/
- Forum: https://astromech.net/forums/
- Dome Mechanisms: https://astromech.net/forums/forumdisplay.php?26-Dome

### General Arduino/Embedded
- Arduino Forum: https://forum.arduino.cc/
- PlatformIO Community: https://community.platformio.org/
- Adafruit Learning System: https://learn.adafruit.com/
- SparkFun Tutorials: https://learn.sparkfun.com/tutorials

---

## Useful Tutorials

1. **Motor Control:**
   - DC Motor Control: https://learn.sparkfun.com/tutorials/motors-and-selecting-the-right-one
   - PID Control: https://www.teachmemicro.com/arduino-pid-control-tutorial/

2. **Encoder Reading:**
   - Quadrature Encoders: https://howtomechatronics.com/tutorials/arduino/rotary-encoder-works-use-arduino/
   - Interrupt-based Decoding: https://www.youtube.com/watch?v=oLBYHbLO8W0

3. **Serial Communication:**
   - Arduino Serial Guide: https://www.arduino.cc/reference/en/language/functions/communication/serial/
   - Custom Protocols: https://learn.adafruit.com/creating-arduino-serial-commands/overview

4. **Power Supply Design:**
   - Buck Converter Setup: https://www.youtube.com/watch?v=CEhBN5_fO6Q
   - Power Distribution Best Practices: https://www.allaboutcircuits.com/technical-articles/power-distribution-strategies/

---

**Document Version:** 2.0
**Last Updated:** 2025-11-25
**Hardware:** ESP32 Thing Plus (WRL-15663) + Pololu 5172 (298:1)
