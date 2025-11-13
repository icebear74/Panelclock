#include "WebHandlers.hpp"
#include "WebServerManager.hpp"
#include "WebPages.hpp"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include "MemoryLogger.hpp"

// Externals from main application / WebServerManager.hpp
extern WebServer* server;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;
extern TankerkoenigModule* tankerkoenigModule;
extern bool portalRunning;

// Forward declarations of global functions (declared in WebServerManager.hpp)
void saveDeviceConfig();
void saveHardwareConfig();
void applyLiveConfig();

// -----------------------------
// Implementations
// -----------------------------

void handleRoot() {
    if (!server) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += (const char*)FPSTR(HTML_INDEX);
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
    delay(1000);
    ESP.restart();
}

void handleConfigLocation() {
    if (!server || !deviceConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_LOCATION);
    replaceAll(content, "{latitude}", String(deviceConfig->userLatitude, 6).c_str());
    replaceAll(content, "{longitude}", String(deviceConfig->userLongitude, 6).c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveLocation() {
    if (!server || !deviceConfig) return;
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
    PsramString tz_options_html;
    for (const auto& tz : timezones) {
        tz_options_html += "<option value=\""; tz_options_html += tz.second; tz_options_html += "\"";
        if (deviceConfig->timezone == tz.second) tz_options_html += " selected";
        tz_options_html += ">"; tz_options_html += tz.first; tz_options_html += "</option>";
    }
    replaceAll(content, "{tz_options}", tz_options_html.c_str());

    // Wetter-Platzhalter ersetzen
    replaceAll(content, "{weatherEnabled_checked}", deviceConfig->weatherEnabled ? "checked" : "");
    replaceAll(content, "{weatherApiKey}", deviceConfig->weatherApiKey.c_str());
    replaceAll(content, "{weatherShowCurrent_checked}", deviceConfig->weatherShowCurrent ? "checked" : "");
    replaceAll(content, "{weatherShowHourly_checked}", deviceConfig->weatherShowHourly ? "checked" : "");
    replaceAll(content, "{weatherShowDaily_checked}", deviceConfig->weatherShowDaily ? "checked" : "");
    replaceAll(content, "{weatherAlertsEnabled_checked}", deviceConfig->weatherAlertsEnabled ? "checked" : "");
    replaceAll(content, "{weatherHourlyMode_0_selected}", deviceConfig->weatherHourlyMode == 0 ? "selected" : "");
    replaceAll(content, "{weatherHourlyMode_1_selected}", deviceConfig->weatherHourlyMode == 1 ? "selected" : "");


    replaceAll(content, "{tankerApiKey}", deviceConfig->tankerApiKey.c_str());
    replaceAll(content, "{tankerkoenigStationIds}", deviceConfig->tankerkoenigStationIds.c_str());
    
    char num_buf[20];
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherFetchIntervalMin); replaceAll(content, "{weatherFetchIntervalMin}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherDisplaySec); replaceAll(content, "{weatherDisplaySec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherDailyForecastDays); replaceAll(content, "{weatherDailyForecastDays}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherHourlySlotMorning); replaceAll(content, "{weatherHourlySlotMorning}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherHourlySlotNoon); replaceAll(content, "{weatherHourlySlotNoon}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherHourlySlotEvening); replaceAll(content, "{weatherHourlySlotEvening}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->weatherHourlyInterval); replaceAll(content, "{weatherHourlyInterval}", num_buf);
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
    replaceAll(content, "{fritzboxEnabled_checked}", deviceConfig->fritzboxEnabled ? "checked" : "");
    replaceAll(content, "{fritzboxIp}", deviceConfig->fritzboxIp.c_str());

    // Neue Kalender-Urgent-Parameter
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarFastBlinkHours); replaceAll(content, "{calendarFastBlinkHours}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentThresholdHours); replaceAll(content, "{calendarUrgentThresholdHours}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentDurationSec); replaceAll(content, "{calendarUrgentDurationSec}", num_buf);
    snprintf(num_buf, sizeof(num_buf), "%d", deviceConfig->calendarUrgentRepeatMin); replaceAll(content, "{calendarUrgentRepeatMin}", num_buf);

    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleSaveModules() {
    if (!server || !deviceConfig) return;
    deviceConfig->timezone = server->arg("timezone").c_str();

    // Wetter-Werte speichern
    deviceConfig->weatherEnabled = server->hasArg("weatherEnabled");
    deviceConfig->weatherApiKey = server->arg("weatherApiKey").c_str();
    deviceConfig->weatherFetchIntervalMin = server->arg("weatherFetchIntervalMin").toInt();
    deviceConfig->weatherDisplaySec = server->arg("weatherDisplaySec").toInt();
    deviceConfig->weatherShowCurrent = server->hasArg("weatherShowCurrent");
    deviceConfig->weatherShowHourly = server->hasArg("weatherShowHourly");
    deviceConfig->weatherShowDaily = server->hasArg("weatherShowDaily");
    deviceConfig->weatherDailyForecastDays = server->arg("weatherDailyForecastDays").toInt();
    deviceConfig->weatherHourlyMode = server->arg("weatherHourlyMode").toInt();
    deviceConfig->weatherHourlySlotMorning = server->arg("weatherHourlySlotMorning").toInt();
    deviceConfig->weatherHourlySlotNoon = server->arg("weatherHourlySlotNoon").toInt();
    deviceConfig->weatherHourlySlotEvening = server->arg("weatherHourlySlotEvening").toInt();
    deviceConfig->weatherHourlyInterval = server->arg("weatherHourlyInterval").toInt();
    deviceConfig->weatherAlertsEnabled = server->hasArg("weatherAlertsEnabled");
    deviceConfig->weatherAlertsDisplaySec = server->arg("weatherAlertsDisplaySec").toInt();
    deviceConfig->weatherAlertsRepeatMin = server->arg("weatherAlertsRepeatMin").toInt();

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
                char* strtok_ctx;
                char* id_token = strtok_r((char*)temp_ids.c_str(), ",", &strtok_ctx);
                while(id_token != nullptr) { final_ids.push_back(id_token); id_token = strtok_r(nullptr, ",", &strtok_ctx); }

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
    deviceConfig->calendarScrollMs = server->arg("calendarScrollMs").toInt();
    deviceConfig->calendarDateColor = server->arg("calendarDateColor").c_str();
    deviceConfig->calendarTextColor = server->arg("calendarTextColor").c_str();

    // Neue Kalender-Urgent-Parameter aus Webinterface lesen
    if (server->hasArg("calendarFastBlinkHours")) deviceConfig->calendarFastBlinkHours = server->arg("calendarFastBlinkHours").toInt();
    if (server->hasArg("calendarUrgentThresholdHours")) deviceConfig->calendarUrgentThresholdHours = server->arg("calendarUrgentThresholdHours").toInt();
    if (server->hasArg("calendarUrgentDurationSec")) deviceConfig->calendarUrgentDurationSec = server->arg("calendarUrgentDurationSec").toInt();
    if (server->hasArg("calendarUrgentRepeatMin")) deviceConfig->calendarUrgentRepeatMin = server->arg("calendarUrgentRepeatMin").toInt();

    deviceConfig->dartsOomEnabled = server->hasArg("dartsOomEnabled");
    deviceConfig->dartsProTourEnabled = server->hasArg("dartsProTourEnabled");
    deviceConfig->dartsDisplaySec = server->arg("dartsDisplaySec").toInt();
    deviceConfig->trackedDartsPlayers = server->arg("trackedDartsPlayers").c_str();
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
        delay(1000);
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

            DynamicJsonDocument newResultsDoc(32768);
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
    page += FPSTR(HTML_STREAM_PAGE);
    page += FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page);
}
