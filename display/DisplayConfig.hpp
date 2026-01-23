/**
 * @file DisplayConfig.hpp
 * @brief Display Configuration Management
 * 
 * Manages configuration storage in NVS (Non-Volatile Storage)
 * - Receives config from Control-ESP via SPI
 * - Persists config locally
 * - Provides config to display driver
 */

#ifndef DISPLAY_CONFIG_HPP
#define DISPLAY_CONFIG_HPP

#include <Arduino.h>
#include <Preferences.h>

// HUB75 pin configuration structure
struct HUB75Pins {
    uint8_t R1, G1, B1;
    uint8_t R2, G2, B2;
    uint8_t A, B, C, D, E;
    uint8_t CLK, LAT, OE;
};

class DisplayConfig {
public:
    DisplayConfig();
    ~DisplayConfig();
    
    bool begin();
    bool loadFromNVS();
    bool saveToNVS();
    
    // Getters
    HUB75Pins getHUB75Pins() const { return _hub75Pins; }
    uint8_t getBrightness() const { return _brightness; }
    uint32_t getConfigHash() const { return _configHash; }
    
    // Setters (called when config received from Control-ESP)
    void setHUB75Pins(const HUB75Pins& pins);
    void setBrightness(uint8_t brightness);
    void setConfigHash(uint32_t hash) { _configHash = hash; }
    
    // Config update from JSON (received via SPI)
    bool updateFromJSON(const char* jsonConfig);
    
private:
    Preferences _prefs;
    HUB75Pins _hub75Pins;
    uint8_t _brightness;
    uint32_t _configHash;
    
    void setDefaultConfig();
    uint32_t calculateHash();
};

#endif // DISPLAY_CONFIG_HPP
