
// ESP32-S3 (Controller)
// Author/Code designer: Joshua Pizana
// Purpose: Reads the two joystick axes, smooths them, turns them into percent values, 
// shows status on the OLED, and broadcasts control packets over ESP-NOW.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#include "joystick.h"
#include "ui_oled.h"

// -------------------- YOUR PINS --------------------
static constexpr int STEER_X_PIN    = 4;
static constexpr int THROTTLE_Y_PIN = 5;

// OLED I2C
static constexpr int OLED_SDA = 11;
static constexpr int OLED_SCL = 12;
static constexpr uint8_t OLED_ADDR = 0x3C;

// Timing
static constexpr uint32_t UI_PERIOD_MS   = 80;
static constexpr uint32_t SEND_PERIOD_MS = 50;   // 20 Hz heartbeat

// -------------------- PACKET --------------------
struct ControlPacket {
  uint32_t magic;     // "CAR!"
  uint16_t seq;
  int8_t   steerPct;  // -100..100
  int8_t   thrPct;    // -100..100
  uint8_t  flags;     // reserved
};
static constexpr uint32_t PKT_MAGIC = 0x43415221;

// -------------------- RECEIVER MACs --------------------
uint8_t MOTOR_MAC[6] = { 0xC0, 0xCD, 0xD6, 0xCA, 0x04, 0xA8 };
uint8_t SERVO_MAC[6] = { 0x00, 0x70, 0x07, 0x1C, 0x09, 0xF0 };

// -------------------- MODULES --------------------
Joystick2Axis js(STEER_X_PIN, THROTTLE_Y_PIN);
OledUI ui;

SteerState prevSteer = SteerState::CENTER;
ThrState   prevThr   = ThrState::STOP;

uint32_t lastUi = 0;
uint32_t lastSend = 0;
uint16_t seq = 0;

// We'll just show "TX" status; per-peer success tracking is optional and more complex on core 3.x
volatile esp_now_send_status_t lastStatus = ESP_NOW_SEND_FAIL;

// Core 3.x send callback signature:
static void onSent(const wifi_tx_info_t* info, esp_now_send_status_t status) {
  (void)info;
  lastStatus = status;
}

static bool addPeer(const uint8_t mac[6]) {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;     // 0 = current WiFi channel
  peer.encrypt = false; // start simple
  esp_err_t err = esp_now_add_peer(&peer);
  return (err == ESP_OK) || (err == ESP_ERR_ESPNOW_EXIST);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // OLED
  if (!ui.begin(OLED_SDA, OLED_SCL, OLED_ADDR)) {
    Serial.println("OLED init failed.");
    while (true) delay(100);
  }
  ui.showBoot("RC Controller", "ESP-NOW setup...", "Hold neutral");

  // Joystick
  js.begin();
  js.calibrate(60, 5);
  js.setDeadzone(400);
  js.setSmoothingN(8);
  js.setHysteresis(500, 250);
  js.setPercentDeadzone(6);

  Serial.printf("Centers: steer=%d throttle=%d\n", js.centerSteer(), js.centerThrottle());

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  Serial.print("Controller MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true) delay(100);
  }

  // Register callback (now correct signature)
  esp_now_register_send_cb(onSent);

  if (!addPeer(MOTOR_MAC)) Serial.println("Failed to add MOTOR peer");
  if (!addPeer(SERVO_MAC)) Serial.println("Failed to add SERVO peer");

  ui.draw("ESP-NOW OK",
          steerToText(SteerState::CENTER), 0,
          thrToText(ThrState::STOP), 0);
}

void loop() {
  js.update();

  SteerState steer = js.steerState();
  ThrState thr     = js.thrState();
  int8_t steerP    = js.steerPct();
  int8_t thrP      = js.throttlePct();

  if (steer != prevSteer) {
    Serial.printf("STEER -> %s (pct=%d)\n", steerToText(steer), (int)steerP);
    prevSteer = steer;
  }
  if (thr != prevThr) {
    Serial.printf("THROTTLE -> %s (pct=%d)\n", thrToText(thr), (int)thrP);
    prevThr = thr;
  }

  const uint32_t now = millis();

  // Heartbeat send
  if (now - lastSend >= SEND_PERIOD_MS) {
    lastSend = now;

    ControlPacket pkt{};
    pkt.magic = PKT_MAGIC;
    pkt.seq = ++seq;
    pkt.steerPct = steerP;
    pkt.thrPct = thrP;
    pkt.flags = 0;

    // Send to both receivers
    esp_now_send(MOTOR_MAC, (uint8_t*)&pkt, sizeof(pkt));
    esp_now_send(SERVO_MAC, (uint8_t*)&pkt, sizeof(pkt));
  }

  // OLED update
  if (now - lastUi >= UI_PERIOD_MS) {
    lastUi = now;

    char link[32];
    snprintf(link, sizeof(link), "TX %s #%u",
             (lastStatus == ESP_NOW_SEND_SUCCESS) ? "OK" : "--",
             seq);

    ui.draw(link,
            steerToText(steer), steerP,
            thrToText(thr),     thrP);
  }
}
