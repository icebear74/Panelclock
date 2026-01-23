#include "WebServerManager.hpp"
#include "WebPages.hpp" // Die HTML-Konstanten einbinden
#include "FileManager.hpp"
#include "WebHandlers.hpp"
#include <WiFi.h>
#include <algorithm> // Für std::sort
#include <ArduinoJson.h>
#include "MemoryLogger.hpp"

// (Die FileManager-Routen werden in FileManager.cpp registriert)

// Globale Variable(en) aus Application (server, dnsServer, deviceConfig, ...)
extern WebServer* server;
extern DNSServer* dnsServer;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;
extern TankerkoenigModule* tankerkoenigModule;
extern bool portalRunning;

/**
 * @brief Richtet alle Routen und Handler für den Webserver ein.
 */
void setupWebServer(bool portalMode) {
    if (!server || !dnsServer) return;
    if (portalMode) {
        dnsServer->start(53, "*", WiFi.softAPIP());
    }

    // Core UI/Config routes (handlers implemented in WebHandlers.cpp)
    server->on("/", HTTP_GET, handleRoot);
    server->on("/config_base", HTTP_GET, handleConfigBase);
    server->on("/config_modules", HTTP_GET, handleConfigModules);
    server->on("/save_base", HTTP_POST, handleSaveBase);
    server->on("/save_modules", HTTP_POST, handleSaveModules);
    server->on("/config_location", HTTP_GET, handleConfigLocation);
    server->on("/save_location", HTTP_POST, handleSaveLocation);
    server->on("/api/tankerkoenig/search", HTTP_GET, handleTankerkoenigSearchLive);
    server->on("/api/themeparks/list", HTTP_GET, handleThemeParksList);
    server->on("/api/sofascore/tournaments", HTTP_GET, handleSofascoreTournamentsList);
    server->on("/api/sofascore/debug_snapshot", HTTP_POST, handleSofascoreDebugSnapshot);
    server->on("/config_hardware", HTTP_GET, handleConfigHardware);
    server->on("/save_hardware", HTTP_POST, handleSaveHardware);
    
    // Countdown routes
    server->on("/countdown", HTTP_GET, handleCountdownPage);
    server->on("/api/countdown/start", HTTP_POST, handleCountdownStart);
    server->on("/api/countdown/stop", HTTP_POST, handleCountdownStop);
    server->on("/api/countdown/pause", HTTP_POST, handleCountdownPause);
    server->on("/api/countdown/reset", HTTP_POST, handleCountdownReset);
    server->on("/api/countdown/status", HTTP_GET, handleCountdownStatus);
    
    // Backup routes
    server->on("/backup", HTTP_GET, handleBackupPage);
    server->on("/api/backup/create", HTTP_POST, handleBackupCreate);
    server->on("/api/backup/download", HTTP_GET, handleBackupDownload);
    server->on("/api/backup/upload", HTTP_POST, []() {
        // Empty lambda - response handled by handleBackupUpload
    }, handleBackupUpload);
    server->on("/api/backup/restore", HTTP_POST, handleBackupRestore);
    server->on("/api/backup/list", HTTP_GET, handleBackupList);
    
    // Firmware update routes
    server->on("/firmware", HTTP_GET, handleFirmwarePage);
    server->on("/update", HTTP_POST, []() {
        // Empty lambda - response handled by handleFirmwareUpload
    }, handleFirmwareUpload);
    
    // Debug routes
    server->on("/debug", HTTP_GET, handleDebugData);
    server->on("/debug/station", HTTP_GET, handleDebugStationHistory);
    server->on("/api/toggle_debug_file", HTTP_POST, handleToggleDebugFile);
    
    // Stream page for remote debugging
    server->on("/stream", HTTP_GET, handleStreamPage);

    // File manager (UI + API) registers its own routes
    setupFileManagerRoutes();

    server->onNotFound(handleNotFound);
    server->begin();
}

void handleWebServer(bool portalIsRunning) {
    if (!server || (portalIsRunning && !dnsServer)) return;
    if (portalIsRunning) {
        dnsServer->processNextRequest();
    }
    server->handleClient();
}