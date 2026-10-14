/**
 * I2C Scanner Diagnostic Tool
 *
 * Scans all I2C addresses (0x01 to 0x7F) and reports which devices are found.
 * Use this to verify the motor driver is connected and communicating.
 *
 * Expected: Motor driver at 0x5D (93 decimal)
 */

#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  delay(1000);

  Serial.println();
  Serial.println(F("========================================"));
  Serial.println(F("I2C Scanner - HoloLifter Diagnostic"));
  Serial.println(F("========================================"));
  Serial.println();
  Serial.println(F("Initializing I2C bus..."));

  Wire.begin();

  Serial.println(F("Scanning I2C addresses 0x01 to 0x7F..."));
  Serial.println();

  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    // The i2c_scanner uses the return value of Write.endTransmission
    // to see if a device acknowledged the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F("I2C device found at address 0x"));
      if (address < 16) {
        Serial.print(F("0"));
      }
      Serial.print(address, HEX);
      Serial.print(F(" ("));
      Serial.print(address);
      Serial.print(F(" decimal)"));

      // Identify known devices
      if (address == 0x5D) {
        Serial.print(F(" <- SparkFun Motor Driver (SCMD)"));
      }

      Serial.println();
      nDevices++;
    }
    else if (error == 4) {
      Serial.print(F("Unknown error at address 0x"));
      if (address < 16) {
        Serial.print(F("0"));
      }
      Serial.println(address, HEX);
    }
  }

  Serial.println();
  Serial.println(F("========================================"));
  if (nDevices == 0) {
    Serial.println(F("No I2C devices found!"));
    Serial.println();
    Serial.println(F("Troubleshooting:"));
    Serial.println(F("1. Check Qwiic cable is connected"));
    Serial.println(F("2. Verify motor driver has power (6V rail)"));
    Serial.println(F("3. Check cable is not damaged"));
    Serial.println(F("4. Try different Qwiic port on RedBoard"));
  }
  else {
    Serial.print(F("Found "));
    Serial.print(nDevices);
    Serial.println(F(" device(s)."));
    Serial.println();

    // Check if motor driver was found
    bool motorDriverFound = false;
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0) {
      motorDriverFound = true;
    }

    if (motorDriverFound) {
      Serial.println(F("✓ Motor driver found at 0x5D - I2C OK!"));
    } else {
      Serial.println(F("✗ Motor driver NOT found at 0x5D"));
      Serial.println(F("  Expected address: 0x5D (93 decimal)"));
    }
  }
  Serial.println(F("========================================"));
  Serial.println();
}

void loop() {
  // Scan continuously every 5 seconds
  delay(5000);
  Serial.println(F("Rescanning..."));

  byte error, address;
  int nDevices = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print(F("0x"));
      if (address < 16) Serial.print(F("0"));
      Serial.print(address, HEX);
      Serial.print(F(" "));
      nDevices++;
    }
  }

  if (nDevices > 0) {
    Serial.print(F(" ("));
    Serial.print(nDevices);
    Serial.println(F(" devices)"));
  } else {
    Serial.println(F("No devices found"));
  }
}
