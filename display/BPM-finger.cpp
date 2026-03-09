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
// FIXED: Using the working sequence D0, D2, D1, D3
Stepper stepperMotor(STEPS_PER_REV, D0, D2, D1, D3);

// ===== ESP-NOW =====
static const uint8_t ESPNOW_CHANNEL = 1;

typedef struct __attribute__((packed)) {
  uint32_t seq;
  uint16_t bpm;
  uint8_t finger;   // 0/1
  uint8_t state;    // 0=NO_FINGER 1=SLOW 2=NORMAL 3=FAST
} HeartPacket;

volatile bool newData = false;
HeartPacket incoming = {0, 0, 0, 0};

unsigned long lastPacketTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastStepTime = 0;
unsigned long lastUiRefresh = 0;
unsigned long lastDebugPrint = 0;
bool ledState = false;

// =========================
// UI
// =========================
String getStateText(uint8_t state) {
  if (state == 0) return "NO FINGER";
  if (state == 1) return "SLOW";
  if (state == 2) return "NORMAL";
  if (state == 3) return "FAST";
  return "UNKNOWN";
}

bool linkAlive() {
  return (millis() - lastPacketTime) < 2000;
}

void drawBootScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("BOOTING...");
  display.setCursor(0, 15);
  display.println("NAME: FIND_ME");
  display.setCursor(0, 30);
  display.println("MAC ADDRESS:");
  display.println(WiFi.macAddress());
  display.display();
  delay(3000); 
}

void drawScreen() {
  display.clearDisplay();
  display.setTextColor(WHITE);

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Heart Monitor");
  display.setCursor(85, 0);
  display.print(linkAlive() ? " [OK]" : "[LOST]");

  display.setTextSize(2);
  display.setCursor(0, 18);

  if (linkAlive() && incoming.finger) {
    display.print(incoming.bpm);
    display.setTextSize(1);
    display.print(" BPM");
  } else {
    display.print("-- BPM");
  }

  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Status: ");
  display.println(linkAlive() ? getStateText(incoming.state) : "WAITING...");

  display.drawFastHLine(0, 52, 128, WHITE);
  display.setCursor(0, 56);
  display.setTextSize(1);
  display.print("MAC: ");
  display.print(WiFi.macAddress());

  display.display();
}

// =========================
// Stepper Timing
// =========================
int getStepDelay(uint8_t state) {
  if (state == 1) return 100;
  if (state == 2) return 45;
  if (state == 3) return 20;
  return 200;
}

int getStepBurst(uint8_t state) {
  if (state == 1) return 4;
  if (state == 2) return 6;
  if (state == 3) return 10;
  return 0;
}

// =========================
// ESP-NOW receive callback
// =========================
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  if (len != sizeof(HeartPacket)) return;
  memcpy((void*)&incoming, data, sizeof(HeartPacket));
  lastPacketTime = millis();
  newData = true;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Wire.begin(D4, D5);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    while (true) { digitalWrite(LED_PIN, !digitalRead(LED_PIN)); delay(200); }
  }

  // Set up as Access Point so the Sender can find the "FIND_ME" name
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("FIND_ME", "12345678", 1); 
  delay(100);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onReceive);
  }

  drawBootScreen();
  stepperMotor.setSpeed(10); 
  drawScreen();
}

void loop() {
  unsigned long now = millis();

  // Blink LED
  if (now - lastBlinkTime > 500) {
    lastBlinkTime = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
  }

  // Refresh Screen
  if (newData || (now - lastUiRefresh > 1000)) {
    newData = false;
    lastUiRefresh = now;
    drawScreen();
  }

  // Handle Pause Button
  bool paused = (digitalRead(BUTTON_PIN) == LOW);
  if (paused) return;

  // Motor Control
  if (linkAlive() && incoming.finger) {
    int stepDelay = getStepDelay(incoming.state);
    int burst = getStepBurst(incoming.state);

    if (burst > 0 && (now - lastStepTime > (unsigned long)stepDelay)) {
      lastStepTime = now;
      stepperMotor.step(burst); 
    }
  }

  delay(1);
}
