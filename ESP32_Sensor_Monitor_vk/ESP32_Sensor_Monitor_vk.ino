#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>

// ── WiFi Access Point credentials ───────────────────────────
const char* AP_SSID = "ESP32-Vish";
const char* AP_PASS = "JaiBholenath";

// ── File path ────────────────────────────────────────────────
const char* CSV_PATH = "/sensor_data.csv";

// ── Pin definitions ──────────────────────────────────────────
#define PIR_PIN        12
#define DHT_PIN        14
#define VIBRATION_PIN  16
#define BUZZER_PIN     13
#define RCWL_PIN       17
#define DHT_TYPE       DHT11

// ── Timing ───────────────────────────────────────────────────
const unsigned long READ_INTERVAL = 1000;   // 1 second
const unsigned long BEEP_DURATION  = 500;   // 0.5 second beep

// ── State ────────────────────────────────────────────────────
unsigned long lastReadTime    = 0;
unsigned long buzzerStartTime = 0;
bool          buzzerActive    = false;
unsigned long rowCount        = 0;   // tracks how many rows logged

// ── Objects ──────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT               dht(DHT_PIN, DHT_TYPE);
WebServer         server(80);

// ────────────────────────────────────────────────────────────
//  Helper: count rows already in the CSV (called at boot)
// ────────────────────────────────────────────────────────────
unsigned long countExistingRows() {
  File f = LittleFS.open(CSV_PATH, "r");
  if (!f) return 0;
  unsigned long count = 0;
  while (f.available()) {
    if (f.read() == '\n') count++;
  }
  f.close();
  // subtract 1 for the header line
  return (count > 0) ? count - 1 : 0;
}

// ────────────────────────────────────────────────────────────
//  Helper: get file size in KB
// ────────────────────────────────────────────────────────────
String fileSizeStr() {
  File f = LittleFS.open(CSV_PATH, "r");
  if (!f) return "0 KB";
  size_t sz = f.size();
  f.close();
  if (sz < 1024) return String(sz) + " B";
  return String(sz / 1024.0, 1) + " KB";
}

// ────────────────────────────────────────────────────────────
//  Web handler: root dashboard page
// ────────────────────────────────────────────────────────────
void handleRoot() {
  String fSize = fileSizeStr();

  String html = R"rawhtml(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="5">
<title>ESP32 Sensor Logger</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: sans-serif; background: #0f172a; color: #e2e8f0; min-height: 100vh; padding: 24px; }
  h1   { font-size: 22px; font-weight: 600; margin-bottom: 4px; color: #f8fafc; }
  p.sub { font-size: 13px; color: #94a3b8; margin-bottom: 24px; }
  .card { background: #1e293b; border: 1px solid #334155; border-radius: 12px;
          padding: 20px; margin-bottom: 16px; }
  .card h2 { font-size: 12px; text-transform: uppercase; letter-spacing: 1px;
             color: #64748b; margin-bottom: 12px; }
  .stat-row { display: flex; justify-content: space-between; align-items: center;
              padding: 6px 0; border-bottom: 1px solid #1e3a5f22; font-size: 14px; }
  .stat-row:last-child { border-bottom: none; }
  .stat-label { color: #94a3b8; }
  .stat-value { font-weight: 500; color: #e2e8f0; }
  .badge-green { background:#14532d; color:#86efac; padding:2px 10px;
                 border-radius:99px; font-size:12px; }
  .badge-red   { background:#450a0a; color:#fca5a5; padding:2px 10px;
                 border-radius:99px; font-size:12px; }
  .btn { display:inline-block; padding:10px 22px; border-radius:8px;
         font-size:14px; font-weight:500; text-decoration:none;
         cursor:pointer; border:none; }
  .btn-blue { background:#2563eb; color:#fff; }
  .btn-blue:hover { background:#1d4ed8; }
  .btn-red  { background:#dc2626; color:#fff; margin-left:10px; }
  .btn-red:hover  { background:#b91c1c; }
  .btn-row { margin-top: 4px; }
  .note { font-size:12px; color:#64748b; margin-top:10px; }
</style>
</head>
<body>
<h1>ESP32 Sensor Logger</h1>
<p class="sub">Auto-refreshes every 5 seconds &nbsp;|&nbsp; 192.168.4.1</p>

<div class="card">
  <h2>Log File</h2>
  <div class="stat-row">
    <span class="stat-label">File</span>
    <span class="stat-value">/sensor_data.csv</span>
  </div>
  <div class="stat-row">
    <span class="stat-label">Size</span>
    <span class="stat-value">)rawhtml" + fSize + R"rawhtml(</span>
  </div>
  <div class="stat-row">
    <span class="stat-label">Rows logged</span>
    <span class="stat-value">)rawhtml" + String(rowCount) + R"rawhtml(</span>
  </div>
  <div class="stat-row">
    <span class="stat-label">Sample rate</span>
    <span class="stat-value">1 per second</span>
  </div>
</div>

<div class="card">
  <h2>Actions</h2>
  <div class="btn-row">
    <a class="btn btn-blue" href="/data.csv" download="sensor_data.csv">
      &#8681; Download CSV
    </a>
    <a class="btn btn-red" href="/clear"
       onclick="return confirm('Delete all logged data?')">
      &#128465; Clear Data
    </a>
  </div>
  <p class="note">CSV columns: Timestamp_s, Temperature_C, Humidity_pct, PIR, Vibration, RCWL, Buzzer</p>
</div>

<div class="card">
  <h2>Storage</h2>
  <div class="stat-row">
    <span class="stat-label">Total flash</span>
    <span class="stat-value">)rawhtml" + String(LittleFS.totalBytes() / 1024) + R"rawhtml( KB</span>
  </div>
  <div class="stat-row">
    <span class="stat-label">Used</span>
    <span class="stat-value">)rawhtml" + String(LittleFS.usedBytes() / 1024) + R"rawhtml( KB</span>
  </div>
  <div class="stat-row">
    <span class="stat-label">Free</span>
    <span class="stat-value">)rawhtml" +
    String((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024) +
    R"rawhtml( KB</span>
  </div>
</div>
</body>
</html>)rawhtml";

  server.send(200, "text/html", html);
}

// ────────────────────────────────────────────────────────────
//  Web handler: serve CSV as a downloadable file
// ────────────────────────────────────────────────────────────
void handleDownload() {
  File f = LittleFS.open(CSV_PATH, "r");
  if (!f) {
    server.send(404, "text/plain", "No data file found yet.");
    return;
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"sensor_data.csv\"");
  server.streamFile(f, "text/csv");
  f.close();
}

// ────────────────────────────────────────────────────────────
//  Web handler: clear (delete) the CSV file
// ────────────────────────────────────────────────────────────
void handleClear() {
  LittleFS.remove(CSV_PATH);
  rowCount = 0;

  // Re-create file with header only
  File f = LittleFS.open(CSV_PATH, "w");
  if (f) {
    f.println("Timestamp_s,Temperature_C,Humidity_pct,PIR,Vibration,RCWL,Buzzer");
    f.close();
  }

  // Redirect back to dashboard
  server.sendHeader("Location", "/");
  server.send(303);

  Serial.println("[WEB] Data cleared by user.");
}

// ────────────────────────────────────────────────────────────
//  Append one row to the CSV
// ────────────────────────────────────────────────────────────
void appendCSV(unsigned long ts_s,
               float temp, float hum,
               bool pir, bool vib, bool rcwl, bool buzz) {
  File f = LittleFS.open(CSV_PATH, "a");
  if (!f) {
    Serial.println("[FS] ERROR: cannot open CSV for append!");
    return;
  }

  // Format: 0,28.5,65.0,1,0,0,0
  f.print(ts_s);       f.print(",");
  if (isnan(temp)) f.print("ERR"); else f.print(temp, 1);
  f.print(",");
  if (isnan(hum))  f.print("ERR"); else f.print(hum, 1);
  f.print(",");
  f.print(pir  ? 1 : 0); f.print(",");
  f.print(vib  ? 1 : 0); f.print(",");
  f.print(rcwl ? 1 : 0); f.print(",");
  f.println(buzz ? 1 : 0);

  f.close();
  rowCount++;
}

// ────────────────────────────────────────────────────────────
//  Setup
// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=============================");
  Serial.println("  ESP32 Multi-Sensor Monitor");
  Serial.println("=============================\n");

  // ── GPIO setup ─────────────────────────────────────────
  pinMode(PIR_PIN,       INPUT);
  pinMode(VIBRATION_PIN, INPUT_PULLUP);
  pinMode(RCWL_PIN,      INPUT);
  pinMode(BUZZER_PIN,    OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  dht.begin();

  // ── LCD setup ──────────────────────────────────────────
  Wire.begin(21, 22);
  delay(100);
  lcd.init(); delay(50); lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("  Sensor Monitor");
  lcd.setCursor(0, 1); lcd.print("  Starting...   ");

  // ── LittleFS ───────────────────────────────────────────
  if (!LittleFS.begin(true)) {    // true = format if mount fails
    Serial.println("[FS] LittleFS mount FAILED");
  } else {
    Serial.println("[FS] LittleFS mounted OK");

    // Write CSV header if file doesn't exist yet
    if (!LittleFS.exists(CSV_PATH)) {
      File f = LittleFS.open(CSV_PATH, "w");
      if (f) {
        f.println("Timestamp_s,Temperature_C,Humidity_pct,PIR,Vibration,RCWL,Buzzer");
        f.close();
        Serial.println("[FS] CSV created with header");
      }
    } else {
      rowCount = countExistingRows();
      Serial.printf("[FS] Existing CSV found — %lu rows\n", rowCount);
    }
  }

  // ── WiFi Access Point ──────────────────────────────────
  WiFi.softAP(AP_SSID, AP_PASS);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("[WiFi] AP started — SSID: %s  Pass: %s\n", AP_SSID, AP_PASS);
  Serial.printf("[WiFi] Dashboard: http://%s\n\n", ip.toString().c_str());

  // ── Web server routes ──────────────────────────────────
  server.on("/",          handleRoot);
  server.on("/data.csv",  handleDownload);
  server.on("/clear",     handleClear);
  server.begin();
  Serial.println("[Web] Server started on port 80");

  // ── LCD: show WiFi info for 3 seconds ──────────────────
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("ESP32-Vish");
  lcd.setCursor(0, 1); lcd.print("192.168.4.1     ");
  delay(3000);
  lcd.clear();

  Serial.println("All systems ready.\n");
}

// ────────────────────────────────────────────────────────────
//  Main loop
// ────────────────────────────────────────────────────────────
void loop() {
  server.handleClient();   // Must be called every loop

  unsigned long now = millis();

  // ── Non-blocking buzzer off ───────────────────────────
  if (buzzerActive && (now - buzzerStartTime >= BEEP_DURATION)) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
  }

  // ── Sensor read + log cycle ───────────────────────────
  if (now - lastReadTime >= READ_INTERVAL) {
    lastReadTime = now;

    unsigned long ts_s = now / 1000;

    bool  pirMotion  = digitalRead(PIR_PIN)       == HIGH;
    bool  vibration  = digitalRead(VIBRATION_PIN) == LOW;
    bool  rcwlMotion = digitalRead(RCWL_PIN)       == HIGH;
    float temp       = dht.readTemperature();
    float hum        = dht.readHumidity();
    bool  dhtOk      = !isnan(temp) && !isnan(hum);

    // Buzzer on PIR motion
    if (pirMotion && !buzzerActive) {
      digitalWrite(BUZZER_PIN, HIGH);
      buzzerStartTime = now;
      buzzerActive    = true;
    }

    // Log to CSV (buzzerActive reflects current beep state)
    appendCSV(ts_s, temp, hum, pirMotion, vibration, rcwlMotion, buzzerActive);

    // ── LCD ──────────────────────────────────────────────
    char line1[17];
    if (dhtOk) snprintf(line1, sizeof(line1), "T:%-4.1fC  H:%-3.0f%%", temp, hum);
    else        snprintf(line1, sizeof(line1), "DHT Error!      ");

    char line2[17];
    snprintf(line2, sizeof(line2), "PIR:%-3s VIB:%-3s ",
             pirMotion ? "ON " : "OFF",
             vibration ? "ON " : "OFF");

    lcd.setCursor(0, 0); lcd.print(line1);
    lcd.setCursor(0, 1); lcd.print(line2);

    // ── Serial ───────────────────────────────────────────
    Serial.println("─────────────────────────────");
    Serial.printf("  Timestamp   : %lu s\n",   ts_s);
    if (dhtOk) {
      Serial.printf("  Temperature : %.1f °C\n", temp);
      Serial.printf("  Humidity    : %.1f %%\n",  hum);
    } else {
      Serial.println("  DHT11       : Read failed");
    }
    Serial.printf("  PIR         : %s\n", pirMotion    ? "Motion"    : "No Motion");
    Serial.printf("  Vibration   : %s\n", vibration    ? "Detected"  : "None");
    Serial.printf("  RCWL Radar  : %s\n", rcwlMotion   ? "Motion"    : "No Motion");
    Serial.printf("  Buzzer      : %s\n", buzzerActive ? "BEEPING"   : "Silent");
    Serial.printf("  Rows logged : %lu\n", rowCount);
    Serial.println("─────────────────────────────\n");
  }
}
