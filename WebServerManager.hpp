#ifndef WEBSERVER_MANAGER_HPP
#define WEBSERVER_MANAGER_HPP

#include <WebServer.h>
#include <DNSServer.h>
#include "webconfig.hpp"
#include "HardwareConfig.hpp"

// Externe Deklarationen der globalen Objekte
extern WebServer server;
extern DNSServer dnsServer;
extern DeviceConfig deviceConfig;
extern HardwareConfig hardwareConfig;
extern const std::vector<std::pair<const char*, const char*>> timezones;

// Externe Deklaration der Speicherfunktionen
void saveDeviceConfig();
void saveHardwareConfig();

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
hr{border:0;border-top:1px solid #444;margin:20px 0;}
</style></head><body><div class="container">
)rawliteral";
const char HTML_PAGE_FOOTER[] PROGMEM = R"rawliteral(</div></body></html>)rawliteral";

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<h1>Panelclock Hauptmen&uuml;</h1>
<a href="/config_base" class="button">Grundkonfiguration (mit Neustart)</a>
<a href="/config_modules" class="button">Anzeige-Module</a>
)rawliteral";

const char HTML_CONFIG_BASE[] PROGMEM = R"rawliteral(
<h2>Grundkonfiguration</h2>
<form action="/save_base" method="POST">
<div class="group">
    <h3>Allgemein &amp; WLAN</h3>
    <label for="hostname">Hostname</label><input type="text" id="hostname" name="hostname" value="{hostname}">
    <label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" value="{ssid}">
    <label for="password">Passwort</label><input type="password" id="password" name="password" value="{password}">
    <label for="otaPassword">OTA Update Passwort</label><input type="password" id="otaPassword" name="otaPassword" value="{otaPassword}">
</div>
<hr>
<div class="group">
    <h3>Hardware Pin-Belegung (Experte!)</h3>
    <p style="color:#ff8c00;">Warnung: Falsche Werte hier k&ouml;nnen die Anzeige unbrauchbar machen!</p>
    <label for="R1">Pin R1</label><input type="number" id="R1" name="R1" value="{R1}">
    <label for="G1">Pin G1</label><input type="number" id="G1" name="G1" value="{G1}">
    <label for="B1">Pin B1</label><input type="number" id="B1" name="B1" value="{B1}">
    <label for="R2">Pin R2</label><input type="number" id="R2" name="R2" value="{R2}">
    <label for="G2">Pin G2</label><input type="number" id="G2" name="G2" value="{G2}">
    <label for="B2">Pin B2</label><input type="number" id="B2" name="B2" value="{B2}">
    <label for="A">Pin A</label><input type="number" id="A" name="A" value="{A}">
    <label for="B">Pin B</label><input type="number" id="B" name="B" value="{B}">
    <label for="C">Pin C</label><input type="number" id="C" name="C" value="{C}">
    <label for="D">Pin D</label><input type="number" id="D" name="D" value="{D}">
    <label for="E">Pin E</label><input type="number" id="E" name="E" value="{E}">
    <label for="CLK">Pin CLK</label><input type="number" id="CLK" name="CLK" value="{CLK}">
    <label for="LAT">Pin LAT</label><input type="number" id="LAT" name="LAT" value="{LAT}">
    <label for="OE">Pin OE</label><input type="number" id="OE" name="OE" value="{OE}">
</div>
<input type="submit" value="Speichern &amp; Neustarten">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_CONFIG_MODULES[] PROGMEM = R"rawliteral(
<h2>Anzeige-Module</h2>
<form action="/save_modules" method="POST">
<div class="group">
    <h3>Zeitzone</h3>
    <label for="timezone">Zeitzone</label><select id="timezone" name="timezone">{tz_options}</select>
</div>
<div class="group">
    <h3>Tankstellen-Anzeige</h3>
    <label for="tankerApiKey">Tankerk&ouml;nig API-Key</label><input type="text" id="tankerApiKey" name="tankerApiKey" value="{tankerApiKey}">
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
<input type="submit" value="Speichern &amp; Neustarten">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

// --- Handler-Funktionen ---

void handleRoot() {
    String page = FPSTR(HTML_PAGE_HEADER);
    page += FPSTR(HTML_INDEX);
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleConfigBase() {
    String page = FPSTR(HTML_PAGE_HEADER);
    String content = FPSTR(HTML_CONFIG_BASE);
    // DeviceConfig Werte
    content.replace("{hostname}", deviceConfig.hostname);
    content.replace("{ssid}", deviceConfig.ssid);
    content.replace("{password}", deviceConfig.password);
    content.replace("{otaPassword}", deviceConfig.otaPassword);
    // HardwareConfig Werte
    content.replace("{R1}", String(hardwareConfig.R1));
    content.replace("{G1}", String(hardwareConfig.G1));
    content.replace("{B1}", String(hardwareConfig.B1));
    content.replace("{R2}", String(hardwareConfig.R2));
    content.replace("{G2}", String(hardwareConfig.G2));
    content.replace("{B2}", String(hardwareConfig.B2));
    content.replace("{A}", String(hardwareConfig.A));
    content.replace("{B}", String(hardwareConfig.B));
    content.replace("{C}", String(hardwareConfig.C));
    content.replace("{D}", String(hardwareConfig.D));
    content.replace("{E}", String(hardwareConfig.E));
    content.replace("{CLK}", String(hardwareConfig.CLK));
    content.replace("{LAT}", String(hardwareConfig.LAT));
    content.replace("{OE}", String(hardwareConfig.OE));
    page += content;
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleConfigModules() {
    String page = FPSTR(HTML_PAGE_HEADER);
    String content = FPSTR(HTML_CONFIG_MODULES);
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
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
}

void handleSaveBase() {
    // DeviceConfig Teile speichern
    deviceConfig.hostname = server.arg("hostname");
    deviceConfig.ssid = server.arg("ssid");
    if (server.hasArg("password") && server.arg("password").length() > 0) deviceConfig.password = server.arg("password");
    if (server.hasArg("otaPassword") && server.arg("otaPassword").length() > 0) deviceConfig.otaPassword = server.arg("otaPassword");
    saveDeviceConfig();

    // HardwareConfig Teile speichern
    hardwareConfig.R1 = server.arg("R1").toInt();
    hardwareConfig.G1 = server.arg("G1").toInt();
    hardwareConfig.B1 = server.arg("B1").toInt();
    hardwareConfig.R2 = server.arg("R2").toInt();
    hardwareConfig.G2 = server.arg("G2").toInt();
    hardwareConfig.B2 = server.arg("B2").toInt();
    hardwareConfig.A = server.arg("A").toInt();
    hardwareConfig.B = server.arg("B").toInt();
    hardwareConfig.C = server.arg("C").toInt();
    hardwareConfig.D = server.arg("D").toInt();
    hardwareConfig.E = server.arg("E").toInt();
    hardwareConfig.CLK = server.arg("CLK").toInt();
    hardwareConfig.LAT = server.arg("LAT").toInt();
    hardwareConfig.OE = server.arg("OE").toInt();
    saveHardwareConfig();

    String page = FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Grundkonfiguration gespeichert. Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
    delay(1000);
    ESP.restart();
}

void handleSaveModules() {
    deviceConfig.timezone = server.arg("timezone");
    deviceConfig.tankerApiKey = server.arg("tankerApiKey");
    deviceConfig.stationId = server.arg("stationId");
    deviceConfig.stationFetchIntervalMin = server.arg("stationFetchIntervalMin").toInt();
    deviceConfig.stationDisplaySec = server.arg("stationDisplaySec").toInt();
    deviceConfig.icsUrl = server.arg("icsUrl");
    deviceConfig.calendarFetchIntervalMin = server.arg("calendarFetchIntervalMin").toInt();
    deviceConfig.calendarDisplaySec = server.arg("calendarDisplaySec").toInt();
    deviceConfig.calendarScrollMs = server.arg("calendarScrollMs").toInt();
    deviceConfig.calendarDateColor = server.arg("calendarDateColor");
    deviceConfig.calendarTextColor = server.arg("calendarTextColor");
    saveDeviceConfig();
    
    String page = FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Modul-Konfiguration gespeichert. Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
    page += FPSTR(HTML_PAGE_FOOTER);
    server.send(200, "text/html", page);
    delay(1000);
    ESP.restart();
}


void handleNotFound();

void setupWebServer(bool portalMode) {
    if (portalMode) {
        dnsServer.start(53, "*", WiFi.softAPIP());
    }
    server.on("/", HTTP_GET, handleRoot);
    server.on("/config_base", HTTP_GET, handleConfigBase);
    server.on("/config_modules", HTTP_GET, handleConfigModules);
    server.on("/save_base", HTTP_POST, handleSaveBase);
    server.on("/save_modules", HTTP_POST, handleSaveModules);
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