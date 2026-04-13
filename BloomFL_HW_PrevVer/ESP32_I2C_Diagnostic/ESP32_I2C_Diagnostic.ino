/*
 * ============================================================
 *  ESP32 Diagnostic Sketch
 *  - Confirms Serial Monitor is working
 *  - Scans I2C bus and prints the LCD's actual address
 *
 *  Upload this first, open Serial Monitor at 115200 baud,
 *  then press the EN (Reset) button on your ESP32.
 * ============================================================
 */

#include <Wire.h>

void setup() {
  // ── Serial check ────────────────────────────────────────
  Serial.begin(115200);
  delay(2000); // Give Serial Monitor time to connect

  Serial.println("\n\n===== ESP32 Diagnostic =====");
  Serial.println("If you can read this, Serial is working!");
  Serial.println("Starting I2C scan...\n");

  // ── I2C scan ────────────────────────────────────────────
  Wire.begin(21, 22); // SDA=21, SCL=22

  byte devicesFound = 0;

  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("  [FOUND] I2C device at address: 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      devicesFound++;
    }
  }

  Serial.println();
  if (devicesFound == 0) {
    Serial.println("  [ERROR] No I2C devices found!");
    Serial.println("  >> Check SDA→GPIO21 and SCL→GPIO22 wiring.");
    Serial.println("  >> Check that LCD is powered (VCC=5V, GND=GND).");
  } else {
    Serial.print("  Scan complete. Devices found: ");
    Serial.println(devicesFound);
    Serial.println("\n  >> Use the address shown above in LiquidCrystal_I2C lcd(0xADDRESS, 16, 2)");
  }

  Serial.println("\n============================");
}

void loop() {
  // Nothing needed here — diagnostic runs once in setup()
}
