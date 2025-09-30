#ifndef CLOCKMODULE_HPP
#define CLOCKMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <WiFi.h>
#include <time.h>

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
  ClockModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas)
    : u8g2(u8g2), canvas(canvas) {}

  void setTime(const struct tm &t) { timeinfo = t; }

  // Regelmäßig im loop aufrufen!
  void tick() {
    unsigned long now = millis();
    if (now - lastRssiUpdate > 30000 || lastRssiUpdate == 0) {
      lastRssi = WiFi.RSSI();
      lastRssiUpdate = now;
    }
  }

  void draw() {
    canvas.fillScreen(0);
    // Rahmen
    canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), rgb565(50,50,50));

    // --- WLAN-Stärkebalken links ---
    drawWifiStrengthBar();

    // bind u8g2 to this canvas and draw
    u8g2.begin(canvas);
    u8g2.setFontMode(0);
    u8g2.setFontDirection(0);

    // Time (um 5 Pixel nach rechts verschoben, damit Balken Platz hat)
    u8g2.setForegroundColor(MAGENTA);
    u8g2.setBackgroundColor(BLACK);
    u8g2.setFont(u8g2_font_fub20_tf);
    u8g2.setCursor(7, 25); // Original war (2,25) → jetzt (7,25)
    u8g2.print(&timeinfo, "%H:%M:%S");

    // Date
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setForegroundColor(YELLOW);
    u8g2.setCursor(123, 18);
    u8g2.print(&timeinfo, "%d.%m.%Y");

    // Weekday
    u8g2.setForegroundColor(rgb565(0,255,0));
    u8g2.setCursor(123, 9);
    switch (timeinfo.tm_wday) {
      case 0: u8g2.print("Sonntag"); break;
      case 1: u8g2.print("Montag"); break;
      case 2: u8g2.print("Dienstag"); break;
      case 3: u8g2.print("Mittwoch"); break;
      case 4: u8g2.print("Donnerstag"); break;
      case 5: u8g2.print("Freitag"); break;
      case 6: u8g2.print("Samstag"); break;
      default: break;
    }

    // Day-of-year and ISO week
    u8g2.setCursor(123, 27);
    u8g2.setForegroundColor(CYAN);
    u8g2.print(&timeinfo, "T:%j ");
    {
      char wkbuf[8];
      int kw = isoWeekNumber(timeinfo);
      if (kw < 1) kw = 1;
      snprintf(wkbuf, sizeof(wkbuf), "KW:%02d", kw);
      u8g2.print(wkbuf);
    }
  }

  uint16_t* getBuffer() { return canvas.getBuffer(); }
  int width() const { return canvas.width(); }
  int height() const { return canvas.height(); }

private:
  U8G2_FOR_ADAFRUIT_GFX &u8g2;
  GFXcanvas16 &canvas;
  struct tm timeinfo{};
  int lastRssi = -100;
  unsigned long lastRssiUpdate = 0;

  static inline uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
    return ((r/8)<<11)|((g/4)<<5)|(b/8);
  }
  static constexpr uint16_t BLACK = 0x0000;
  static constexpr uint16_t YELLOW = 0xFFE0;
  static constexpr uint16_t MAGENTA = 0xF81F;
  static constexpr uint16_t CYAN = 0x07FF;

  // Farbverlauf: -90 (rot) ... -30 (grün)
  uint16_t rssiToColor(int rssi) {
    int rssiClamped = constrain(rssi, -90, -30);
    uint8_t r = map(rssiClamped, -90, -30, 255, 0); // schlecht = rot, gut = 0
    uint8_t g = map(rssiClamped, -90, -30, 0, 255); // schlecht = 0, gut = grün
    return rgb565(r, g, 0);
  }

  void drawWifiStrengthBar() {
    int barX = 1;
    int barW = 2; // jetzt exakt 2 Pixel breit!
    int barH = canvas.height();
    int barY = 0;
    uint16_t color = rssiToColor(lastRssi);
    canvas.fillRect(barX, barY, barW, barH, color);
    canvas.drawRect(barX, barY, barW, barH, 0x0000); // schwarzer Rahmen
  }

  // ISO week calculation
  static int isoWeekNumber(const struct tm &t) {
    struct tm tmp = t;
    time_t tt = mktime(&tmp);
    if (tt == (time_t)-1) return 0;
    struct tm normalized;
    localtime_r(&tt, &normalized);
    int wday = normalized.tm_wday == 0 ? 7 : normalized.tm_wday;
    int daysToThursday = 4 - wday;
    time_t thursday_tt = tt + (time_t)daysToThursday * 24 * 3600;
    struct tm thursday_tm;
    localtime_r(&thursday_tt, &thursday_tm);
    int isoYear = thursday_tm.tm_year + 1900;
    struct tm jan1 = {0};
    jan1.tm_year = isoYear - 1900;
    jan1.tm_mon = 0;
    jan1.tm_mday = 1;
    jan1.tm_hour = 0; jan1.tm_min = 0; jan1.tm_sec = 0;
    time_t jan1_tt = mktime(&jan1);
    if (jan1_tt == (time_t)-1) return 0;
    struct tm jan1_tm; localtime_r(&jan1_tt, &jan1_tm);
    int jan1_wday = jan1_tm.tm_wday == 0 ? 7 : jan1_tm.tm_wday;
    int week1_monday_offset = (jan1_wday <= 4) ? (1 - jan1_wday) : (8 - jan1_wday);
    time_t week1_monday_tt = jan1_tt + (time_t)week1_monday_offset * 24 * 3600;
    int weekno = (int)((thursday_tt - week1_monday_tt) / (7 * 24 * 3600)) + 1;
    return weekno;
  }
};

#endif // CLOCKMODULE_HPP