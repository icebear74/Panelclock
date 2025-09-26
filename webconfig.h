#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <Arduino.h>

struct DeviceConfig {
  String ssid;
  String password;
  String tankerApiKey;
  String stationId;
  String otaPassword;
  String hostname;
};

extern DeviceConfig deviceConfig;

// Startet das Konfigurations-Portal (AP + WebServer), falls keine WiFi-Daten vorhanden sind.
// Wenn das Portal aktiv ist, muss handleConfigPortal() periodisch in loop() aufgerufen werden.
void startConfigPortalIfNeeded();

// Erzwingt den Start des Konfig-Portals (AP + WebServer)
void startConfigPortal();

// Lädt /config.json aus LittleFS (gibt true zurück wenn Erfolg)
bool loadConfig();

// Speichert deviceConfig nach /config.json (gibt true zurück wenn Erfolg)
bool saveConfig();

// Muss in loop() aufgerufen werden, wenn das Konfig-Portal aktiv ist
void handleConfigPortal();

// Versucht sich mit übergebenen Credentials zu verbinden (gibt true zurück bei Erfolg)
bool tryConnectAndSave(const String& ssid, const String& password);

// Web UI handlers
void handleRoot();
void handleSave();
void handleApiScan();
void handleApiConnect();
void handleApiGetConfig();
void handleApiPostConfig();

// New: status + wifi pages and APIs
void handleStatus();
void handleApiStatus();
void handleWifiPage();
void handleApiWifiSave();

#endif