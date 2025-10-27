#pragma once
#include <Arduino.h>
#include <vector>
#include <time.h>
#include <algorithm>
#include <map>
#include "PsramUtils.hpp"

static inline int parseDecimal(const char* p, size_t len) {
    int v = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = p[i];
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
    }
    return v;
}

// *** KOMPLETT NEUE, ROBUSTE PARSE-FUNKTION ***
time_t parseICalDateTime(const char* line, size_t len, bool& isAllDay) {
    isAllDay = false;
    if (!line || len == 0) return 0;

    const char* value_ptr = line;
    const char* colon = (const char*)memchr(line, ':', len);
    if (colon) {
        value_ptr = colon + 1;
    }

    const char* semicolon = (const char*)memchr(line, ';', len);
    if (semicolon && semicolon < colon) { // Property parameter
        const char* value_date = strstr(semicolon, "VALUE=DATE");
        if (value_date) {
            isAllDay = true;
        }
    }

    // Isoliere den reinen Datum/Zeit-String
    const char* dt_str = value_ptr;
    size_t dt_len = strlen(dt_str);
    
    // Entferne alle nicht-alphanumerischen Zeichen am Ende
    while (dt_len > 0 && !isalnum(dt_str[dt_len - 1])) {
        dt_len--;
    }

    if (dt_len < 8) return 0;

    int year = parseDecimal(dt_str, 4);
    int month = parseDecimal(dt_str + 4, 2);
    int day = parseDecimal(dt_str + 6, 2);
    int hour = 0, minute = 0, second = 0;

    if (dt_len > 8 && dt_str[8] == 'T') {
        if (dt_len >= 15) {
            hour = parseDecimal(dt_str + 9, 2);
            minute = parseDecimal(dt_str + 11, 2);
            second = parseDecimal(dt_str + 13, 2);
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
    t.tm_isdst = -1;

    bool isUTC = (dt_len > 0 && dt_str[dt_len - 1] == 'Z');

    if (isUTC) {
        return timegm(&t);
    } else {
        // Behandelt als lokale Zeit des ESP32, was für "floating times" die korrekte Annahme ist.
        return mktime(&t);
    }
}


struct Event {
    PsramString summary;
    PsramString rrule;
    PsramString uid;
    time_t dtstart = 0;
    time_t dtend = 0;
    time_t recurrence_id = 0;
    std::vector<time_t, PsramAllocator<time_t>> exdates;
    bool isAllDay = false;
    time_t duration = 0;
};

void parseVEvent(const char* veventBlock, size_t len, Event& event);

template<typename Allocator>
void parseRRule(const Event& masterEvent, std::vector<time_t, Allocator>& occurrences, int numFutureEventsToFind = 15);

// Implementations

inline void parseVEvent(const char* veventBlock, size_t len, Event& event) {
    if (!veventBlock || len == 0) return;
    const char* p = veventBlock;
    const char* end = veventBlock + len;

    bool dtstart_is_allday = false;
    bool dtend_is_allday = false;

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

        if (lineLen > 8 && strncmp(p, "SUMMARY:", 8) == 0) {
            const char* val = p + 8;
            event.summary = PsramString(val, lineLen - 8);
        } else if (lineLen > 6 && strncmp(p, "RRULE:", 6) == 0) {
            const char* val = p + 6;
            event.rrule = PsramString(val, lineLen - 6);
        } else if (lineLen > 4 && strncmp(p, "UID:", 4) == 0) {
            const char* val = p + 4;
            event.uid = PsramString(val, lineLen - 4);
        } else if (strncmp(p, "DTSTART", 7) == 0) {
            event.dtstart = parseICalDateTime(p, lineLen, dtstart_is_allday);
        } else if (strncmp(p, "DTEND", 5) == 0) {
            event.dtend = parseICalDateTime(p, lineLen, dtend_is_allday);
        } else if (strncmp(p, "EXDATE", 6) == 0) {
            bool dummy;
            time_t ex = parseICalDateTime(p, lineLen, dummy);
            if (ex != 0) event.exdates.push_back(ex);
        } else if (strncmp(p, "RECURRENCE-ID", 13) == 0) {
            bool dummy;
            event.recurrence_id = parseICalDateTime(p, lineLen, dummy);
        }
        
        p = lineEnd + 1;
    }

    event.isAllDay = dtstart_is_allday;
    if (event.dtend > event.dtstart) {
        event.duration = event.dtend - event.dtstart;
    } else if (event.isAllDay) {
        event.duration = 86400;
    } else {
        event.duration = 0;
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
        } else if (part.rfind("UNTIL=", 0) == 0) {
            PsramString u = part.substr(6);
            bool d; until = parseICalDateTime(u.c_str(), u.length(), d);
        }
    }

    time_t now_t;
    time(&now_t);
    struct tm now_tm;
    gmtime_r(&now_t, &now_tm);

    struct tm start_tm;
    gmtime_r(&masterEvent.dtstart, &start_tm);

    struct tm current_tm = start_tm;
    if (freq == "YEARLY") {
        if (current_tm.tm_year < now_tm.tm_year) {
            int year_diff = now_tm.tm_year - current_tm.tm_year;
            int intervals_to_skip = year_diff / interval;
            current_tm.tm_year += intervals_to_skip * interval;
        }
    } else if (freq == "MONTHLY") {
         if (current_tm.tm_year < now_tm.tm_year || (current_tm.tm_year == now_tm.tm_year && current_tm.tm_mon < now_tm.tm_mon)) {
            int month_diff = (now_tm.tm_year - current_tm.tm_year) * 12 + (now_tm.tm_mon - current_tm.tm_mon);
            int intervals_to_skip = month_diff / interval;
            current_tm.tm_mon += intervals_to_skip * interval;
            current_tm.tm_year += current_tm.tm_mon / 12;
            current_tm.tm_mon %= 12;
        }
    }
    
    int eventsFound = 0;
    int futureEventsFound = 0;
    int loop_count = 0;

    while (futureEventsFound < numFutureEventsToFind) {
        if (loop_count++ > 500) break;

        time_t current_t = timegm(&current_tm);

        if (until && current_t > until) break;
        if (count != -1 && eventsFound >= count) break;

        if (current_t >= masterEvent.dtstart) {
            bool is_exdate = false;
            for(time_t ex : masterEvent.exdates) {
                if (ex == current_t) { is_exdate = true; break; }
            }
            if (!is_exdate) {
                occurrences.push_back(current_t);
                eventsFound++;
                if (current_t >= now_t) {
                    futureEventsFound++;
                }
            }
        }

        if (freq == "YEARLY") current_tm.tm_year += interval;
        else if (freq == "MONTHLY") current_tm.tm_mon += interval;
        else if (freq == "WEEKLY") current_tm.tm_mday += 7 * interval;
        else if (freq == "DAILY") current_tm.tm_mday += interval;
        else break;
        
        time_t temp_t = timegm(&current_tm);
        gmtime_r(&temp_t, &current_tm);
    }
}