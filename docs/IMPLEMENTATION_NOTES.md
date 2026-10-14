# HoloLifter #HL Command Protocol Implementation Notes

> Historical note: this page records how the #HL protocol was first added,
> in earlier versions built on a SparkFun RedBoard Qwiic (ATmega328P) at 9600
> baud. The current build is the ESP32 Thing Plus on the WCB mesh, and the
> current protocol is docs/SERIAL_COMMANDS.md; where they differ, that page is
> right.

## Overview

This document provides guidance for implementing the `#HL` command prefix in the existing HoloLifter codebase to ensure compatibility with the C2B5 droid ecosystem.

---

## Required Code Changes

### 1. Update platformio.ini - Baud Rate Change

**File:** `platformio.ini`

**Current:**
```ini
monitor_speed = 115200
```

**Change to:**
```ini
monitor_speed = 9600
```

**Reason:** Match C2B5 ecosystem standard (all other boards use 9600 baud)

---

### 2. Update CommandParser - Add Prefix Filtering

**File:** `lib/CommandParser/CommandParser.h`

**Add to header comment:**
```cpp
/**
 * CommandParser Library
 *
 * Parse HoloLifter serial commands with #HL prefix for C2B5 ecosystem integration.
 *
 * Command Format:
 *   #HL<COMMAND>[<PARAMETER>]\r
 *   Example: "#HLU\r" (move up)
 *   Example: "#HLG5000\r" (goto position 5000)
 *   Example: "#HLV128\r" (set speed 128)
 *
 * Response Format:
 *   #HL:<TYPE> <data>\r
 *   Example: "#HL:OK\r"
 *   Example: "#HL:ERROR Not homed\r"
 *   Example: "#HL:STATUS state=IDLE pos=0 ...\r"
 */
```

**Add new method:**
```cpp
public:
    /**
     * Check if incoming data has #HL prefix
     * @return true if valid HoloLifter command
     */
    bool hasHLPrefix() const;
```

---

### 3. Update CommandParser Implementation

**File:** `lib/CommandParser/CommandParser.cpp`

**Modify `begin()` method:**
```cpp
void CommandParser::begin(uint32_t baud) {
    Serial.begin(baud);
    // NOTE: Baud rate should be 9600 for C2B5 ecosystem compatibility
}
```

**Modify `update()` method to handle \r terminator:**
```cpp
void CommandParser::update() {
    while (Serial.available() > 0) {
        char c = Serial.read();

        // Handle carriage return (standard for C2B5 ecosystem)
        // Note: Also accept \n for backward compatibility
        if (c == '\r' || c == '\n') {
            if (bufferIndex > 0) {
                cmdBuffer[bufferIndex] = '\0';

                // Check for #HL prefix before parsing
                if (strncmp(cmdBuffer, "#HL", 3) == 0) {
                    // Remove #HL prefix before parsing
                    memmove(cmdBuffer, cmdBuffer + 3, bufferIndex - 2);
                    bufferIndex -= 3;
                    parseCommand();
                    commandReady = true;
                } else {
                    // Ignore commands without #HL prefix (compatibility)
                    bufferIndex = 0;
                }
            }
        }
        // ... rest of update logic
    }
}
```

**Add new method implementation:**
```cpp
bool CommandParser::hasHLPrefix() const {
    return (bufferIndex >= 3 &&
            cmdBuffer[0] == '#' &&
            (cmdBuffer[1] == 'H' || cmdBuffer[1] == 'h') &&
            (cmdBuffer[2] == 'L' || cmdBuffer[2] == 'l'));
}
```

**Modify `sendOK()` to add prefix:**
```cpp
void CommandParser::sendOK(const char* message) {
    Serial.print(F("#HL:OK"));
    if (message != nullptr) {
        Serial.print(F(" "));
        Serial.print(message);
    }
    Serial.print(F("\r"));  // Use \r instead of println()
}
```

**Modify `sendError()` to add prefix:**
```cpp
void CommandParser::sendError(const char* message) {
    Serial.print(F("#HL:ERROR "));
    Serial.print(message);
    Serial.print(F("\r"));  // Use \r instead of println()
}
```

**Add new response methods:**
```cpp
/**
 * Send status response
 * @param data Status data string (key=value format)
 */
void CommandParser::sendStatus(const char* data) {
    Serial.print(F("#HL:STATUS "));
    Serial.print(data);
    Serial.print(F("\r"));
}

/**
 * Send position response
 * @param position Current position value
 */
void CommandParser::sendPosition(int32_t position) {
    Serial.print(F("#HL:POS "));
    Serial.print(position);
    Serial.print(F("\r"));
}

/**
 * Send encoder response
 * @param count Encoder count value
 */
void CommandParser::sendEncoder(int32_t count) {
    Serial.print(F("#HL:ENC "));
    Serial.print(count);
    Serial.print(F("\r"));
}

/**
 * Send limits response
 * @param upper Upper limit state (0 or 1)
 * @param lower Lower limit state (0 or 1)
 */
void CommandParser::sendLimits(uint8_t upper, uint8_t lower) {
    Serial.print(F("#HL:LIMITS upper="));
    Serial.print(upper);
    Serial.print(F(" lower="));
    Serial.print(lower);
    Serial.print(F("\r"));
}

/**
 * Send info response
 * @param version Firmware version string
 * @param board Board type
 * @param build Build date
 */
void CommandParser::sendInfo(const char* version, const char* board, const char* build) {
    Serial.print(F("#HL:INFO ver="));
    Serial.print(version);
    Serial.print(F(" board="));
    Serial.print(board);
    Serial.print(F(" build="));
    Serial.print(build);
    Serial.print(F("\r"));
}
```

---

### 4. Update Command Mappings in main.cpp

**File:** `src/main.cpp`

The actual command parsing in main.cpp needs to map the new single-letter commands:

**Old command format:**
```cpp
if (strcmp(cmd, "up") == 0) { /* ... */ }
if (strcmp(cmd, "down") == 0) { /* ... */ }
if (strcmp(cmd, "home") == 0) { /* ... */ }
if (strcmp(cmd, "status") == 0) { /* ... */ }
```

**New command format (after #HL prefix removed):**
```cpp
if (strcmp(cmd, "u") == 0) {  // #HLU -> U
    handleMoveUp();
}
else if (strcmp(cmd, "d") == 0) {  // #HLD -> D
    handleMoveDown();
}
else if (strcmp(cmd, "s") == 0) {  // #HLS -> S
    handleStop();
}
else if (strcmp(cmd, "h") == 0) {  // #HLH -> H
    handleHome();
}
else if (cmd[0] == 'g') {  // #HLG<pos> -> G<pos>
    handleGoto(cmd + 1);  // Pass position parameter
}
else if (strcmp(cmd, "q") == 0) {  // #HLQ -> Q
    handleStatus();
}
else if (strcmp(cmd, "p") == 0) {  // #HLP -> P
    handlePosition();
}
else if (strcmp(cmd, "l") == 0) {  // #HLL -> L
    handleLimits();
}
else if (strcmp(cmd, "e") == 0) {  // #HLE -> E
    handleEncoder();
}
else if (cmd[0] == 'v') {  // #HLV<speed> -> V<speed>
    handleSetSpeed(cmd + 1);
}
else if (strcmp(cmd, "on") == 0) {  // #HLON -> ON
    handleMotorEnable();
}
else if (strcmp(cmd, "of") == 0) {  // #HLOF -> OF
    handleMotorDisable();
}
else if (strcmp(cmd, "c") == 0) {  // #HLC -> C
    handleCalibrate();
}
else if (strcmp(cmd, "save") == 0) {  // #HLSAVE -> SAVE
    handleConfigSave();
}
else if (strcmp(cmd, "load") == 0) {  // #HLLOAD -> LOAD
    handleConfigLoad();
}
else if (strcmp(cmd, "cfgrst") == 0) {  // #HLCFGRST -> CFGRST
    handleConfigReset();
}
else if (cmd[0] == 'f' && cmd[1] == '1') {  // #HLF1 -> F1
    handleHolofanOn();
}
else if (cmd[0] == 'f' && cmd[1] == '0') {  // #HLF0 -> F0
    handleHolofanOff();
}
else if (strcmp(cmd, "?") == 0) {  // #HL? -> ?
    handleHelp();
}
else if (strcmp(cmd, "i") == 0) {  // #HLI -> I
    handleInfo();
}
else if (strcmp(cmd, "r") == 0) {  // #HLR -> R
    handleReset();
}
else {
    parser.sendError("Unknown command");
}
```

---

### 5. Update config.h Constants

**File:** `include/config.h`

**Add firmware version info:**
```cpp
// Firmware Information
#define FIRMWARE_VERSION "1.0.0"
#define BOARD_TYPE "RedBoard"   // earlier versions
#define BUILD_DATE __DATE__

// Serial Configuration
#define SERIAL_BAUD 9600          // C2B5 ecosystem standard
#define CMD_PREFIX "#HL"          // Command prefix
#define RESPONSE_PREFIX "#HL:"    // Response prefix
```

---

### 6. Update Help Text

**File:** `src/main.cpp` (in `handleHelp()` function)

```cpp
void handleHelp() {
    Serial.print(F("#HL:HELP\r"));
    Serial.print(F("HoloLifter Commands (C2B5 Ecosystem Compatible)\r"));
    Serial.print(F("\r"));
    Serial.print(F("Movement:\r"));
    Serial.print(F("  #HLU       - Move UP\r"));
    Serial.print(F("  #HLD       - Move DOWN\r"));
    Serial.print(F("  #HLS       - STOP (emergency)\r"));
    Serial.print(F("  #HLH       - HOME (run homing)\r"));
    Serial.print(F("  #HLG<pos>  - GOTO position\r"));
    Serial.print(F("\r"));
    Serial.print(F("Status:\r"));
    Serial.print(F("  #HLQ       - QUERY status\r"));
    Serial.print(F("  #HLP       - Report POSITION\r"));
    Serial.print(F("  #HLL       - Report LIMITS\r"));
    Serial.print(F("  #HLE       - Report ENCODER\r"));
    Serial.print(F("\r"));
    Serial.print(F("Config:\r"));
    Serial.print(F("  #HLV<val>  - Set VELOCITY/speed\r"));
    Serial.print(F("  #HLON      - Motor enable\r"));
    Serial.print(F("  #HLOF      - Motor disable\r"));
    Serial.print(F("  #HLC       - CALIBRATE\r"));
    Serial.print(F("  #HLSAVE    - Save config\r"));
    Serial.print(F("  #HLLOAD    - Load config\r"));
    Serial.print(F("  #HLCFGRST  - Reset config\r"));
    Serial.print(F("\r"));
    Serial.print(F("Holofan:\r"));
    Serial.print(F("  #HLF1      - Holofan ON\r"));
    Serial.print(F("  #HLF0      - Holofan OFF\r"));
    Serial.print(F("\r"));
    Serial.print(F("Utility:\r"));
    Serial.print(F("  #HL?       - This help\r"));
    Serial.print(F("  #HLI       - System info\r"));
    Serial.print(F("  #HLR       - Reset/reboot\r"));
    Serial.print(F("\r"));
    Serial.print(F("See docs/SERIAL_COMMANDS.md for details\r"));
}
```

---

## Implementation Checklist

### Phase 1: Core Protocol Changes

- [ ] Update platformio.ini baud rate to 9600
- [ ] Modify CommandParser to filter for `#HL` prefix
- [ ] Modify CommandParser to use `\r` terminator
- [ ] Add `#HL:` prefix to all responses
- [ ] Test basic command parsing

### Phase 2: Command Mapping

- [ ] Update all command handlers to use single-letter codes
- [ ] Map old commands to new commands:
  - [ ] `up` → `u` (after `#HLU` prefix removed)
  - [ ] `down` → `d`
  - [ ] `stop` → `s`
  - [ ] `home` → `h`
  - [ ] `goto` → `g<param>`
  - [ ] `status` → `q`
  - [ ] `position` → `p`
  - [ ] `limits` → `l`
  - [ ] `encoder` → `e`
  - [ ] `speed` → `v<param>`
  - [ ] `help` → `?`
  - [ ] etc.
- [ ] Test all mapped commands

### Phase 3: Response Formatting

- [ ] Add specialized response methods
- [ ] Update status output format
- [ ] Update error message formatting
- [ ] Test response parsing

### Phase 4: Documentation

- [ ] Update help text
- [ ] Add version info response
- [ ] Update README with new command format
- [ ] Update inline code comments

### Phase 5: Testing

- [ ] Run automated test script
- [ ] Verify no command conflicts with C2B5 ecosystem
- [ ] Test on actual hardware
- [ ] Test with serial monitor
- [ ] Test rapid command sequences
- [ ] Test error conditions

---

## Backward Compatibility Notes

### Optional: Support Both Formats During Transition

If you want to support both old and new command formats temporarily:

```cpp
void CommandParser::update() {
    // ... read character logic ...

    if (c == '\r' || c == '\n') {
        if (bufferIndex > 0) {
            cmdBuffer[bufferIndex] = '\0';

            // Check for #HL prefix
            if (strncmp(cmdBuffer, "#HL", 3) == 0) {
                // New format: Remove #HL prefix
                memmove(cmdBuffer, cmdBuffer + 3, bufferIndex - 2);
                bufferIndex -= 3;
                parseCommand();
                commandReady = true;
                newFormat = true;
            }
            else {
                // Old format: Accept as-is (for backward compatibility)
                parseCommand();
                commandReady = true;
                newFormat = false;
            }
        }
    }
}

void CommandParser::sendOK(const char* message) {
    if (newFormat) {
        Serial.print(F("#HL:OK"));
    } else {
        Serial.print(F("OK"));
    }
    // ... rest of method
}
```

However, for clean C2B5 integration, it's recommended to **require the `#HL` prefix** to avoid accidental command triggering on shared serial buses.

---

## Testing After Implementation

### Quick Verification Steps

1. **Connect to board at 9600 baud**
   ```bash
   screen /dev/ttyUSB0 9600
   # or
   pio device monitor --baud 9600
   ```

2. **Test basic command**
   ```
   #HL?<CR>
   ```
   Expected: Help text with `#HL:` prefix

3. **Test status query**
   ```
   #HLQ<CR>
   ```
   Expected: `#HL:STATUS state=... pos=... ...`

4. **Test invalid command**
   ```
   #HLX<CR>
   ```
   Expected: `#HL:ERROR Unknown command`

5. **Test command without prefix (should be ignored)**
   ```
   up<CR>
   ```
   Expected: No response

6. **Run full automated test suite**
   ```bash
   python3 docs/TESTING_PLAN.md /dev/ttyUSB0
   ```

---

## Common Pitfalls

### 1. Forgetting \r Terminator

**Wrong:** Sending commands with `\n` only
**Right:** Send commands with `\r` (or `\r\n`)

### 2. Case Sensitivity

Commands should be **case insensitive** after prefix:
- `#HLU\r` and `#hlu\r` should both work

### 3. Buffer Overflow

Ensure buffer can handle longest command:
- `#HLCFGRST\r` = 10 characters
- Recommended buffer size: 64 bytes minimum

### 4. Response Timing

Some commands (like homing) take time to complete:
- Send `#HL:OK\r` immediately
- Status updates can be queried with `#HLQ\r`

### 5. Serial Monitor Line Endings

Configure serial monitor to send `CR` or `Both NL & CR`:
- Arduino IDE: Dropdown in serial monitor
- PlatformIO: May need to manually add `\r`

---

## Migration Path

### For Existing Users

If users have scripts or tools using the old command format:

1. **Option A: Update Scripts**
   - Find/replace: `up\n` → `#HLU\r`
   - Update baud rate: 115200 → 9600

2. **Option B: Wrapper Script**
   Create a translation layer:
   ```python
   # old_to_new.py
   OLD_TO_NEW = {
       'up': '#HLU',
       'down': '#HLD',
       'stop': '#HLS',
       # ... etc
   }

   def translate(old_cmd):
       return OLD_TO_NEW.get(old_cmd, old_cmd)
   ```

3. **Option C: Dual-Mode Firmware**
   Support both formats with compile-time flag:
   ```cpp
   #define SUPPORT_LEGACY_COMMANDS 1
   ```

---

## Performance Considerations

### Memory Impact

Adding `#HL` prefix handling adds minimal overhead:
- Code size: +200-300 bytes
- RAM: +3 bytes per command (prefix check)
- Acceptable even for the ATmega328P of earlier versions (32KB flash, 2KB RAM)

### Processing Time

- Prefix check: ~10 microseconds
- Negligible impact on responsiveness
- Well within 100ms response time target

---

**Document Version:** 1.0
**Last Updated:** 2025-01-19
**Next Review:** After code implementation complete

*May the Force guide your implementation!*
