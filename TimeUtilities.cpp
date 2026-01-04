#include "TimeUtilities.hpp"
#include <time.h>

namespace TimeUtilities {
    // Globale Variablen für Sonnenauf- und Sonnenuntergang
    time_t globalSunrise = 0;
    time_t globalSunset = 0;
    
    bool isNightTime(time_t currentTime) {
        if (currentTime == 0) {
            time(&currentTime);
        }
        
        // Wenn keine Wetterdaten verfügbar sind, verwende eine einfache Zeitregel
        // (zwischen 20:00 und 06:00 Uhr als Nacht)
        if (globalSunrise == 0 || globalSunset == 0) {
            struct tm tm_now;
            localtime_r(&currentTime, &tm_now);
            int hour = tm_now.tm_hour;
            return (hour >= 20 || hour < 6);
        }
        
        // Prüfe ob aktuelle Zeit nach Sonnenuntergang oder vor Sonnenaufgang ist
        return (currentTime >= globalSunset || currentTime < globalSunrise);
    }
    
    /**
     * @brief Prüft ob ein Jahr ein Schaltjahr ist (mit 100- und 400-Jahres-Regel)
     * @param year Das Jahr
     * @return true wenn Schaltjahr, false sonst
     */
    bool isLeapYear(int year) {
        // Ein Jahr ist ein Schaltjahr wenn:
        // - teilbar durch 4 UND
        // - NICHT teilbar durch 100 ODER teilbar durch 400
        if (year % 400 == 0) return true;      // Teilbar durch 400 → Schaltjahr (z.B. 2000)
        if (year % 100 == 0) return false;     // Teilbar durch 100 → kein Schaltjahr (z.B. 1900)
        if (year % 4 == 0) return true;        // Teilbar durch 4 → Schaltjahr (z.B. 2024)
        return false;                          // Sonst kein Schaltjahr
    }
    
    /**
     * @brief Zählt die Schaltjahre zwischen zwei Jahren (mit 100- und 400-Jahres-Regel)
     * @param year1 Startjahr
     * @param year2 Endjahr
     * @return Anzahl der Schaltjahre im Intervall
     */
    int countLeapYears(int year1, int year2) {
        int count = 0;
        for (int y = year1; y <= year2; y++) {
            if (isLeapYear(y)) count++;
        }
        return count;
    }
    
    /**
     * @brief Berechnet den Tag der Frühjahrs-Tagundnachtgleiche für ein gegebenes Jahr
     * @param year Das Jahr
     * @return Tag im März (typisch 19-21)
     */
    int getVernalEquinoxDay(int year) {
        // Vereinfachte Näherungsformel für die Frühjahrs-Tagundnachtgleiche
        // Basiert auf astronomischen Berechnungen
        // Für Jahre 2000-2100
        if (year < 2000 || year > 2100) year = 2000; // Fallback
        
        // Berechne Schaltjahre zwischen 2000 und dem aktuellen Jahr (exklusive)
        int leapYears = countLeapYears(2001, year - 1);
        
        // Näherungsformel mit korrekter Schaltjahreskorrektur
        double d = 20.0 + 0.242189 * (year - 2000) - leapYears;
        return (int)d;
    }
    
    /**
     * @brief Berechnet den Tag der Sommersonnenwende für ein gegebenes Jahr
     * @param year Das Jahr
     * @return Tag im Juni (typisch 20-22)
     */
    int getSummerSolsticeDay(int year) {
        // Vereinfachte Näherungsformel für die Sommersonnenwende
        if (year < 2000 || year > 2100) year = 2000;
        
        int leapYears = countLeapYears(2001, year - 1);
        double d = 21.0 + 0.242189 * (year - 2000) - leapYears;
        return (int)d;
    }
    
    /**
     * @brief Berechnet den Tag der Herbst-Tagundnachtgleiche für ein gegebenes Jahr
     * @param year Das Jahr
     * @return Tag im September (typisch 22-24)
     */
    int getAutumnalEquinoxDay(int year) {
        // Vereinfachte Näherungsformel für die Herbst-Tagundnachtgleiche
        if (year < 2000 || year > 2100) year = 2000;
        
        int leapYears = countLeapYears(2001, year - 1);
        double d = 23.0 + 0.242189 * (year - 2000) - leapYears;
        return (int)d;
    }
    
    /**
     * @brief Berechnet den Tag der Wintersonnenwende für ein gegebenes Jahr
     * @param year Das Jahr
     * @return Tag im Dezember (typisch 20-22)
     */
    int getWinterSolsticeDay(int year) {
        // Vereinfachte Näherungsformel für die Wintersonnenwende
        if (year < 2000 || year > 2100) year = 2000;
        
        int leapYears = countLeapYears(2001, year - 1);
        double d = 21.0 + 0.242189 * (year - 2000) - leapYears;
        return (int)d;
    }
    
    Season getCurrentSeason(time_t currentTime) {
        if (currentTime == 0) {
            time(&currentTime);
        }
        
        struct tm tm_now;
        localtime_r(&currentTime, &tm_now);
        int month = tm_now.tm_mon + 1; // 1-12
        int day = tm_now.tm_mday;      // 1-31
        int year = tm_now.tm_year + 1900;
        
        // Berechne die exakten Tage für die astronomischen Ereignisse dieses Jahres
        int vernalEquinox = getVernalEquinoxDay(year);      // März (Frühling beginnt)
        int summerSolstice = getSummerSolsticeDay(year);     // Juni (Sommer beginnt)
        int autumnalEquinox = getAutumnalEquinoxDay(year);   // September (Herbst beginnt)
        int winterSolstice = getWinterSolsticeDay(year);     // Dezember (Winter beginnt)
        
        // Bestimme die Jahreszeit basierend auf den berechneten Daten
        
        // Frühling: Von Frühjahrs-Tagundnachtgleiche (März) bis Sommersonnenwende (Juni)
        if ((month == 3 && day >= vernalEquinox) ||
            (month == 4) ||
            (month == 5) ||
            (month == 6 && day < summerSolstice)) {
            return Season::SPRING;
        }
        
        // Sommer: Von Sommersonnenwende (Juni) bis Herbst-Tagundnachtgleiche (September)
        else if ((month == 6 && day >= summerSolstice) ||
                 (month == 7) ||
                 (month == 8) ||
                 (month == 9 && day < autumnalEquinox)) {
            return Season::SUMMER;
        }
        
        // Herbst: Von Herbst-Tagundnachtgleiche (September) bis Wintersonnenwende (Dezember)
        else if ((month == 9 && day >= autumnalEquinox) ||
                 (month == 10) ||
                 (month == 11) ||
                 (month == 12 && day < winterSolstice)) {
            return Season::AUTUMN;
        }
        
        // Winter: Von Wintersonnenwende (Dezember) bis Frühjahrs-Tagundnachtgleiche (März)
        else {
            return Season::WINTER;
        }
    }
    
    const char* getSeasonName(Season season) {
        switch (season) {
            case Season::SPRING: return "Frühling";
            case Season::SUMMER: return "Sommer";
            case Season::AUTUMN: return "Herbst";
            case Season::WINTER: return "Winter";
            default: return "Unbekannt";
        }
    }
}
