/*
 * ============================================================
 *  ESP32 Multi-Sensor Monitor
 * ============================================================
 *  Hardware:
 *    - PIR Motion Sensor     → GPIO 27
 *    - RCWL-0516 Radar       → GPIO 26
 *    - DHT11 Temp/Humidity   → GPIO 4
 *    - 16x2 I2C LCD          → SDA: GPIO 21 | SCL: GPIO 22
 *
 *  Libraries Required (install via Arduino Library Manager):
 *    - DHT sensor library  by Adafruit
 *    - Adafruit Unified Sensor by Adafruit
 *    - LiquidCrystal I2C  by Frank de Brabander
 *
 *  Readings are refreshed every 2 seconds (non-blocking).
 * ============================================================
 */

#include <Wire.h>              // I2C communication for LCD
#include <LiquidCrystal_I2C.h> // LCD driver
#include <DHT.h>               // DHT11 sensor driver

// ──────────────────────────────────────────────
//  Pin Definitions
// ──────────────────────────────────────────────
#define PIR_PIN    27   // PIR motion sensor output
#define RCWL_PIN   26   // RCWL-0516 radar sensor output
#define DHT_PIN     4   // DHT11 data pin
#define DHT_TYPE  DHT11 // Sensor model

// ──────────────────────────────────────────────
//  Timing
// ──────────────────────────────────────────────
const unsigned long REFRESH_INTERVAL = 2000; // ms between updates
unsigned long lastUpdateTime = 0;

// ──────────────────────────────────────────────
//  Object Initialisation
// ──────────────────────────────────────────────
// LCD: I2C address 0x27, 16 columns, 2 rows
// If the display stays blank, try address 0x3F instead.
LiquidCrystal_I2C lcd(0x27, 16, 2);

DHT dht(DHT_PIN, DHT_TYPE);

// ──────────────────────────────────────────────
//  Setup
// ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 Multi-Sensor Monitor ===");

  // Configure sensor input pins
  pinMode(PIR_PIN,  INPUT);
  pinMode(RCWL_PIN, INPUT);

  // Start DHT sensor
  dht.begin();

  // Start I2C on ESP32 default pins (SDA=21, SCL=22)
  Wire.begin(21, 22);

  // Initialise LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // Splash screen for 1.5 s so the DHT can stabilise
  lcd.setCursor(0, 0);
  lcd.print("  ESP32 Monitor ");
  lcd.setCursor(0, 1);
  lcd.print("  Initialising..");
  delay(1500);
  lcd.clear();

  Serial.println("Sensors ready. Updates every 2 seconds.\n");
}

// ──────────────────────────────────────────────
//  Main Loop  (non-blocking via millis())
// ──────────────────────────────────────────────
void loop() {
  unsigned long currentTime = millis();

  // Only update when the interval has elapsed
  if (currentTime - lastUpdateTime >= REFRESH_INTERVAL) {
    lastUpdateTime = currentTime;

    // ── Read all sensors ──────────────────────

    // Motion sensors (HIGH = motion detected)
    bool pirMotion  = digitalRead(PIR_PIN)  == HIGH;
    bool rcwlMotion = digitalRead(RCWL_PIN) == HIGH;

    // Temperature & Humidity
    float humidity    = dht.readHumidity();
    float temperature = dht.readTemperature(); // °C

    // Check for DHT read failure
    bool dhtOk = !(isnan(humidity) || isnan(temperature));

    // ── Build display strings ─────────────────

    // Line 1: temperature and humidity (16 chars max)
    char line1[17]; // 16 chars + null terminator
    if (dhtOk) {
      // e.g. "T:28.5C  H:65.0%"
      snprintf(line1, sizeof(line1), "T:%-4.1fC  H:%-3.0f%%",
               temperature, humidity);
    } else {
      snprintf(line1, sizeof(line1), "DHT Error!      ");
    }

    // Line 2: motion status (fits within 16 chars)
    char line2[17];
    if (pirMotion && rcwlMotion) {
      snprintf(line2, sizeof(line2), "PIR+Radar Motion");  // 16 chars
    } else if (pirMotion) {
      snprintf(line2, sizeof(line2), "PIR: Motion     ");
    } else if (rcwlMotion) {
      snprintf(line2, sizeof(line2), "Radar: Motion   ");
    } else {
      snprintf(line2, sizeof(line2), "No Motion       ");
    }

    // ── Update LCD ────────────────────────────
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);

    // ── Serial Monitor output ─────────────────
    Serial.println("------ Sensor Readings ------");
    if (dhtOk) {
      Serial.printf("  Temperature : %.1f °C\n", temperature);
      Serial.printf("  Humidity    : %.1f %%\n",  humidity);
    } else {
      Serial.println("  DHT11       : Read failed – check wiring");
    }
    Serial.printf("  PIR Sensor  : %s\n", pirMotion  ? "Motion Detected" : "No Motion");
    Serial.printf("  RCWL-0516   : %s\n", rcwlMotion ? "Motion Detected" : "No Motion");
    Serial.println("-----------------------------\n");
  }

  // Other non-blocking tasks can go here without being blocked.
}
