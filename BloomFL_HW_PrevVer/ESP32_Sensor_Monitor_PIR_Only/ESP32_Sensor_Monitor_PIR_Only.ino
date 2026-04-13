#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ── Pin definitions ─────────────────────────────────────────
#define PIR_PIN   27
#define DHT_PIN    4
#define DHT_TYPE  DHT11

// ── Timing ──────────────────────────────────────────────────
const unsigned long INTERVAL = 2000;
unsigned long lastUpdate = 0;

// ── Objects ─────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHT_PIN, DHT_TYPE);

// ── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=============================");
  Serial.println("  ESP32 Sensor Monitor");
  Serial.println("=============================\n");

  pinMode(PIR_PIN, INPUT);
  dht.begin();

  // I2C + LCD init (double init fixes ESP32 boot timing issue)
  Wire.begin(21, 22);
  delay(100);
  lcd.init();
  delay(50);
  lcd.init();
  lcd.backlight();

  // Splash screen (also lets DHT11 stabilise)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("  Sensor Monitor");
  lcd.setCursor(0, 1);
  lcd.print("  Starting...   ");
  delay(2000);
  lcd.clear();

  Serial.println("Sensors initialised. Readings every 2 seconds.\n");
}

// ── Main loop ────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastUpdate >= INTERVAL) {
    lastUpdate = now;

    // ── Read sensors ────────────────────────────────────────
    bool pirDetected  = digitalRead(PIR_PIN) == HIGH;

    float temperature = dht.readTemperature();  // °C
    float humidity    = dht.readHumidity();
    bool  dhtOk       = !isnan(temperature) && !isnan(humidity);

    // ── Motion verdict ──────────────────────────────────────
    const char* motionVerdict = pirDetected ? "Motion Detected!" : "No Motion       ";

    // ── Build LCD line 1: temperature & humidity ────────────
    char line1[17];
    if (dhtOk) {
      snprintf(line1, sizeof(line1), "T:%-4.1fC  H:%-3.0f%%", temperature, humidity);
    } else {
      snprintf(line1, sizeof(line1), "DHT Error!      ");
    }

    // ── Update LCD ──────────────────────────────────────────
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(motionVerdict);

    // ── Serial Monitor output ───────────────────────────────
    Serial.println("─────────────────────────────");
    if (dhtOk) {
      Serial.printf("  Temperature : %.1f °C\n", temperature);
      Serial.printf("  Humidity    : %.1f %%\n",  humidity);
    } else {
      Serial.println("  DHT11       : Read failed");
    }
    Serial.printf("  PIR Sensor  : %s\n", pirDetected ? "Motion Detected" : "No Motion");
    Serial.println("─────────────────────────────\n");
  }
}
