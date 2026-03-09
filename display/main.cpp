#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <stdint.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Stepper.h>

// ===== OLED =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== Hardware =====
const int LED_PIN = D9;
const int BUTTON_PIN = D7;

// ===== Stepper =====
const int STEPS_PER_REV = 2048; 
Stepper stepperMotor(STEPS_PER_REV, D0, D2, D1, D3);

// ===== ESP-NOW Struct =====
typedef struct __attribute__((packed)) {
  uint32_t seq;
  uint16_t bpm;
  uint8_t finger; 
  uint8_t state;  
} HeartPacket;

volatile bool newData = false;
HeartPacket incoming = {0, 0, 0, 0};

unsigned long lastPacketTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastStepTime = 0;
unsigned long lastUiRefresh = 0;
bool ledState = false;

// =========================
// UI Helpers
// =========================
bool linkAlive() {
  return (millis() - lastPacketTime) < 2000;
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Torque Mode");
  display.setCursor(95, 0);
  display.print(linkAlive() ? "OK" : "??");

  display.setTextSize(2);
  display.setCursor(0, 20);
  
  if (linkAlive() && incoming.finger) {
    display.println("SPINNING");
    display.setTextSize(1);
    display.println("Slow & Strong");
  } else {
    display.println("IDLE");
    display.setTextSize(1);
    display.println("Waiting...");
  }

  display.drawFastHLine(0, 54, 128, WHITE);
  display.setCursor(0, 57);
  display.setTextSize(1);
  display.print("MAC: ");
  display.print(WiFi.macAddress());
  display.display();
}

void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len == sizeof(HeartPacket)) {
    memcpy((void*)&incoming, data, sizeof(HeartPacket));
    lastPacketTime = millis();
    newData = true;
  }
}

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(D4, D5);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("FIND_ME", "12345678", 1); 

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onReceive);
  }

  // VERY SLOW SPEED: 5 RPM provides the highest torque.
  // This helps the motor start spinning without needing a nudge.
  stepperMotor.setSpeed(5); 
  
  display.clearDisplay();
  display.setCursor(0,10);
  display.println("MAC: " + WiFi.macAddress());
  display.display();
  delay(3000);
}

// =========================
// Loop
// =========================
void loop() {
  unsigned long now = millis();

  if (now - lastBlinkTime > 500) {
    lastBlinkTime = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  if (newData || (now - lastUiRefresh > 1000)) {
    newData = false;
    lastUiRefresh = now;
    drawScreen();
  }

  // Motor Control
  if (linkAlive() && incoming.finger) {
    // Smaller steps (32) more frequently (every 5ms)
    // This keeps the motor "energized" and makes it harder to stall.
    if (now - lastStepTime > 5) {
      lastStepTime = now;
      stepperMotor.step(32); 
    }
  }

  delay(1);
}
