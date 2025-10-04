#ifndef GENERAL_TIME_CONVERTER_HPP
#define GENERAL_TIME_CONVERTER_HPP

#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// *** FINALE KORREKTUR: Explizite Deklaration von timegm für den C++ Compiler ***
// Dies teilt dem Compiler mit, dass die Funktion existiert und vom Linker gefunden wird.
#ifdef __cplusplus
extern "C" {
#endif
time_t timegm(struct tm *tm);
#ifdef __cplusplus
}
#endif

class GeneralTimeConverter {
public:
    GeneralTimeConverter(const char* tzString = "UTC") {
        isValid = parseTzString(tzString);
    }

    /**
     * @brief (NEU) Setzt eine neue Zeitzone zur Laufzeit.
     * @param tzString Der neue POSIX-Zeitzonen-String.
     * @return true bei Erfolg, false bei einem Parse-Fehler.
     */
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
        if (!isValid || dstOffsetSec == stdOffsetSec) return false; // Keine Sommerzeit, wenn Offsets gleich sind

        struct tm t;
        gmtime_r(&utc_epoch, &t);
        int year = t.tm_year + 1900;

        time_t dst_start_utc = calculateRuleDate(year, dstStartRule, stdOffsetSec);
        time_t dst_end_utc = calculateRuleDate(year, dstEndRule, dstOffsetSec);

        // Standard-Reihenfolge (Nordhalbkugel)
        if (dst_start_utc < dst_end_utc) {
            return (utc_epoch >= dst_start_utc && utc_epoch < dst_end_utc);
        } 
        // Inverse Reihenfolge (Südhalbkugel)
        else {
            return (utc_epoch >= dst_start_utc || utc_epoch < dst_end_utc);
        }
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
        // Reset state
        stdOffsetSec = 0;
        dstOffsetSec = 0;
        memset(&dstStartRule, 0, sizeof(Rule));
        memset(&dstEndRule, 0, sizeof(Rule));
        dstStartRule.hour = 2; // Default
        dstEndRule.hour = 2; // Default

        char buffer[100];
        strncpy(buffer, tzString, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        char* stdPart = strtok(buffer, ",");
        if (!stdPart) return false;

        char* rulePart = strtok(NULL, ""); // Nimm den Rest des Strings

        // Parse std name and offset
        char stdName[20] = {0};
        float stdOffHours = 0;
        int numParsed = sscanf(stdPart, "%[A-Za-z]%f", stdName, &stdOffHours);
        if (numParsed == 0) return false;

        this->stdOffsetSec = (numParsed > 1) ? -stdOffHours * 3600 : 0;

        // Parse dst name and offset
        char* dstNameStart = stdPart + strlen(stdName);
        while (*dstNameStart && (*dstNameStart < '0' || *dstNameStart > '9') && *dstNameStart != '-' && *dstNameStart != '+') {
            dstNameStart++;
        }
        while (*dstNameStart && ((*dstNameStart >= '0' && *dstNameStart <= '9') || *dstNameStart == '.' || *dstNameStart == '-' || *dstNameStart == '+')) {
            dstNameStart++;
        }

        if (*dstNameStart) { // DST part exists
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
                    // Parsing failed, but maybe it's a timezone without DST rules
                }
            }
        } else {
            // No rules, DST offset must be same as STD offset
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
        } else { // Julian day format J<n> or <n>
            // Diese Implementierung unterstützt nur das M-Format für die Einfachheit.
            return false;
        }
        return true;
    }

    // *** KORREKTUR: Methode als const deklariert, damit sie von anderen const-Methoden aufgerufen werden kann ***
    time_t calculateRuleDate(int year, const Rule& rule, int offsetForLocalTime) const {
        struct tm t = {0};
        t.tm_year = year - 1900;
        t.tm_mon = rule.month - 1;
        t.tm_hour = rule.hour;
        t.tm_isdst = 0;

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