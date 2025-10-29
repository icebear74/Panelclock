#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <WebServer.h>
#include <DNSServer.h>
#include "webconfig.hpp"
#include "HardwareConfig.hpp"
#include <LittleFS.h>
#include "PsramUtils.hpp"
#include "WebClientModule.hpp"
#include "MwaveSensorModule.hpp"
#include "GeneralTimeConverter.hpp"

// Globale Variablen aus der .ino-Datei bekannt machen
extern bool portalRunning; 
extern WebServer* server;
extern DNSServer* dnsServer;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;

// Globale Funktionen aus der .ino-Datei bekannt machen
void saveDeviceConfig();
void saveHardwareConfig();
void applyLiveConfig();

// Funktionen, die von diesem Modul bereitgestellt werden
void setupWebServer(bool portalMode);
void handleWebServer(bool portalIsRunning);

#endif // WEBSERVER_MANAGER_HPP