#ifndef CALENDARMODULE_HPP
#define CALENDARMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <HTTPClient.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include "BerlinTime.hpp"
#include "RRuleParser.hpp"

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
  time_t endEpoch;
};

class CalendarModule {
public:
  CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas)
    : u8g2(u8g2), canvas(canvas) {}

  void setICSUrl(const String &url) { icsUrl = url; }
  String getICSUrl() const { return icsUrl; }
  void setFetchIntervalMinutes(uint32_t minutes) { fetchInterval = minutes * 60000UL; }
  uint32_t getFetchIntervalMillis() const { return fetchInterval; }
  void setScrollStepInterval(uint32_t ms) {
    scrollStepInterval = ms;
    resetScroll();
  }
  void setColors(const String& dateColorHex, const String& textColorHex) {
    dateColor = hexColorTo565(dateColorHex);
    textColor = hexColorTo565(textColorHex);
  }

  // Updatelogik wird über robustCalendarUpdateTask in Panelclock.ino ausgeführt!
  void robustUpdateIfDue() {
    // leer für Abwärtskompatibilität
  }

  void parseICS(const String& ics) {
    events.clear();
    int idx = 0;
    CalendarEvent cur;
    bool inEvent = false;
    String rrule;
    while (idx < (int)ics.length()) {
      int next = ics.indexOf('\n', idx);
      if (next < 0) next = ics.length();
      String line = ics.substring(idx, next);
      line.trim();
      if (line == "BEGIN:VEVENT") {
        inEvent = true;
        cur = CalendarEvent();
        cur.endEpoch = 0;
        rrule = "";
      } else if (line == "END:VEVENT" && inEvent) {
        if (rrule.length()) {
          std::vector<time_t> instances = parseRRule(rrule, cur.startEpoch, 20);
          for (time_t inst : instances) {
            CalendarEvent ev = cur;
            ev.startEpoch = inst;
            ev.endEpoch = inst + (cur.endEpoch - cur.startEpoch);
            struct tm t;
            gmtime_r(&ev.startEpoch, &t);
            ev.date = icsDateToGerman(String(t.tm_year+1900) + pad2(t.tm_mon+1) + pad2(t.tm_mday));
            ev.time = cur.time;
            events.push_back(ev);
          }
        } else if (cur.summary.length() && cur.date.length()) {
          events.push_back(cur);
        }
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
        } else if (line.startsWith("DTEND;VALUE=DATE:")) {
          String datestr = line.substring(17);
          cur.endEpoch = icsDateToEpoch(datestr, true) - 1;
        } else if (line.startsWith("DTEND:")) {
          String dtstr = line.substring(6);
          cur.endEpoch = icsDateTimeToEpoch(dtstr);
        } else if (line.startsWith("RRULE:")) {
          rrule = line.substring(6);
        }
      }
      idx = next + 1;
    }
    for (auto& ev : events) {
      if (ev.endEpoch == 0) ev.endEpoch = ev.startEpoch;
    }
  }

  void onSuccessfulUpdate() {
    std::sort(events.begin(), events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
      return a.startEpoch < b.startEpoch;
    });
    resetScroll();
  }

  void onFailedUpdate(int httpCode) {
    events.clear();
    CalendarEvent err;
    err.date = "";
    err.time = "";
    err.summary = String("ICS HTTP: ") + httpCode;
    err.startEpoch = time(nullptr) + 1;
    err.endEpoch = err.startEpoch;
    events.push_back(err);
    resetScroll();
  }

  std::vector<CalendarEvent> getUpcomingEvents(int maxCount = 6) {
    std::vector<CalendarEvent> result;
    time_t now = time(nullptr);
    for (const auto& ev : events) {
      if (ev.endEpoch >= now) result.push_back(ev);
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
      if (w <= (maxPixel - 1)) lastOk = i;
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
    u8g2.print("Start   Ende   Zeit   Termin");
    y += fontH;

    auto upcomming = getUpcomingEvents(6);
    if (upcomming.empty()) {
      u8g2.setFont(font);
      u8g2.setForegroundColor(textColor);
      u8g2.setCursor(2, y);
      u8g2.print("Keine Termine");
      return;
    }

    int xStart = 2;
    int xEnd = 48;
    int xTime = 74;
    int xSummary = 110;
    int maxSummaryPixel = canvas.width() - xSummary - 2;

    ensureScrollPos(upcomming, maxSummaryPixel);

    for (size_t i = 0; i < upcomming.size(); ++i) {
      const auto& ev = upcomming[i];
      struct tm tStartUtc, tEndUtc;
      gmtime_r(&ev.startEpoch, &tStartUtc);
      gmtime_r(&ev.endEpoch, &tEndUtc);

      struct tm tStart = utcToBerlin(tStartUtc);
      struct tm tEnd = utcToBerlin(tEndUtc);

      char bufStart[12];
      strftime(bufStart, sizeof(bufStart), "%d.%m.%y", &tStart);
      u8g2.setForegroundColor(dateColor);
      u8g2.setCursor(xStart, y);
      u8g2.print(bufStart);

      char bufEnd[12] = "";
      bool isMulti = (ev.endEpoch > ev.startEpoch && (tEnd.tm_yday != tStart.tm_yday || tEnd.tm_year != tStart.tm_year));
      if (isMulti) {
        strftime(bufEnd, sizeof(bufEnd), "%d.%m.%y", &tEnd);
        u8g2.setCursor(xEnd, y);
        u8g2.print(bufEnd);
      }

      u8g2.setCursor(xTime, y);
      if (isMulti) {
        u8g2.print("     ");
      } else if (ev.time.length() > 0) {
        char bufTime[8];
        strftime(bufTime, sizeof(bufTime), "%H:%M", &tStart);
        u8g2.print(bufTime);
      } else {
        u8g2.print("00:00");
      }

      u8g2.setForegroundColor(textColor);
      u8g2.setCursor(xSummary, y);
      size_t idx = i < scrollPos.size() ? i : 0;
      String shownText = ev.summary;
      int visibleChars = fitTextToPixelWidth(shownText, maxSummaryPixel - 1).length();
      String pad = "   ";
      String scrollText = shownText + pad + shownText.substring(0, visibleChars);
      int maxScroll = scrollText.length() - visibleChars;
      int offset = scrollPos[idx].offset;
      if (offset > maxScroll) offset = 0;
      String part = scrollText.substring(offset, offset + visibleChars);
      u8g2.print(part);

      y += fontH;
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

  static String pad2(int val) {
    String s = String(val);
    if (s.length() < 2) s = "0" + s;
    return s;
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

  void resetScroll() { scrollPos.clear(); }

  void ensureScrollPos(const std::vector<CalendarEvent>& upcomming, int maxTextPixel) {
    if (scrollPos.size() != upcomming.size()) {
      scrollPos.resize(upcomming.size());
      for (size_t i = 0; i < scrollPos.size(); ++i) {
        String shownText = upcomming[i].summary;
        int visibleChars = fitTextToPixelWidth(shownText, maxTextPixel - 1).length();
        String pad = "   ";
        String scrollText = shownText + pad + shownText.substring(0, visibleChars);
        int maxScroll = (shownText.length() > visibleChars) ? (scrollText.length() - visibleChars) : 0;
        scrollPos[i].offset = 0;
        scrollPos[i].maxScroll = maxScroll;
      }
    }
  }
};

#endif // CALENDARMODULE_HPP