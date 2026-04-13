#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include "esp_camera.h"

// ============= CONFIGURATION =============
// Wi-Fi Access Point Settings
const char* AP_SSID = "Koundal";
const char* AP_PASSWORD = "JaiBholenath";  // Must be at least 8 characters

// Camera Settings
#define QQVGA_WIDTH  160
#define QQVGA_HEIGHT 120
#define BYTES_PER_PIXEL 2  // RGB565 format

// OV7670 I2C Address
#define OV7670_I2C_ADDRESS 0x21

// Pin Definitions
#define PIN_SIOC    22  // I2C Clock
#define PIN_SIOD    21  // I2C Data
#define PIN_XCLK    4   // Master clock
#define PIN_PCLK    18  // Pixel clock
#define PIN_VSYNC   25  // Vertical sync
#define PIN_HREF    23  // Horizontal reference
#define PIN_D0      32
#define PIN_D1      33
#define PIN_D2      34
#define PIN_D3      35
#define PIN_D4      27
#define PIN_D5      26
#define PIN_D6      19
#define PIN_D7      5

// File Settings
#define IMAGE_FILE_PATH "/image.raw"

// ============= GLOBAL VARIABLES =============
WebServer server(80);
uint8_t* frameBuffer = nullptr;
const size_t frameSize = QQVGA_WIDTH * QQVGA_HEIGHT * BYTES_PER_PIXEL;
bool cameraInitialized = false;

// ============= OV7670 REGISTER DEFINITIONS =============
// Key OV7670 registers
#define OV7670_REG_GAIN       0x00
#define OV7670_REG_BLUE       0x01
#define OV7670_REG_RED        0x02
#define OV7670_REG_VREF       0x03
#define OV7670_REG_COM1       0x04
#define OV7670_REG_BAVE       0x05
#define OV7670_REG_AECHH      0x07
#define OV7670_REG_PIDH       0x0A  // Product ID High
#define OV7670_REG_PIDL       0x0B  // Product ID Low
#define OV7670_REG_COM3       0x0C
#define OV7670_REG_COM7       0x12  // Common Control 7
#define OV7670_REG_COM8       0x13
#define OV7670_REG_COM9       0x14
#define OV7670_REG_COM10      0x15
#define OV7670_REG_HSTART     0x17
#define OV7670_REG_HSTOP      0x18
#define OV7670_REG_VSTRT      0x19
#define OV7670_REG_VSTOP      0x1A
#define OV7670_REG_MVFP       0x1E  // Mirror/VFlip
#define OV7670_REG_CLKRC      0x11  // Clock control
#define OV7670_REG_DBLV       0x6B
#define OV7670_REG_SCALING_XSC 0x70
#define OV7670_REG_SCALING_YSC 0x71
#define OV7670_REG_SCALING_DCWCTR 0x72
#define OV7670_REG_SCALING_PCLK_DIV 0x73

// COM7 bits
#define COM7_RGB       0x04
#define COM7_QVGA      0x10
#define COM7_QCIF      0x08

// ============= OV7670 I2C FUNCTIONS =============

/**
 * Write a byte to OV7670 register via I2C
 */
bool writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(OV7670_I2C_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  uint8_t error = Wire.endTransmission();
  delay(1);  // OV7670 needs time between register writes
  return (error == 0);
}

/**
 * Read a byte from OV7670 register via I2C
 */
uint8_t readRegister(uint8_t reg) {
  Wire.beginTransmission(OV7670_I2C_ADDRESS);
  Wire.write(reg);
  Wire.endTransmission();
  
  Wire.requestFrom(OV7670_I2C_ADDRESS, 1);
  if (Wire.available()) {
    return Wire.read();
  }
  return 0xFF;  // Error value
}

/**
 * Initialize OV7670 camera with QQVGA RGB565 settings
 */
bool initOV7670() {
  Serial.println("Initializing OV7670...");

  uint8_t pidh = readRegister(OV7670_REG_PIDH);
  uint8_t pidl = readRegister(OV7670_REG_PIDL);
  Serial.printf("Camera Product ID: 0x%02X 0x%02X\n", pidh, pidl);

  if (pidh == 0xFF || pidh == 0x00) {
    Serial.println("ERROR: No valid response from camera. Check wiring, XCLK, power, and SCCB lines.");
    return false;
  }
  
  // Reset all registers to default
  if (!writeRegister(OV7670_REG_COM7, 0x80)) {
    Serial.println("ERROR: Failed to reset camera");
    return false;
  }
  delay(100);  // Wait for reset to complete
  
  // Configure for QQVGA RGB565
  // COM7: RGB output, QVGA mode
  writeRegister(OV7670_REG_COM7, COM7_RGB);
  delay(10);
  
  // Clock settings - divide internal clock
  writeRegister(OV7670_REG_CLKRC, 0x01);  // Use external clock with prescaler
  
  // COM3: Enable scaling
  writeRegister(OV7670_REG_COM3, 0x04);
  
  // COM14: Manual scaling, PCLK divider
  writeRegister(0x3E, 0x00);
  
  // Scaling settings for QQVGA (160x120)
  writeRegister(OV7670_REG_SCALING_XSC, 0x3A);
  writeRegister(OV7670_REG_SCALING_YSC, 0x35);
  writeRegister(OV7670_REG_SCALING_DCWCTR, 0x22);  // Downsample by 4
  writeRegister(OV7670_REG_SCALING_PCLK_DIV, 0xF2); // Divide PCLK by 4
  
  // COM15: RGB565 output format
  writeRegister(0x40, 0xD0);
  
  // Set RGB565 format
  writeRegister(0x12, 0x14);  // COM7: RGB
  writeRegister(0x8C, 0x00);  // RGB444 disabled
  writeRegister(0x04, 0x00);  // COM1
  
  // Window settings
  writeRegister(OV7670_REG_HSTART, 0x16);
  writeRegister(OV7670_REG_HSTOP, 0x04);
  writeRegister(OV7670_REG_VSTRT, 0x02);
  writeRegister(OV7670_REG_VSTOP, 0x7a);
  writeRegister(OV7670_REG_VREF, 0x0a);
  
  // Additional settings for better image quality
  writeRegister(OV7670_REG_COM8, 0xE7);  // Enable AGC, AWB, AEC
  writeRegister(OV7670_REG_COM9, 0x4A);  // Max AGC value
  writeRegister(OV7670_REG_COM10, 0x00); // VSYNC negative
  
  // Disable some features to reduce complexity
  writeRegister(0x0E, 0x61);  // COM5
  writeRegister(0x0F, 0x4B);  // COM6
  
  // Color matrix and edge enhancement
  writeRegister(0x16, 0x02);  // Reserved
  writeRegister(0x1E, 0x00);  // MVFP: No mirror/vflip
  writeRegister(0x21, 0x02);  // ADCCTR1
  writeRegister(0x22, 0x91);  // ADCCTR2
  writeRegister(0x29, 0x07);  // RSVD
  writeRegister(0x33, 0x0B);  // CHLF
  writeRegister(0x35, 0x0B);  // RSVD
  writeRegister(0x37, 0x1D);  // ADC
  writeRegister(0x38, 0x71);  // ACOM
  writeRegister(0x39, 0x2A);  // OFON
  writeRegister(0x3C, 0x78);  // COM12
  writeRegister(0x4D, 0x40);  // RSVD
  writeRegister(0x4E, 0x20);  // RSVD
  writeRegister(0x69, 0x00);  // GFIX
  writeRegister(0x6B, 0x4A);  // DBLV
  writeRegister(0x74, 0x10);  // REG74
  writeRegister(0x8D, 0x4F);  // RSVD
  writeRegister(0x8E, 0x00);  // RSVD
  writeRegister(0x8F, 0x00);  // RSVD
  writeRegister(0x90, 0x00);  // RSVD
  writeRegister(0x91, 0x00);  // RSVD
  writeRegister(0x96, 0x00);  // RSVD
  writeRegister(0x9A, 0x00);  // RSVD
  writeRegister(0xB0, 0x84);  // ABLC1
  writeRegister(0xB1, 0x0C);  // RSVD
  writeRegister(0xB2, 0x0E);  // RSVD
  writeRegister(0xB3, 0x82);  // THL_ST
  writeRegister(0xB8, 0x0A);  // RSVD
  
  delay(100);
  Serial.println("OV7670 initialized successfully!");
  return true;
}

// ============= CLOCK GENERATION =============

/**
 * Generate XCLK signal using LEDC (PWM)
 * OV7670 typically needs 10-48MHz clock; we'll use ~8MHz
 */
void setupXCLK() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(PIN_XCLK, 10000000, 1)) {  // try 10 MHz first
    Serial.println("ERROR: ledcAttach failed");
    return;
  }
  ledcWrite(PIN_XCLK, 1);
#else
  ledcSetup(0, 10000000, 1);
  ledcAttachPin(PIN_XCLK, 0);
  ledcWrite(0, 1);
#endif

  Serial.println("XCLK = 10 MHz started");
  delay(300);
}
// ============= FRAME CAPTURE =============

/**
 * Read one byte from the 8-bit parallel data bus (D0-D7)
 */
inline uint8_t readDataByte() {
  uint8_t data = 0;
  data |= (digitalRead(PIN_D0) << 0);
  data |= (digitalRead(PIN_D1) << 1);
  data |= (digitalRead(PIN_D2) << 2);
  data |= (digitalRead(PIN_D3) << 3);
  data |= (digitalRead(PIN_D4) << 4);
  data |= (digitalRead(PIN_D5) << 5);
  data |= (digitalRead(PIN_D6) << 6);
  data |= (digitalRead(PIN_D7) << 7);
  return data;
}

/**
 * Capture a frame from OV7670
 * WARNING: This is a simplified bit-banging approach.
 * For production, use DMA with I2S peripheral.
 */
bool captureFrame() {
  if (frameBuffer == nullptr) {
    Serial.println("ERROR: Frame buffer not allocated!");
    return false;
  }
  
  Serial.println("Starting frame capture...");
  uint32_t startTime = millis();
  size_t byteIndex = 0;
  
  // Wait for VSYNC to go HIGH (start of frame)
  Serial.println("Waiting for VSYNC HIGH...");
  uint32_t timeout = millis();
  while (digitalRead(PIN_VSYNC) == LOW) {
    if (millis() - timeout > 1000) {
      Serial.println("ERROR: VSYNC timeout (waiting for HIGH)");
      return false;
    }
  }
  
  // Wait for VSYNC to go LOW (frame starts)
  Serial.println("Waiting for VSYNC LOW (frame start)...");
  timeout = millis();
  while (digitalRead(PIN_VSYNC) == HIGH) {
    if (millis() - timeout > 1000) {
      Serial.println("ERROR: VSYNC timeout (waiting for LOW)");
      return false;
    }
  }
  
  Serial.println("Capturing frame data...");
  
  // Capture pixel data
  // For QQVGA RGB565: 160 * 120 * 2 = 38,400 bytes
  while (byteIndex < frameSize) {
    // Wait for PCLK rising edge
    while (digitalRead(PIN_PCLK) == LOW) {
      // Check if we've lost sync (VSYNC went HIGH again)
      if (digitalRead(PIN_VSYNC) == HIGH) {
        if (byteIndex > 0) {
          Serial.printf("Frame capture incomplete: %d/%d bytes\n", byteIndex, frameSize);
          return false;
        }
      }
    }
    
    // Check HREF - only read when HREF is HIGH
    if (digitalRead(PIN_HREF) == HIGH) {
      frameBuffer[byteIndex++] = readDataByte();
      
      // Progress indicator every 1000 bytes
      if (byteIndex % 1000 == 0) {
        Serial.printf("  Progress: %d/%d bytes\n", byteIndex, frameSize);
      }
    }
    
    // Wait for PCLK to go LOW again
    while (digitalRead(PIN_PCLK) == HIGH) {}
  }
  
  uint32_t captureTime = millis() - startTime;
  Serial.printf("Frame captured successfully! %d bytes in %d ms\n", byteIndex, captureTime);
  
  return true;
}

// ============= SPIFFS FUNCTIONS =============

/**
 * Save frame buffer to SPIFFS
 */
bool saveFrameToSPIFFS() {
  if (frameBuffer == nullptr) {
    Serial.println("ERROR: No frame data to save!");
    return false;
  }
  
  Serial.println("Saving frame to SPIFFS...");
  
  File file = SPIFFS.open(IMAGE_FILE_PATH, FILE_WRITE);
  if (!file) {
    Serial.println("ERROR: Failed to open file for writing!");
    return false;
  }
  
  size_t written = file.write(frameBuffer, frameSize);
  file.close();
  
  if (written == frameSize) {
    Serial.printf("Frame saved successfully! (%d bytes)\n", written);
    return true;
  } else {
    Serial.printf("ERROR: Only wrote %d of %d bytes!\n", written, frameSize);
    return false;
  }
}

/**
 * Read saved frame from SPIFFS
 */
bool loadFrameFromSPIFFS() {
  if (!SPIFFS.exists(IMAGE_FILE_PATH)) {
    Serial.println("No saved image found!");
    return false;
  }
  
  File file = SPIFFS.open(IMAGE_FILE_PATH, FILE_READ);
  if (!file) {
    Serial.println("ERROR: Failed to open file for reading!");
    return false;
  }
  
  size_t fileSize = file.size();
  Serial.printf("Loading image from SPIFFS (%d bytes)...\n", fileSize);
  
  if (frameBuffer == nullptr) {
    frameBuffer = (uint8_t*)malloc(frameSize);
  }
  
  size_t bytesRead = file.read(frameBuffer, fileSize);
  file.close();
  
  Serial.printf("Loaded %d bytes from SPIFFS\n", bytesRead);
  return (bytesRead == fileSize);
}

// ============= HTTP SERVER HANDLERS =============

/**
 * Root page - simple web interface
 */
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><title>ESP32 Camera</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial; text-align: center; margin: 20px; background: #f0f0f0; }";
  html += "button { background: #4CAF50; color: white; padding: 15px 30px; font-size: 18px; ";
  html += "border: none; border-radius: 5px; cursor: pointer; margin: 10px; }";
  html += "button:hover { background: #45a049; }";
  html += ".info { background: white; padding: 20px; border-radius: 10px; margin: 20px auto; max-width: 600px; }";
  html += "</style></head><body>";
  html += "<h1>ESP32 OV7670 Camera</h1>";
  html += "<div class='info'>";
  html += "<p><strong>Resolution:</strong> " + String(QQVGA_WIDTH) + "x" + String(QQVGA_HEIGHT) + " (QQVGA)</p>";
  html += "<p><strong>Format:</strong> RGB565 RAW</p>";
  html += "<p><strong>File Size:</strong> " + String(frameSize) + " bytes</p>";
  
  // Check if image exists
  if (SPIFFS.exists(IMAGE_FILE_PATH)) {
    File file = SPIFFS.open(IMAGE_FILE_PATH, FILE_READ);
    html += "<p><strong>Saved Image:</strong> " + String(file.size()) + " bytes</p>";
    file.close();
  } else {
    html += "<p><strong>Saved Image:</strong> None</p>";
  }
  
  html += "</div>";
  html += "<button onclick='capture()'>Capture New Image</button><br>";
  html += "<button onclick='download()'>Download RAW Image</button><br>";
  html += "<div id='status' style='margin-top: 20px; font-size: 16px;'></div>";
  html += "<script>";
  html += "function capture() {";
  html += "  document.getElementById('status').innerHTML = 'Capturing image, please wait...';";
  html += "  fetch('/capture').then(r => r.text()).then(d => {";
  html += "    document.getElementById('status').innerHTML = d;";
  html += "    setTimeout(() => location.reload(), 2000);";
  html += "  });";
  html += "}";
  html += "function download() {";
  html += "  window.location = '/download';";
  html += "}";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}

/**
 * /capture endpoint - triggers image capture
 */
void handleCapture() {
  Serial.println("\n=== HTTP /capture request received ===");
  
  if (!cameraInitialized) {
    server.send(500, "text/plain", "ERROR: Camera not initialized!");
    return;
  }
  
  // Capture frame
  bool success = captureFrame();
  
  if (!success) {
    server.send(500, "text/plain", "ERROR: Failed to capture frame. Check serial monitor for details.");
    return;
  }
  
  // Save to SPIFFS
  if (!saveFrameToSPIFFS()) {
    server.send(500, "text/plain", "ERROR: Failed to save frame to SPIFFS!");
    return;
  }
  
  server.send(200, "text/plain", "SUCCESS: Image captured and saved! You can now download it.");
}

/**
 * /download endpoint - serves the RAW image file
 */
void handleDownload() {
  Serial.println("\n=== HTTP /download request received ===");
  
  if (!SPIFFS.exists(IMAGE_FILE_PATH)) {
    server.send(404, "text/plain", "ERROR: No image file found! Capture an image first.");
    return;
  }
  
  File file = SPIFFS.open(IMAGE_FILE_PATH, FILE_READ);
  if (!file) {
    server.send(500, "text/plain", "ERROR: Failed to open image file!");
    return;
  }
  
  // Stream file to client
  server.sendHeader("Content-Disposition", "attachment; filename=image_160x120_rgb565.raw");
  server.streamFile(file, "application/octet-stream");
  file.close();
  
  Serial.println("Image file sent successfully!");
}

/**
 * /info endpoint - system information
 */
void handleInfo() {
  String info = "ESP32 OV7670 Camera System\n\n";
  info += "Resolution: " + String(QQVGA_WIDTH) + "x" + String(QQVGA_HEIGHT) + "\n";
  info += "Format: RGB565 (2 bytes per pixel)\n";
  info += "Frame Size: " + String(frameSize) + " bytes\n";
  info += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  info += "SPIFFS Total: " + String(SPIFFS.totalBytes()) + " bytes\n";
  info += "SPIFFS Used: " + String(SPIFFS.usedBytes()) + " bytes\n";
  info += "\nCamera Status: " + String(cameraInitialized ? "Initialized" : "Not Initialized") + "\n";
  
  if (SPIFFS.exists(IMAGE_FILE_PATH)) {
    File file = SPIFFS.open(IMAGE_FILE_PATH, FILE_READ);
    info += "Saved Image Size: " + String(file.size()) + " bytes\n";
    file.close();
  } else {
    info += "Saved Image: None\n";
  }
  
  server.send(200, "text/plain", info);
}

// ============= SETUP =============

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=================================");
  Serial.println("ESP32 OV7670 Camera Server");
  Serial.println("=================================\n");
  
  // Allocate frame buffer
  Serial.printf("Allocating frame buffer (%d bytes)...\n", frameSize);
  frameBuffer = (uint8_t*)malloc(frameSize);
  if (frameBuffer == nullptr) {
    Serial.println("FATAL ERROR: Failed to allocate frame buffer!");
    Serial.println("Not enough RAM available!");
    while (1) { delay(1000); }
  }
  Serial.println("Frame buffer allocated successfully!");
  
  // Initialize SPIFFS
  Serial.println("Initializing SPIFFS...");
  if (!SPIFFS.begin(true)) {  // Format on first failure
    Serial.println("ERROR: SPIFFS initialization failed!");
    while (1) { delay(1000); }
  }
  Serial.printf("SPIFFS mounted: Total=%d bytes, Used=%d bytes\n", 
                SPIFFS.totalBytes(), SPIFFS.usedBytes());
  
  // Setup camera pins
  Serial.println("Configuring camera pins...");
  
  // I2C pins (already handled by Wire.begin)
  Wire.begin(PIN_SIOD, PIN_SIOC);
  Wire.setClock(100000);  // 100kHz for OV7670
  
  // Control input pins
  pinMode(PIN_PCLK, INPUT);
  pinMode(PIN_VSYNC, INPUT);
  pinMode(PIN_HREF, INPUT);
  
  // Data input pins
  pinMode(PIN_D0, INPUT);
  pinMode(PIN_D1, INPUT);
  pinMode(PIN_D2, INPUT);
  pinMode(PIN_D3, INPUT);
  pinMode(PIN_D4, INPUT);
  pinMode(PIN_D5, INPUT);
  pinMode(PIN_D6, INPUT);
  pinMode(PIN_D7, INPUT);
  
  // Setup XCLK generation
  setupXCLK();
  delay(300);  // Give camera time to stabilize
  
  // Initialize OV7670
  cameraInitialized = initOV7670();
  if (!cameraInitialized) {
    Serial.println("\nWARNING: Camera initialization failed!");
    Serial.println("Server will still start, but capture won't work.");
    Serial.println("Check wiring and power supply!");
  }
  
  // Start Wi-Fi Access Point
  Serial.println("\nStarting Wi-Fi Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  IPAddress IP = WiFi.softAPIP();
  Serial.println("Access Point started!");
  Serial.printf("SSID: %s\n", AP_SSID);
  Serial.printf("Password: %s\n", AP_PASSWORD);
  Serial.printf("IP Address: %s\n", IP.toString().c_str());
  
  // Setup HTTP server
  Serial.println("\nStarting HTTP server...");
  server.on("/", handleRoot);
  server.on("/capture", handleCapture);
  server.on("/download", handleDownload);
  server.on("/info", handleInfo);
  server.begin();
  Serial.println("HTTP server started!");
  
  Serial.println("\n=================================");
  Serial.println("System Ready!");
  Serial.println("=================================");
  Serial.println("Connect to Wi-Fi:");
  Serial.printf("  SSID: %s\n", AP_SSID);
  Serial.printf("  Password: %s\n", AP_PASSWORD);
  Serial.printf("Then open browser: http://%s\n", IP.toString().c_str());
  Serial.println("=================================\n");
}

// ============= MAIN LOOP =============

void loop() {
  server.handleClient();
  delay(10);
}
