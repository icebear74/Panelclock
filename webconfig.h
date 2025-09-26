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

void startConfigPortalIfNeeded();
bool loadConfig();
bool saveConfig();
void handleConfigPortal();
bool tryConnectAndSave(const String& ssid, const String& password);

#endif