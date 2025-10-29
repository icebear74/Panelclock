#ifndef HARDWARE_CONFIG_HPP
#define HARDWARE_CONFIG_HPP

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Die Struct-Definition bleibt im Header.
struct HardwareConfig {
    uint8_t R1 = 1, G1 = 2, B1 = 4;
    uint8_t R2 = 5, G2 = 6, B2 = 7;
    uint8_t A = 15, B = 16, C = 17, D = 18, E = 3;
    uint8_t CLK = 19, LAT = 20, OE = 21;

    // Optionale Hardware-Pins
    uint8_t mwaveRxPin = 42;
    uint8_t mwaveTxPin = 41;
    uint8_t displayRelayPin = 255; // 255 = nicht verwendet
};

// Den globalen Zeiger als 'extern' deklarieren.
extern HardwareConfig* hardwareConfig;

// Die Funktionen nur deklarieren.
void loadHardwareConfig();
void saveHardwareConfig();

#endif // HARDWARE_CONFIG_HPP