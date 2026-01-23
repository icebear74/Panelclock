/**
 * @file DisplayConfig.cpp
 * @brief Display Configuration Management Implementation
 */

#include "DisplayConfig.hpp"
#include <ArduinoJson.h>

DisplayConfig::DisplayConfig() : _brightness(128), _configHash(0) {
    setDefaultConfig();
}

DisplayConfig::~DisplayConfig() {
    _prefs.end();
}

bool DisplayConfig::begin() {
    Serial.println("[DisplayConfig] Initializing...");
    
    if (!_prefs.begin("display", false)) {
        Serial.println("[DisplayConfig] ERROR: Failed to open NVS");
        return false;
    }
    
    // Try to load config from NVS
    if (loadFromNVS()) {
        Serial.println("[DisplayConfig] Configuration loaded from NVS");
        Serial.printf("[DisplayConfig] Hash: 0x%08X, Brightness: %d\n", 
                     _configHash, _brightness);
        return true;
    }
    
    Serial.println("[DisplayConfig] Using default configuration");
    setDefaultConfig();
    saveToNVS(); // Save defaults
    return true;
}

bool DisplayConfig::loadFromNVS() {
    if (!_prefs.isKey("hash")) {
        return false;  // No config stored yet
    }
    
    _configHash = _prefs.getUInt("hash", 0);
    _brightness = _prefs.getUChar("brightness", 128);
    
    // Load HUB75 pins
    _hub75Pins.R1 = _prefs.getUChar("R1", 1);
    _hub75Pins.G1 = _prefs.getUChar("G1", 2);
    _hub75Pins.B1 = _prefs.getUChar("B1", 4);
    _hub75Pins.R2 = _prefs.getUChar("R2", 5);
    _hub75Pins.G2 = _prefs.getUChar("G2", 6);
    _hub75Pins.B2 = _prefs.getUChar("B2", 7);
    _hub75Pins.A = _prefs.getUChar("A", 15);
    _hub75Pins.B = _prefs.getUChar("B", 16);
    _hub75Pins.C = _prefs.getUChar("C", 17);
    _hub75Pins.D = _prefs.getUChar("D", 18);
    _hub75Pins.E = _prefs.getUChar("E", 3);
    _hub75Pins.CLK = _prefs.getUChar("CLK", 19);
    _hub75Pins.LAT = _prefs.getUChar("LAT", 20);
    _hub75Pins.OE = _prefs.getUChar("OE", 21);
    
    return true;
}

bool DisplayConfig::saveToNVS() {
    Serial.println("[DisplayConfig] Saving configuration to NVS...");
    
    _prefs.putUInt("hash", _configHash);
    _prefs.putUChar("brightness", _brightness);
    
    // Save HUB75 pins
    _prefs.putUChar("R1", _hub75Pins.R1);
    _prefs.putUChar("G1", _hub75Pins.G1);
    _prefs.putUChar("B1", _hub75Pins.B1);
    _prefs.putUChar("R2", _hub75Pins.R2);
    _prefs.putUChar("G2", _hub75Pins.G2);
    _prefs.putUChar("B2", _hub75Pins.B2);
    _prefs.putUChar("A", _hub75Pins.A);
    _prefs.putUChar("B", _hub75Pins.B);
    _prefs.putUChar("C", _hub75Pins.C);
    _prefs.putUChar("D", _hub75Pins.D);
    _prefs.putUChar("E", _hub75Pins.E);
    _prefs.putUChar("CLK", _hub75Pins.CLK);
    _prefs.putUChar("LAT", _hub75Pins.LAT);
    _prefs.putUChar("OE", _hub75Pins.OE);
    
    Serial.println("[DisplayConfig] Configuration saved");
    return true;
}

void DisplayConfig::setHUB75Pins(const HUB75Pins& pins) {
    _hub75Pins = pins;
    _configHash = calculateHash();
}

void DisplayConfig::setBrightness(uint8_t brightness) {
    _brightness = brightness;
}

bool DisplayConfig::updateFromJSON(const char* jsonConfig) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonConfig);
    
    if (error) {
        Serial.printf("[DisplayConfig] JSON parse error: %s\n", error.c_str());
        return false;
    }
    
    // Extract HUB75 pins
    if (doc.containsKey("hw")) {
        JsonObject hw = doc["hw"];
        _hub75Pins.R1 = hw["R1"] | 1;
        _hub75Pins.G1 = hw["G1"] | 2;
        _hub75Pins.B1 = hw["B1"] | 4;
        _hub75Pins.R2 = hw["R2"] | 5;
        _hub75Pins.G2 = hw["G2"] | 6;
        _hub75Pins.B2 = hw["B2"] | 7;
        _hub75Pins.A = hw["A"] | 15;
        _hub75Pins.B = hw["B"] | 16;
        _hub75Pins.C = hw["C"] | 17;
        _hub75Pins.D = hw["D"] | 18;
        _hub75Pins.E = hw["E"] | 3;
        _hub75Pins.CLK = hw["CLK"] | 19;
        _hub75Pins.LAT = hw["LAT"] | 20;
        _hub75Pins.OE = hw["OE"] | 21;
    }
    
    // Extract display settings
    if (doc.containsKey("display")) {
        JsonObject display = doc["display"];
        _brightness = display["brightness"] | 128;
    }
    
    // Extract and verify hash
    if (doc.containsKey("hash")) {
        uint32_t receivedHash = strtoul(doc["hash"] | "0", nullptr, 16);
        _configHash = receivedHash;
    } else {
        _configHash = calculateHash();
    }
    
    Serial.printf("[DisplayConfig] Config updated from JSON (hash: 0x%08X)\n", _configHash);
    return true;
}

void DisplayConfig::setDefaultConfig() {
    // Default HUB75 pin configuration (matching Control-ESP defaults)
    _hub75Pins.R1 = 1;
    _hub75Pins.G1 = 2;
    _hub75Pins.B1 = 4;
    _hub75Pins.R2 = 5;
    _hub75Pins.G2 = 6;
    _hub75Pins.B2 = 7;
    _hub75Pins.A = 15;
    _hub75Pins.B = 16;
    _hub75Pins.C = 17;
    _hub75Pins.D = 18;
    _hub75Pins.E = 3;
    _hub75Pins.CLK = 19;
    _hub75Pins.LAT = 20;
    _hub75Pins.OE = 21;
    
    _brightness = 128;
    _configHash = calculateHash();
}

uint32_t DisplayConfig::calculateHash() {
    // Simple hash of all config values
    uint32_t hash = 0;
    hash ^= (_hub75Pins.R1 << 0) | (_hub75Pins.G1 << 8) | (_hub75Pins.B1 << 16) | (_hub75Pins.R2 << 24);
    hash ^= (_hub75Pins.G2 << 0) | (_hub75Pins.B2 << 8) | (_hub75Pins.A << 16) | (_hub75Pins.B << 24);
    hash ^= (_hub75Pins.C << 0) | (_hub75Pins.D << 8) | (_hub75Pins.E << 16) | (_hub75Pins.CLK << 24);
    hash ^= (_hub75Pins.LAT << 0) | (_hub75Pins.OE << 8) | (_brightness << 16);
    return hash;
}
