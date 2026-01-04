#ifndef TIMEUTILITIES_HPP
#define TIMEUTILITIES_HPP

#include <time.h>

/**
 * @brief Globale Zeitzone und astronomische Daten
 * 
 * Diese Werte werden vom WeatherModule aktualisiert und können
 * von anderen Modulen gelesen werden.
 */
namespace TimeUtilities {
    // Sonnenauf- und Sonnenuntergang für den aktuellen Tag (UTC)
    extern time_t globalSunrise;
    extern time_t globalSunset;
    
    // Jahreszeit-Erkennung
    enum class Season {
        SPRING,  // Frühling (~20. März bis ~20. Juni)
        SUMMER,  // Sommer (~21. Juni bis ~22. September)
        AUTUMN,  // Herbst (~23. September bis ~20. Dezember)
        WINTER   // Winter (~21. Dezember bis ~19. März)
    };
    
    /**
     * @brief Prüft ob es aktuell Nacht ist (nach Sonnenuntergang oder vor Sonnenaufgang)
     * @param currentTime Aktuelle Zeit (UTC), default: jetzt
     * @return true wenn es Nacht ist, false sonst
     */
    bool isNightTime(time_t currentTime = 0);
    
    /**
     * @brief Gibt die aktuelle Jahreszeit zurück basierend auf präzisen astronomischen Berechnungen
     *        Die exakten Daten der Sonnenwenden und Tagundnachtgleichen werden für jedes Jahr berechnet
     * @param currentTime Aktuelle Zeit (UTC), default: jetzt
     * @return Jahreszeit (SPRING, SUMMER, AUTUMN, WINTER)
     */
    Season getCurrentSeason(time_t currentTime = 0);
    
    /**
     * @brief Gibt den Namen der Jahreszeit als String zurück
     * @param season Die Jahreszeit
     * @return Name der Jahreszeit auf Deutsch
     */
    const char* getSeasonName(Season season);
}

#endif // TIMEUTILITIES_HPP
