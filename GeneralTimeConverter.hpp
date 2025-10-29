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

// --- Funktionsdeklarationen für globale Hilfsfunktionen ---
// Die Implementierungen befinden sich in GeneralTimeConverter.cpp
extern bool is_leap(unsigned yr);
extern time_t timegm(struct tm *tm);


class GeneralTimeConverter {
public:
    GeneralTimeConverter(const char* tzString = "UTC");
    bool setTimezone(const char* tzString);
    bool isSuccessfullyParsed() const;
    time_t toLocal(time_t utc_epoch) const;
    bool isDST(time_t utc_epoch) const;
    bool isSameDay(time_t epoch1, time_t epoch2) const;

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

    bool parseTzString(const char* tzString);
    bool parseRule(const char* ruleStr, Rule& rule);
    time_t calculateRuleDate(int year, const Rule& rule, int offsetForLocalTime) const;
};

#endif // GENERAL_TIME_CONVERTER_HPP