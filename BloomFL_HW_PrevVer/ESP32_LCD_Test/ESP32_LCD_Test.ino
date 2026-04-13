#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Serial.begin(115200);

  Wire.begin(21, 22);

  lcd.init();
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Hello, ESP32!");
  lcd.setCursor(0, 1);
  lcd.print("Dim effect ON");
}

void loop() {
  lcd.backlight();
  delay(10);      // ON time

  lcd.noBacklight();
  delay(40);      // OFF time → more OFF = dimmer
}