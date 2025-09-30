#pragma once
#include <time.h>

// Ersatz für timegm() auf ESP32/Arduino:
inline time_t timegm(struct tm* t) {
    // Temporär TZ auf UTC setzen
    char* tz = getenv("TZ");
    setenv("TZ", "", 1); // UTC
    tzset();
    time_t ret = mktime(t);
    if (tz) {
        setenv("TZ", tz, 1);
    } else {
        unsetenv("TZ");
    }
    tzset();
    return ret;
}

// Liefert die Berliner Zeit aus einer UTC-struct tm.
// Korrekte Sommerzeit nach EU-Regel!
inline struct tm utcToBerlin(const struct tm &utc) {
    struct tm berlin = utc;
    int year = berlin.tm_year + 1900;

    int offset = 1;
    struct tm startDST = {0};
    startDST.tm_year = year - 1900;
    startDST.tm_mon = 2;
    startDST.tm_mday = 31;
    time_t t_start = mktime(&startDST);
    struct tm *p_start = gmtime(&t_start);
    int lastMarchSunday = 31 - p_start->tm_wday;

    struct tm endDST = {0};
    endDST.tm_year = year - 1900;
    endDST.tm_mon = 9;
    endDST.tm_mday = 31;
    time_t t_end = mktime(&endDST);
    struct tm *p_end = gmtime(&t_end);
    int lastOctoberSunday = 31 - p_end->tm_wday;

    struct tm dstStart = {0};
    dstStart.tm_year = year - 1900;
    dstStart.tm_mon = 2;
    dstStart.tm_mday = lastMarchSunday;
    dstStart.tm_hour = 2;
    time_t dst_start_time = timegm(&dstStart);

    struct tm dstEnd = {0};
    dstEnd.tm_year = year - 1900;
    dstEnd.tm_mon = 9;
    dstEnd.tm_mday = lastOctoberSunday;
    dstEnd.tm_hour = 3;
    time_t dst_end_time = timegm(&dstEnd);

    struct tm utcCopy = utc;
    time_t now_utc = timegm(&utcCopy);

    bool dst = (now_utc >= dst_start_time && now_utc < dst_end_time);
    if (dst) offset = 2;

    berlin.tm_hour += offset;
    if (berlin.tm_hour >= 24) {
        berlin.tm_hour -= 24;
        berlin.tm_mday += 1;
    }
    berlin.tm_isdst = dst ? 1 : 0;
    return berlin;
}