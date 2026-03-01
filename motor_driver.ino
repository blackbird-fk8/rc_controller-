
// ESP32 Dev Module (Motor Driver)
// Mac Address: c0:cd:d6:ca:04:a8

// Author: Joshua Pizana
// Code designer: Joshua Pizana
// Description: 

// Hardware control specs
// IBT-2 / BTS2960
// 12V motor (x2)

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

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
static constexpr int  LED_PIN = 2;            // common ESP32 DevKit LED
static constexpr bool LED_ACTIVE_LOW = false; // set true if LED is inverted (should'nt)

static inline void ledWrite(bool on) {
  digitalWrite(LED_PIN, LED_ACTIVE_LOW ? !on : on);
}
static void blinkN(uint8_t n) {
  for (uint8_t i = 0; i < n; i++) {
    ledWrite(true);  delay(60);
    ledWrite(false); delay(60);
  }
}

// =================== BTS7960 / IBT-2 PINS ===================
// Your pins:
static constexpr int PIN_RPWM = 25;
static constexpr int PIN_LPWM = 26;
static constexpr int PIN_REN  = 27;
static constexpr int PIN_LEN  = 14;

// PWM settings
static constexpr uint32_t PWM_FREQ = 20000; // 20 kHz
static constexpr uint8_t  PWM_RES  = 8;     // 8-bit duty 0..255

// Control tuning
static constexpr int8_t THR_DEADBAND_PCT = 5;    // ignore small jitter
static constexpr uint32_t FAILSAFE_MS = 250;

// =================== STATE ===================
ControlPacket lastPkt;
volatile bool havePkt = false;
volatile uint32_t lastRxMs = 0;

// Motor state IDs (no enum to avoid Arduino preprocessor weirdness)
#define MREV  0
#define MSTOP 1
#define MFWD  2

static int prevState = MSTOP;

// =================== HELPERS ===================
static inline uint8_t pctToDuty(int pctAbs) {
  pctAbs = constrain(pctAbs, 0, 100);
  return (uint8_t)((pctAbs * 255) / 100);
}

static void motorStop(bool disableEnablePins = true) {
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, 0);

  if (disableEnablePins) {
    digitalWrite(PIN_REN, LOW);
    digitalWrite(PIN_LEN, LOW);
  } else {
    digitalWrite(PIN_REN, HIGH);
    digitalWrite(PIN_LEN, HIGH);
  }
}

static void motorForward(uint8_t duty) {
  digitalWrite(PIN_REN, HIGH);
  digitalWrite(PIN_LEN, HIGH);
  ledcWrite(PIN_RPWM, 0);
  ledcWrite(PIN_LPWM, duty);   // swapped
}

static void motorReverse(uint8_t duty) {
  digitalWrite(PIN_REN, HIGH);
  digitalWrite(PIN_LEN, HIGH);
  ledcWrite(PIN_LPWM, 0);
  ledcWrite(PIN_RPWM, duty);   // swapped
}

static int classifyMotor(int8_t thrPct) {
  if (thrPct >  THR_DEADBAND_PCT) return MFWD;
  if (thrPct < -THR_DEADBAND_PCT) return MREV;
  return MSTOP;
}

static void blinkForState(int st) {
  // STOP=1 blink, REV=2 blinks, FWD=3 blinks
  if (st == MSTOP) blinkN(1);
  else if (st == MREV) blinkN(2);
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
  blinkN(2); // boot indicator

  // BTS7960 pins
  pinMode(PIN_REN, OUTPUT);
  pinMode(PIN_LEN, OUTPUT);
  digitalWrite(PIN_REN, LOW);
  digitalWrite(PIN_LEN, LOW);

  // Arduino ESP32 core 3.x LEDC API: attach PWM to pins
  ledcAttach(PIN_RPWM, PWM_FREQ, PWM_RES);
  ledcAttach(PIN_LPWM, PWM_FREQ, PWM_RES);

  motorStop(true);

  // ESP-NOW init
  WiFi.mode(WIFI_STA);
  Serial.print("Motor RX MAC: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (true) delay(100);
  }
  esp_now_register_recv_cb(onRecv);

  Serial.println("Motor receiver ready.");
}

void loop() {
  const uint32_t now = millis();

  // FAILSAFE
  if (now - lastRxMs > FAILSAFE_MS) {
    if (prevState != MSTOP) {
      prevState = MSTOP;
      blinkForState(prevState);
    }
    motorStop(true);

    static uint32_t lastFsPrint = 0;
    if (now - lastFsPrint > 600) {
      Serial.println("FAILSAFE: motor STOP");
      lastFsPrint = now;
    }
  }

  if (havePkt) {
    havePkt = false;

    ControlPacket p;
    memcpy(&p, &lastPkt, sizeof(p));

    const int8_t thr = p.thrPct;
    int st = classifyMotor(thr);

    // Blink only when state changes
    if (st != prevState) {
      prevState = st;
      blinkForState(st);
    }

    // Apply output
    if (st == MFWD) {
      uint8_t duty = pctToDuty(thr);
      motorForward(duty);
      Serial.printf("MOTOR: FWD thr=%d duty=%u seq=%u\n", (int)thr, duty, p.seq);
    } else if (st == MREV) {
      uint8_t duty = pctToDuty(-thr);
      motorReverse(duty);
      Serial.printf("MOTOR: REV thr=%d duty=%u seq=%u\n", (int)thr, duty, p.seq);
    } else {
      motorStop(true);
      Serial.printf("MOTOR: STOP thr=%d seq=%u\n", (int)thr, p.seq);
    }
  }

  delay(5);
