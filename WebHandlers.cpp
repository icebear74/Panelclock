#include "WebHandlers.hpp"
#include "WebServerManager.hpp"
#include "WebPages.hpp"
#include "BackupManager.hpp"
#include "ThemeParkModule.hpp"
#include "SofaScoreLiveModule.hpp"
#include "PanelManager.hpp"
#include "Application.hpp"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>
#include "MemoryLogger.hpp"
#include "MultiLogger.hpp"
#include "PsramUtils.hpp"
#include "Version.hpp"

// PSRAM Allocator for ArduinoJson (same pattern as ThemeParkModule and WeatherModule)
struct SpiRamAllocator : ArduinoJson::Allocator {
    void* allocate(size_t size) override { return ps_malloc(size); }
    void deallocate(void* pointer) override { free(pointer); }
    void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
};

// Externals from main application / WebServerManager.hpp
extern WebServer* server;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;
extern TankerkoenigModule* tankerkoenigModule;
extern BackupManager* backupManager;
extern ThemeParkModule* themeParkModule;
extern FritzboxModule* fritzboxModule;
extern bool portalRunning;

// Forward declarations of global functions (declared in WebServerManager.hpp)
void saveDeviceConfig();
void saveHardwareConfig();
void applyLiveConfig();
void prepareForRestart();

/**
 * @brief Ensures all web server buffers are flushed before ESP32 restart
 * 
 * This function prevents race conditions where the ESP32 restarts before
 * HTTP responses are fully transmitted to the client. It:
 * 1. Calls prepareForRestart() to cleanly shutdown all modules
 * 2. Flushes client output buffers
 * 3. Processes remaining web server operations
 * 4. Waits for client disconnect or timeout
 * 5. Adds final delay for TCP stack completion
 * 
 * Should be called after server->send() and before ESP.restart()
 * 
 * Note: ESP32 WebServer is synchronous, so server->client() returns the
 * client handling the current request context.
 */
void flushBuffersBeforeRestart() {
    // Step 1: Clean shutdown of all modules (connections, files, data)
    prepareForRestart();
    
    if (!server) return;
    
    // Get the client handling the current request
    WiFiClient client = server->client();
    if (!client) return;
    
    // Flush client output buffers
    client.flush();
    
    // Process any remaining web server operations to ensure response is sent
    delay(100);
    server->handleClient();
    
    // Wait for all network buffers to be transmitted
    // Keep handling client until disconnected or timeout
    // Note: Unsigned arithmetic handles millis() wraparound correctly
    unsigned long startWait = millis();
    while (client.connected() && (millis() - startWait) < BUFFER_FLUSH_TIMEOUT_MS) {
        server->handleClient();
        delay(50);  // Check every 50ms to reduce CPU usage while ensuring timely transmission
    }
    
    // Final delay to ensure TCP stack completes transmission
    delay(500);
}

// -----------------------------
// Implementations
// -----------------------------

void handleRoot() {
    if (!server) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_INDEX);
    
    // Add version information
    PsramString version = PANELCLOCK_VERSION;
    replaceAll(content, PsramString("{version}"), version);
    
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleConfigBase() {
    if (!server || !deviceConfig || !hardwareConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_BASE);
    replaceAll(content, "{hostname}", deviceConfig->hostname.c_str());
    replaceAll(content, "{ssid}", deviceConfig->ssid.c_str());
    replaceAll(content, "{password}", deviceConfig->password.c_str());
    replaceAll(content, "{otaPassword}", deviceConfig->otaPassword.c_str());
    
    char pin_buf[8];
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->R1); replaceAll(content, "{R1}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->G1); replaceAll(content, "{G1}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->B1); replaceAll(content, "{B1}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->R2); replaceAll(content, "{R2}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->G2); replaceAll(content, "{G2}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->B2); replaceAll(content, "{B2}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->A); replaceAll(content, "{A}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->B); replaceAll(content, "{B}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->C); replaceAll(content, "{C}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->D); replaceAll(content, "{D}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->E); replaceAll(content, "{E}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->CLK); replaceAll(content, "{CLK}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->LAT); replaceAll(content, "{LAT}", pin_buf);
    snprintf(pin_buf, sizeof(pin_buf), "%d", hardwareConfig->OE); replaceAll(content, "{OE}", pin_buf);

    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveBase() {
    if (!server || !deviceConfig || !hardwareConfig) return;
    deviceConfig->hostname = server->arg("hostname").c_str();
    deviceConfig->ssid = server->arg("ssid").c_str();
    if (server->hasArg("password") && server->arg("password").length() > 0) deviceConfig->password = server->arg("password").c_str();
    if (server->hasArg("otaPassword") && server->arg("otaPassword").length() > 0) deviceConfig->otaPassword = server->arg("otaPassword").c_str();
    saveDeviceConfig();

    hardwareConfig->R1 = server->arg("R1").toInt(); hardwareConfig->G1 = server->arg("G1").toInt(); hardwareConfig->B1 = server->arg("B1").toInt();
    hardwareConfig->R2 = server->arg("R2").toInt(); hardwareConfig->G2 = server->arg("G2").toInt(); hardwareConfig->B2 = server->arg("B2").toInt();
    hardwareConfig->A = server->arg("A").toInt(); hardwareConfig->B = server->arg("B").toInt(); hardwareConfig->C = server->arg("C").toInt();
    hardwareConfig->D = server->arg("D").toInt(); hardwareConfig->E = server->arg("E").toInt();
    hardwareConfig->CLK = server->arg("CLK").toInt(); hardwareConfig->LAT = server->arg("LAT").toInt(); hardwareConfig->OE = server->arg("OE").toInt();
    saveHardwareConfig();

    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Grundkonfiguration gespeichert. Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
    
    // Ensure all buffers are flushed before restarting
    flushBuffersBeforeRestart();
    
    ESP.restart();
}

void handleConfigLocation() {
    if (!server || !deviceConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_LOCATION);
    
    // Add timezone options
    PsramString tz_options_html;
    for (const auto& tz : timezones) {
        tz_options_html += "<option value=\""; tz_options_html += tz.second; tz_options_html += "\"";
        if (deviceConfig->timezone == tz.second) tz_options_html += " selected";
        tz_options_html += ">"; tz_options_html += tz.first; tz_options_html += "</option>";
    }
    replaceAll(content, "{tz_options}", tz_options_html.c_str());
    
    replaceAll(content, "{latitude}", String(deviceConfig->userLatitude, 6).c_str());
    replaceAll(content, "{longitude}", String(deviceConfig->userLongitude, 6).c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveLocation() {
    if (!server || !deviceConfig) return;
    
    // Save timezone
    if (server->hasArg("timezone")) {
        deviceConfig->timezone = server->arg("timezone").c_str();
    }
    
    if (server->hasArg("latitude") && server->hasArg("longitude")) {
        deviceConfig->userLatitude = server->arg("latitude").toFloat();
        deviceConfig->userLongitude = server->arg("longitude").toFloat();
        saveDeviceConfig();
    }
    
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Standort wurde aktualisiert.</p><script>setTimeout(function(){ window.location.href = '/config_location'; }, 2000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleConfigModules() {
    if (!server || !deviceConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_MODULES);

    // Datenmocking-Checkbox
    replaceAll(content, "{dataMockingEnabled_checked}", deviceConfig->dataMockingEnabled ? "checked" : "");

    // Wetter-Platzhalter ersetzen
    replaceAll(content, "{weatherEnabled_checked}", deviceConfig->weatherEnabled ? "checked" : "");
    replaceAll(content, "{weatherApiKey}", deviceConfig->weatherApiKey.c_str());
    replaceAll(content, "{weatherShowCurrent_checked}", deviceConfig->weatherShowCurrent ? "checked" : "");
    replaceAll(content, "{weatherShowHourly_checked}", deviceConfig->weatherShowHourly ? "checked" : "");
    replaceAll(content, "{weatherShowDaily_checked}", deviceConfig->weatherShowDaily ? "checked" : "");
    replaceAll(content, "{weatherAlertsEnabled_checked}", deviceConfig->weatherAlertsEnabled ? "checked" : "");


    replaceAll(content, "{tankerApiKey}", deviceConfig->tankerApiKey.c_str());
    replaceAll(content, "{tankerkoenigStationIds}", deviceConfig->tankerkoenigStationIds.c_str());
    
    char num_buf[20];
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherFetchIntervalMin); replaceAll(content, "{weatherFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherDisplaySec); replaceAll(content, "{weatherDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherDailyForecastDays); replaceAll(content, "{weatherDailyForecastDays}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherHourlyHours); replaceAll(content, "{weatherHourlyHours}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherAlertsDisplaySec); replaceAll(content, "{weatherAlertsDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherAlertsRepeatMin); replaceAll(content, "{weatherAlertsRepeatMin}", num_buf);


    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->stationFetchIntervalMin); replaceAll(content, "{stationFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->stationDisplaySec); replaceAll(content, "{stationDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->movingAverageDays); replaceAll(content, "{movingAverageDays}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->trendAnalysisDays); replaceAll(content, "{trendAnalysisDays}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarFetchIntervalMin); replaceAll(content, "{calendarFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarDisplaySec); replaceAll(content, "{calendarDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarScrollMs); replaceAll(content, "{calendarScrollMs}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsDisplaySec); replaceAll(content, "{dartsDisplaySec}", num_buf);

    replaceAll(content, "{icsUrl}", deviceConfig->icsUrl.c_str());
    replaceAll(content, "{calendarDateColor}", deviceConfig->calendarDateColor.c_str());
    replaceAll(content, "{calendarTextColor}", deviceConfig->calendarTextColor.c_str());
    replaceAll(content, "{dartsOomEnabled_checked}", deviceConfig->dartsOomEnabled ? "checked" : "");
    replaceAll(content, "{dartsProTourEnabled_checked}", deviceConfig->dartsProTourEnabled ? "checked" : "");
    replaceAll(content, "{trackedDartsPlayers}", deviceConfig->trackedDartsPlayers.c_str());
    
    // SofaScore configuration
    replaceAll(content, "{dartsSofascoreEnabled_checked}", deviceConfig->dartsSofascoreEnabled ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsSofascoreFetchIntervalMin); replaceAll(content, "{dartsSofascoreFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsSofascoreDisplaySec); replaceAll(content, "{dartsSofascoreDisplaySec}", num_buf);
    replaceAll(content, "{dartsSofascoreTournamentIds}", deviceConfig->dartsSofascoreTournamentIds.c_str());
    replaceAll(content, "{dartsSofascoreFullscreen_checked}", deviceConfig->dartsSofascoreFullscreen ? "checked" : "");
    replaceAll(content, "{dartsSofascoreInterruptOnLive_checked}", deviceConfig->dartsSofascoreInterruptOnLive ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsSofascorePlayNextMinutes); replaceAll(content, "{dartsSofascorePlayNextMinutes}", num_buf);
    replaceAll(content, "{dartsSofascoreContinuousLive_checked}", deviceConfig->dartsSofascoreContinuousLive ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsSofascoreLiveCheckIntervalSec); replaceAll(content, "{dartsSofascoreLiveCheckIntervalSec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->dartsSofascoreLiveDataFetchIntervalSec); replaceAll(content, "{dartsSofascoreLiveDataFetchIntervalSec}", num_buf);
    
    replaceAll(content, "{fritzboxEnabled_checked}", deviceConfig->fritzboxEnabled ? "checked" : "");
    replaceAll(content, "{fritzboxIp}", deviceConfig->fritzboxIp.c_str());

    // Neue Kalender-Urgent-Parameter
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarFastBlinkHours); replaceAll(content, "{calendarFastBlinkHours}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentThresholdHours); replaceAll(content, "{calendarUrgentThresholdHours}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentDurationSec); replaceAll(content, "{calendarUrgentDurationSec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentRepeatMin); replaceAll(content, "{calendarUrgentRepeatMin}", num_buf);

    // Theme Park configuration
    replaceAll(content, "{themeParkEnabled_checked}", deviceConfig->themeParkEnabled ? "checked" : "");
    replaceAll(content, "{themeParkIds}", deviceConfig->themeParkIds.c_str());
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->themeParkFetchIntervalMin); replaceAll(content, "{themeParkFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->themeParkDisplaySec); replaceAll(content, "{themeParkDisplaySec}", num_buf);

    // Curious Holidays configuration
    replaceAll(content, "{curiousHolidaysEnabled_checked}", deviceConfig->curiousHolidaysEnabled ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->curiousHolidaysDisplaySec); replaceAll(content, "{curiousHolidaysDisplaySec}", num_buf);
    
    // Advent Wreath configuration
    replaceAll(content, "{adventWreathEnabled_checked}", deviceConfig->adventWreathEnabled ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->adventWreathDisplaySec); replaceAll(content, "{adventWreathDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->adventWreathRepeatMin); replaceAll(content, "{adventWreathRepeatMin}", num_buf);
    replaceAll(content, "{adventWreathColorMode0_selected}", deviceConfig->adventWreathColorMode == 0 ? "selected" : "");
    replaceAll(content, "{adventWreathColorMode1_selected}", deviceConfig->adventWreathColorMode == 1 ? "selected" : "");
    replaceAll(content, "{adventWreathColorMode2_selected}", deviceConfig->adventWreathColorMode == 2 ? "selected" : "");
    replaceAll(content, "{adventWreathCustomColors}", deviceConfig->adventWreathCustomColors.c_str());
    replaceAll(content, "{adventWreathInterrupt_checked}", deviceConfig->adventWreathInterrupt ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->adventWreathFlameSpeedMs); replaceAll(content, "{adventWreathFlameSpeedMs}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->adventWreathDaysBefore24); replaceAll(content, "{adventWreathDaysBefore24}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->adventWreathBerryCount); replaceAll(content, "{adventWreathBerryCount}", num_buf);
    replaceAll(content, "{adventWreathOnlyFromFirstAdvent_checked}", deviceConfig->adventWreathOnlyFromFirstAdvent ? "checked" : "");
    replaceAll(content, "{adventWreathBgColor}", deviceConfig->adventWreathBgColor.c_str());
    replaceAll(content, "{christmasTreeBgColor}", deviceConfig->christmasTreeBgColor.c_str());
    replaceAll(content, "{fireplaceBgColor}", deviceConfig->fireplaceBgColor.c_str());
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeDaysBefore24); replaceAll(content, "{christmasTreeDaysBefore24}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeDaysAfter24); replaceAll(content, "{christmasTreeDaysAfter24}", num_buf);
    replaceAll(content, "{christmasTreeEnabled_checked}", deviceConfig->christmasTreeEnabled ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeLightSpeedMs); replaceAll(content, "{christmasTreeLightSpeedMs}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeLightCount); replaceAll(content, "{christmasTreeLightCount}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeOrnamentCount); replaceAll(content, "{christmasTreeOrnamentCount}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->christmasTreeGiftCount); replaceAll(content, "{christmasTreeGiftCount}", num_buf);
    replaceAll(content, "{christmasTreeLightMode0_selected}", deviceConfig->christmasTreeLightMode == 0 ? "selected" : "");
    replaceAll(content, "{christmasTreeLightMode1_selected}", deviceConfig->christmasTreeLightMode == 1 ? "selected" : "");
    replaceAll(content, "{christmasTreeLightColor}", deviceConfig->christmasTreeLightColor.c_str());
    replaceAll(content, "{adventWreathFullscreen_checked}", deviceConfig->adventWreathFullscreen ? "checked" : "");
    replaceAll(content, "{showNewYearCountdown_checked}", deviceConfig->showNewYearCountdown ? "checked" : "");
    replaceAll(content, "{ledBorderEnabled_checked}", deviceConfig->ledBorderEnabled ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->ledBorderSpeedMs); replaceAll(content, "{ledBorderSpeedMs}", num_buf);
    replaceAll(content, "{ledBorderColors}", deviceConfig->ledBorderColors.c_str());
    
    // Kamin-Konfiguration
    replaceAll(content, "{fireplaceEnabled_checked}", deviceConfig->fireplaceEnabled ? "checked" : "");
    replaceAll(content, "{fireplaceNightModeOnly_checked}", deviceConfig->fireplaceNightModeOnly ? "checked" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->fireplaceFlameSpeedMs); replaceAll(content, "{fireplaceFlameSpeedMs}", num_buf);
    replaceAll(content, "{fireplaceFlameColor0_selected}", deviceConfig->fireplaceFlameColor == 0 ? "selected" : "");
    replaceAll(content, "{fireplaceFlameColor1_selected}", deviceConfig->fireplaceFlameColor == 1 ? "selected" : "");
    replaceAll(content, "{fireplaceFlameColor2_selected}", deviceConfig->fireplaceFlameColor == 2 ? "selected" : "");
    replaceAll(content, "{fireplaceFlameColor3_selected}", deviceConfig->fireplaceFlameColor == 3 ? "selected" : "");
    replaceAll(content, "{fireplaceBrickColor}", deviceConfig->fireplaceBrickColor.c_str());
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->fireplaceStockingCount); replaceAll(content, "{fireplaceStockingCount}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->fireplaceCandleCount); replaceAll(content, "{fireplaceCandleCount}", num_buf);
    replaceAll(content, "{fireplaceClockEnabled_checked}", deviceConfig->fireplaceClockEnabled ? "checked" : "");
    
    // Parse custom colors for individual color pickers
    PsramString colors = deviceConfig->adventWreathCustomColors;
    PsramString candleColors[4] = {"#FF0000", "#FFD700", "#008000", "#FFFFFF"};
    int colorIdx = 0;
    size_t pos = 0;
    while (colorIdx < 4 && pos < colors.length()) {
        size_t commaPos = colors.find(',', pos);
        if (commaPos == PsramString::npos) commaPos = colors.length();
        candleColors[colorIdx] = colors.substr(pos, commaPos - pos);
        pos = commaPos + 1;
        colorIdx++;
    }
    replaceAll(content, "{candleColor1}", candleColors[0].c_str());
    replaceAll(content, "{candleColor2}", candleColors[1].c_str());
    replaceAll(content, "{candleColor3}", candleColors[2].c_str());
    replaceAll(content, "{candleColor4}", candleColors[3].c_str());
    
    // Parse LED border colors for individual color pickers
    PsramString ledColors_str = deviceConfig->ledBorderColors;
    PsramString ledColors[4] = {"#FF0000", "#00FF00", "#0000FF", "#FFFF00"};
    colorIdx = 0;
    pos = 0;
    while (colorIdx < 4 && pos < ledColors_str.length()) {
        size_t commaPos = ledColors_str.find(',', pos);
        if (commaPos == PsramString::npos) commaPos = ledColors_str.length();
        ledColors[colorIdx] = ledColors_str.substr(pos, commaPos - pos);
        pos = commaPos + 1;
        colorIdx++;
    }
    replaceAll(content, "{ledColor1}", ledColors[0].c_str());
    replaceAll(content, "{ledColor2}", ledColors[1].c_str());
    replaceAll(content, "{ledColor3}", ledColors[2].c_str());
    replaceAll(content, "{ledColor4}", ledColors[3].c_str());
    
    // Global scrolling configuration
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->globalScrollSpeedMs); replaceAll(content, "{globalScrollSpeedMs}", num_buf);
    replaceAll(content, "{scrollMode0_selected}", deviceConfig->scrollMode == 0 ? "selected" : "");
    replaceAll(content, "{scrollMode1_selected}", deviceConfig->scrollMode == 1 ? "selected" : "");
    replaceAll(content, "{scrollReverse0_selected}", deviceConfig->scrollReverse == 0 ? "selected" : "");
    replaceAll(content, "{scrollReverse1_selected}", deviceConfig->scrollReverse == 1 ? "selected" : "");
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->scrollPauseSec); replaceAll(content, "{scrollPauseSec}", num_buf);

    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveModules() {
    if (!server || !deviceConfig) return;
    
    // Datenmocking
    deviceConfig->dataMockingEnabled = server->hasArg("dataMockingEnabled");

    // Wetter-Werte speichern
    deviceConfig->weatherEnabled = server->hasArg("weatherEnabled");
    deviceConfig->weatherApiKey = server->arg("weatherApiKey").c_str();
    deviceConfig->weatherFetchIntervalMin = server->arg("weatherFetchIntervalMin").toInt();
    deviceConfig->weatherDisplaySec = server->arg("weatherDisplaySec").toInt();
    deviceConfig->weatherShowCurrent = server->hasArg("weatherShowCurrent");
    deviceConfig->weatherShowHourly = server->hasArg("weatherShowHourly");
    deviceConfig->weatherShowDaily = server->hasArg("weatherShowDaily");
    deviceConfig->weatherDailyForecastDays = server->arg("weatherDailyForecastDays").toInt();
    deviceConfig->weatherHourlyHours = server->arg("weatherHourlyHours").toInt();
    deviceConfig->weatherAlertsEnabled = server->hasArg("weatherAlertsEnabled");
    deviceConfig->weatherAlertsDisplaySec = server->arg("weatherAlertsDisplaySec").toInt();
    deviceConfig->weatherAlertsRepeatMin = server->arg("weatherAlertsRepeatMin").toInt();

    // Theme Park configuration
    deviceConfig->themeParkEnabled = server->hasArg("themeParkEnabled");
    deviceConfig->themeParkIds = server->arg("themeParkIds").c_str();
    deviceConfig->themeParkFetchIntervalMin = server->arg("themeParkFetchIntervalMin").toInt();
    deviceConfig->themeParkDisplaySec = server->arg("themeParkDisplaySec").toInt();

    deviceConfig->tankerApiKey = server->arg("tankerApiKey").c_str();
    deviceConfig->stationFetchIntervalMin = server->arg("stationFetchIntervalMin").toInt();
    deviceConfig->stationDisplaySec = server->arg("stationDisplaySec").toInt();
    deviceConfig->movingAverageDays = server->arg("movingAverageDays").toInt();
    deviceConfig->trendAnalysisDays = server->arg("trendAnalysisDays").toInt();
    deviceConfig->tankerkoenigStationIds = server->arg("tankerkoenigStationIds").c_str();
    PsramString ids = deviceConfig->tankerkoenigStationIds;
    if (!ids.empty()) {
        size_t commaPos = ids.find(',');
        deviceConfig->stationId = (commaPos != PsramString::npos) ? ids.substr(0, commaPos) : ids;
    } else { deviceConfig->stationId = ""; }

    const size_t JSON_DOC_SIZE = 32768;
    void* docMem = ps_malloc(JSON_DOC_SIZE);
    if (!docMem) { Log.println("[WebServer] FATAL: PSRAM für JSON-Cleanup fehlgeschlagen!"); }
    else {
        JsonDocument* oldCacheDoc = new (docMem) JsonDocument();
        if (LittleFS.exists("/station_cache.json")) {
            File oldFile = LittleFS.open("/station_cache.json", "r");
            if (oldFile) { deserializeJson(*oldCacheDoc, oldFile); oldFile.close(); }
        }

        if ((*oldCacheDoc)["ok"] == true) {
            void* newDocMem = ps_malloc(JSON_DOC_SIZE);
            if(!newDocMem) { Log.println("[WebServer] FATAL: PSRAM für neues Cache-Dokument fehlgeschlagen!"); }
            else {
                JsonDocument* newCacheDoc = new (newDocMem) JsonDocument();
                (*newCacheDoc)["ok"] = true;
                (*newCacheDoc)["license"] = (*oldCacheDoc)["license"];
                (*newCacheDoc)["data-version"] = (*oldCacheDoc)["data-version"];
                (*newCacheDoc)["status"] = "ok";
                JsonArray newStations = (*newCacheDoc)["stations"].to<JsonArray>();

                PsramVector<PsramString> final_ids;
                PsramString temp_ids = deviceConfig->tankerkoenigStationIds;
                
                // FIX: Create a mutable copy for strtok_r to safely modify
                char* temp_buffer = strdup(temp_ids.c_str());
                if (temp_buffer) {
                    char* strtok_ctx;
                    char* id_token = strtok_r(temp_buffer, ",", &strtok_ctx);
                    while(id_token != nullptr) { 
                        final_ids.push_back(id_token); 
                        id_token = strtok_r(nullptr, ",", &strtok_ctx); 
                    }
                    free(temp_buffer);
                }

                JsonArray oldStations = (*oldCacheDoc)["stations"];
                for (JsonObject oldStation : oldStations) {
                    const char* oldId = oldStation["id"];
                    bool needed = false;
                    for (const auto& final_id : final_ids) {
                        if (strcmp(oldId, final_id.c_str()) == 0) { needed = true; break; }
                    }
                    if (needed) { newStations.add(oldStation); }
                }
                File newFile = LittleFS.open("/station_cache.json", "w");
                if (newFile) { serializeJson(*newCacheDoc, newFile); newFile.close(); }
                
                newCacheDoc->~JsonDocument();
                free(newDocMem);
            }
        }
        oldCacheDoc->~JsonDocument();
        free(docMem);
    }
    
    deviceConfig->icsUrl = server->arg("icsUrl").c_str();
    deviceConfig->calendarFetchIntervalMin = server->arg("calendarFetchIntervalMin").toInt();
    deviceConfig->calendarDisplaySec = server->arg("calendarDisplaySec").toInt();
    deviceConfig->calendarDateColor = server->arg("calendarDateColor").c_str();
    deviceConfig->calendarTextColor = server->arg("calendarTextColor").c_str();

    // Neue Kalender-Urgent-Parameter aus Webinterface lesen
    if (server->hasArg("calendarFastBlinkHours")) deviceConfig->calendarFastBlinkHours = server->arg("calendarFastBlinkHours").toInt();
    if (server->hasArg("calendarUrgentThresholdHours")) deviceConfig->calendarUrgentThresholdHours = server->arg("calendarUrgentThresholdHours").toInt();
    if (server->hasArg("calendarUrgentDurationSec")) deviceConfig->calendarUrgentDurationSec = server->arg("calendarUrgentDurationSec").toInt();
    if (server->hasArg("calendarUrgentRepeatMin")) deviceConfig->calendarUrgentRepeatMin = server->arg("calendarUrgentRepeatMin").toInt();

    // Curious Holidays configuration
    deviceConfig->curiousHolidaysEnabled = server->hasArg("curiousHolidaysEnabled");
    if (server->hasArg("curiousHolidaysDisplaySec")) deviceConfig->curiousHolidaysDisplaySec = server->arg("curiousHolidaysDisplaySec").toInt();
    
    // Advent Wreath configuration
    deviceConfig->adventWreathEnabled = server->hasArg("adventWreathEnabled");
    if (server->hasArg("adventWreathDisplaySec")) deviceConfig->adventWreathDisplaySec = server->arg("adventWreathDisplaySec").toInt();
    if (server->hasArg("adventWreathRepeatMin")) deviceConfig->adventWreathRepeatMin = server->arg("adventWreathRepeatMin").toInt();
    if (server->hasArg("adventWreathColorMode")) deviceConfig->adventWreathColorMode = server->arg("adventWreathColorMode").toInt();
    if (server->hasArg("adventWreathCustomColors")) deviceConfig->adventWreathCustomColors = server->arg("adventWreathCustomColors").c_str();
    deviceConfig->adventWreathInterrupt = server->hasArg("adventWreathInterrupt");
    if (server->hasArg("adventWreathFlameSpeedMs")) deviceConfig->adventWreathFlameSpeedMs = server->arg("adventWreathFlameSpeedMs").toInt();
    if (server->hasArg("adventWreathDaysBefore24")) deviceConfig->adventWreathDaysBefore24 = server->arg("adventWreathDaysBefore24").toInt();
    if (server->hasArg("adventWreathBerryCount")) deviceConfig->adventWreathBerryCount = server->arg("adventWreathBerryCount").toInt();
    deviceConfig->adventWreathOnlyFromFirstAdvent = server->hasArg("adventWreathOnlyFromFirstAdvent");
    if (server->hasArg("adventWreathBgColor")) deviceConfig->adventWreathBgColor = server->arg("adventWreathBgColor").c_str();
    if (server->hasArg("christmasTreeBgColor")) deviceConfig->christmasTreeBgColor = server->arg("christmasTreeBgColor").c_str();
    if (server->hasArg("fireplaceBgColor")) deviceConfig->fireplaceBgColor = server->arg("fireplaceBgColor").c_str();
    if (server->hasArg("christmasTreeDaysBefore24")) deviceConfig->christmasTreeDaysBefore24 = server->arg("christmasTreeDaysBefore24").toInt();
    if (server->hasArg("christmasTreeDaysAfter24")) deviceConfig->christmasTreeDaysAfter24 = server->arg("christmasTreeDaysAfter24").toInt();
    deviceConfig->christmasTreeEnabled = server->hasArg("christmasTreeEnabled");
    if (server->hasArg("christmasTreeLightSpeedMs")) deviceConfig->christmasTreeLightSpeedMs = server->arg("christmasTreeLightSpeedMs").toInt();
    if (server->hasArg("christmasTreeLightCount")) deviceConfig->christmasTreeLightCount = server->arg("christmasTreeLightCount").toInt();
    if (server->hasArg("christmasTreeOrnamentCount")) deviceConfig->christmasTreeOrnamentCount = server->arg("christmasTreeOrnamentCount").toInt();
    if (server->hasArg("christmasTreeGiftCount")) deviceConfig->christmasTreeGiftCount = server->arg("christmasTreeGiftCount").toInt();
    if (server->hasArg("christmasTreeLightMode")) deviceConfig->christmasTreeLightMode = server->arg("christmasTreeLightMode").toInt();
    if (server->hasArg("christmasTreeLightColor")) deviceConfig->christmasTreeLightColor = server->arg("christmasTreeLightColor").c_str();
    deviceConfig->adventWreathFullscreen = server->hasArg("adventWreathFullscreen");
    deviceConfig->showNewYearCountdown = server->hasArg("showNewYearCountdown");
    deviceConfig->ledBorderEnabled = server->hasArg("ledBorderEnabled");
    if (server->hasArg("ledBorderSpeedMs")) deviceConfig->ledBorderSpeedMs = server->arg("ledBorderSpeedMs").toInt();
    if (server->hasArg("ledBorderColors")) deviceConfig->ledBorderColors = server->arg("ledBorderColors").c_str();
    
    // Kamin-Konfiguration
    deviceConfig->fireplaceEnabled = server->hasArg("fireplaceEnabled");
    deviceConfig->fireplaceNightModeOnly = server->hasArg("fireplaceNightModeOnly");
    if (server->hasArg("fireplaceFlameSpeedMs")) deviceConfig->fireplaceFlameSpeedMs = server->arg("fireplaceFlameSpeedMs").toInt();
    if (server->hasArg("fireplaceFlameColor")) deviceConfig->fireplaceFlameColor = server->arg("fireplaceFlameColor").toInt();
    if (server->hasArg("fireplaceBrickColor")) deviceConfig->fireplaceBrickColor = server->arg("fireplaceBrickColor").c_str();
    if (server->hasArg("fireplaceStockingCount")) deviceConfig->fireplaceStockingCount = server->arg("fireplaceStockingCount").toInt();
    if (server->hasArg("fireplaceCandleCount")) deviceConfig->fireplaceCandleCount = server->arg("fireplaceCandleCount").toInt();
    deviceConfig->fireplaceClockEnabled = server->hasArg("fireplaceClockEnabled");
    
    // Global scrolling configuration
    if (server->hasArg("globalScrollSpeedMs")) deviceConfig->globalScrollSpeedMs = server->arg("globalScrollSpeedMs").toInt();
    if (server->hasArg("scrollMode")) deviceConfig->scrollMode = server->arg("scrollMode").toInt();
    if (server->hasArg("scrollPauseSec")) deviceConfig->scrollPauseSec = server->arg("scrollPauseSec").toInt();
    if (server->hasArg("scrollReverse")) deviceConfig->scrollReverse = server->arg("scrollReverse").toInt();

    deviceConfig->dartsOomEnabled = server->hasArg("dartsOomEnabled");
    deviceConfig->dartsProTourEnabled = server->hasArg("dartsProTourEnabled");
    deviceConfig->dartsDisplaySec = server->arg("dartsDisplaySec").toInt();
    deviceConfig->trackedDartsPlayers = server->arg("trackedDartsPlayers").c_str();
    
    // SofaScore configuration
    deviceConfig->dartsSofascoreEnabled = server->hasArg("dartsSofascoreEnabled");
    deviceConfig->dartsSofascoreFetchIntervalMin = server->arg("dartsSofascoreFetchIntervalMin").toInt();
    deviceConfig->dartsSofascoreDisplaySec = server->arg("dartsSofascoreDisplaySec").toInt();
    deviceConfig->dartsSofascoreTournamentIds = server->arg("dartsSofascoreTournamentIds").c_str();
    deviceConfig->dartsSofascoreFullscreen = server->hasArg("dartsSofascoreFullscreen");
    deviceConfig->dartsSofascoreInterruptOnLive = server->hasArg("dartsSofascoreInterruptOnLive");
    deviceConfig->dartsSofascorePlayNextMinutes = server->arg("dartsSofascorePlayNextMinutes").toInt();
    deviceConfig->dartsSofascoreContinuousLive = server->hasArg("dartsSofascoreContinuousLive");
    deviceConfig->dartsSofascoreLiveCheckIntervalSec = server->arg("dartsSofascoreLiveCheckIntervalSec").toInt();
    deviceConfig->dartsSofascoreLiveDataFetchIntervalSec = server->arg("dartsSofascoreLiveDataFetchIntervalSec").toInt();
    
    if (sofascoreMod) {
        sofascoreMod->setConfig(
            deviceConfig->dartsSofascoreEnabled,
            deviceConfig->dartsSofascoreFetchIntervalMin,
            deviceConfig->dartsSofascoreDisplaySec,
            deviceConfig->dartsSofascoreTournamentIds,
            deviceConfig->dartsSofascoreFullscreen,
            deviceConfig->dartsSofascoreInterruptOnLive,
            deviceConfig->dartsSofascorePlayNextMinutes,
            deviceConfig->dartsSofascoreContinuousLive,
            deviceConfig->dartsSofascoreLiveCheckIntervalSec,
            deviceConfig->dartsSofascoreLiveDataFetchIntervalSec
        );
    }
    
    deviceConfig->fritzboxEnabled = server->hasArg("fritzboxEnabled");
    if (server->hasArg("fritzboxIp") && server->arg("fritzboxIp").length() > 0) {
        deviceConfig->fritzboxIp = server->arg("fritzboxIp").c_str();
    } else {
        if(deviceConfig->fritzboxEnabled) {
            // Use stack buffer to avoid heap allocation
            char gwIP[16];
            snprintf(gwIP, sizeof(gwIP), "%d.%d.%d.%d", 
                WiFi.gatewayIP()[0], WiFi.gatewayIP()[1], 
                WiFi.gatewayIP()[2], WiFi.gatewayIP()[3]);
            deviceConfig->fritzboxIp = gwIP; 
        }
        else { deviceConfig->fritzboxIp = ""; }
    }

    saveDeviceConfig();
    applyLiveConfig();

    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Modul-Konfiguration live &uuml;bernommen!</p><script>setTimeout(function(){ window.location.href = '/config_modules'; }, 2000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleConfigHardware() {
    if (!server || !deviceConfig || !hardwareConfig || !mwaveSensorModule || !timeConverter) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_HARDWARE);

    replaceAll(content, "{mwaveSensorEnabled_checked}", deviceConfig->mwaveSensorEnabled ? "checked" : "");
    
    char num_buf[20];
    snprintf(num_buf, sizeof(num_buf), "%d", hardwareConfig->mwaveRxPin); replaceAll(content, "{mwaveRxPin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", hardwareConfig->mwaveTxPin); replaceAll(content, "{mwaveTxPin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", hardwareConfig->displayRelayPin); replaceAll(content, "{displayRelayPin}", num_buf);
    
    snprintf(num_buf, sizeof(num_buf), "%.1f", deviceConfig->mwaveOnCheckPercentage); replaceAll(content, "{mwaveOnCheckPercentage}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->mwaveOnCheckDuration); replaceAll(content, "{mwaveOnCheckDuration}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%.1f", deviceConfig->mwaveOffCheckOnPercent); replaceAll(content, "{mwaveOffCheckOnPercent}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->mwaveOffCheckDuration); replaceAll(content, "{mwaveOffCheckDuration}", num_buf);


    PsramString table_html = "<table><tr><th>Zeitpunkt</th><th>Zustand</th></tr>";
    const auto& log = mwaveSensorModule->getDisplayStateLog();
    if (log.empty()) {
        table_html += "<tr><td colspan='2'>Noch keine Eintr&auml;ge vorhanden.</td></tr>";
    } else {
        for (int i = log.size() - 1; i >= 0; --i) {
            const auto& entry = log[i];
            struct tm timeinfo_log;
            time_t localTime = timeConverter->toLocal(entry.timestamp);
            localtime_r(&localTime, &timeinfo_log);
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &timeinfo_log);
            
            table_html += "<tr><td>"; table_html += time_str; table_html += "</td><td>";
            table_html += entry.state ? "<span style='color:lightgreen;'>AN</span>" : "<span style='color:red;'>AUS</span>";
            table_html += "</td></tr>";
        }
    }
    table_html += "</table>";
    replaceAll(content, "{debug_log_table}", table_html.c_str());
    
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveHardware() {
    if (!server || !deviceConfig || !hardwareConfig) return;
    bool restartNeeded = false;
    
    if (server->arg("mwaveRxPin").toInt() != hardwareConfig->mwaveRxPin) restartNeeded = true;
    if (server->arg("mwaveTxPin").toInt() != hardwareConfig->mwaveTxPin) restartNeeded = true;
    if (server->arg("displayRelayPin").toInt() != hardwareConfig->displayRelayPin) restartNeeded = true;
    if (server->hasArg("mwaveSensorEnabled") != deviceConfig->mwaveSensorEnabled) restartNeeded = true;

    deviceConfig->mwaveSensorEnabled = server->hasArg("mwaveSensorEnabled");
    deviceConfig->mwaveOnCheckPercentage = server->arg("mwaveOnCheckPercentage").toFloat();
    deviceConfig->mwaveOnCheckDuration = server->arg("mwaveOnCheckDuration").toInt();
    deviceConfig->mwaveOffCheckOnPercent = server->arg("mwaveOffCheckOnPercent").toFloat();
    deviceConfig->mwaveOffCheckDuration = server->arg("mwaveOffCheckDuration").toInt();
    saveDeviceConfig();

    hardwareConfig->mwaveRxPin = server->arg("mwaveRxPin").toInt();
    hardwareConfig->mwaveTxPin = server->arg("mwaveTxPin").toInt();
    hardwareConfig->displayRelayPin = server->arg("displayRelayPin").toInt();
    saveHardwareConfig();

    if (restartNeeded) {
        PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
        page += "<h1>Gespeichert!</h1><p>Hardware-Konfiguration gespeichert. Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
        page += (const char*)FPSTR(HTML_PAGE_FOOTER);
        server->send(200, "text/html", page.c_str());
        
        // Ensure all buffers are flushed before restarting
        flushBuffersBeforeRestart();
        
        ESP.restart();
    } else {
        applyLiveConfig();
        PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
        page += "<h1>Gespeichert!</h1><p>Schwellenwerte live &uuml;bernommen!</p><script>setTimeout(function(){ window.location.href = '/config_hardware'; }, 2000);</script>";
        page += (const char*)FPSTR(HTML_PAGE_FOOTER);
        server->send(200, "text/html", page.c_str());
    }
}

void handleDebugStationHistory() {
    if (!server || !tankerkoenigModule) return;
    if (!server->hasArg("id")) {
        server->send(400, "text/plain", "Fehler: Stations-ID fehlt.");
        return;
    }
    PsramString stationId = server->arg("id").c_str();

    StationData stationInfo;
    PsramVector<StationData> stationCache = tankerkoenigModule->getStationCache();
    for (const auto& station : stationCache) {
        if (station.id == stationId) {
            stationInfo = station;
            break;
        }
    }

    StationPriceHistory history = tankerkoenigModule->getStationPriceHistory(stationId);

    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_DEBUG_STATION_HISTORY);

    if (stationInfo.id.empty()) {
        replaceAll(content, "{station_brand}", "Unbekannte Tankstelle");
        replaceAll(content, "{station_name}", "");
        replaceAll(content, "{station_address}", "Keine Stammdaten gefunden.");
        replaceAll(content, "{station_id}", stationId.c_str());
    } else {
        replaceAll(content, "{station_brand}", stationInfo.brand.c_str());
        replaceAll(content, "{station_name}", stationInfo.name.c_str());
        PsramString address = stationInfo.street;
        address += " ";
        address += stationInfo.houseNumber;
        address += ", ";
        address += stationInfo.postCode;
        address += " ";
        address += stationInfo.place;
        replaceAll(content, "{station_address}", address.c_str());
        replaceAll(content, "{station_id}", stationInfo.id.c_str());
    }

    PsramString table_rows = "";
    if (history.dailyStats.empty()) {
        table_rows = "<tr><td colspan='7'>Keine historischen Daten f&uuml;r diese Tankstelle gefunden.</td></tr>";
    } else {
        std::sort(history.dailyStats.begin(), history.dailyStats.end(), [](const DailyPriceStats& a, const DailyPriceStats& b) {
            return a.date > b.date;
        });

        char buffer[16];
        for (const auto& day : history.dailyStats) {
            table_rows += "<tr><td>";
            table_rows += day.date;
            table_rows += "</td>";

            snprintf(buffer, sizeof(buffer), "%.3f", day.e5_low);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td>";
            
            snprintf(buffer, sizeof(buffer), "%.3f", day.e5_high);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td>";

            snprintf(buffer, sizeof(buffer), "%.3f", day.e10_low);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td>";

            snprintf(buffer, sizeof(buffer), "%.3f", day.e10_high);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td>";

            snprintf(buffer, sizeof(buffer), "%.3f", day.diesel_low);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td>";

            snprintf(buffer, sizeof(buffer), "%.3f", day.diesel_high);
            table_rows += "<td>";
            table_rows += buffer;
            table_rows += "</td></tr>";
        }
    }
    
    replaceAll(content, "{history_table}", table_rows.c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleDebugData() {
    if (!server) return;
    
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_DEBUG_DATA);
    
    PsramString table_rows = "";
    if (tankerkoenigModule) {
        PsramVector<StationData> stationCache = tankerkoenigModule->getStationCache();
        if (stationCache.empty()) {
            table_rows = "<tr><td colspan='4'>Keine Tankstellen-Daten im Cache gefunden.</td></tr>";
        } else {
            for (const auto& station : stationCache) {
                table_rows += "<tr><td>";
                table_rows += station.id;
                table_rows += "</td><td>";
                table_rows += station.brand;
                table_rows += "</td><td><a href=\"/debug/station?id=";
                table_rows += station.id;
                table_rows += "\">";
                table_rows += station.name;
                table_rows += "</a></td><td>";
                table_rows += station.street;
                table_rows += " ";
                table_rows += station.houseNumber;
                table_rows += ", ";
                table_rows += station.postCode;
                table_rows += " ";
                table_rows += station.place;
                table_rows += "</td></tr>";
            }
        }
    } else {
        table_rows = "<tr><td colspan='4' style='color:red;'>Fehler: TankerkoenigModule nicht initialisiert.</td></tr>";
    }

    replaceAll(content, "{station_cache_table}", table_rows.c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);

    server->send(200, "text/html", page.c_str());
}

void handleTankerkoenigSearchLive() {
    if (!server || !deviceConfig || !webClient) { server->send(500, "application/json", "{\"ok\":false, \"message\":\"Server, Config oder WebClient nicht initialisiert\"}"); return; }
    if (deviceConfig->userLatitude == 0.0 || deviceConfig->userLongitude == 0.0) { server->send(400, "application/json", "{\"ok\":false, \"message\":\"Kein Standort konfiguriert. Bitte zuerst 'Mein Standort' festlegen.\"}"); return; }
    if (deviceConfig->tankerApiKey.empty()) { server->send(400, "application/json", "{\"ok\":false, \"message\":\"Kein Tankerkönig API-Key konfiguriert.\"}"); return; }

    // Store String values to avoid dangling pointers
    String radiusStr = server->arg("radius");
    String sortStr = server->arg("sort");
    PsramString url;
    url += "https://creativecommons.tankerkoenig.de/json/list.php?lat=";
    char coord_buf[20];
    snprintf(coord_buf, sizeof(coord_buf), "%.6f", deviceConfig->userLatitude);
    url += coord_buf;
    url += "&lng=";
    snprintf(coord_buf, sizeof(coord_buf), "%.6f", deviceConfig->userLongitude);
    url += coord_buf;
    url += "&rad="; url += radiusStr.c_str(); url += "&sort="; url += sortStr.c_str(); url += "&type=all&apikey="; url += deviceConfig->tankerApiKey.c_str();

    struct Result { int httpCode; PsramString payload; } result;
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();

    webClient->getRequest(url, [&](int httpCode, const char* payload, size_t len) {
        result.httpCode = httpCode;
        if (payload) { result.payload.assign(payload, len); }
        xSemaphoreGive(sem);
    });

    if (xSemaphoreTake(sem, pdMS_TO_TICKS(20000)) == pdTRUE) {
        if (result.httpCode == 200) {
            const size_t JSON_DOC_SIZE = 32768; 
            void* docMem = ps_malloc(JSON_DOC_SIZE);
            if (!docMem) { Log.println("[WebServer] FATAL: PSRAM für JSON-Merge fehlgeschlagen!"); server->send(500, "application/json", "{\"ok\":false,\"message\":\"Interner Speicherfehler\"}"); vSemaphoreDelete(sem); return; }
            
            JsonDocument* currentCacheDoc = new (docMem) JsonDocument();
            
            if (LittleFS.exists("/station_cache.json")) {
                File cacheFile = LittleFS.open("/station_cache.json", "r");
                if (cacheFile) { deserializeJson(*currentCacheDoc, cacheFile); cacheFile.close(); }
            }
            if (!currentCacheDoc->is<JsonObject>()) { currentCacheDoc->to<JsonObject>(); }

            // Use PSRAM allocator for large JSON document
            struct SpiRamAllocator : ArduinoJson::Allocator {
                void* allocate(size_t size) override { return ps_malloc(size); }
                void deallocate(void* pointer) override { free(pointer); }
                void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
            };
            
            SpiRamAllocator allocator;
            JsonDocument newResultsDoc(&allocator);
            deserializeJson(newResultsDoc, result.payload);

            if (newResultsDoc["ok"] == true) {
                JsonArray newStations = newResultsDoc["stations"].as<JsonArray>();
                JsonArray cachedStations = (*currentCacheDoc)["stations"].as<JsonArray>();
                if (cachedStations.isNull()) cachedStations = (*currentCacheDoc)["stations"].to<JsonArray>();

                for (JsonObject newStation : newStations) {
                    const char* newId = newStation["id"];
                    bool exists = false;
                    for (JsonObject cachedStation : cachedStations) {
                        if (strcmp(cachedStation["id"], newId) == 0) { exists = true; break; }
                    }
                    if (!exists) { cachedStations.add(newStation); }
                }
                (*currentCacheDoc)["ok"] = true;

                File cacheFile = LittleFS.open("/station_cache.json", "w");
                if (cacheFile) { serializeJson(*currentCacheDoc, cacheFile); cacheFile.close(); }
            }
            
            currentCacheDoc->~JsonDocument();
            free(docMem);

            server->send(200, "application/json", result.payload.c_str());
        } else {
            PsramString escaped_payload;
            escaped_payload = result.payload;
            PsramString error_msg;
            error_msg = "{\"ok\":false, \"message\":\"API Fehler: HTTP ";
            char code_buf[5];
            snprintf(code_buf, sizeof(code_buf), "%d", result.httpCode);
            error_msg += code_buf;
            error_msg += "\", \"details\":\"";
            error_msg += escaped_payload;
            error_msg += "\"}";
            server->send(result.httpCode > 0 ? result.httpCode : 500, "application/json", error_msg.c_str());
        }
    } else { server->send(504, "application/json", "{\"ok\":false, \"message\":\"Timeout bei der Anfrage an den WebClient-Task.\"}"); }
    vSemaphoreDelete(sem);
}

void handleNotFound() {
    if (!server) return;
    if (portalRunning && !server->hostHeader().equals(WiFi.softAPIP().toString())) {
        server->sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        server->send(302, "text/plain", "");
        return;
    }
    server->send(404, "text/plain", "404: Not Found");
}
void handleStreamPage() {
    if (!server) return;
    String page = FPSTR(HTML_PAGE_HEADER);
    String htmlContent = FPSTR(HTML_STREAM_PAGE);
    
    // Replace {debugFileChecked} placeholder
    const char* checked = deviceConfig->debugFileEnabled ? "checked" : "";
    htmlContent.replace("{debugFileChecked}", checked);
    
    page += htmlContent;
    page += FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page);
}

void handleToggleDebugFile() {
    if (!server) return;
    
    if (!server->hasArg("plain")) {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"No body\"}");
        return;
    }
    
    String body = server->arg("plain");
    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    bool enabled = doc["enabled"] | false;
    deviceConfig->debugFileEnabled = enabled;
    // Don't save to config for security - only apply live
    // saveDeviceConfig();
    
    // Apply immediately via the existing applyLiveConfig function
    applyLiveConfig();
    
    server->send(200, "application/json", "{\"success\":true}");
}

// =============================================================================
// Backup & Restore Handlers
// =============================================================================

void handleBackupPage() {
    if (!server) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += (const char*)FPSTR(HTML_BACKUP_PAGE);
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleBackupCreate() {
    if (!server || !backupManager) {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"BackupManager not available\"}");
        return;
    }
    
    Log.println("[WebHandler] Creating manual backup...");
    bool success = backupManager->createBackup(true); // manual backup
    
    if (success) {
        // Get the most recent backup
        PsramVector<BackupInfo> backups = backupManager->listBackups();
        if (backups.size() > 0) {
            PsramString response = "{\"success\":true,\"filename\":\"";
            response += backups[0].filename;
            response += "\"}";
            server->send(200, "application/json", response.c_str());
        } else {
            server->send(200, "application/json", "{\"success\":true,\"filename\":\"unknown\"}");
        }
    } else {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Failed to create backup\"}");
    }
}

void handleBackupDownload() {
    if (!server || !backupManager) {
        server->send(500, "text/plain", "BackupManager not available");
        return;
    }
    
    if (!server->hasArg("file")) {
        server->send(400, "text/plain", "Missing 'file' parameter");
        return;
    }
    
    PsramString filename = server->arg("file").c_str();
    PsramString fullPath = backupManager->getBackupPath(filename);
    
    if (!LittleFS.exists(fullPath.c_str())) {
        server->send(404, "text/plain", "Backup file not found");
        return;
    }
    
    File file = LittleFS.open(fullPath.c_str(), "r");
    if (!file) {
        server->send(500, "text/plain", "Could not open backup file");
        return;
    }
    
    Log.printf("[WebHandler] Downloading backup: %s\n", filename.c_str());
    
    server->sendHeader("Content-Disposition", "attachment; filename=\"" + String(filename.c_str()) + "\"");
    server->streamFile(file, "application/json");
    file.close();
}

void handleBackupUpload() {
    if (!server || !backupManager) {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"BackupManager not available\"}");
        return;
    }
    
    HTTPUpload& upload = server->upload();
    static File uploadFile;
    static PsramString uploadFilename;
    
    if (upload.status == UPLOAD_FILE_START) {
        // Generate a temporary filename for the uploaded backup
        char tempName[64];
        snprintf(tempName, sizeof(tempName), "uploaded_backup_%lu.json", millis());
        uploadFilename = tempName;
        
        PsramString fullPath = backupManager->getBackupPath(uploadFilename);
        Log.printf("[WebHandler] Starting backup upload: %s\n", fullPath.c_str());
        
        uploadFile = LittleFS.open(fullPath.c_str(), "w");
        if (!uploadFile) {
            Log.println("[WebHandler] ERROR: Could not create upload file");
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) {
            uploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) {
            uploadFile.close();
            Log.printf("[WebHandler] Upload complete: %u bytes\n", upload.totalSize);
            
            // Send success response with filename
            PsramString response = "{\"success\":true,\"filename\":\"";
            response += uploadFilename;
            response += "\"}";
            server->send(200, "application/json", response.c_str());
        } else {
            server->send(500, "application/json", "{\"success\":false,\"error\":\"Upload failed\"}");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile) {
            uploadFile.close();
        }
        // Delete the potentially corrupted file
        if (uploadFilename.length() > 0 && backupManager) {
            PsramString fullPath = backupManager->getBackupPath(uploadFilename);
            if (LittleFS.exists(fullPath.c_str())) {
                LittleFS.remove(fullPath.c_str());
                Log.printf("[WebHandler] Aborted upload file deleted: %s\n", fullPath.c_str());
            }
        }
        Log.println("[WebHandler] Upload aborted");
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Upload aborted\"}");
    }
}

void handleBackupRestore() {
    if (!server || !backupManager) {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"BackupManager not available\"}");
        return;
    }
    
    // Parse JSON body
    String body = server->arg("plain");
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }
    
    if (!doc.containsKey("filename")) {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing filename\"}");
        return;
    }
    
    PsramString filename = doc["filename"].as<const char*>();
    Log.printf("[WebHandler] Restoring from backup: %s\n", filename.c_str());
    
    bool success = backupManager->restoreFromBackup(filename);
    
    if (success) {
        server->send(200, "application/json", "{\"success\":true,\"message\":\"Restore successful, rebooting...\"}");
        
        // Ensure all buffers are flushed before restarting
        flushBuffersBeforeRestart();
        
        ESP.restart();
    } else {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Restore failed\"}");
    }
}

void handleBackupList() {
    if (!server || !backupManager) {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"BackupManager not available\"}");
        return;
    }
    
    PsramVector<BackupInfo> backups = backupManager->listBackups();
    
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    JsonArray array = doc["backups"].to<JsonArray>();
    
    for (const auto& backup : backups) {
        JsonObject obj = array.add<JsonObject>();
        obj["filename"] = backup.filename.c_str();
        obj["timestamp"] = backup.timestamp.c_str();
        obj["size"] = backup.size;
    }
    
    String response;
    serializeJson(doc, response);
    server->send(200, "application/json", response.c_str());
}

// =============================================================================
// Theme Park Handlers
// =============================================================================

void handleThemeParksList() {
    if (!server || !webClient) {
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"Server or WebClient not initialized\"}");
        return;
    }
    
    Log.println("[ThemePark] handleThemeParksList called - fetching parks from API");
    
    // Fetch parks list on-demand using webClient with custom headers
    PsramString url = "https://api.wartezeiten.app/v1/parks";
    PsramString headers = "accept: application/json\nlanguage: de";
    
    struct Result { int httpCode; PsramString payload; } result;
    result.httpCode = 0;
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    
    webClient->getRequest(url, headers, [&](int httpCode, const char* payload, size_t len) {
        Log.printf("[ThemePark] Callback received - HTTP %d, payload size: %d\n", httpCode, len);
        result.httpCode = httpCode;
        if (payload && len > 0) { 
            result.payload.assign(payload, len); 
            Log.printf("[ThemePark] Payload first 100 chars: %.100s\n", payload);
        }
        xSemaphoreGive(sem);
    });
    
    Log.println("[ThemePark] Waiting for response (20s timeout)...");
    
    if (xSemaphoreTake(sem, pdMS_TO_TICKS(20000)) == pdTRUE) {
        Log.printf("[ThemePark] Response received, HTTP code: %d\n", result.httpCode);
        
        if (result.httpCode == 200) {
            // Parse the response and format it for the web interface using PSRAM allocator
            struct SpiRamAllocator : ArduinoJson::Allocator {
                void* allocate(size_t size) override { return ps_malloc(size); }
                void deallocate(void* pointer) override { free(pointer); }
                void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
            };
            
            SpiRamAllocator allocator1;
            JsonDocument inputDoc(&allocator1);
            DeserializationError error = deserializeJson(inputDoc, result.payload.c_str());
            
            if (error) {
                Log.printf("[ThemePark] JSON parse error: %s\n", error.c_str());
                server->send(500, "application/json", "{\"ok\":false, \"message\":\"Failed to parse API response\"}");
                vSemaphoreDelete(sem);
                return;
            }
            
            Log.println("[ThemePark] JSON parsed successfully");
            
            // Build response
            SpiRamAllocator allocator2;
            JsonDocument responseDoc(&allocator2);
            responseDoc["ok"] = true;
            JsonArray parksArray = responseDoc.createNestedArray("parks");
            
            // The API returns an array of parks
            if (inputDoc.is<JsonArray>()) {
                JsonArray apiParks = inputDoc.as<JsonArray>();
                Log.printf("[ThemePark] Found %d parks in API response\n", apiParks.size());
                
                // Also parse and save to cache via the module
                if (themeParkModule) {
                    themeParkModule->parseAvailableParks(result.payload.c_str(), result.payload.length());
                }
                
                for (JsonObject park : apiParks) {
                    const char* id = park["id"] | "";
                    const char* name = park["name"] | "";
                    const char* country = park["land"] | "";  // API uses "land" not "country"
                    
                    if (id && name && strlen(id) > 0 && strlen(name) > 0) {
                        JsonObject parkObj = parksArray.createNestedObject();
                        parkObj["id"] = id;
                        parkObj["name"] = name;
                        parkObj["country"] = country;
                        Log.printf("[ThemePark] Added park: %s (%s) - %s\n", name, id, country);
                    }
                }
            } else {
                Log.println("[ThemePark] ERROR: API response is not a JSON array");
            }
            
            String response;
            serializeJson(responseDoc, response);
            Log.printf("[ThemePark] Sending response with %d parks\n", parksArray.size());
            server->send(200, "application/json", response);
        } else {
            Log.printf("[ThemePark] HTTP error: %d\n", result.httpCode);
            PsramString error_msg = "{\"ok\":false, \"message\":\"API Error: HTTP ";
            char code_buf[5];
            snprintf(code_buf, sizeof(code_buf), "%d", result.httpCode);
            error_msg += code_buf;
            error_msg += "\"}";
            server->send(result.httpCode > 0 ? result.httpCode : 500, "application/json", error_msg.c_str());
        }
        vSemaphoreDelete(sem);
    } else {
        Log.println("[ThemePark] TIMEOUT waiting for API response");
        server->send(504, "application/json", "{\"ok\":false, \"message\":\"Timeout waiting for API response\"}");
        vSemaphoreDelete(sem);
    }
}

void handleSofascoreTournamentsList() {
    if (!server || !webClient) {
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"Server or WebClient not initialized\"}");
        return;
    }
    
    Log.println("[SofaScore] handleSofascoreTournamentsList called - fetching today's tournaments");
    
    // Get today's date
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);
    char dateStr[16];
    strftime(dateStr, sizeof(dateStr), "%Y-%m-%d", timeinfo);
    
    // Build URL for today only
    char urlBuffer[128];
    snprintf(urlBuffer, sizeof(urlBuffer), "https://api.sofascore.com/api/v1/sport/darts/active-tournaments?date=%s", dateStr);
    PsramString url = urlBuffer;
    PsramString headers = "";
    
    Log.printf("[SofaScore] Fetching tournaments for today: %s\n", dateStr);
    
    struct Result { 
        int httpCode; 
        PsramString payload; 
    } result;
    result.httpCode = 0;
    
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (!sem) {
        Log.println("[SofaScore] Failed to create semaphore");
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"Failed to create semaphore\"}");
        return;
    }
    
    webClient->getRequest(url, headers, [&](int httpCode, const char* payload, size_t len) {
        Log.printf("[SofaScore] Callback received - HTTP %d, payload size: %d\n", httpCode, len);
        result.httpCode = httpCode;
        if (payload && len > 0) {
            result.payload.assign(payload, len);
            Log.printf("[SofaScore] Payload first 100 chars: %.100s\n", payload);
        }
        xSemaphoreGive(sem);
    });
    
    Log.println("[SofaScore] Waiting for response (20s timeout)...");
    
    // Wait for response with 20 second timeout (same as ThemePark to avoid race conditions)
    if (xSemaphoreTake(sem, pdMS_TO_TICKS(20000)) != pdTRUE) {
        Log.println("[SofaScore] TIMEOUT waiting for API response");
        vSemaphoreDelete(sem);
        server->send(504, "application/json", "{\"ok\":false, \"message\":\"Timeout waiting for API response\"}");
        return;
    }
    
    Log.printf("[SofaScore] Response received, HTTP code: %d\n", result.httpCode);
    vSemaphoreDelete(sem);
    
    if (result.httpCode != 200) {
        Log.printf("[SofaScore] HTTP error: %d\n", result.httpCode);
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "{\"ok\":false, \"message\":\"API Error: HTTP %d\"}", result.httpCode);
        server->send(result.httpCode > 0 ? result.httpCode : 500, "application/json", errorMsg);
        return;
    }
    
    if (result.payload.empty()) {
        Log.println("[SofaScore] Empty response");
        server->send(200, "application/json", "{\"ok\":true, \"tournaments\":[]}");
        return;
    }
    
    // Parse the response using PSRAM
    struct SpiRamAllocator : ArduinoJson::Allocator {
        void* allocate(size_t size) override { return ps_malloc(size); }
        void deallocate(void* pointer) override { free(pointer); }
        void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
    };
    
    SpiRamAllocator allocator1;
    JsonDocument inputDoc(&allocator1);
    DeserializationError error = deserializeJson(inputDoc, result.payload.c_str());
    
    if (error) {
        Log.printf("[SofaScore] JSON parse error: %s\n", error.c_str());
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"Failed to parse API response\"}");
        return;
    }
    
    Log.println("[SofaScore] JSON parsed successfully");
    
    // Build response
    SpiRamAllocator allocator2;
    JsonDocument responseDoc(&allocator2);
    responseDoc["ok"] = true;
    JsonArray tournamentsArray = responseDoc.createNestedArray("tournaments");
    
    // Parse tournaments from response
    // API returns: {"tournaments":[{"tournamentId":123,"name":"Name","slug":"slug"}]}
    JsonArray tournaments = inputDoc["tournaments"].as<JsonArray>();
    for (JsonObject tournament : tournaments) {
        int id = tournament["tournamentId"] | 0;
        const char* name = tournament["name"];
        const char* slug = tournament["slug"];
        
        if (id > 0 && name && strlen(name) > 0 && slug && strlen(slug) > 0) {
            JsonObject tournamentObj = tournamentsArray.createNestedObject();
            tournamentObj["id"] = id;
            tournamentObj["name"] = name;
            tournamentObj["slug"] = slug;
            Log.printf("[SofaScore] Added tournament: %s (ID: %d, Slug: %s)\n", name, id, slug);
        }
    }
    
    String response;
    serializeJson(responseDoc, response);
    Log.printf("[SofaScore] Sending response with %d tournaments\n", tournamentsArray.size());
    server->send(200, "application/json", response);
}

// =============================================================================
// Firmware Update Handlers
// =============================================================================

void handleFirmwarePage() {
    if (!server) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += (const char*)FPSTR(HTML_FIRMWARE_UPDATE);
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleFirmwareUpload() {
    if (!server) return;
    
    HTTPUpload& upload = server->upload();
    static size_t uploadSize = 0;
    
    if (upload.status == UPLOAD_FILE_START) {
        uploadSize = 0;
        
        Log.printf("[FirmwareUpdate] Update Start: %s\n", upload.filename.c_str());
        Log.printf("[FirmwareUpdate] Free PSRAM before update: %u bytes\n", psramFree());
        
        // Start update - use UPDATE_SIZE_UNKNOWN to auto-detect size
        // The Update library will use PSRAM internally for buffering when available
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            server->send(500, "text/plain", "Update begin failed");
            return;
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        // Write data to update partition
        // The Update library handles buffering internally, using PSRAM when available
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
            server->send(500, "text/plain", "Update write failed");
            return;
        }
        uploadSize += upload.currentSize;
        
        // Log progress every 100KB
        if ((uploadSize / 102400) > ((uploadSize - upload.currentSize) / 102400)) {
            Log.printf("[FirmwareUpdate] Progress: %u bytes\n", uploadSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Log.printf("[FirmwareUpdate] Update Success: %u bytes\n", uploadSize);
            Log.printf("[FirmwareUpdate] Free PSRAM after update: %u bytes\n", psramFree());
            
            // Send success response
            server->send(200, "text/plain", "Update successful! Rebooting...");
            
            // Ensure all buffers are flushed before restarting
            flushBuffersBeforeRestart();
            
            Log.println("[FirmwareUpdate] Restarting device...");
            ESP.restart();
        } else {
            Update.printError(Serial);
            server->send(500, "text/plain", "Update end failed");
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        Log.println("[FirmwareUpdate] Update aborted");
        server->send(500, "text/plain", "Update aborted");
    }
}


void handleSofascoreDebugSnapshot() {
    if (!server) {
        return;
    }
    
#if SOFASCORE_DEBUG_JSON
    Log.println("[SofaScore] Debug snapshot requested via web interface");
    
    // Get PanelManager from Application instance
    if (!Application::_instance) {
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"Application not initialized\"}");
        return;
    }
    
    PanelManager* panelManager = Application::_instance->getPanelManager();
    if (!panelManager) {
        server->send(500, "application/json", "{\"ok\":false, \"message\":\"PanelManager not initialized\"}");
        return;
    }
    
    SofaScoreLiveModule* sofascoreModule = nullptr;
    // Find SofaScore module in panel manager
    const auto& modules = panelManager->getAllModules();
    for (DrawableModule* module : modules) {
        if (module && strcmp(module->getModuleName(), "SofaScoreLiveModule") == 0) {
            // Use static_cast since we verified the type via getModuleName()
            sofascoreModule = static_cast<SofaScoreLiveModule*>(module);
            break;
        }
    }
    
    if (!sofascoreModule) {
        server->send(404, "application/json", "{\"ok\":false, \"message\":\"SofaScore module not found\"}");
        return;
    }
    
    // Trigger debug snapshot
    sofascoreModule->debugSaveCurrentState();
    
    server->send(200, "application/json", "{\"ok\":true, \"message\":\"Debug snapshot saved to /json_debug/\"}");
#else
    server->send(501, "application/json", "{\"ok\":false, \"message\":\"Debug feature not compiled (SOFASCORE_DEBUG_JSON=0)\"}");
#endif
}
