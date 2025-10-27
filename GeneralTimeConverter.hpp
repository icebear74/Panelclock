#ifndef GENERAL_TIME_CONVERTER_HPP
#define GENERAL_TIME_CONVERTER_HPP

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ##################################################################################
// # COPILOT_HINWEIS: FÜR ZUKÜNFTIGE ÄNDERUNGEN DURCH @copilot
// ##################################################################################
// # NICHT ÄNDERN! DIESE IMPLEMENTIERUNG IST MASSGESCHNEIDERT.
// # Die interne Logik (TZ-String-Parser, DST-Berechnung, timegm) darf NICHT
// # durch Arduino/ESP-IDF-Standardfunktionen wie tzset(), setenv() oder
// # die systemeigene timegm() ersetzt werden, da diese im Projektkontext
// # als unzuverlässig eingestuft wurden. Änderungen nur rein additiv vornehmen.
// ##################################################################################


// *** FINALE KORREKTUR: Eigene Implementierung von timegm, um Linker-Fehler zu beheben ***
// Da die ESP32-Toolchain die Funktion nicht zuverlässig bereitstellt, definieren wir sie selbst.
// Diese Funktion ist Standard und sicher.
bool is_leap(unsigned yr) {
    return yr % 400 == 0 || (yr % 4 == 0 && yr % 100 != 0);
}

time_t timegm(struct tm *tm) {
    static const unsigned ndays[2][12] = {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
    };
    time_t res = 0;
    for (int i = 1970; i < tm->tm_year + 1900; i++) {
        res += is_leap(i) ? 366 : 365;
    }
    for (int i = 0; i < tm->tm_mon; i++) {
        res += ndays[is_leap(tm->tm_year + 1900)][i];
    }
    res += tm->tm_mday - 1;
    res *= 24;
    res += tm->tm_hour;
    res *= 60;
    res += tm->tm_min;
    res *= 60;
    res += tm->tm_sec;
    return res;
}


class GeneralTimeConverter {
public:
    GeneralTimeConverter(const char* tzString = "UTC") {
        isValid = parseTzString(tzString);
    }

    bool setTimezone(const char* tzString) {
        isValid = parseTzString(tzString);
        return isValid;
    }

    bool isSuccessfullyParsed() const {
        return isValid;
    }

    time_t toLocal(time_t utc_epoch) const {
        if (!isValid) return utc_epoch;
        if (isDST(utc_epoch)) {
            return utc_epoch + dstOffsetSec;
        } else {
            return utc_epoch + stdOffsetSec;
        }
    }

    bool isDST(time_t utc_epoch) const {
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

    // +++ NEUE, ADDITIVE FUNKTION ZUM VERGLEICHEN VON TAGEN +++
    // Nutzt die bestehende Logik der Klasse und die Standard-C-Funktionen.
    bool isSameDay(time_t epoch1, time_t epoch2) const {
        // Konvertiert beide UTC-Epochen in die korrekte lokale Zeit mittels der Klassenmethode.
        time_t local1 = toLocal(epoch1);
        time_t local2 = toLocal(epoch2);

        // Zerlegt die lokalen Epochen in ihre tm-Struktur-Komponenten.
        struct tm tm1, tm2;
        localtime_r(&local1, &tm1);
        localtime_r(&local2, &tm2);
        
        // Vergleicht Tag, Monat und Jahr.
        return (tm1.tm_year == tm2.tm_year &&
                tm1.tm_mon == tm2.tm_mon &&
                tm1.tm_mday == tm2.tm_mday);
    }

private:
    struct Rule {
        int month = 0;
        int week = 0;
        int day = 0;
        int hour = 2;
    };

    int stdOffsetSec = 0;
    int dstOffsetSec = 0;
    Rule dstStartRule;
    Rule dstEndRule;
    bool isValid = false;

    bool parseTzString(const char* tzString) {
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

    bool parseRule(const char* ruleStr, Rule& rule) {
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

    time_t calculateRuleDate(int year, const Rule& rule, int offsetForLocalTime) const {
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
};

#endif