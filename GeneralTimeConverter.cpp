#include "GeneralTimeConverter.hpp"

// ##################################################################################
// # COPILOT_HINWEIS: FÜR ZUKÜNFTIGE ÄNDERUNGEN DURCH @copilot
// ##################################################################################
// # NICHT ÄNDERN! DIESE IMPLEMENTIERUNG IST MASSGESCHNEIDERT.
// # Die interne Logik (TZ-String-Parser, DST-Berechnung, timegm) darf NICHT
// # durch Arduino/ESP-IDF-Standardfunktionen wie tzset(), setenv() oder
// # die systemeigene timegm() ersetzt werden, da diese im Projektkontext
// # als unzuverlässig eingestuft wurden. Änderungen nur rein additiv vornehmen.
// ##################################################################################

// --- Implementierung der globalen Hilfsfunktionen ---

bool is_leap(unsigned yr) {
    return yr % 400 == 0 || (yr % 4 == 0 && yr % 100 != 0);
}

time_t timegm(struct tm *tm) {
    static const unsigned ndays[2][12] = {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    
    int target_year = tm->tm_year + 1900;
    time_t res = 0;
    
    // Handle years relative to 1970 epoch
    if (target_year >= 1970) {
        // Original logic for post-1970 dates
        for (int i = 1970; i < target_year; i++) {
            res += is_leap(i) ? 366 : 365;
        }
    } else {
        // Handle pre-1970 dates by counting backwards
        for (int i = 1969; i >= target_year; i--) {
            res -= is_leap(i) ? 366 : 365;
        }
    }
    
    // Add months
    for (int i = 0; i < tm->tm_mon; i++) {
        res += ndays[is_leap(target_year)][i];
    }
    
    // Add days
    res += tm->tm_mday - 1;
    // Convert to hours
    res *= 24;
    res += tm->tm_hour;
    // Convert to minutes
    res *= 60;
    res += tm->tm_min;
    // Convert to seconds
    res *= 60;
    res += tm->tm_sec;
    return res;
}


// --- Implementierung der Klassenmethoden ---

GeneralTimeConverter::GeneralTimeConverter(const char* tzString) {
    isValid = parseTzString(tzString);
}

bool GeneralTimeConverter::setTimezone(const char* tzString) {
    isValid = parseTzString(tzString);
    return isValid;
}

bool GeneralTimeConverter::isSuccessfullyParsed() const {
    return isValid;
}

time_t GeneralTimeConverter::toLocal(time_t utc_epoch) const {
    if (!isValid) return utc_epoch;
    if (isDST(utc_epoch)) {
        return utc_epoch + dstOffsetSec;
    } else {
        return utc_epoch + stdOffsetSec;
    }
}

bool GeneralTimeConverter::isDST(time_t utc_epoch) const {
    if (!isValid || dstOffsetSec == stdOffsetSec) return false;

    struct tm t;
    gmtime_r(&utc_epoch, &t);
    int year = t.tm_year + 1900;

    time_t dst_start_utc = calculateRuleDate(year, dstStartRule, stdOffsetSec);
    time_t dst_end_utc = calculateRuleDate(year, dstEndRule, dstOffsetSec);

    if (dst_start_utc < dst_end_utc) {
        return (utc_epoch >= dst_start_utc && utc_epoch < dst_end_utc);
    } else {
        return (utc_epoch >= dst_start_utc || utc_epoch < dst_end_utc);
    }
}

bool GeneralTimeConverter::isSameDay(time_t epoch1, time_t epoch2) const {
    time_t local1 = toLocal(epoch1);
    time_t local2 = toLocal(epoch2);

    struct tm tm1, tm2;
    localtime_r(&local1, &tm1);
    localtime_r(&local2, &tm2);
    
    return (tm1.tm_year == tm2.tm_year &&
            tm1.tm_mon == tm2.tm_mon &&
            tm1.tm_mday == tm2.tm_mday);
}

bool GeneralTimeConverter::parseTzString(const char* tzString) {
    stdOffsetSec = 0;
    dstOffsetSec = 0;
    memset(&dstStartRule, 0, sizeof(Rule));
    memset(&dstEndRule, 0, sizeof(Rule));
    dstStartRule.hour = 2;
    dstEndRule.hour = 2;

    char buffer[100];
    strncpy(buffer, tzString, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* stdPart = strtok(buffer, ",");
    if (!stdPart) return false;

    char* rulePart = strtok(NULL, "");

    char stdName[20] = {0};
    float stdOffHours = 0;
    int numParsed = sscanf(stdPart, "%[A-Za-z]%f", stdName, &stdOffHours);
    if (numParsed == 0) return false;

    this->stdOffsetSec = (numParsed > 1) ? -stdOffHours * 3600 : 0;

    char* dstNameStart = stdPart + strlen(stdName);
    while (*dstNameStart && (*dstNameStart < '0' || *dstNameStart > '9') && *dstNameStart != '-' && *dstNameStart != '+') {
        dstNameStart++;
    }
    while (*dstNameStart && ((*dstNameStart >= '0' && *dstNameStart <= '9') || *dstNameStart == '.' || *dstNameStart == '-' || *dstNameStart == '+')) {
        dstNameStart++;
    }

    if (*dstNameStart) {
        char dstName[20] = {0};
        float dstOffHours = 0;
        if (sscanf(dstNameStart, "%[A-Za-z]%f", dstName, &dstOffHours) >= 1) {
            if (strpbrk(dstNameStart, "0123456789")) {
                this->dstOffsetSec = -dstOffHours * 3600;
            } else {
                this->dstOffsetSec = this->stdOffsetSec + 3600;
            }
        }
    } else {
         this->dstOffsetSec = this->stdOffsetSec;
    }

    if (rulePart) {
        char* startRuleStr = rulePart;
        char* endRuleStr = strchr(rulePart, ',');
        if (endRuleStr) {
            *endRuleStr = '\0';
            endRuleStr++;
            if (!parseRule(startRuleStr, this->dstStartRule) || !parseRule(endRuleStr, this->dstEndRule)) {
            }
        }
    } else {
        this->dstOffsetSec = this->stdOffsetSec;
    }
    
    return true;
}

bool GeneralTimeConverter::parseRule(const char* ruleStr, Rule& rule) {
    if (ruleStr[0] == 'M') {
        int h = 2, m, w, d;
        char* slash = strchr(ruleStr, '/');
        if (slash) sscanf(slash, "/%d", &h);
        if (sscanf(ruleStr, "M%d.%d.%d", &m, &w, &d) != 3) return false;
        rule.month = m; rule.week = w; rule.day = d; rule.hour = h;
    } else {
        return false;
    }
    return true;
}

time_t GeneralTimeConverter::calculateRuleDate(int year, const Rule& rule, int offsetForLocalTime) const {
    struct tm t = {0};
    t.tm_year = year - 1900;
    t.tm_mon = rule.month - 1;
    t.tm_hour = rule.hour;
    t.tm_isdst = -1;

    if (rule.week == 5) {
        t.tm_mday = 1;
        t.tm_mon++;
        time_t first_of_next_month = timegm(&t);
        gmtime_r(&first_of_next_month, &t);
        time_t last_day_of_month = first_of_next_month - 86400;
        gmtime_r(&last_day_of_month, &t);
        int days_to_subtract = (t.tm_wday - rule.day + 7) % 7;
        t.tm_mday -= days_to_subtract;
    } else {
        t.tm_mday = 1;
        time_t first_of_month = timegm(&t);
        gmtime_r(&first_of_month, &t);
        int days_to_add = (rule.day - t.tm_wday + 7) % 7;
        t.tm_mday += days_to_add + (rule.week - 1) * 7;
    }

    time_t local_transition_epoch = timegm(&t);
    return local_transition_epoch - offsetForLocalTime;
}

// NEU: Implementierung der Getter
int GeneralTimeConverter::getStdOffsetSec() const {
    return stdOffsetSec;
}

int GeneralTimeConverter::getDstOffsetSec() const {
    return dstOffsetSec;
}