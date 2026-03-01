#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

class OledUI {
public:
  bool begin(int sda, int scl, uint8_t addr) {
    Wire.begin(sda, scl);

    if (!_display.begin(SSD1306_SWITCHCAPVCC, addr)) {
      return false;
    }
    _display.clearDisplay();
    _display.display();
    return true;
  }

  void showBoot(const char* line1, const char* line2, const char* line3) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);
    _display.setCursor(0, 18); _display.println(line1);
    _display.setCursor(0, 30); _display.println(line2);
    _display.setCursor(0, 42); _display.println(line3);
    _display.display();
  }

  // T-shaped layout with percent readout
  void draw(const char* linkText,
            const char* steerText, int steerPct,
            const char* thrText,   int thrPct) {
    _display.clearDisplay();
    _display.setTextSize(1);
    _display.setTextColor(SSD1306_WHITE);

    // Top status
    _display.setCursor(0, 0);
    _display.print("LINK: ");
    _display.println(linkText);

    // Dividers
    const int yDiv = 16;
    const int xDiv = 64;
    _display.drawLine(0, yDiv, 127, yDiv, SSD1306_WHITE);
    _display.drawLine(xDiv, yDiv, xDiv, 63, SSD1306_WHITE);

    // Bottom-left: SERVO
    _display.setCursor(2, yDiv + 4);
    _display.print("SERVO");
    _display.setCursor(2, yDiv + 16);
    _display.print(steerText);
    _display.setCursor(2, yDiv + 28);
    _display.print("X:");
    _display.print(steerPct);
    _display.print("%");

    // Bottom-right: MOTOR
    _display.setCursor(xDiv + 2, yDiv + 4);
    _display.print("MOTOR");
    _display.setCursor(xDiv + 2, yDiv + 16);
    _display.print(thrText);
    _display.setCursor(xDiv + 2, yDiv + 28);
    _display.print("Y:");
    _display.print(thrPct);
    _display.print("%");

    _display.display();
  }

private:
  Adafruit_SSD1306 _display = Adafruit_SSD1306(128, 64, &Wire, -1);
};
