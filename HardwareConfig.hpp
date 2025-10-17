#ifndef HARDWARE_CONFIG_HPP
#define HARDWARE_CONFIG_HPP

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

struct HardwareConfig {
    uint8_t R1 = 1, G1 = 2, B1 = 4;
    uint8_t R2 = 5, G2 = 6, B2 = 7;
    uint8_t A = 15, B = 16, C = 17, D = 18, E = 3;
    uint8_t CLK = 19, LAT = 20, OE = 21;

    // --- NEU: Optionale Hardware-Pins ---
    uint8_t mwaveRxPin = 42;
    uint8_t mwaveTxPin = 41;
    uint8_t displayRelayPin = 255; // 255 = nicht verwendet
};

// --- KORREKTUR: Auf Zeiger umgestellt ---
extern HardwareConfig* hardwareConfig;

void loadHardwareConfig() {
    if (!hardwareConfig) return; // Sicherheitsabfrage
    if (LittleFS.exists("/hardware.json")) {
        File configFile = LittleFS.open("/hardware.json", "r");
        JsonDocument doc;
        if (deserializeJson(doc, configFile) == DeserializationError::Ok) {
            hardwareConfig->R1 = doc["R1"] | hardwareConfig->R1;
            hardwareConfig->G1 = doc["G1"] | hardwareConfig->G1;
            hardwareConfig->B1 = doc["B1"] | hardwareConfig->B1;
            hardwareConfig->R2 = doc["R2"] | hardwareConfig->R2;
            hardwareConfig->G2 = doc["G2"] | hardwareConfig->G2;
            hardwareConfig->B2 = doc["B2"] | hardwareConfig->B2;
            hardwareConfig->A = doc["A"] | hardwareConfig->A;
            hardwareConfig->B = doc["B"] | hardwareConfig->B;
            hardwareConfig->C = doc["C"] | hardwareConfig->C;
            hardwareConfig->D = doc["D"] | hardwareConfig->D;
            hardwareConfig->E = doc["E"] | hardwareConfig->E;
            hardwareConfig->CLK = doc["CLK"] | hardwareConfig->CLK;
            hardwareConfig->LAT = doc["LAT"] | hardwareConfig->LAT;
            hardwareConfig->OE = doc["OE"] | hardwareConfig->OE;

            hardwareConfig->mwaveRxPin = doc["mwaveRxPin"] | hardwareConfig->mwaveRxPin;
            hardwareConfig->mwaveTxPin = doc["mwaveTxPin"] | hardwareConfig->mwaveTxPin;
            hardwareConfig->displayRelayPin = doc["displayRelayPin"] | hardwareConfig->displayRelayPin;
            
            Serial.println("Hardware-Konfiguration aus hardware.json geladen.");
        }
        configFile.close();
    } else {
        Serial.println("Keine hardware.json gefunden, verwende Standard-Pinbelegung.");
    }
}

void saveHardwareConfig() {
    if (!hardwareConfig) return; // Sicherheitsabfrage
    JsonDocument doc;
    doc["R1"] = hardwareConfig->R1; doc["G1"] = hardwareConfig->G1; doc["B1"] = hardwareConfig->B1;
    doc["R2"] = hardwareConfig->R2; doc["G2"] = hardwareConfig->G2; doc["B2"] = hardwareConfig->B2;
    doc["A"] = hardwareConfig->A;   doc["B"] = hardwareConfig->B;   doc["C"] = hardwareConfig->C;
    doc["D"] = hardwareConfig->D;   doc["E"] = hardwareConfig->E;
    doc["CLK"] = hardwareConfig->CLK; doc["LAT"] = hardwareConfig->LAT; doc["OE"] = hardwareConfig->OE;

    doc["mwaveRxPin"] = hardwareConfig->mwaveRxPin;
    doc["mwaveTxPin"] = hardwareConfig->mwaveTxPin;
    doc["displayRelayPin"] = hardwareConfig->displayRelayPin;
    
    File configFile = LittleFS.open("/hardware.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("Hardware-Konfiguration gespeichert.");
    }
}

#endif