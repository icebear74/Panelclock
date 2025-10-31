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
#include "DataModule.hpp"

// Globale Variablen aus der .ino-Datei bekannt machen
extern bool portalRunning; 
extern WebServer* server;
extern DNSServer* dnsServer;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;
extern DataModule* dataModule;

// Globale Funktionen aus der .ino-Datei bekannt machen
void saveDeviceConfig();
void saveHardwareConfig();
void applyLiveConfig();

/**
 * @brief Richtet alle Routen und Handler für den Webserver ein.
 * @param portalMode Wenn true, wird der DNS-Server für das Captive-Portal gestartet.
 */
void setupWebServer(bool portalMode);

/**
 * @brief Behandelt eingehende Client-Anfragen an den Webserver und den DNS-Server.
 * @param portalIsRunning Wenn true, werden DNS-Anfragen für das Captive-Portal verarbeitet.
 */
void handleWebServer(bool portalIsRunning);

#endif // WEBSERVER_MANAGER_HPP