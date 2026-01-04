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
    
    Season getCurrentSeason(time_t currentTime) {
        if (currentTime == 0) {
            time(&currentTime);
        }
        
        struct tm tm_now;
        localtime_r(&currentTime, &tm_now);
        int month = tm_now.tm_mon + 1; // 1-12
        int day = tm_now.tm_mday;      // 1-31
        
        // Astronomische Jahreszeiten (basierend auf Sonnenwenden und Tagundnachtgleichen)
        // Diese Daten variieren leicht von Jahr zu Jahr, aber wir verwenden Durchschnittswerte
        
        // Frühling: ~20. März bis ~20. Juni
        if (month == 3 && day >= 20) {
            return Season::SPRING;
        } else if (month == 4 || month == 5) {
            return Season::SPRING;
        } else if (month == 6 && day <= 20) {
            return Season::SPRING;
        }
        
        // Sommer: ~21. Juni bis ~22. September
        else if (month == 6 && day >= 21) {
            return Season::SUMMER;
        } else if (month == 7 || month == 8) {
            return Season::SUMMER;
        } else if (month == 9 && day <= 22) {
            return Season::SUMMER;
        }
        
        // Herbst: ~23. September bis ~20. Dezember
        else if (month == 9 && day >= 23) {
            return Season::AUTUMN;
        } else if (month == 10 || month == 11) {
            return Season::AUTUMN;
        } else if (month == 12 && day <= 20) {
            return Season::AUTUMN;
        }
        
        // Winter: ~21. Dezember bis ~19. März
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
