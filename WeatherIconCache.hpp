#pragma once
#include "WeatherIcons_Main.hpp"
#include <map>
#include "PsramUtils.hpp" // Falls PSRAM für Icondaten genutzt wird

/*
 * Cache für skalierte Wettericons im PSRAM.
 * - Key: Iconname + Zielgröße + Tag/Nacht
 * - Value: gescaltes Icon als RGB888, vollständig im PSRAM gespeichert!
 */
class WeatherIconCache {
public:
    /*
     * Gibt ein skaliertes Icon aus dem Cache zurück.
     * Wenn nicht vorhanden, wird es Bilinear aus dem Quellicon erstellt und gecached!
     * Alles wird im PSRAM gehalten.
     */
    const WeatherIcon* getScaled(const std::string& name, uint8_t targetSize, bool isNight);
    void clear();
    ~WeatherIconCache() { clear(); }
private:
    struct Key {
        std::string name;
        uint8_t targetSize;
        bool isNight;
        bool operator<(const Key& rhs) const {
            return std::tie(name, targetSize, isNight) < std::tie(rhs.name, rhs.targetSize, rhs.isNight);
        }
    };
    std::map<Key, WeatherIcon*> cache;

    // Skaliert ein RGB888-Icon bilinear (pure C++, integer, im PSRAM).
    WeatherIcon* scaleBilinear(const WeatherIcon* src, uint8_t targetSize, bool doNightTransform=false);
};