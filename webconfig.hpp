#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <vector>
#include <functional> // Hinzugefügt für std::function
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "PsramUtils.hpp"

// Struktur für die Geräteeinstellungen
struct DeviceConfig {
    String hostname = "Panel-Clock";
    String ssid;
    String password;
    String otaPassword;
    String timezone = "CET-1CEST,M3.5.0,M10.5.0/3";
    String tankerApiKey;
    String stationId;
    int stationFetchIntervalMin = 5;
    String icsUrl;
    int calendarFetchIntervalMin = 60;
    int calendarScrollMs = 50;
    String calendarDateColor = "#FBE000"; // Gelb
    String calendarTextColor = "#FFFFFF"; // Weiß
    int calendarDisplaySec = 30;
    int stationDisplaySec = 15;
};

DeviceConfig deviceConfig;
WebServer server(80);
DNSServer dnsServer;

const std::vector<std::pair<const char*, const char*>> timezones = {
    {"(UTC+1) Berlin, Amsterdam", "CET-1CEST,M3.5.0,M10.5.0/3"},
    {"(UTC+1) London, Dublin", "GMT0BST,M3.5.0/1,M10.5.0"},
    {"(UTC+2) Athens, Helsinki", "EET-2EEST,M3.5.0/3,M10.5.0/4"},
    {"(UTC-4) Atlantic Time (Canada)", "AST4ADT,M3.2.0,M11.1.0"},
    {"(UTC-5) Eastern Time (US)", "EST5EDT,M3.2.0,M11.1.0"},
    {"(UTC-6) Central Time (US)", "CST6CDT,M3.2.0,M11.1.0"},
    {"(UTC-7) Mountain Time (US)", "MST7MDT,M3.2.0,M11.1.0"},
    {"(UTC-8) Pacific Time (US)", "PST8PDT,M3.2.0,M11.1.0"},
    {"(UTC-9) Alaska", "AKST9AKDT,M3.2.0,M11.1.0"},
    {"(UTC-10) Hawaii", "HST10"},
    {"(UTC+10) Brisbane", "AEST-10"},
    {"(UTC+10) Sydney, Melbourne", "AEST-10AEDT,M10.1.0,M4.1.0/3"}
};

const char config_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Panelclock Config</title>
    <style>
        body { font-family: sans-serif; background: #222; color: #eee; }
        .container { max-width: 600px; margin: 0 auto; padding: 20px; }
        h1, h2 { color: #4CAF50; border-bottom: 1px solid #444; padding-bottom: 10px; }
        label { display: block; margin-top: 15px; color: #bbb; }
        input[type="text"], input[type="password"], input[type="number"], select {
            width: 100%; padding: 8px; margin-top: 5px; border-radius: 4px;
            border: 1px solid #555; background: #333; color: #eee; box-sizing: border-box;
        }
        input[type="color"] { width: 100%; height: 40px; padding: 5px; margin-top: 5px; border-radius: 4px; border: 1px solid #555; box-sizing: border-box; }
        input[type="submit"] {
            background-color: #4CAF50; color: white; padding: 14px 20px;
            margin-top: 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px;
        }
        input[type="submit"]:hover { background-color: #45a049; }
        .group { background: #2a2a2a; border: 1px solid #444; border-radius: 8px; padding: 20px; margin-top: 20px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Panelclock Konfiguration</h1>
        <form action="/save" method="POST">
            <div class="group">
                <h2>Allgemein</h2>
                <label for="hostname">Hostname</label>
                <input type="text" id="hostname" name="hostname" value="{hostname}">
                <label for="otaPassword">OTA Update Passwort (optional)</label>
                <input type="password" id="otaPassword" name="otaPassword" value="{otaPassword}">
                <label for="timezone">Zeitzone</label>
                <select id="timezone" name="timezone">{tz_options}</select>
            </div>
            <div class="group">
                <h2>WLAN</h2>
                <label for="ssid">SSID</label>
                <input type="text" id="ssid" name="ssid" value="{ssid}">
                <label for="password">Passwort</label>
                <input type="password" id="password" name="password" value="{password}">
            </div>
            <div class="group">
                <h2>Tankstellen-Anzeige</h2>
                <label for="tankerApiKey">Tankerkönig API-Key</label>
                <input type="text" id="tankerApiKey" name="tankerApiKey" value="{tankerApiKey}">
                <label for="stationId">Tankstellen-ID</label>
                <input type="text" id="stationId" name="stationId" value="{stationId}">
                <label for="stationFetchIntervalMin">Abrufintervall (Minuten)</label>
                <input type="number" id="stationFetchIntervalMin" name="stationFetchIntervalMin" value="{stationFetchIntervalMin}">
                <label for="stationDisplaySec">Anzeigedauer (Sekunden)</label>
                <input type="number" id="stationDisplaySec" name="stationDisplaySec" value="{stationDisplaySec}">
            </div>
            <div class="group">
                <h2>Kalender-Anzeige</h2>
                <label for="icsUrl">iCal URL (.ics)</label>
                <input type="text" id="icsUrl" name="icsUrl" value="{icsUrl}">
                <label for="calendarFetchIntervalMin">Abrufintervall (Minuten)</label>
                <input type="number" id="calendarFetchIntervalMin" name="calendarFetchIntervalMin" value="{calendarFetchIntervalMin}">
                <label for="calendarDisplaySec">Anzeigedauer (Sekunden)</label>
                <input type="number" id="calendarDisplaySec" name="calendarDisplaySec" value="{calendarDisplaySec}">
                <label for="calendarScrollMs">Scroll-Geschwindigkeit (ms pro Schritt)</label>
                <input type="number" id="calendarScrollMs" name="calendarScrollMs" value="{calendarScrollMs}">
                <label for="calendarDateColor">Farbe Datum</label>
                <input type="color" id="calendarDateColor" name="calendarDateColor" value="{calendarDateColor}">
                <label for="calendarTextColor">Farbe Text</label>
                <input type="color" id="calendarTextColor" name="calendarTextColor" value="{calendarTextColor}">
            </div>
            <input type="submit" value="Speichern & Neustarten">
        </form>
    </div>
</body>
</html>
)rawliteral";

void loadDeviceConfig() {
    if (LittleFS.exists("/config.json")) {
        File configFile = LittleFS.open("/config.json", "r");
        if (configFile) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, configFile);
            if (!error) {
                deviceConfig.hostname = doc["hostname"] | "Panel-Clock";
                deviceConfig.ssid = doc["ssid"] | "";
                deviceConfig.password = doc["password"] | "";
                deviceConfig.otaPassword = doc["otaPassword"] | "";
                deviceConfig.timezone = doc["timezone"] | "CET-1CEST,M3.5.0,M10.5.0/3";
                deviceConfig.tankerApiKey = doc["tankerApiKey"] | "";
                deviceConfig.stationId = doc["stationId"] | "";
                deviceConfig.stationFetchIntervalMin = doc["stationFetchIntervalMin"] | 5;
                deviceConfig.icsUrl = doc["icsUrl"] | "";
                deviceConfig.calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | 60;
                deviceConfig.calendarScrollMs = doc["calendarScrollMs"] | 50;
                deviceConfig.calendarDateColor = doc["calendarDateColor"] | "#FBE000";
                deviceConfig.calendarTextColor = doc["calendarTextColor"] | "#FFFFFF";
                deviceConfig.calendarDisplaySec = doc["calendarDisplaySec"] | 30;
                deviceConfig.stationDisplaySec = doc["stationDisplaySec"] | 15;
                Serial.println("Konfiguration geladen.");
            } else {
                Serial.println("Fehler beim Parsen der Konfiguration.");
            }
            configFile.close();
        }
    } else {
        Serial.println("Keine Konfigurationsdatei gefunden, verwende Standardwerte.");
    }
}

void saveDeviceConfig() {
    JsonDocument doc;
    doc["hostname"] = deviceConfig.hostname;
    doc["ssid"] = deviceConfig.ssid;
    doc["password"] = deviceConfig.password;
    doc["otaPassword"] = deviceConfig.otaPassword;
    doc["timezone"] = deviceConfig.timezone;
    doc["tankerApiKey"] = deviceConfig.tankerApiKey;
    doc["stationId"] = deviceConfig.stationId;
    doc["stationFetchIntervalMin"] = deviceConfig.stationFetchIntervalMin;
    doc["icsUrl"] = deviceConfig.icsUrl;
    doc["calendarFetchIntervalMin"] = deviceConfig.calendarFetchIntervalMin;
    doc["calendarScrollMs"] = deviceConfig.calendarScrollMs;
    doc["calendarDateColor"] = deviceConfig.calendarDateColor;
    doc["calendarTextColor"] = deviceConfig.calendarTextColor;
    doc["calendarDisplaySec"] = deviceConfig.calendarDisplaySec;
    doc["stationDisplaySec"] = deviceConfig.stationDisplaySec;

    File configFile = LittleFS.open("/config.json", "w");
    if (configFile) {
        serializeJson(doc, configFile);
        configFile.close();
        Serial.println("Konfiguration gespeichert.");
    } else {
        Serial.println("Fehler beim Speichern der Konfiguration.");
    }
}

void handleRoot();
void handleSave();
void handleNotFound();

void startConfigPortal(std::function<void(const char*)> statusCallback) {
    const char* apSsid = "Panelclock-Setup";
    if (statusCallback) {
        statusCallback(("Kein WLAN, starte\nKonfig-Portal:\n" + String(apSsid)).c_str());
    }

    WiFi.softAP(apSsid);
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleNotFound);
    
    server.begin();
}

void setupWebServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.onNotFound(handleNotFound);
    server.begin();
}

void handleWebServer(bool portalIsRunning) {
    if (portalIsRunning) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

void handleRoot() {
    String page = FPSTR(config_html);
    String tz_options_html;
    for (const auto& tz : timezones) {
        tz_options_html += "<option value=\"" + String(tz.second) + "\"";
        if (String(tz.second) == deviceConfig.timezone) {
            tz_options_html += " selected";
        }
        tz_options_html += ">" + String(tz.first) + "</option>";
    }
    
    page.replace("{tz_options}", tz_options_html);
    page.replace("{hostname}", deviceConfig.hostname);
    page.replace("{ssid}", deviceConfig.ssid);
    page.replace("{password}", deviceConfig.password);
    page.replace("{otaPassword}", deviceConfig.otaPassword);
    page.replace("{tankerApiKey}", deviceConfig.tankerApiKey);
    page.replace("{stationId}", deviceConfig.stationId);
    page.replace("{stationFetchIntervalMin}", String(deviceConfig.stationFetchIntervalMin));
    page.replace("{icsUrl}", deviceConfig.icsUrl);
    page.replace("{calendarFetchIntervalMin}", String(deviceConfig.calendarFetchIntervalMin));
    page.replace("{calendarScrollMs}", String(deviceConfig.calendarScrollMs));
    page.replace("{calendarDateColor}", deviceConfig.calendarDateColor);
    page.replace("{calendarTextColor}", deviceConfig.calendarTextColor);
    page.replace("{calendarDisplaySec}", String(deviceConfig.calendarDisplaySec));
    page.replace("{stationDisplaySec}", String(deviceConfig.stationDisplaySec));
    
    server.send(200, "text/html", page);
}

void handleSave() {
    deviceConfig.hostname = server.arg("hostname");
    deviceConfig.ssid = server.arg("ssid");
    if (server.hasArg("password") && server.arg("password").length() > 0) {
        deviceConfig.password = server.arg("password");
    }
    if (server.hasArg("otaPassword") && server.arg("otaPassword").length() > 0) {
        deviceConfig.otaPassword = server.arg("otaPassword");
    }
    deviceConfig.timezone = server.arg("timezone");
    deviceConfig.tankerApiKey = server.arg("tankerApiKey");
    deviceConfig.stationId = server.arg("stationId");
    deviceConfig.stationFetchIntervalMin = server.arg("stationFetchIntervalMin").toInt();
    deviceConfig.icsUrl = server.arg("icsUrl");
    deviceConfig.calendarFetchIntervalMin = server.arg("calendarFetchIntervalMin").toInt();
    deviceConfig.calendarScrollMs = server.arg("calendarScrollMs").toInt();
    deviceConfig.calendarDateColor = server.arg("calendarDateColor");
    deviceConfig.calendarTextColor = server.arg("calendarTextColor");
    deviceConfig.calendarDisplaySec = server.arg("calendarDisplaySec").toInt();
    deviceConfig.stationDisplaySec = server.arg("stationDisplaySec").toInt();
    
    saveDeviceConfig();
    
    String page = "<h1>Gespeichert!</h1><p>Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 5000);</script>";
    server.send(200, "text/html", page);
    delay(1000);
    ESP.restart();
}

void handleNotFound() {
    if (!server.hostHeader().equals(deviceConfig.hostname.c_str()) && !server.hostHeader().equals(WiFi.softAPIP().toString())) {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
    } else {
        server.send(404, "text/plain", "Not found");
    }
}

#endif