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
        
        // Meteorologische Jahreszeiten (basierend auf Monaten)
        if (month >= 3 && month <= 5) {
            return Season::SPRING;  // März, April, Mai
        } else if (month >= 6 && month <= 8) {
            return Season::SUMMER;  // Juni, Juli, August
        } else if (month >= 9 && month <= 11) {
            return Season::AUTUMN;  // September, Oktober, November
        } else {
            return Season::WINTER;  // Dezember, Januar, Februar
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
