
// ESP32 Dev Module (Steer/Servo)
// Mac Address: 00:70:07:1c:09:f0 // Change to your Mac Address

// Hardware control specs
// Servo

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>

// =================== ESPNOW PACKET ===================
struct ControlPacket {
  uint32_t magic;
  uint16_t seq;
  int8_t   steerPct;
  int8_t   thrPct;
  uint8_t  flags;
};
static constexpr uint32_t PKT_MAGIC = 0x43415221;

// =================== ONBOARD LED ===================
static constexpr int  LED_PIN = 2;             // common ESP32 DevKit LED
static constexpr bool LED_ACTIVE_LOW = false;  // set true if inverted

static inline void ledWrite(bool on) {
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}
static void blinkN(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    ledWrite(true);  delay(60);
    ledWrite(false); delay(60);
  }
}

// =================== SERVO ===================
static constexpr int SERVO_PIN = 18;

static constexpr int SERVO_MIN_US    = 1000;
static constexpr int SERVO_MAX_US    = 2000;
static constexpr int SERVO_CENTER_US = 1500;

static constexpr uint32_t FAILSAFE_MS = 250;
static constexpr int8_t STEER_DEADBAND_PCT = 3;

// State IDs (no enum, avoids Arduino prototype weirdness)
#define STEER_LEFT   0
#define STEER_CENTER 1
#define STEER_RIGHT  2

Servo servo;

// Shared state
ControlPacket lastPkt;
volatile bool havePkt = false;
volatile uint32_t lastRxMs = 0;

static int prevState = STEER_CENTER;

// ------------------- Helpers -------------------
static inline int steerPctToUs(int8_t pct) {
  int p = constrain((int)pct, -100, 100);

  if (abs(p) <= STEER_DEADBAND_PCT) return SERVO_CENTER_US;

  long us = map((long)p, -100, 100, SERVO_MIN_US, SERVO_MAX_US);
  return (int)constrain((int)us, SERVO_MIN_US, SERVO_MAX_US);
}

static int classifySteer(int8_t pct) {
  if (pct >  +5) return STEER_RIGHT;
  if (pct <  -5) return STEER_LEFT;
  return STEER_CENTER;
}

static void blinkForState(int st) {
  // CENTER=1 blink, LEFT=2 blinks, RIGHT=3 blinks
  if (st == STEER_CENTER) blinkN(1);
  else if (st == STEER_LEFT) blinkN(2);
  else blinkN(3);
}

// ESP-NOW receive callback (core 3.x signature)
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  (void)info;
  if (len != (int)sizeof(ControlPacket)) return;

  ControlPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (pkt.magic != PKT_MAGIC) return;

  memcpy(&lastPkt, &pkt, sizeof(pkt));
  lastRxMs = millis();
  havePkt = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // LED
  pinMode(LED_PIN, OUTPUT);
  ledWrite(false);
  blinkN(2);

  // Servo
  servo.setPeriodHertz(50);
  servo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  servo.writeMicroseconds(SERVO_CENTER_US);

  // ESP-NOW
  WiFi.mode(WIFI_STA);
  Serial.print("Servo RX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true) delay(100);
  }
  esp_now_register_recv_cb(onRecv);

  Serial.println("Servo receiver ready.");
}

void loop() {
  const uint32_t now = millis();

  // FAILSAFE
  if (now - lastRxMs > FAILSAFE_MS) {
    if (prevState != STEER_CENTER) {
      prevState = STEER_CENTER;
      blinkForState(prevState);
    }
    servo.writeMicroseconds(SERVO_CENTER_US);

    static uint32_t lastFsPrint = 0;
    if (now - lastFsPrint > 600) {
      Serial.println("FAILSAFE: servo CENTER");
      lastFsPrint = now;
    }
  }

  if (havePkt) {
    havePkt = false;

    ControlPacket p;
    memcpy(&p, &lastPkt, sizeof(p));

    int st = classifySteer(p.steerPct);
    if (st != prevState) {
      prevState = st;
      blinkForState(st);
    }

    int us = steerPctToUs(p.steerPct);
    servo.writeMicroseconds(us);

    Serial.printf("SERVO: steer=%d -> %dus seq=%u\n", (int)p.steerPct, us, p.seq);
  }

  delay(5);
}
