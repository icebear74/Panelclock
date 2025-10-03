#ifndef CALENDARMODULE_HPP
#define CALENDARMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include "RRuleParser.hpp"
#include "GeneralTimeConverter.hpp"

struct CalendarEvent {
  String summary;
  time_t startEpoch; // Immer in UTC
  time_t endEpoch;   // Immer in UTC
  bool isAllDay;
};

uint16_t hexColorTo565(const String& hex) {
  if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
  long r = strtol(hex.substring(1,3).c_str(), NULL, 16);
  long g = strtol(hex.substring(3,5).c_str(), NULL, 16);
  long b = strtol(hex.substring(5,7).c_str(), NULL, 16);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class CalendarModule {
public:
  CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter)
    : u8g2(u8g2), canvas(canvas), timeConverter(converter) {}

  void setICSUrl(const String &url) { icsUrl = url; }
  String getICSUrl() const { return icsUrl; }
  void setFetchIntervalMinutes(uint32_t minutes) { fetchInterval = minutes * 60000UL; }
  uint32_t getFetchIntervalMillis() const { return fetchInterval; }
  void setScrollStepInterval(uint32_t ms) { scrollStepInterval = ms; resetScroll(); }
  void setColors(const String& dateColorHex, const String& textColorHex) {
    dateColor = hexColorTo565(dateColorHex);
    textColor = hexColorTo565(textColorHex);
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
        cur.isAllDay = false;
        rrule = "";
      } else if (line == "END:VEVENT" && inEvent) {
        if (cur.summary.length() > 0 && cur.startEpoch > 0) {
          if (cur.endEpoch == 0) cur.endEpoch = cur.startEpoch;
          time_t duration = cur.endEpoch - cur.startEpoch;
          
          if (rrule.length() > 0) {
            std::vector<time_t> instances = parseRRule(rrule, cur.startEpoch, 20);
            for (time_t inst_utc : instances) {
              CalendarEvent ev = cur;
              ev.startEpoch = inst_utc;
              ev.endEpoch = inst_utc + duration;
              events.push_back(ev);
            }
          } else {
            events.push_back(cur);
          }
        }
        inEvent = false;
      } else if (inEvent) {
        if (line.startsWith("SUMMARY:")) {
          cur.summary = line.substring(8);
        } else if (line.startsWith("DTSTART")) {
          cur.startEpoch = parseIcsDateTime(line, cur.isAllDay);
        } else if (line.startsWith("DTEND")) {
          bool dummy;
          cur.endEpoch = parseIcsDateTime(line, dummy);
        } else if (line.startsWith("RRULE:")) {
          rrule = line.substring(6);
        }
      }
      idx = next + 1;
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
    err.summary = String("ICS HTTP: ") + httpCode;
    time_t now; time(&now);
    err.startEpoch = now + 1;
    err.endEpoch = err.startEpoch;
    err.isAllDay = false;
    events.push_back(err);
    resetScroll();
  }

  std::vector<CalendarEvent> getUpcomingEvents(int maxCount = 6) {
    std::vector<CalendarEvent> result;
    time_t now_utc; time(&now_utc);
    for (const auto& ev : events) {
      if (ev.endEpoch >= now_utc) {
        result.push_back(ev);
      }
      if ((int)result.size() >= maxCount) break;
    }
    return result;
  }

  void tickScroll() { if (scrollStepInterval == 0) return; unsigned long now = millis(); if (now - lastScrollStep >= scrollStepInterval) { lastScrollStep = now; for (auto& s : scrollPos) { if (s.maxScroll > 0) { s.offset = (s.offset + 1) % (s.maxScroll + 1); } } } }
  uint32_t getScrollStepInterval() const { return scrollStepInterval; }
  String fitTextToPixelWidth(const String& text, int maxPixel) { int lastOk = 0; for (int i = 1; i <= text.length(); ++i) { if (u8g2.getUTF8Width(text.substring(0, i).c_str()) <= (maxPixel - 1)) lastOk = i; else break; } return text.substring(0, lastOk); }

  void draw() {
    canvas.fillScreen(0);
    canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), 0x39E7);
    u8g2.begin(canvas);

    const uint8_t* font = u8g2_font_5x8_tf;
    u8g2.setFont(font);
    int fontH = 8, y = fontH + 1;
    u8g2.setForegroundColor(dateColor);
    u8g2.setCursor(2, y);
    u8g2.print("Start   Ende   Zeit   Termin");
    y += fontH;

    auto upcomming = getUpcomingEvents(6);
    if (upcomming.empty()) { u8g2.setForegroundColor(textColor); u8g2.setCursor(2, y); u8g2.print("Keine Termine"); return; }
    
    int xStart = 2, xEnd = 48, xTime = 74, xSummary = 110;
    int maxSummaryPixel = canvas.width() - xSummary - 2;
    ensureScrollPos(upcomming, maxSummaryPixel);

    for (size_t i = 0; i < upcomming.size(); ++i) {
      const auto& ev = upcomming[i];
      struct tm tStart, tEnd;

      time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
      time_t localEndEpoch = timeConverter.toLocal(ev.endEpoch);
      localtime_r(&localStartEpoch, &tStart);
      localtime_r(&localEndEpoch, &tEnd);

      char buf[12];
      strftime(buf, sizeof(buf), "%d.%m.%y", &tStart);
      u8g2.setForegroundColor(dateColor);
      u8g2.setCursor(xStart, y);
      u8g2.print(buf);

      bool isMultiDay = (localEndEpoch > localStartEpoch && (tEnd.tm_yday != tStart.tm_yday || tEnd.tm_year != tStart.tm_year));
      if (isMultiDay) { strftime(buf, sizeof(buf), "%d.%m.%y", &tEnd); u8g2.setCursor(xEnd, y); u8g2.print(buf); }

      u8g2.setCursor(xTime, y);
      if (isMultiDay) { u8g2.print("     "); } 
      else if (!ev.isAllDay) { strftime(buf, sizeof(buf), "%H:%M", &tStart); u8g2.print(buf); } 
      else { u8g2.print(" ganzt."); }

      u8g2.setForegroundColor(textColor);
      u8g2.setCursor(xSummary, y);
      size_t idx = i < scrollPos.size() ? i : 0;
      String shownText = ev.summary;
      int visibleChars = fitTextToPixelWidth(shownText, maxSummaryPixel - 1).length();
      String pad = "   ";
      String scrollText = shownText + pad + shownText.substring(0, visibleChars);
      int maxScroll = (shownText.length() > visibleChars) ? (scrollText.length() - visibleChars) : 0;
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
  const GeneralTimeConverter& timeConverter;
  String icsUrl;
  uint32_t fetchInterval = 300000; // *** HIER WAR DER FEHLER - VARIABLE WIEDER HINZUGEFÜGT ***
  std::vector<CalendarEvent> events;

  struct ScrollState { int offset = 0; int maxScroll = 0; };
  std::vector<ScrollState> scrollPos;
  unsigned long lastScrollStep = 0;
  uint32_t scrollStepInterval = 150;
  uint16_t dateColor = 0xFFE0;
  uint16_t textColor = 0xFFFF;

  time_t parseIcsDateTime(const String& line, bool& isAllDay) {
    isAllDay = false;
    int colonPos = line.indexOf(':');
    if (colonPos == -1) return 0;
    
    String dtstr = line.substring(colonPos + 1);
    
    if (line.indexOf("VALUE=DATE") != -1 || dtstr.indexOf('T') == -1) {
        isAllDay = true;
    }

    struct tm t = {0};
    bool isUtc = dtstr.endsWith("Z");

    t.tm_year = dtstr.substring(0, 4).toInt() - 1900;
    t.tm_mon  = dtstr.substring(4, 6).toInt() - 1;
    t.tm_mday = dtstr.substring(6, 8).toInt();

    if (!isAllDay) {
        t.tm_hour = dtstr.substring(9, 11).toInt();
        t.tm_min  = dtstr.substring(11, 13).toInt();
        t.tm_sec  = dtstr.substring(13, 15).toInt();
    } else {
        t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    }
    
    if (isUtc) {
      return timegm(&t);
    } else {
      t.tm_isdst = -1;
      time_t local_epoch = mktime(&t);
      
      time_t temp_utc = local_epoch - 12*3600;
      time_t temp_local = timeConverter.toLocal(temp_utc);
      int offset = temp_local - temp_utc;
      
      return local_epoch - offset;
    }
  }

  void resetScroll() { scrollPos.clear(); }
  void ensureScrollPos(const std::vector<CalendarEvent>& upcomming, int maxTextPixel) { if (scrollPos.size() != upcomming.size()) { scrollPos.resize(upcomming.size()); for (size_t i = 0; i < scrollPos.size(); ++i) { String shownText = upcomming[i].summary; int visibleChars = fitTextToPixelWidth(shownText, maxTextPixel - 1).length(); String pad = "   "; String scrollText = shownText + pad + shownText.substring(0, visibleChars); int maxScroll = (shownText.length() > visibleChars) ? (scrollText.length() - visibleChars) : 0; scrollPos[i].offset = 0; scrollPos[i].maxScroll = maxScroll; } } }
};

#endif