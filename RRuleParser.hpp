#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <map>
#include "PsramUtils.hpp" // uses PsramAllocator / PsramString

// Lightweight helpers to parse integers from ASCII without allocating
static inline int parseDecimal(const char* p, size_t len) {
    int v = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = p[i];
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
    }
    return v;
}

int weekdayStrToInt(const char* s, size_t len);
time_t parseICalDateTime(const char* str, size_t len, bool& isAllDay);

// Event structure using PSRAM types
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

// New parseVEvent that accepts a raw (PSRAM-backed) C string buffer to avoid Arduino String allocations.
// veventBlock must be a valid pointer to the VEVENT block; length is the block length.
void parseVEvent(const char* veventBlock, size_t len, Event& event);

// Backwards-compatible wrapper (keeps existing signature working)
inline void parseVEvent(const String& veventBlock, Event& event) {
    parseVEvent(veventBlock.c_str(), veventBlock.length(), event);
}

// parseRRule uses PsramString operations so allocations go to PSRAM (via PsramAllocator)
template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind = 50);

// ---------------- Implementations ----------------

int weekdayStrToInt(const char* s, size_t len) {
    if (len >= 2) {
        if (s[0] == 'S' && s[1] == 'U') return 0;
        if (s[0] == 'M' && s[1] == 'O') return 1;
        if (s[0] == 'T' && s[1] == 'U') return 2;
        if (s[0] == 'W' && s[1] == 'E') return 3;
        if (s[0] == 'T' && s[1] == 'H') return 4;
        if (s[0] == 'F' && s[1] == 'R') return 5;
        if (s[0] == 'S' && s[1] == 'A') return 6;
    }
    return -1;
}

time_t parseICalDateTime(const char* str, size_t len, bool& isAllDay) {
    isAllDay = true;
    if (!str || len < 8) return 0;
    const char* p = str;
    size_t remaining = len;
    while (remaining && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) { ++p; --remaining; }
    if (remaining < 8) return 0;

    int year = parseDecimal(p, 4);
    int month = parseDecimal(p + 4, 2);
    int day = parseDecimal(p + 6, 2);

    int hour = 0, minute = 0, second = 0;
    const char* tpos = nullptr;
    for (size_t i = 0; i < remaining; ++i) {
        if (p[i] == 'T') { tpos = p + i; break; }
    }
    if (tpos) {
        isAllDay = false;
        const char* tm = tpos + 1;
        size_t tmRem = remaining - (tm - p);
        if (tmRem >= 6) {
            hour = parseDecimal(tm, 2);
            minute = parseDecimal(tm + 2, 2);
            second = parseDecimal(tm + 4, 2);
        }
    } else {
        isAllDay = true;
    }

    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = minute;
    t.tm_sec = second;
    bool isUTC = false;
    if (len > 0 && str[len - 1] == 'Z') isUTC = true;

    if (isUTC) {
        return timegm(&t);
    } else {
        t.tm_isdst = -1;
        return mktime(&t);
    }
}

static const char* memrchr_safe(const char* buf, int c, size_t len) {
    for (size_t i = len; i > 0; --i) {
        if (buf[i-1] == (char)c) return buf + (i-1);
    }
    return nullptr;
}

void parseVEvent(const char* veventBlock, size_t len, Event& event) {
    if (!veventBlock || len == 0) return;
    const char* p = veventBlock;
    const char* end = veventBlock + len;

    while (p < end) {
        const char* lineEnd = (const char*)memchr(p, '\n', end - p);
        size_t lineLen = 0;
        if (lineEnd) {
            lineLen = lineEnd - p;
        } else {
            lineLen = end - p;
            lineEnd = end;
        }
        if (lineLen > 0 && p[lineLen - 1] == '\r') --lineLen;

        if (lineLen > 0) {
            if (lineLen >= 8 && strncmp(p, "SUMMARY:", 8) == 0) {
                const char* val = p + 8;
                size_t vlen = lineLen - 8;
                event.summary = PsramString(val, vlen);
            } else if (lineLen >= 6 && strncmp(p, "RRULE:", 6) == 0) {
                const char* val = p + 6;
                size_t vlen = lineLen - 6;
                event.rrule = PsramString(val, vlen);
            } else if (lineLen >= 4 && strncmp(p, "UID:", 4) == 0) {
                const char* val = p + 4;
                size_t vlen = lineLen - 4;
                event.uid = PsramString(val, vlen);
            } else if (lineLen >= 7 && (strncmp(p, "DTSTART", 7) == 0)) {
                const char* colon = memrchr_safe(p, ':', lineLen);
                if (!colon) colon = p;
                const char* val = colon + 1;
                size_t vlen = p + lineLen - val;
                bool dummy;
                event.dtstart = parseICalDateTime(val, vlen, dummy);
                event.isAllDay = dummy;
            } else if (lineLen >= 5 && (strncmp(p, "DTEND", 5) == 0)) {
                const char* colon = memrchr_safe(p, ':', lineLen);
                if (!colon) colon = p;
                const char* val = colon + 1;
                size_t vlen = p + lineLen - val;
                bool dummy;
                event.dtend = parseICalDateTime(val, vlen, dummy);
            } else if (lineLen >= 6 && (strncmp(p, "EXDATE", 6) == 0)) {
                const char* colon = memrchr_safe(p, ':', lineLen);
                if (!colon) colon = p;
                const char* val = colon + 1;
                size_t vlen = p + lineLen - val;
                bool dummy;
                time_t ex = parseICalDateTime(val, vlen, dummy);
                if (ex != 0) event.exdates.push_back(ex);
            } else if (lineLen >= 12 && (strncmp(p, "RECURRENCE-ID", 13) == 0 || strncmp(p, "RECURRENCE-ID", 12) == 0)) {
                const char* colon = memrchr_safe(p, ':', lineLen);
                if (!colon) colon = p;
                const char* val = colon + 1;
                size_t vlen = p + lineLen - val;
                bool dummy;
                event.recurrence_id = parseICalDateTime(val, vlen, dummy);
            }
        }

        p = lineEnd + 1;
    }
}

template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind) {
    if (masterEvent.rrule.empty() || masterEvent.dtstart == 0) return;
    occurrences.clear();

    PsramString rrule_str = masterEvent.rrule; 
    PsramString freq = "";
    int interval = 1;
    int count = -1;
    time_t until = 0;
    // KORREKTUR: Vector explizit auf PSRAM umgestellt
    std::vector<std::pair<int, int>, PsramAllocator<std::pair<int, int>>> bydays;
    int wkst = 1;

    size_t last = 0;
    while (last < rrule_str.length()) {
        size_t next = rrule_str.find(';', last);
        if (next == PsramString::npos) next = rrule_str.length();
        PsramString part = rrule_str.substr(last, next - last);
        last = (next == rrule_str.length()) ? next : next + 1;

        if (part.rfind("FREQ=", 0) == 0) {
            freq = part.substr(5);
        } else if (part.rfind("INTERVAL=", 0) == 0) {
            interval = atoi(part.substr(9).c_str());
        } else if (part.rfind("COUNT=", 0) == 0) {
            count = atoi(part.substr(6).c_str());
        } else if (part.rfind("WKST=", 0) == 0) {
            PsramString w = part.substr(5);
            wkst = weekdayStrToInt(w.c_str(), w.length());
        } else if (part.rfind("UNTIL=", 0) == 0) {
            PsramString u = part.substr(6);
            bool d; until = parseICalDateTime(u.c_str(), u.length(), d);
        } else if (part.rfind("BYDAY=", 0) == 0) {
            PsramString days = part.substr(6);
            size_t idx = 0;
            while (idx < days.length()) {
                size_t nextComma = days.find(',', idx);
                if (nextComma == PsramString::npos) nextComma = days.length();
                PsramString dayPart = days.substr(idx, nextComma - idx);
                int nth = 0;
                PsramString wdStr;
                if (dayPart.length() > 2) {
                    PsramString prefix = dayPart.substr(0, dayPart.length() - 2);
                    nth = atoi(prefix.c_str());
                    wdStr = dayPart.substr(dayPart.length() - 2);
                } else {
                    wdStr = dayPart;
                }
                int wd = weekdayStrToInt(wdStr.c_str(), wdStr.length());
                bydays.push_back({nth, wd});
                idx = (nextComma == days.length()) ? nextComma : nextComma + 1;
            }
        }
    }

    time_t now;
    time(&now);
    int eventsFound = 0;
    int futureEventsFound = 0;
    time_t current_base = masterEvent.dtstart;

    while ((count == -1 || eventsFound < count) && futureEventsFound < numFutureEventsToFind) {
        if (until && current_base > until) break;
        if (current_base > now + 3600 * 24 * 365 * 10) break;

        struct tm t_base;
        gmtime_r(&current_base, &t_base);

        if (freq == "WEEKLY") {
            int daysToSubtract = (t_base.tm_wday - wkst + 7) % 7;
            time_t weekStart = current_base - (daysToSubtract * 24 * 3600);
            if (!bydays.empty()) {
                for (auto const& pr : bydays) {
                    // KORREKTUR: Unbenutzte Variable entfernt
                    // int nth = pr.first;
                    int wd = pr.second;
                    int dayOffset = (wd - wkst + 7) % 7;
                    time_t eventTime = weekStart + dayOffset * 24 * 3600;
                    if (eventTime >= masterEvent.dtstart) occurrences.push_back(eventTime);
                }
            } else {
                occurrences.push_back(current_base);
            }
        } else if (freq == "DAILY") {
            occurrences.push_back(current_base);
        } else if (freq == "MONTHLY") {
            if (!bydays.empty()) {
                for (auto const& pr : bydays) {
                    int nth = pr.first;
                    int wd = pr.second;
                    struct tm t_month = t_base;
                    t_month.tm_mday = 1;
                    time_t firstDayOfMonth = mktime(&t_month);
                    struct tm t_first; gmtime_r(&firstDayOfMonth, &t_first);
                    int dayOffset = (wd - t_first.tm_wday + 7) % 7;
                    int targetDay = 1 + dayOffset;
                    if (nth > 0) {
                        targetDay += (nth - 1) * 7;
                    } else if (nth < 0) {
                        t_month.tm_mon += 1; t_month.tm_mday = 0;
                        time_t lastDayOfMonth = mktime(&t_month);
                        struct tm t_last; gmtime_r(&lastDayOfMonth, &t_last);
                        targetDay = t_last.tm_mday - ((t_last.tm_wday - wd + 7) % 7);
                        targetDay += (nth + 1) * 7;
                        t_month.tm_mon -= 1;
                    }
                    t_month.tm_mday = targetDay;
                    time_t eventTime = mktime(&t_month);
                    struct tm t_event; gmtime_r(&eventTime, &t_event);
                    if (t_event.tm_mon == t_month.tm_mon && eventTime >= masterEvent.dtstart) {
                        occurrences.push_back(eventTime);
                    }
                }
            } else {
                occurrences.push_back(current_base);
            }
        } else if (freq == "YEARLY") {
            occurrences.push_back(current_base);
        }

        struct tm t_temp;
        gmtime_r(&current_base, &t_temp);
        if (freq == "MONTHLY") t_temp.tm_mon += interval;
        else if (freq == "DAILY") t_temp.tm_mday += interval;
        else if (freq == "WEEKLY") t_temp.tm_mday += 7 * interval;
        else if (freq == "YEARLY") t_temp.tm_year += interval;
        else break;

        current_base = mktime(&t_temp);

        if (!occurrences.empty()) {
            std::sort(occurrences.begin(), occurrences.end());
            occurrences.erase(std::unique(occurrences.begin(), occurrences.end()), occurrences.end());
            eventsFound = (int)occurrences.size();
            futureEventsFound = 0;
            for (time_t t : occurrences) if (t >= now) futureEventsFound++;
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