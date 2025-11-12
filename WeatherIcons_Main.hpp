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
        auto probe = [&](const std::map<std::string, const WeatherIcon*>& map) -> const WeatherIcon* {
            auto it = map.find(name);
            if (it == map.end() || it->second == nullptr || it->second->data == nullptr)
                return nullptr; // Icon fehlt

            const WeatherIcon* icon = it->second;
            if (icon->width != 48 || icon->height != 48) return nullptr; // Falsche Größe

            // Dummy-Prüfung: Icon beginnt mit 0x00 0x00 0x00 und enthält NUR diese, dann Dummy!
            size_t icon_bytes = icon->width * icon->height * 3;
            if (icon->data[0] == 0x00 && icon->data[1] == 0x00 && icon->data[2] == 0x00) {
                for (size_t i = 3; i < icon_bytes; ++i) {
                    if (icon->data[i] != 0x00) { return icon; }
                }
                return nullptr; // Dummy!
            }
            return icon;
        };

        const WeatherIcon* icon = isNight ? probe(nightIcons) : nullptr;
        if (!icon) icon = probe(dayIcons);
        if (!icon) icon = unknown;
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