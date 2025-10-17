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

static inline void replaceAll(PsramString& str, const PsramString& from, const PsramString& to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != PsramString::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); 
    }
}

extern WebServer* server;
extern DNSServer* dnsServer;
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern WebClientModule* webClient;
extern const std::vector<std::pair<const char*, const char*>> timezones;
extern MwaveSensorModule* mwaveSensorModule;
extern GeneralTimeConverter* timeConverter;

void saveDeviceConfig();
void saveHardwareConfig();
void applyLiveConfig();

File uploadFile; 

const char HTML_PAGE_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Panelclock Config</title><style>body{font-family:sans-serif;background:#222;color:#eee;}
.container{max-width:800px;margin:0 auto;padding:20px;}h1,h2{color:#4CAF50;border-bottom:1px solid #444;padding-bottom:10px;}
label{display:block;margin-top:15px;color:#bbb;}input[type="text"],input[type="password"],input[type="number"],select{width:100%;padding:8px;margin-top:5px;border-radius:4px;border:1px solid #555;background-color:#333;color:#eee;box-sizing:border-box;}
input[type="checkbox"]{margin-right:10px;transform:scale(1.5);}
input[type="color"]{width:100%;height:40px;padding:5px;margin-top:5px;border-radius:4px;border:1px solid #555;box-sizing:border-box;}
input[type="submit"],.button{background-color:#4CAF50;color:white;padding:14px 20px;margin-top:20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;text-align:center;text-decoration:none;display:inline-block;}
input[type="submit"]:hover,.button:hover{background-color:#45a049;}
.button-danger{background-color:#f44336;}.button-danger:hover{background-color:#da190b;}
.group{background:#2a2a2a;border:1px solid #444;border-radius:8px;padding:20px;margin-top:20px;}
.footer-link{margin-top:20px;text-align:center;}.footer-link a{color:#4CAF50;text-decoration:none;}
hr{border:0;border-top:1px solid #444;margin:20px 0;}
table{width:100%;border-collapse:collapse;margin-top:20px;}th,td{border:1px solid #555;padding:8px;text-align:left;}th{background-color:#4CAF50;color:black;}
</style></head><body><div class="container">
)rawliteral";
const char HTML_PAGE_FOOTER[] PROGMEM = R"rawliteral(</div></body></html>)rawliteral";

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<h1>Panelclock Hauptmen&uuml;</h1>
<a href="/config_base" class="button button-danger">Grundkonfiguration (mit Neustart)</a>
<a href="/config_modules" class="button">Anzeige-Module (Live-Update)</a>
<a href="/config_hardware" class="button">Optionale Hardware</a>
<a href="/config_certs" class="button">Zertifikat-Management</a>
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
    <h3>Hardware Pin-Belegung (Display)</h3>
    <p style="color:#ff8c00;">Warnung: Falsche Werte hier erfordern einen Neustart!</p>
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
<input type="submit" value="Speichern &amp; Neustarten" class="button-danger">
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
<div class="group">
    <h3>Darts-Ranglisten</h3>
    <input type="checkbox" id="dartsOomEnabled" name="dartsOomEnabled" {dartsOomEnabled_checked}><label for="dartsOomEnabled" style="display:inline;">Order of Merit anzeigen</label><br>
    <input type="checkbox" id="dartsProTourEnabled" name="dartsProTourEnabled" {dartsProTourEnabled_checked}><label for="dartsProTourEnabled" style="display:inline;">Pro Tour anzeigen</label><br><br>
    <label for="dartsDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="dartsDisplaySec" name="dartsDisplaySec" value="{dartsDisplaySec}">
    <label for="trackedDartsPlayers">Lieblingsspieler (kommagetrennt)</label><input type="text" id="trackedDartsPlayers" name="trackedDartsPlayers" value="{trackedDartsPlayers}">
</div>
<div class="group">
    <h3>Fritz!Box Anrufanzeige</h3>
    <p style="color:#ffc107;">Hinweis: Damit dies funktioniert, muss der "Call Monitor" auf der Fritz!Box einmalig per Telefon aktiviert werden. W&auml;hlen Sie dazu: <strong>#96*5*</strong></p>
    <input type="checkbox" id="fritzboxEnabled" name="fritzboxEnabled" {fritzboxEnabled_checked}><label for="fritzboxEnabled" style="display:inline;">Anrufanzeige aktivieren</label><br><br>
    <label for="fritzboxIp">Fritz!Box IP-Adresse (leer lassen f&uuml;r autom. Erkennung)</label><input type="text" id="fritzboxIp" name="fritzboxIp" value="{fritzboxIp}">
</div>
<input type="submit" value="Speichern (Live-Update)">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_CONFIG_CERTS[] PROGMEM = R"rawliteral(
<h2>Zertifikat-Management</h2>
<p>Hier können PEM-Zertifikate hochgeladen und die Dateinamen für die Dienste konfiguriert werden.</p>
<div class="group">
    <h3>Dateinamen zuweisen (Live-Update)</h3>
    <form action="/save_certs" method="POST">
        <label for="tankerkoenigCertFile">Tankerkönig Zertifikat (z.B. tanker.pem)</label>
        <input type="text" id="tankerkoenigCertFile" name="tankerkoenigCertFile" value="{tankerkoenigCertFile}">
        <label for="dartsCertFile">Darts-Ranking Zertifikat (z.B. darts.pem)</label>
        <input type="text" id="dartsCertFile" name="dartsCertFile" value="{dartsCertFile}">
        <label for="googleCertFile">Google Kalender Zertifikat (z.B. google.pem)</label>
        <input type="text" id="googleCertFile" name="googleCertFile" value="{googleCertFile}">
        <input type="submit" value="Dateinamen speichern (Live-Update)">
    </form>
</div>
<div class="group">
    <h3>Zertifikat hochladen</h3>
    <p>Die hochgeladene Datei wird im Verzeichnis <code>/certs/</code> gespeichert.</p>
    <form method='POST' action='/upload_cert' enctype='multipart/form-data'>
        <input type='file' name='upload' class="button" style="padding:0;width:auto;">
        <input type='submit' value='Datei hochladen'>
    </form>
</div>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_CONFIG_HARDWARE[] PROGMEM = R"rawliteral(
<h2>Optionale Hardware</h2>
<form action="/save_hardware" method="POST">
<div class="group">
    <h3>Mikrowellen-Sensor & Display-Steuerung</h3>
    <input type="checkbox" id="mwaveSensorEnabled" name="mwaveSensorEnabled" {mwaveSensorEnabled_checked}><label for="mwaveSensorEnabled" style="display:inline;">Sensor-gesteuerte Anzeige aktivieren</label>
    
    <h4>Pins</h4>
    <p style="color:#ff8c00;">Warnung: &Auml;nderungen an den Pins l&ouml;sen einen Neustart aus!</p>
    <label for="mwaveRxPin">Sensor RX Pin</label><input type="number" id="mwaveRxPin" name="mwaveRxPin" value="{mwaveRxPin}">
    <label for="mwaveTxPin">Sensor TX Pin</label><input type="number" id="mwaveTxPin" name="mwaveTxPin" value="{mwaveTxPin}">
    <label for="displayRelayPin">Display Relais Pin (255 = ungenutzt)</label><input type="number" id="displayRelayPin" name="displayRelayPin" value="{displayRelayPin}">

    <h4>Schwellenwerte</h4>
    <label for="mwaveOnCheckPercentage">Einschalt-Schwelle (ON-Anteil in % im schnellen Fenster)</label><input type="number" step="0.1" id="mwaveOnCheckPercentage" name="mwaveOnCheckPercentage" value="{mwaveOnCheckPercentage}">
    <label for="mwaveOnCheckDuration">Dauer schnelles Fenster (Sekunden)</label><input type="number" id="mwaveOnCheckDuration" name="mwaveOnCheckDuration" value="{mwaveOnCheckDuration}">
    <hr>
    <label for="mwaveOffCheckOnPercent">Ausschalt-Schwelle (max. ON-Anteil in % im langsamen Fenster)</label><input type="number" step="0.1" id="mwaveOffCheckOnPercent" name="mwaveOffCheckOnPercent" value="{mwaveOffCheckOnPercent}">
    <label for="mwaveOffCheckDuration">Dauer langsames Fenster (Sekunden)</label><input type="number" id="mwaveOffCheckDuration" name="mwaveOffCheckDuration" value="{mwaveOffCheckDuration}">
</div>
<input type="submit" value="Speichern">
</form>

<div class="group">
    <h3>Debug: Letzte 10 Zustandswechsel</h3>
    {debug_log_table}
</div>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

void handleFileUpload() {
    if (!server) return;
    HTTPUpload& upload = server->upload();
    if (upload.status == UPLOAD_FILE_START) {
        if (upload.filename.isEmpty()) { Serial.println("Upload: Dateiname ist leer."); return; }
        if (!LittleFS.exists("/certs")) LittleFS.mkdir("/certs");
        
        String filename_lowercase = upload.filename;
        filename_lowercase.toLowerCase();
        
        String filepath = "/certs/" + filename_lowercase;
        uploadFile = LittleFS.open(filepath, "w");
        
        if(uploadFile) Serial.printf("Upload Start: %s\n", filepath.c_str());
        else Serial.printf("Upload Fehler: Konnte Datei nicht erstellen: %s\n", filepath.c_str());

    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
        if (uploadFile) uploadFile.close();
        Serial.printf("Upload Ende: %s, %u Bytes\n", upload.filename.c_str(), upload.totalSize);
    }
}
void handleUploadSuccess() {
    if (!server) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    // --- KORREKTUR: Fehlendes " in href und ; am Ende ---
    page += "<h1>Upload erfolgreich!</h1><p>Die Datei wurde gespeichert. Bitte trage den Dateinamen nun oben ein und speichere die Konfiguration.</p><script>setTimeout(function(){ window.location.href = '/config_certs'; }, 2000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}
void handleConfigCerts() {
    if (!server || !deviceConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_CERTS);
    replaceAll(content, "{tankerkoenigCertFile}", deviceConfig->tankerkoenigCertFile.c_str());
    replaceAll(content, "{dartsCertFile}", deviceConfig->dartsCertFile.c_str());
    replaceAll(content, "{googleCertFile}", deviceConfig->googleCertFile.c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}
void handleSaveCerts() {
    if (!server || !deviceConfig || !webClient) return;
    String tankerCert = server->arg("tankerkoenigCertFile");
    tankerCert.toLowerCase();
    deviceConfig->tankerkoenigCertFile = tankerCert.c_str();

    String dartsCert = server->arg("dartsCertFile");
    dartsCert.toLowerCase();
    deviceConfig->dartsCertFile = dartsCert.c_str();

    String googleCert = server->arg("googleCertFile");
    googleCert.toLowerCase();
    deviceConfig->googleCertFile = googleCert.c_str();
    
    saveDeviceConfig();

    webClient->updateResourceCertificateByHost("dartsrankings.com", deviceConfig->dartsCertFile.c_str());
    webClient->updateResourceCertificateByHost("creativecommons.tankerkoenig.de", deviceConfig->tankerkoenigCertFile.c_str());
    webClient->updateResourceCertificateByHost("google.com", deviceConfig->googleCertFile.c_str());
    
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Zertifikats-Konfiguration live &uuml;bernommen!</p><script>setTimeout(function(){ window.location.href = '/config_certs'; }, 2000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

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
    replaceAll(content, "{R1}", String(hardwareConfig->R1).c_str());
    replaceAll(content, "{G1}", String(hardwareConfig->G1).c_str());
    replaceAll(content, "{B1}", String(hardwareConfig->B1).c_str());
    replaceAll(content, "{R2}", String(hardwareConfig->R2).c_str());
    replaceAll(content, "{G2}", String(hardwareConfig->G2).c_str());
    replaceAll(content, "{B2}", String(hardwareConfig->B2).c_str());
    replaceAll(content, "{A}", String(hardwareConfig->A).c_str());
    replaceAll(content, "{B}", String(hardwareConfig->B).c_str());
    replaceAll(content, "{C}", String(hardwareConfig->C).c_str());
    replaceAll(content, "{D}", String(hardwareConfig->D).c_str());
    replaceAll(content, "{E}", String(hardwareConfig->E).c_str());
    replaceAll(content, "{CLK}", String(hardwareConfig->CLK).c_str());
    replaceAll(content, "{LAT}", String(hardwareConfig->LAT).c_str());
    replaceAll(content, "{OE}", String(hardwareConfig->OE).c_str());
    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}
void handleConfigModules() {
    if (!server || !deviceConfig) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_MODULES);
    PsramString tz_options_html;
    for (const auto& tz : timezones) {
        tz_options_html += "<option value=\"";
        tz_options_html += tz.second;
        tz_options_html += "\"";
        if (deviceConfig->timezone == tz.second) tz_options_html += " selected";
        tz_options_html += ">";
        tz_options_html += tz.first;
        tz_options_html += "</option>";
    }
    replaceAll(content, "{tz_options}", tz_options_html.c_str());
    replaceAll(content, "{tankerApiKey}", deviceConfig->tankerApiKey.c_str());
    replaceAll(content, "{stationId}", deviceConfig->stationId.c_str());
    replaceAll(content, "{stationFetchIntervalMin}", String(deviceConfig->stationFetchIntervalMin).c_str());
    replaceAll(content, "{stationDisplaySec}", String(deviceConfig->stationDisplaySec).c_str());
    replaceAll(content, "{icsUrl}", deviceConfig->icsUrl.c_str());
    replaceAll(content, "{calendarFetchIntervalMin}", String(deviceConfig->calendarFetchIntervalMin).c_str());
    replaceAll(content, "{calendarDisplaySec}", String(deviceConfig->calendarDisplaySec).c_str());
    replaceAll(content, "{calendarScrollMs}", String(deviceConfig->calendarScrollMs).c_str());
    replaceAll(content, "{calendarDateColor}", deviceConfig->calendarDateColor.c_str());
    replaceAll(content, "{calendarTextColor}", deviceConfig->calendarTextColor.c_str());

    replaceAll(content, "{dartsOomEnabled_checked}", deviceConfig->dartsOomEnabled ? "checked" : "");
    replaceAll(content, "{dartsProTourEnabled_checked}", deviceConfig->dartsProTourEnabled ? "checked" : "");
    replaceAll(content, "{dartsDisplaySec}", String(deviceConfig->dartsDisplaySec).c_str());
    replaceAll(content, "{trackedDartsPlayers}", deviceConfig->trackedDartsPlayers.c_str());

    replaceAll(content, "{fritzboxEnabled_checked}", deviceConfig->fritzboxEnabled ? "checked" : "");
    replaceAll(content, "{fritzboxIp}", deviceConfig->fritzboxIp.c_str());

    page += content;
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleConfigHardware() {
    if (!server || !deviceConfig || !hardwareConfig || !mwaveSensorModule || !timeConverter) return;
    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    PsramString content = (const char*)FPSTR(HTML_CONFIG_HARDWARE);

    replaceAll(content, "{mwaveSensorEnabled_checked}", deviceConfig->mwaveSensorEnabled ? "checked" : "");
    replaceAll(content, "{mwaveRxPin}", String(hardwareConfig->mwaveRxPin).c_str());
    replaceAll(content, "{mwaveTxPin}", String(hardwareConfig->mwaveTxPin).c_str());
    replaceAll(content, "{displayRelayPin}", String(hardwareConfig->displayRelayPin).c_str());
    replaceAll(content, "{mwaveOnCheckPercentage}", String(deviceConfig->mwaveOnCheckPercentage).c_str());
    replaceAll(content, "{mwaveOnCheckDuration}", String(deviceConfig->mwaveOnCheckDuration).c_str());
    replaceAll(content, "{mwaveOffCheckOnPercent}", String(deviceConfig->mwaveOffCheckOnPercent).c_str());
    replaceAll(content, "{mwaveOffCheckDuration}", String(deviceConfig->mwaveOffCheckDuration).c_str());

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
            
            table_html += "<tr><td>";
            table_html += time_str;
            table_html += "</td><td>";
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
        // --- KORREKTUR: Fehlendes " in href und ; am Ende ---
        page += "<h1>Gespeichert!</h1><p>Hardware-Konfiguration gespeichert. Das Ger&auml;t wird neu gestartet, um die &Auml;nderungen zu &uuml;bernehmen...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
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


void handleSaveBase() {
    if (!server || !deviceConfig || !hardwareConfig) return;
    deviceConfig->hostname = server->arg("hostname").c_str();
    deviceConfig->ssid = server->arg("ssid").c_str();
    if (server->hasArg("password") && server->arg("password").length() > 0) deviceConfig->password = server->arg("password").c_str();
    if (server->hasArg("otaPassword") && server->arg("otaPassword").length() > 0) deviceConfig->otaPassword = server->arg("otaPassword").c_str();
    saveDeviceConfig();

    hardwareConfig->R1 = server->arg("R1").toInt();
    hardwareConfig->G1 = server->arg("G1").toInt();
    hardwareConfig->B1 = server->arg("B1").toInt();
    hardwareConfig->R2 = server->arg("R2").toInt();
    hardwareConfig->G2 = server->arg("G2").toInt();
    hardwareConfig->B2 = server->arg("B2").toInt();
    hardwareConfig->A = server->arg("A").toInt();
    hardwareConfig->B = server->arg("B").toInt();
    hardwareConfig->C = server->arg("C").toInt();
    hardwareConfig->D = server->arg("D").toInt();
    hardwareConfig->E = server->arg("E").toInt();
    hardwareConfig->CLK = server->arg("CLK").toInt();
    hardwareConfig->LAT = server->arg("LAT").toInt();
    hardwareConfig->OE = server->arg("OE").toInt();
    saveHardwareConfig();

    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Grundkonfiguration gespeichert. Das Ger&auml;t wird neu gestartet...</p><script>setTimeout(function(){ window.location.href = '/'; }, 3000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
    delay(1000);
    ESP.restart();
}
void handleSaveModules() {
    if (!server || !deviceConfig) return;
    deviceConfig->timezone = server->arg("timezone").c_str();
    deviceConfig->tankerApiKey = server->arg("tankerApiKey").c_str();
    deviceConfig->stationId = server->arg("stationId").c_str();
    deviceConfig->stationFetchIntervalMin = server->arg("stationFetchIntervalMin").toInt();
    deviceConfig->stationDisplaySec = server->arg("stationDisplaySec").toInt();
    deviceConfig->icsUrl = server->arg("icsUrl").c_str();
    deviceConfig->calendarFetchIntervalMin = server->arg("calendarFetchIntervalMin").toInt();
    deviceConfig->calendarDisplaySec = server->arg("calendarDisplaySec").toInt();
    deviceConfig->calendarScrollMs = server->arg("calendarScrollMs").toInt();
    deviceConfig->calendarDateColor = server->arg("calendarDateColor").c_str();
    deviceConfig->calendarTextColor = server->arg("calendarTextColor").c_str();
    
    deviceConfig->dartsOomEnabled = server->hasArg("dartsOomEnabled");
    deviceConfig->dartsProTourEnabled = server->hasArg("dartsProTourEnabled");
    deviceConfig->dartsDisplaySec = server->arg("dartsDisplaySec").toInt();
    deviceConfig->trackedDartsPlayers = server->arg("trackedDartsPlayers").c_str();

    deviceConfig->fritzboxEnabled = server->hasArg("fritzboxEnabled");
    if (server->hasArg("fritzboxIp") && server->arg("fritzboxIp").length() > 0) {
        deviceConfig->fritzboxIp = server->arg("fritzboxIp").c_str();
    } else {
        deviceConfig->fritzboxIp = WiFi.gatewayIP().toString().c_str();
    }

    saveDeviceConfig();
    applyLiveConfig();

    PsramString page = (const char*)FPSTR(HTML_PAGE_HEADER);
    page += "<h1>Gespeichert!</h1><p>Modul-Konfiguration live &uuml;bernommen!</p><script>setTimeout(function(){ window.location.href = '/config_modules'; }, 2000);</script>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server->send(200, "text/html", page.c_str());
}

void handleNotFound();

void setupWebServer(bool portalMode) {
    if (!server || !dnsServer) return;
    if (portalMode) {
        dnsServer->start(53, "*", WiFi.softAPIP());
    }
    server->on("/", HTTP_GET, handleRoot);
    server->on("/config_base", HTTP_GET, handleConfigBase);
    server->on("/config_modules", HTTP_GET, handleConfigModules);
    server->on("/save_base", HTTP_POST, handleSaveBase);
    server->on("/save_modules", HTTP_POST, handleSaveModules);
    
    server->on("/config_certs", HTTP_GET, handleConfigCerts);
    server->on("/save_certs", HTTP_POST, handleSaveCerts);
    server->on("/upload_cert", HTTP_POST, handleUploadSuccess, handleFileUpload);
    
    server->on("/config_hardware", HTTP_GET, handleConfigHardware);
    server->on("/save_hardware", HTTP_POST, handleSaveHardware);
    
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

#endif