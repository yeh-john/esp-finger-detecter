#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ===== Hardware =====
const int PULSE_PIN = D0;
const int LED_PIN = D2;

// ===== Target Config =====
const char* TARGET_NAME = "FIND_ME";
uint8_t TARGET_MAC[] = {0x98, 0x3D, 0xAE, 0xAA, 0x71, 0x99};

// ===== Detection Tuning =====
const int FINGER_THRESHOLD = 3000;
const int MARGIN = 130;
const int MIN_BPM = 45;
const int MAX_BPM = 180;
const unsigned long REFRACTORY_MS = 350;

// ===== State =====
bool connected = false;
uint8_t current_target[6];

typedef struct __attribute__((packed)) {
  uint32_t seq;
  uint16_t bpm;
  uint8_t finger;
  uint8_t state;
} HeartPacket;

HeartPacket packet = {0, 0, 0, 0};

// BPM Variables
float baseline = 0;
bool baselineInitialized = false;
bool inBeat = false;
unsigned long lastBeatTime = 0;
int bpmHistory[5] = {0};
int bpmIndex = 0;
int stableBpm = 0;

// =========================
// AUTO-HUNT LOGIC
// =========================
bool autoHunt() {
  Serial.println("\n[SEARCHING] Looking for " + String(TARGET_NAME) + "...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int n = WiFi.scanNetworks();
  if (n <= 0) return false;

  int nameMatchIdx = -1;
  int nameMatchCount = 0;
  bool macFound = false;

  for (int i = 0; i < n; i++) {
    // Check Name
    if (WiFi.SSID(i) == TARGET_NAME) {
      nameMatchIdx = i;
      nameMatchCount++;
    }
    // Check MAC
    uint8_t* b = WiFi.BSSID(i);
    if (memcmp(b, TARGET_MAC, 6) == 0) {
      macFound = true;
    }
  }

  // CONNECTION PRIORITY LOGIC
  // 1. If exactly one name match, use it.
  if (nameMatchCount == 1) {
    memcpy(current_target, WiFi.BSSID(nameMatchIdx), 6);
    Serial.println("[MATCH] Found unique " + String(TARGET_NAME));
    return true;
  }
  
  // 2. If multiple names found or name not found, fallback to specific MAC
  if (macFound) {
    memcpy(current_target, TARGET_MAC, 6);
    Serial.println("[MATCH] Found Target by MAC address.");
    return true;
  }

  return false;
}

void setupESPNOW() {
  if (esp_now_init() != ESP_OK) return;

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, current_target, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) == ESP_OK) {
    connected = true;
    Serial.println("[OK] ESP-NOW Link Established.");
  }
}

// =========================
// SENSOR LOGIC
// =========================
void processBPM(int raw) {
  unsigned long now = millis();
  bool finger = (raw < FINGER_THRESHOLD);
  packet.finger = finger ? 1 : 0;

  if (!finger) {
    stableBpm = 0;
    baselineInitialized = false;
    return;
  }

  if (!baselineInitialized) { baseline = raw; baselineInitialized = true; return; }
  
  baseline = baseline * 0.98f + raw * 0.02f;
  int threshold = (int)baseline + MARGIN;

  if (!inBeat && raw > threshold && (now - lastBeatTime > REFRACTORY_MS)) {
    inBeat = true;
    if (lastBeatTime > 0) {
      int currentBpm = 60000 / (now - lastBeatTime);
      if (currentBpm >= MIN_BPM && currentBpm <= MAX_BPM) {
        bpmHistory[bpmIndex] = currentBpm;
        bpmIndex = (bpmIndex + 1) % 5;
        int sum = 0;
        for(int i=0; i<5; i++) sum += bpmHistory[i];
        stableBpm = sum / 5;
      }
    }
    lastBeatTime = now;
  }
  if (inBeat && raw < baseline) inBeat = false;
}

// =========================
// MAIN
// =========================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);

  // Standalone loop: Scan until connected
  while (!connected) {
    // Blink while hunting
    digitalWrite(LED_PIN, HIGH);
    if (autoHunt()) {
      setupESPNOW();
    } else {
      Serial.println("[RETRY] Target not found. Retrying in 5s...");
      digitalWrite(LED_PIN, LOW);
      delay(5000);
    }
  }
  
  // Solid light for 1s to signal connection success
  digitalWrite(LED_PIN, HIGH);
  delay(1000);
  digitalWrite(LED_PIN, LOW);
}

void loop() {
  int raw = analogRead(PULSE_PIN);
  processBPM(raw);

  static unsigned long lastSend = 0;
  if (millis() - lastSend > 800) {
    lastSend = millis();

    packet.seq++;
    packet.bpm = (uint16_t)stableBpm;
    packet.state = (stableBpm == 0) ? 0 : (stableBpm < 60 ? 1 : (stableBpm < 100 ? 2 : 3));

    esp_now_send(current_target, (uint8_t*)&packet, sizeof(packet));
    
    // Status over Serial for testing
    Serial.printf("BPM: %d | Finger: %d\n", packet.bpm, packet.finger);
  }
  delay(10);
}
