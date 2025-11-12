#pragma once
#include <Arduino.h>
#include <string>
#include <map>

// Wetter-Icon-Datentyp für RGB888 Icons.
struct WeatherIcon {
    const uint8_t* data;
    uint16_t width, height;
};

// Hauptregistry für alle Icons (WMO und Specials!)
class WeatherIconSet {
public:
    void registerIcon(const std::string& name, const WeatherIcon* day, const WeatherIcon* night = nullptr);

    // Robuste Prüfung: Dummy-Icons werden automatisch auf Unknown gemappt!
    const WeatherIcon* getIcon(const std::string& name, bool isNight) const {
        Serial.printf("getIcon('%s', isNight=%d) called\n", name.c_str(), isNight);
        
        auto probe = [&](const std::map<std::string, const WeatherIcon*>& map, const char* mapName) -> const WeatherIcon* {
            auto it = map.find(name);
            if (it == map.end()) {
                Serial.printf("  %s: icon '%s' not found in map\n", mapName, name.c_str());
                return nullptr;
            }
            if (it->second == nullptr || it->second->data == nullptr) {
                Serial.printf("  %s: icon '%s' has null pointer\n", mapName, name.c_str());
                return nullptr;
            }

            const WeatherIcon* icon = it->second;
            if (icon->width != 48 || icon->height != 48) {
                Serial.printf("  %s: icon '%s' has wrong size %dx%d\n", mapName, name.c_str(), icon->width, icon->height);
                return nullptr;
            }

            // Dummy-Prüfung: Icon beginnt mit Marker-Sequenz 0x00 0x01 0x02 ... 0x0A (11 bytes)
            bool isDummy = true;
            for (uint8_t i = 0; i < 11; i++) {
                if (pgm_read_byte(icon->data + i) != i) {
                    isDummy = false;
                    break;
                }
            }
            
            if (isDummy) {
                Serial.printf("  %s: icon '%s' detected as DUMMY (marker sequence found), pointer=0x%08x\n", 
                             mapName, name.c_str(), (uint32_t)icon->data);
                return nullptr;
            }
            
            Serial.printf("  %s: icon '%s' is VALID, pointer=0x%08x\n", mapName, name.c_str(), (uint32_t)icon->data);
            return icon;
        };

        const WeatherIcon* icon = isNight ? probe(nightIcons, "nightIcons") : nullptr;
        if (!icon) icon = probe(dayIcons, "dayIcons");
        if (!icon) {
            icon = unknown;
            Serial.printf("  Fallback to UNKNOWN icon, pointer=0x%08x\n", (uint32_t)(unknown ? unknown->data : 0));
        }
        Serial.printf("getIcon returning pointer=0x%08x\n", (uint32_t)(icon ? icon->data : 0));
        return icon;
    }

    void setUnknown(const WeatherIcon* icon) { unknown = icon; }
    const WeatherIcon* getUnknown() const { return unknown; }

private:
    std::map<std::string, const WeatherIcon*> dayIcons, nightIcons;
    const WeatherIcon* unknown = nullptr;
};

// Cache und Resizer (Bilinear/Bicubic einfach tauschbar)
class WeatherIconCache;
extern WeatherIconSet globalWeatherIconSet;
extern WeatherIconCache globalWeatherIconCache;

// Register function for main weather icons
void registerWeatherIcons();

// Inline implementation of registerIcon
inline void WeatherIconSet::registerIcon(const std::string& name, const WeatherIcon* day, const WeatherIcon* night) {
    if (day) dayIcons[name] = day;
    if (night) nightIcons[name] = night;
}

// Mapping für WMO → Iconname
#include "WeatherWMOMap.hpp"