#ifndef CLOCKMODULE_HPP
#define CLOCKMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <time.h>
#include "GeneralTimeConverter.hpp"

// Der GFXcanvas-Include-Block bleibt unverändert
#if __has_include("gfxcanvas.h")
  #include "gfxcanvas.h"
#elif __has_include(<gfxcanvas.h>)
  #include <gfxcanvas.h>
#elif __has_include("GFXcanvas.h")
  #include "GFXcanvas.h"
#elif __has_include(<GFXcanvas.h>)
  #include <GFXcanvas.h>
#elif __has_include("Adafruit_GFX.h")
  #include <Adafruit_GFX.h>
#else
  #error "gfxcanvas header not found. Please install the Adafruit GFX library (provides GFXcanvas16 / gfxcanvas.h)."
#endif

class ClockModule {
public:
  ClockModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter);

  void setTime(const struct tm &t);
  void setSensorState(bool displayIsOn, time_t onTime, time_t offTime, float onPercentage);
  void tick();
  void draw();
  // ENTFERNT: getBuffer(), width(), height() sind nicht mehr nötig.

private:
  void drawWifiStrengthBar();
  static int isoWeekNumber(const struct tm &t);
  static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);

  U8G2_FOR_ADAFRUIT_GFX &u8g2;
  GFXcanvas16 &canvas;
  GeneralTimeConverter& timeConverter;
  struct tm timeinfo{};
  int lastRssi = -100;
  unsigned long lastRssiUpdate = 0;
  bool isDisplayOn = true;
  time_t lastOnEventTime = 0;
  time_t lastOffEventTime = 0;
  float onPercentageValue = 0.0f;

  static constexpr uint16_t BLACK = 0x0000;
  static constexpr uint16_t YELLOW = 0xFFE0;
  static constexpr uint16_t MAGENTA = 0xF81F;
  static constexpr uint16_t CYAN = 0x07FF;
};

#endif // CLOCKMODULE_HPP