#ifndef CALENDARMODULE_HPP
#define CALENDARMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <HTTPClient.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include "BerlinTime.hpp"

uint16_t hexColorTo565(const String& hex) {
  if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
  uint8_t r = strtol(hex.substring(1,3).c_str(), nullptr, 16);
  uint8_t g = strtol(hex.substring(3,5).c_str(), nullptr, 16);
  uint8_t b = strtol(hex.substring(5,7).c_str(), nullptr, 16);
  return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

struct CalendarEvent {
  String date;
  String time;
  String summary;
  time_t startEpoch;
};

class CalendarModule {
public:
  CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas)
    : u8g2(u8g2), canvas(canvas) {}

  void setICSUrl(const String &url) { icsUrl = url; }
  void setFetchIntervalMinutes(uint32_t minutes) { fetchInterval = minutes * 60000UL; }
  void setScrollStepInterval(uint32_t ms) {
    scrollStepInterval = ms;
    resetScroll();
  }
  void setColors(const String& dateColorHex, const String& textColorHex) {
    dateColor = hexColorTo565(dateColorHex);
    textColor = hexColorTo565(textColorHex);
  }

  void robustUpdateIfDue() {
    unsigned long now = millis();
    if (now - lastFetch > fetchInterval || events.empty()) {
      if (!icsUrl.length()) return;
      HTTPClient http;
      http.begin(icsUrl);
      http.setTimeout(4000);
      int code = http.GET();
      if (code == 200) {
        String ics = http.getString();
        parseICS(ics);
      } else {
        lastFetch = now;
        events.clear();
        CalendarEvent err;
        err.date = "";
        err.time = "";
        err.summary = String("ICS HTTP: ") + code;
        err.startEpoch = time(nullptr) + 1;
        events.push_back(err);
        resetScroll();
        return;
      }
      http.end();
      std::sort(events.begin(), events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
        return a.startEpoch < b.startEpoch;
      });
      lastFetch = now;
      resetScroll();
    }
  }

  std::vector<CalendarEvent> getUpcomingEvents(int maxCount = 4) {
    std::vector<CalendarEvent> result;
    time_t now = time(nullptr);
    for (const auto& ev : events) {
      if (ev.startEpoch >= now) result.push_back(ev);
      if ((int)result.size() >= maxCount) break;
    }
    return result;
  }

  void tickScroll() {
    if (scrollStepInterval == 0) return;
    unsigned long now = millis();
    if (now - lastScrollStep >= scrollStepInterval) {
      lastScrollStep = now;
      for (auto& s : scrollPos) {
        if (s.maxScroll > 0) {
          s.offset = (s.offset + 1) % (s.maxScroll + 1);
        }
      }
    }
  }

  uint32_t getScrollStepInterval() const { return scrollStepInterval; }

  String fitTextToPixelWidth(const String& text, int maxPixel) {
    int lastOk = 0;
    for (int i = 1; i <= text.length(); ++i) {
      int w = u8g2.getUTF8Width(text.substring(0, i).c_str());
      if (w <= maxPixel) lastOk = i;
      else break;
    }
    return text.substring(0, lastOk);
  }

  void draw() {
    canvas.fillScreen(0);
    canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), rgb565(50,50,50));
    u8g2.begin(canvas);

    const uint8_t* font = u8g2_font_5x8_tf;
    u8g2.setFont(font);

    int fontH = 8;
    int y = fontH + 1;
    u8g2.setForegroundColor(dateColor);
    u8g2.setCursor(2, y);
    u8g2.print("Datum    Zeit   Termin");
    y += fontH;

    auto upcomming = getUpcomingEvents(4);
    if (upcomming.empty()) {
      u8g2.setFont(font);
      u8g2.setForegroundColor(textColor);
      u8g2.setCursor(2, y);
      u8g2.print("Keine Termine");
      return;
    }

    int textX = 105;
    int maxTextPixel = canvas.width() - textX - 2;

    ensureScrollPos(upcomming, maxTextPixel);

    for (size_t i = 0; i < upcomming.size(); ++i) {
      const auto &ev = upcomming[i];
      u8g2.setFont(font);
      u8g2.setForegroundColor(dateColor);
      u8g2.setCursor(2, y);

      // NEU: Kalenderzeit immer Berlin!
      struct tm tStart;
      gmtime_r(&ev.startEpoch, &tStart);
      tStart = utcToBerlin(tStart);
      char datebuf[12], timebuf[8];
      strftime(datebuf, sizeof(datebuf), "%d.%m.%Y", &tStart);
      strftime(timebuf, sizeof(timebuf), "%H:%M", &tStart);

      u8g2.print(datebuf);
      u8g2.setCursor(65, y);
      u8g2.print(timebuf);

      String shownText = ev.summary;
      u8g2.setForegroundColor(textColor);

      String fitted = fitTextToPixelWidth(shownText, maxTextPixel);

      if (scrollStepInterval == 0) {
        if (fitted.length() < shownText.length())
          fitted = fitted.substring(0, fitted.length() > 3 ? fitted.length() - 3 : fitted.length()) + "...";
        u8g2.setCursor(textX, y);
        u8g2.print(fitted);
        y += fontH;
      } else {
        size_t idx = i < scrollPos.size() ? i : 0;
        int visibleChars = fitTextToPixelWidth(shownText, maxTextPixel).length();
        String pad = "   ";
        String scrollText = shownText + pad + shownText.substring(0, visibleChars);
        int maxScroll = scrollText.length() - visibleChars;
        int offset = scrollPos[idx].offset;
        if (offset > maxScroll) offset = 0;
        String part = scrollText.substring(offset, offset + visibleChars);
        u8g2.setCursor(textX, y);
        u8g2.print(part);
        y += fontH;
      }
    }
  }

private:
  U8G2_FOR_ADAFRUIT_GFX &u8g2;
  GFXcanvas16 &canvas;
  String icsUrl;
  unsigned long lastFetch = 0;
  uint32_t fetchInterval = 300000;
  std::vector<CalendarEvent> events;

  struct ScrollState {
    int offset = 0;
    int maxScroll = 0;
  };
  std::vector<ScrollState> scrollPos;
  unsigned long lastScrollStep = 0;
  uint32_t scrollStepInterval = 150;
  uint16_t dateColor = 0xFFE0;
  uint16_t textColor = 0xFFFF;

  void parseICS(const String& ics) {
    events.clear();
    int idx = 0;
    CalendarEvent cur;
    bool inEvent = false;
    while (idx < (int)ics.length()) {
      int next = ics.indexOf('\n', idx);
      if (next < 0) next = ics.length();
      String line = ics.substring(idx, next);
      line.trim();
      if (line == "BEGIN:VEVENT") {
        inEvent = true;
        cur = CalendarEvent();
      } else if (line == "END:VEVENT" && inEvent) {
        if (cur.summary.length() && cur.date.length())
          events.push_back(cur);
        inEvent = false;
      } else if (inEvent) {
        if (line.startsWith("SUMMARY:")) {
          cur.summary = line.substring(8);
        } else if (line.startsWith("DTSTART;VALUE=DATE:")) {
          String datestr = line.substring(19);
          cur.date = icsDateToGerman(datestr);
          cur.time = "";
          cur.startEpoch = icsDateToEpoch(datestr, true);
        } else if (line.startsWith("DTSTART:")) {
          String dtstr = line.substring(8);
          cur.date = icsDateToGerman(dtstr.substring(0,8));
          cur.time = icsTimeToGerman(dtstr.substring(9,15));
          cur.startEpoch = icsDateTimeToEpoch(dtstr);
        }
      }
      idx = next + 1;
    }
  }

  void resetScroll() { scrollPos.clear(); }

  void ensureScrollPos(const std::vector<CalendarEvent>& upcomming, int maxTextPixel) {
    if (scrollPos.size() != upcomming.size()) {
      scrollPos.resize(upcomming.size());
      for (size_t i = 0; i < scrollPos.size(); ++i) {
        String shownText = upcomming[i].summary;
        int visibleChars = fitTextToPixelWidth(shownText, maxTextPixel).length();
        String pad = "   ";
        String scrollText = shownText + pad + shownText.substring(0, visibleChars);
        int maxScroll = (shownText.length() > visibleChars) ? (scrollText.length() - visibleChars) : 0;
        scrollPos[i].offset = 0;
        scrollPos[i].maxScroll = maxScroll;
      }
    }
  }

  static String icsDateToGerman(const String& yyyymmdd) {
    if (yyyymmdd.length()!=8) return "";
    return yyyymmdd.substring(6,8) + "." + yyyymmdd.substring(4,6) + "." + yyyymmdd.substring(0,4);
  }
  static String icsTimeToGerman(const String& hhmmss) {
    if (hhmmss.length() < 4) return "";
    return hhmmss.substring(0,2) + ":" + hhmmss.substring(2,4);
  }
  static time_t icsDateToEpoch(const String& yyyymmdd, bool allDay=false) {
    struct tm t;
    t.tm_year = yyyymmdd.substring(0,4).toInt() - 1900;
    t.tm_mon  = yyyymmdd.substring(4,6).toInt() - 1;
    t.tm_mday = yyyymmdd.substring(6,8).toInt();
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 1;
    t.tm_isdst = -1;
    return mktime(&t);
  }
  static time_t icsDateTimeToEpoch(const String& dtstr) {
    struct tm t;
    t.tm_year = dtstr.substring(0,4).toInt() - 1900;
    t.tm_mon  = dtstr.substring(4,6).toInt() - 1;
    t.tm_mday = dtstr.substring(6,8).toInt();
    t.tm_hour = dtstr.substring(9,11).toInt();
    t.tm_min  = dtstr.substring(11,13).toInt();
    t.tm_sec  = dtstr.substring(13,15).toInt();
    t.tm_isdst = -1;
    return mktime(&t);
  }
  static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
  }
};

#endif // CALENDARMODULE_HPP