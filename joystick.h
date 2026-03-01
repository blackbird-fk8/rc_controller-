#pragma once
#include <Arduino.h>

enum class SteerState : uint8_t { LEFT, CENTER, RIGHT };
enum class ThrState   : uint8_t { BACK, STOP, FWD };

inline const char* steerToText(SteerState s) {
  switch (s) {
    case SteerState::LEFT:  return "LEFT";
    case SteerState::RIGHT: return "RIGHT";
    default:                return "NEUTRAL";
  }
}

inline const char* thrToText(ThrState s) {
  switch (s) {
    case ThrState::FWD:  return "FWD";
    case ThrState::BACK: return "BACK";
    default:             return "STOP";
  }
}

class Joystick2Axis {
public:
  Joystick2Axis(int steerPin, int throttlePin)
  : _steerPin(steerPin), _thrPin(throttlePin) {}

  void begin() { analogReadResolution(12); }

  void calibrate(int samples = 60, int delayMs = 5) {
    long sSum = 0, tSum = 0;
    for (int i = 0; i < samples; i++) {
      sSum += analogRead(_steerPin);
      tSum += analogRead(_thrPin);
      delay(delayMs);
    }
    _steerCenter = (int)(sSum / samples);
    _thrCenter   = (int)(tSum / samples);

    _steerFilt = _steerCenter;
    _thrFilt   = _thrCenter;

    _steerState = SteerState::CENTER;
    _thrState   = ThrState::STOP;
  }

  void update() {
    _steerRaw = analogRead(_steerPin);
    _thrRaw   = analogRead(_thrPin);

    // IIR low-pass filter
    _steerFilt = _steerFilt + ((_steerRaw - _steerFilt) / _smoothN);
    _thrFilt   = _thrFilt   + ((_thrRaw   - _thrFilt)   / _smoothN);

    updateSteerState();
    updateThrState();
  }

  SteerState steerState() const { return _steerState; }
  ThrState   thrState()   const { return _thrState; }

  void setDeadzone(int dz) { _deadzone = dz; }
  void setSmoothingN(int n) { _smoothN = (n < 2) ? 2 : n; }
  void setHysteresis(int enter, int exit) {
    _enter = max(0, enter);
    _exit  = max(0, exit);
  }
  void setPercentDeadzone(uint8_t pct) { _pctDeadzone = pct; }

  int8_t steerPct() const { return axisToPct(_steerFilt, _steerCenter); }
  int8_t throttlePct() const { return axisToPct(_thrFilt, _thrCenter); }

  int rawSteer() const { return _steerRaw; }
  int rawThrottle() const { return _thrRaw; }
  int filtSteer() const { return _steerFilt; }
  int filtThrottle() const { return _thrFilt; }
  int centerSteer() const { return _steerCenter; }
  int centerThrottle() const { return _thrCenter; }

private:
  int8_t axisToPct(int filt, int center) const {
    int delta = filt - center;

    int spanPos = 4095 - center;
    int spanNeg = center;
    int span = (delta >= 0) ? spanPos : spanNeg;
    if (span < 1) span = 1;

    int pct = (delta * 100) / span;
    pct = constrain(pct, -100, 100);

    if (abs(pct) < _pctDeadzone) pct = 0;
    return (int8_t)pct;
  }

  void updateSteerState() {
    const int v = _steerFilt;
    const int enter = (_enter >= 0) ? _enter : _deadzone;
    const int exit  = (_exit  >= 0) ? _exit  : (_deadzone / 2);

    if (_steerState == SteerState::CENTER) {
      if (v < _steerCenter - enter) _steerState = SteerState::LEFT;
      else if (v > _steerCenter + enter) _steerState = SteerState::RIGHT;
    } else if (_steerState == SteerState::LEFT) {
      if (v > _steerCenter - exit) _steerState = SteerState::CENTER;
    } else { // RIGHT
      if (v < _steerCenter + exit) _steerState = SteerState::CENTER;
    }
  }

  void updateThrState() {
    const int v = _thrFilt;
    const int enter = (_enter >= 0) ? _enter : _deadzone;
    const int exit  = (_exit  >= 0) ? _exit  : (_deadzone / 2);

    if (_thrState == ThrState::STOP) {
      if (v < _thrCenter - enter) _thrState = ThrState::BACK;
      else if (v > _thrCenter + enter) _thrState = ThrState::FWD;
    } else if (_thrState == ThrState::BACK) {
      if (v > _thrCenter - exit) _thrState = ThrState::STOP;
    } else { // FWD
      if (v < _thrCenter + exit) _thrState = ThrState::STOP;
    }
  }

private:
  int _steerPin, _thrPin;

  int _steerCenter = 2048;
  int _thrCenter   = 2048;

  int _deadzone = 400;
  int _smoothN  = 8;

  int _enter = -1;
  int _exit  = -1;

  uint8_t _pctDeadzone = 6;

  int _steerRaw  = 2048;
  int _thrRaw    = 2048;
  int _steerFilt = 2048;
  int _thrFilt   = 2048;

  SteerState _steerState = SteerState::CENTER;
  ThrState   _thrState   = ThrState::STOP;
};
