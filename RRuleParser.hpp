#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>

int weekdayStrToInt(const String& s) {
  if (s == "SU") return 0;
  if (s == "MO") return 1;
  if (s == "TU") return 2;
  if (s == "WE") return 3;
  if (s == "TH") return 4;
  if (s == "FR") return 5;
  if (s == "SA") return 6;
  return -1;
}

time_t parseUntil(const String& untilStr) {
  struct tm t = {0};
  t.tm_year = untilStr.substring(0,4).toInt() - 1900;
  t.tm_mon  = untilStr.substring(4,6).toInt() - 1;
  t.tm_mday = untilStr.substring(6,8).toInt();
  if (untilStr.length() > 8) {
    t.tm_hour = untilStr.substring(9,11).toInt();
    t.tm_min  = untilStr.substring(11,13).toInt();
    t.tm_sec  = untilStr.substring(13,15).toInt();
  } else {
    t.tm_hour = 23; t.tm_min = 59; t.tm_sec = 59;
  }
  t.tm_isdst = -1;
  return mktime(&t);
}

std::vector<time_t> parseRRule(const String& rrule, time_t dtstart, int maxEvents = 50) {
  std::vector<time_t> result;
  String freq = "WEEKLY";
  int interval = 1;
  time_t until = 0;
  std::vector<int> bydays;

  std::vector<String> parts;
  int last = 0;
  while (last < rrule.length()) {
    int next = rrule.indexOf(';', last);
    if (next < 0) next = rrule.length();
    parts.push_back(rrule.substring(last, next));
    last = next + 1;
  }
  for (const auto& part : parts) {
    if (part.startsWith("FREQ=")) freq = part.substring(5);
    if (part.startsWith("INTERVAL=")) interval = part.substring(9).toInt();
    if (part.startsWith("UNTIL=")) until = parseUntil(part.substring(6));
    if (part.startsWith("BYDAY=")) {
      String days = part.substring(6);
      int idx = 0;
      while (idx < days.length()) {
        int next = days.indexOf(',', idx);
        if (next < 0) next = days.length();
        String wd = days.substring(idx, next);
        int wdInt = weekdayStrToInt(wd);
        if (wdInt >= 0) bydays.push_back(wdInt);
        idx = next + 1;
      }
    }
  }
  if (bydays.empty() && freq == "WEEKLY") {
    struct tm t;
    gmtime_r(&dtstart, &t);
    bydays.push_back(t.tm_wday);
  }

  struct tm tStart;
  gmtime_r(&dtstart, &tStart);

  int eventsMade = 0;
  time_t current = dtstart;

  if (freq == "DAILY") {
    while (eventsMade < maxEvents) {
      if (until && current > until) break;
      result.push_back(current);
      ++eventsMade;
      current += interval * 24 * 3600;
    }
  } else if (freq == "WEEKLY") {
    time_t firstWeekStart = dtstart - tStart.tm_wday * 24 * 3600;
    int weekCount = 0;
    while (eventsMade < maxEvents) {
      time_t weekBase = firstWeekStart + weekCount * interval * 7 * 24 * 3600;
      for (int wd : bydays) {
        time_t eventTime = weekBase + wd * 24 * 3600 + tStart.tm_hour*3600 + tStart.tm_min*60 + tStart.tm_sec;
        if (eventTime >= dtstart) {
          if (until && eventTime > until) return result;
          result.push_back(eventTime);
          ++eventsMade;
          if (eventsMade >= maxEvents) break;
        }
      }
      ++weekCount;
    }
  } else if (freq == "MONTHLY") {
    struct tm tEvent = tStart;
    while (eventsMade < maxEvents) {
      if (until && mktime(&tEvent) > until) break;
      result.push_back(mktime(&tEvent));
      ++eventsMade;
      tEvent.tm_mon += interval;
      mktime(&tEvent);
    }
  } else if (freq == "YEARLY") {
    struct tm tEvent = tStart;
    while (eventsMade < maxEvents) {
      if (until && mktime(&tEvent) > until) break;
      result.push_back(mktime(&tEvent));
      ++eventsMade;
      tEvent.tm_year += interval;
      mktime(&tEvent);
    }
  }
  return result;
}