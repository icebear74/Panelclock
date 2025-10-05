#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <WebServer.h>
#include <DNSServer.h>
#include "webconfig.hpp" // Brauchen wir für die Config-Struktur

// Externe Deklarationen der globalen Objekte (diese werden in Panelclock.ino definiert)
extern WebServer server;
extern DNSServer dnsServer;
extern DeviceConfig deviceConfig;
extern const std::vector<std::pair<const char*, const char*>> timezones;

// Externe Deklaration der Speicherfunktion
void saveDeviceConfig();

// --- HTML-Vorlagen ---

const char HTML_PAGE_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Panelclock Config</title><style>body{font-family:sans-serif;background:#222;color:#eee;}
.container{max-width:600px;margin:0 auto;padding:20px;}h1,h2{color:#4CAF50;border-bottom:1px solid #444;padding-bottom:10px;}
label{display:block;margin-top:15px;color:#bbb;}input[type="text"],input[type="password"],input[type="number"],select{width:100%;padding:8px;margin-top:5px;border-radius:4px;border:1px solid #555;background:#333;color:#eee;box-sizing:border-box;}
input[type="color"]{width:100%;height:40px;padding:5px;margin-top:5px;border-radius:4px;border:1px solid #555;box-sizing:border-box;}
input[type="submit"],.button{background-color:#4CAF50;color:white;padding:14px 20px;margin-top:20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;text-align:center;text-decoration:none;display:inline-block;}
input[type="submit"]:hover,.button:hover{background-color:#45a049;}
.group{background:#2a2a2a;border:1px solid #444;border-radius:8px;padding:20px;margin-top:20px;}
.footer-link{margin-top:20px;text-align:center;}.footer-link a{color:#4CAF50;text-decoration:none;}
</style></head><body><div class="container">
)rawliteral";
const char HTML_PAGE_FOOTER[] PROGMEM = R"rawliteral(</div></body></html>)rawliteral";

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<h1>Panelclock Hauptmenü</h1>
<a href="/config_wlan" class="button">WLAN & Allgemein</a>
<a href="/config_modules" class="button">Anzeige-Module</a>
)rawliteral";

// In diesem Schritt verwenden wir noch eine gemeinsame Form, die auf die jeweiligen Seiten aufgeteilt wird.
const char HTML_FORM_START[] PROGMEM = R"rawliteral(<form action="/save" method="POST">)rawliteral";
const char HTML_FORM_END[] PROGMEM = R"rawliteral(<input type="submit" value="Speichern & Neustarten"></form>)rawliteral";

const char HTML_GROUP_WLAN[] PROGMEM = R"rawliteral(
<h2>WLAN & Allgemein</h2>
<div class="group">
    <label for="hostname">Hostname</label><input type="text" id="hostname" name="hostname" value="{hostname}">
    <label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" value="{ssid}">
    <label for="password">Passwort</label><input type="password" id="password" name="password" value="{password}">
    <label for="otaPassword">OTA Update Passwort</label><input type="password" id="otaPassword" name="otaPassword" value="{otaPassword}">
</div>
<div class="footer-link"><a href="/">&laquo; Zurück zum Hauptmenü</a></div>
)rawliteral";

const char HTML_GROUP_MODULES[] PROGMEM = R"rawliteral(
<h2>Anzeige-Module</h2>
<div class="group">
    <h3>Zeitzone</h3>
    <label for="timezone">Zeitzone</label><select id="timezone" name="timezone">{tz_options}</select>
</div>
<div class="group">
    <h3>Tankstellen-Anzeige</h3>
    <label for="tankerApiKey">Tankerkönig API-Key</label><input type="text" id="tankerApiKey" name="tankerApiKey" value="{tankerApiKey}">
    <label for="stationId">Tankstellen-ID</label><input type="text" id="stationId" name="stationId" value="{stationId}">
    <label for="stationFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="stationFetchIntervalMin" name="stationFetchIntervalMin" value="{stationFetchIntervalMin}">
    <label for="stationDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="stationDisplaySec" name="stationDisplaySec" value="{stationDisplaySec}">
</div>
<div class="group">
    <h3>Kalender-Anzeige</h3>
    <label for="icsUrl">iCal URL (.ics)</label><input type="text" id="icsUrl" name="icsUrl" value="{icsUrl}">
    <label for="calendarFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="calendarFetchIntervalMin" name="calendarFetchIntervalMin" value="{calendarFetchIntervalMin}">
    <label for="calendarDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="calendarDisplaySec" name="calendarDisplaySec" value="{calendarDisplaySec}">
    <label for="calendarScrollMs">Scroll-Geschwindigkeit (ms)</label><input type="number" id="calendarScrollMs" name="calendarScrollMs" value="{calendarScrollMs}">
    <label for="calendarDateColor">Farbe Datum</label><input type="color" id="calendarDateColor" name="calendarDateColor" value="{calendarDateColor}">
    <label for="calendarTextColor">Farbe Text</label><input type="color" id="calendarTextColor" name="calendarTextColor" value="{calendarTextColor}">
</div>
<div class="footer-link"><a href="/">&laquo; Zurück zum Hauptmenü</a></div>
)rawliteral";


// --- Handler-Funktionen ---

void handleRoot() {
    String page = FPSTR(HTML_PAGE_HEADER);
    page += FPSTR(HTML_INDEX);
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleConfigWlan() {
    String page = FPSTR(HTML_PAGE_HEADER);
    page += FPSTR(HTML_FORM_START);
    String content = FPSTR(HTML_GROUP_WLAN);
    content.replace("{hostname}", deviceConfig.hostname);
    content.replace("{ssid}", deviceConfig.ssid);
    content.replace("{password}", deviceConfig.password);
    content.replace("{otaPassword}", deviceConfig.otaPassword);
    page += content;
    page += FPSTR(HTML_FORM_END);
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleConfigModules() {
    String page = FPSTR(HTML_PAGE_HEADER);
    page += FPSTR(HTML_FORM_START);
    String content = FPSTR(HTML_GROUP_MODULES);
    String tz_options_html;
    for (const auto& tz : timezones) {
        tz_options_html += "<option value=\"" + String(tz.second) + "\"";
        if (String(tz.second) == deviceConfig.timezone) tz_options_html += " selected";
        tz_options_html += ">" + String(tz.first) + "</option>";
    }
    content.replace("{tz_options}", tz_options_html);
    content.replace("{tankerApiKey}", deviceConfig.tankerApiKey);
    content.replace("{stationId}", deviceConfig.stationId);
    content.replace("{stationFetchIntervalMin}", String(deviceConfig.stationFetchIntervalMin));
    content.replace("{stationDisplaySec}", String(deviceConfig.stationDisplaySec));
    content.replace("{icsUrl}", deviceConfig.icsUrl);
    content.replace("{calendarFetchIntervalMin}", String(deviceConfig.calendarFetchIntervalMin));
    content.replace("{calendarDisplaySec}", String(deviceConfig.calendarDisplaySec));
    content.replace("{calendarScrollMs}", String(deviceConfig.calendarScrollMs));
    content.replace("{calendarDateColor}", deviceConfig.calendarDateColor);
    content.replace("{calendarTextColor}", deviceConfig.calendarTextColor);
    page += content;
    page += FPSTR(HTML_FORM_END);
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleSave() {
    // Da wir noch eine gemeinsame Form haben, lesen wir alle Argumente aus, die da sein könnten.
    if(server.hasArg("hostname")) deviceConfig.hostname = server.arg("hostname");
    if(server.hasArg("ssid")) deviceConfig.ssid = server.arg("ssid");
    if (server.hasArg("password") && server.arg("password").length() > 0) deviceConfig.password = server.arg("password");
    if (server.hasArg("otaPassword") && server.arg("otaPassword").length() > 0) deviceConfig.otaPassword = server.arg("otaPassword");
    if(server.hasArg("timezone")) deviceConfig.timezone = server.arg("timezone");
    if(server.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = server.arg("tankerApiKey");
    if(server.hasArg("stationId")) deviceConfig.stationId = server.arg("stationId");
    if(server.hasArg("stationFetchIntervalMin")) deviceConfig.stationFetchIntervalMin = server.arg("stationFetchIntervalMin").toInt();
    if(server.hasArg("stationDisplaySec")) deviceConfig.stationDisplaySec = server.arg("stationDisplaySec").toInt();
    if(server.hasArg("icsUrl")) deviceConfig.icsUrl = server.arg("icsUrl");
    if(server.hasArg("calendarFetchIntervalMin")) deviceConfig.calendarFetchIntervalMin = server.arg("calendarFetchIntervalMin").toInt();
    if(server.hasArg("calendarDisplaySec")) deviceConfig.calendarDisplaySec = server.arg("calendarDisplaySec").toInt();
    if(server.hasArg("calendarScrollMs")) deviceConfig.calendarScrollMs = server.arg("calendarScrollMs").toInt();
    if(server.hasArg("calendarDateColor")) deviceConfig.calendarDateColor = server.arg("calendarDateColor");
    if(server.hasArg("calendarTextColor")) deviceConfig.calendarTextColor = server.arg("calendarTextColor");

    saveDeviceConfig();
    
    String page = FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
    delay(1000);
    ESP.restart();
}

void handleNotFound(); // Deklaration, Definition in Panelclock.ino

// --- Setup-Funktionen ---

void setupWebServer(bool portalMode) {
    if (portalMode) {
        dnsServer.start(53, "*", WiFi.softAPIP());
    }
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config_wlan", HTTP_GET, handleConfigWlan);
    server.on("/config_modules", HTTP_GET, handleConfigModules);
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

#endif