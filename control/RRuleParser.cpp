#include "RRuleParser.hpp"

// Das Include von GeneralTimeConverter.hpp ist hier nicht mehr nötig,
// da es bereits in RRuleParser.hpp enthalten ist.

static inline int parseDecimal(const char* p, size_t len) {
    int v = 0;
    for (size_t i = 0; i < len; ++i) {
        char c = p[i];
        if (c < '0' || c > '9') break;
        v = v * 10 + (c - '0');
    }
    return v;
}

time_t parseICalDateTime(const char* line, size_t len, bool& isAllDay, const GeneralTimeConverter* converter) {
    isAllDay = false;
    if (!line || len == 0) return 0;

    const char* value_ptr = line;
    const char* colon = (const char*)memchr(line, ':', len);
    if (colon) {
        value_ptr = colon + 1;
    }

    // Check for property parameters (TZID, VALUE=DATE)
    bool hasTZID = false;
    const char* semicolon = (const char*)memchr(line, ';', len);
    if (semicolon && semicolon < colon) { // Property parameter
        const char* value_date = strstr(semicolon, "VALUE=DATE");
        if (value_date) {
            isAllDay = true;
        }
        // Check if there's a TZID parameter
        // TZID format is "TZID=timezone_name" followed by colon
        const char* tzid = strstr(semicolon, "TZID=");
        if (tzid && tzid < colon && tzid >= semicolon) {
            hasTZID = true;
        }
    }

    const char* dt_str = value_ptr;
    size_t dt_len = strlen(dt_str);
    
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
        // Time ends with 'Z' - it's explicitly UTC
        return timegm(&t);
    } else if (hasTZID && converter) {
        // Time has TZID parameter - it's in that local timezone
        // Parse as if it's a time, then subtract the timezone offset to get UTC
        // timegm treats the tm struct as UTC, giving us the epoch if it were UTC
        time_t as_if_utc = timegm(&t);
        
        // To check DST correctly, we need to approximate the UTC time first
        // Use a simplified approach: check DST based on the approximate UTC time
        // This works because DST transitions don't happen at the same time in local and UTC
        int initial_offset = converter->isDST(as_if_utc) ? converter->getDstOffsetSec() : converter->getStdOffsetSec();
        time_t approx_utc = as_if_utc - initial_offset;
        
        // Now check DST again with the approximated UTC time
        // This handles edge cases around DST transitions
        int final_offset = converter->isDST(approx_utc) ? converter->getDstOffsetSec() : converter->getStdOffsetSec();
        return as_if_utc - final_offset;
    } else {
        // No timezone specified - treat as local time using system timezone
        return mktime(&t);
    }
}

void parseVEvent(const char* veventBlock, size_t len, Event& event, const GeneralTimeConverter* converter) {
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
            event.dtstart = parseICalDateTime(p, lineLen, dtstart_is_allday, converter);
        } else if (strncmp(p, "DTEND", 5) == 0) {
            event.dtend = parseICalDateTime(p, lineLen, dtend_is_allday, converter);
        } else if (strncmp(p, "EXDATE", 6) == 0) {
            bool dummy;
            time_t ex = parseICalDateTime(p, lineLen, dummy, converter);
            if (ex != 0) event.exdates.push_back(ex);
        } else if (strncmp(p, "RECURRENCE-ID", 13) == 0) {
            bool dummy;
            event.recurrence_id = parseICalDateTime(p, lineLen, dummy, converter);
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