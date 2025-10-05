#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <map>
#include "PsramUtils.hpp" // *** NEU: Einbindung der zentralen Helfer ***

// Die PsramAllocator und PsramString Definitionen wurden entfernt.

int weekdayStrToInt(const String& s);
time_t parseICalDateTime(const String& dateTimeStr, bool& isAllDay);

struct Event {
    PsramString summary;
    PsramString rrule;
    PsramString uid;
    time_t dtstart = 0;
    time_t dtend = 0;
    time_t recurrence_id = 0;
    std::vector<time_t, PsramAllocator<time_t>> exdates;
    bool isAllDay = false;
};

void parseVEvent(const String& veventBlock, Event& event);

template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind = 50);


// Implementierungen (bleiben unverändert)
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

time_t parseICalDateTime(const String& dateTimeStr, bool& isAllDay) {
    isAllDay = (dateTimeStr.indexOf('T') == -1);
    struct tm t = {0};
    t.tm_year = dateTimeStr.substring(0, 4).toInt() - 1900;
    t.tm_mon  = dateTimeStr.substring(4, 6).toInt() - 1;
    t.tm_mday = dateTimeStr.substring(6, 8).toInt();

    if (!isAllDay) {
        t.tm_hour = dateTimeStr.substring(9, 11).toInt();
        t.tm_min  = dateTimeStr.substring(11, 13).toInt();
        t.tm_sec  = dateTimeStr.substring(13, 15).toInt();
    } else {
        t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
    }

    if (dateTimeStr.endsWith("Z")) {
        return timegm(&t);
    } else {
        t.tm_isdst = -1;
        return mktime(&t);
    }
}

void parseVEvent(const String& veventBlock, Event& event) {
    int currentPos = 0;
    while (currentPos < veventBlock.length()) {
        int nextPos = veventBlock.indexOf('\n', currentPos);
        if (nextPos == -1) nextPos = veventBlock.length();
        String line = veventBlock.substring(currentPos, nextPos);
        line.trim();
        currentPos = nextPos + 1;

        if (line.startsWith("SUMMARY:")) event.summary = line.substring(8).c_str();
        else if (line.startsWith("RRULE:")) event.rrule = line.substring(6).c_str();
        else if (line.startsWith("UID:")) event.uid = line.substring(4).c_str();
        else if (line.startsWith("DTSTART")) {
            int colonPos = line.lastIndexOf(':');
            event.dtstart = parseICalDateTime(line.substring(colonPos + 1), event.isAllDay);
        } else if (line.startsWith("DTEND")) {
            int colonPos = line.lastIndexOf(':');
            bool dummy;
            event.dtend = parseICalDateTime(line.substring(colonPos + 1), dummy);
        } else if (line.startsWith("EXDATE")) {
            int colonPos = line.lastIndexOf(':');
            bool dummy;
            event.exdates.push_back(parseICalDateTime(line.substring(colonPos + 1), dummy));
        } else if (line.startsWith("RECURRENCE-ID")) {
            int colonPos = line.lastIndexOf(':');
            bool dummy;
            event.recurrence_id = parseICalDateTime(line.substring(colonPos + 1), dummy);
        }
    }
}

template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind) {
    if (masterEvent.rrule.empty() || masterEvent.dtstart == 0) return;
    occurrences.clear();
    String freq = "";
    int interval = 1;
    int count = -1;
    time_t until = 0;
    std::vector<std::pair<int, int>> bydays;
    int wkst = 1;
    String rrule_str(masterEvent.rrule.c_str());
    int last = 0;
    while (last < rrule_str.length()) {
        int next = rrule_str.indexOf(';', last);
        if (next < 0) next = rrule_str.length();
        String part = rrule_str.substring(last, next);
        last = next + 1;
        if (part.startsWith("FREQ=")) freq = part.substring(5);
        else if (part.startsWith("INTERVAL=")) interval = part.substring(9).toInt();
        else if (part.startsWith("COUNT=")) count = part.substring(6).toInt();
        else if (part.startsWith("WKST=")) wkst = weekdayStrToInt(part.substring(5));
        else if (part.startsWith("UNTIL=")) { bool d; until = parseICalDateTime(part.substring(6), d); }
        else if (part.startsWith("BYDAY=")) {
            String days = part.substring(6);
            int idx = 0;
            while(idx < days.length()) {
                int nextComma = days.indexOf(',', idx);
                if (nextComma < 0) nextComma = days.length();
                String dayPart = days.substring(idx, nextComma);
                int nth = 0;
                String wdStr;
                if (dayPart.length() > 2) {
                    nth = dayPart.substring(0, dayPart.length() - 2).toInt();
                    wdStr = dayPart.substring(dayPart.length() - 2);
                } else { wdStr = dayPart; }
                bydays.push_back({nth, weekdayStrToInt(wdStr)});
                idx = nextComma + 1;
            }
        }
    }
    time_t now; time(&now);
    int eventsFound = 0;
    int futureEventsFound = 0;
    time_t current_base = masterEvent.dtstart;
    while ((count == -1 || eventsFound < count) && futureEventsFound < numFutureEventsToFind) {
        if (until && current_base > until) break;
        if (current_base > now + 3600*24*365*10) break;
        struct tm t_base; gmtime_r(&current_base, &t_base);
        if (freq == "WEEKLY") {
            int daysToSubtract = (t_base.tm_wday - wkst + 7) % 7;
            time_t weekStart = current_base - (daysToSubtract * 24 * 3600);
            if (!bydays.empty()) {
                for(auto const& [nth, wd] : bydays) {
                    int dayOffset = (wd - wkst + 7) % 7;
                    time_t eventTime = weekStart + dayOffset * 24 * 3600;
                    if(eventTime >= masterEvent.dtstart) occurrences.push_back(eventTime);
                }
            } else { occurrences.push_back(current_base); }
        } else if (freq == "DAILY") { occurrences.push_back(current_base);
        } else if (freq == "MONTHLY") {
            if (!bydays.empty()) {
                for (auto const& [nth, wd] : bydays) {
                    struct tm t_month = t_base;
                    t_month.tm_mday = 1;
                    time_t firstDayOfMonth = mktime(&t_month);
                    struct tm t_first; gmtime_r(&firstDayOfMonth, &t_first);
                    int dayOffset = (wd - t_first.tm_wday + 7) % 7;
                    int targetDay = 1 + dayOffset;
                    if (nth > 0) { targetDay += (nth - 1) * 7;
                    } else if (nth < 0) {
                        t_month.tm_mon += 1; t_month.tm_mday = 0;
                        time_t lastDayOfMonth = mktime(&t_month);
                        struct tm t_last; gmtime_r(&lastDayOfMonth, &t_last);
                        targetDay = t_last.tm_mday - ((t_last.tm_wday - wd + 7) % 7);
                        targetDay += (nth + 1) * 7;
                        t_month.tm_mon -=1; 
                    }
                    t_month.tm_mday = targetDay;
                    time_t eventTime = mktime(&t_month);
                    struct tm t_event; gmtime_r(&eventTime, &t_event);
                    if (t_event.tm_mon == t_month.tm_mon && eventTime >= masterEvent.dtstart) {
                        occurrences.push_back(eventTime);
                    }
                }
            } else { occurrences.push_back(current_base); }
        } else if (freq == "YEARLY") { occurrences.push_back(current_base); }
        struct tm t_temp; gmtime_r(&current_base, &t_temp);
        if (freq == "MONTHLY") t_temp.tm_mon += interval;
        else if (freq == "DAILY") t_temp.tm_mday += interval;
        else if (freq == "WEEKLY") t_temp.tm_mday += 7 * interval;
        else if (freq == "YEARLY") t_temp.tm_year += interval;
        else break;
        current_base = mktime(&t_temp);
        if (!occurrences.empty()) {
            std::sort(occurrences.begin(), occurrences.end());
            occurrences.erase(std::unique(occurrences.begin(), occurrences.end()), occurrences.end());
            eventsFound = occurrences.size();
            futureEventsFound = 0;
            for(time_t t : occurrences) if(t >= now) futureEventsFound++;
        }
    }
    if (!masterEvent.exdates.empty()) {
        occurrences.erase(std::remove_if(occurrences.begin(), occurrences.end(),
          [&](const time_t& res) {
            for (time_t exdate : masterEvent.exdates) if (res == exdate) return true;
            return false;
          }), occurrences.end());
    }
}