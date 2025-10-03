#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Struktur für die Zeitzonen-Optionen im Dropdown
struct TimezoneOption {
  const char* posix;
  const char* friendlyName;
};

// Liste der verfügbaren Zeitzonen
const TimezoneOption timezones[] = {
  { "UTC", "UTC" },
  { "CET-1CEST-2,M3.5.0/2,M10.5.0/3", "Europe/Berlin" },
  { "EET-2EEST-3,M3.5.0/3,M10.5.0/4", "Europe/Helsinki" },
  { "WET0WEST-1,M3.5.0/1,M10.5.0/1", "Europe/Lisbon" },
  { "GMT0BST-1,M3.5.0/1,M10.5.0/2", "Europe/London" },
  { "EST5EDT,M3.2.0,M11.1.0", "America/New_York" },
  { "CST6CDT,M3.2.0,M11.1.0", "America/Chicago" },
  { "MST7MDT,M3.2.0,M11.1.0", "America/Denver" },
  { "PST8PDT,M3.2.0,M11.1.0", "America/Los_Angeles" },
  { "JST-9", "Asia/Tokyo" }
};

// Geräteeinstellungen
struct DeviceConfig {
  String ssid;
  String password;
  String hostname;
  String otaPassword;
  String tankerApiKey;
  String stationId;
  String icsUrl;
  uint16_t calendarFetchIntervalMin;
  uint16_t calendarScrollMs;
  String calendarDateColor;
  String calendarTextColor;
  uint16_t calendarDisplaySec;
  uint16_t stationDisplaySec;
  String timezone;
};

DeviceConfig deviceConfig;
WebServer server(80);
DNSServer dnsServer;
extern bool portalRunning; // Definiert in Panelclock.ino

// Lade- und Speicherfunktionen für die Konfiguration
void loadDeviceConfig() {
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    
    // MODERNE JSON-SYNTAX
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, configFile);
    configFile.close();

    if (error) {
        Serial.println(F("Fehler beim Parsen der Konfigurationsdatei."));
        // Setze Standardwerte, um einen definierten Zustand zu haben
        deviceConfig = DeviceConfig();
        deviceConfig.hostname = "Panelclock";
        deviceConfig.calendarFetchIntervalMin = 15;
        deviceConfig.calendarScrollMs = 150;
        deviceConfig.calendarDateColor = "#FFFF00";
        deviceConfig.calendarTextColor = "#FFFFFF";
        deviceConfig.calendarDisplaySec = 30;
        deviceConfig.stationDisplaySec = 30;
        deviceConfig.timezone = "CET-1CEST-2,M3.5.0/2,M10.5.0/3";
        return;
    }

    deviceConfig.ssid = doc["ssid"].as<String>();
    deviceConfig.password = doc["password"].as<String>();
    deviceConfig.hostname = doc["hostname"] | "Panelclock";
    deviceConfig.otaPassword = doc["otaPassword"].as<String>();
    deviceConfig.tankerApiKey = doc["tankerApiKey"].as<String>();
    deviceConfig.stationId = doc["stationId"].as<String>();
    deviceConfig.icsUrl = doc["icsUrl"].as<String>();
    deviceConfig.calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | 15;
    deviceConfig.calendarScrollMs = doc["calendarScrollMs"] | 150;
    deviceConfig.calendarDateColor = doc["calendarDateColor"] | "#FFFF00";
    deviceConfig.calendarTextColor = doc["calendarTextColor"] | "#FFFFFF";
    deviceConfig.calendarDisplaySec = doc["calendarDisplaySec"] | 30;
    deviceConfig.stationDisplaySec = doc["stationDisplaySec"] | 30;
    deviceConfig.timezone = doc["timezone"] | "CET-1CEST-2,M3.5.0/2,M10.5.0/3";
  } else {
    // Wenn keine Datei existiert, setze ebenfalls Standardwerte
    deviceConfig = DeviceConfig();
    deviceConfig.hostname = "Panelclock";
    deviceConfig.calendarFetchIntervalMin = 15;
    deviceConfig.calendarScrollMs = 150;
    deviceConfig.calendarDateColor = "#FFFF00";
    deviceConfig.calendarTextColor = "#FFFFFF";
    deviceConfig.calendarDisplaySec = 30;
    deviceConfig.stationDisplaySec = 30;
    deviceConfig.timezone = "CET-1CEST-2,M3.5.0/2,M10.5.0/3";
  }
}

void saveDeviceConfig() {
  // MODERNE JSON-SYNTAX
  JsonDocument doc;
  
  doc["ssid"] = deviceConfig.ssid;
  doc["password"] = deviceConfig.password;
  doc["hostname"] = deviceConfig.hostname;
  doc["otaPassword"] = deviceConfig.otaPassword;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["icsUrl"] = deviceConfig.icsUrl;
  doc["calendarFetchIntervalMin"] = deviceConfig.calendarFetchIntervalMin;
  doc["calendarScrollMs"] = deviceConfig.calendarScrollMs;
  doc["calendarDateColor"] = deviceConfig.calendarDateColor;
  doc["calendarTextColor"] = deviceConfig.calendarTextColor;
  doc["calendarDisplaySec"] = deviceConfig.calendarDisplaySec;
  doc["stationDisplaySec"] = deviceConfig.stationDisplaySec;
  doc["timezone"] = deviceConfig.timezone;

  File configFile = LittleFS.open("/config.json", "w");
  if (configFile) {
    serializeJson(doc, configFile);
    configFile.close();
  } else {
    Serial.println(F("Fehler beim Öffnen der Konfigurationsdatei zum Schreiben."));
  }
}

// Handler für das Speichern der Konfiguration
void handleSave() {
  server.send(200, "text/html", "Einstellungen gespeichert.<br>Gerät startet in 3 Sekunden neu...");
  
  deviceConfig.ssid = server.arg("ssid");
  deviceConfig.password = server.arg("password");
  deviceConfig.hostname = server.arg("hostname");
  deviceConfig.otaPassword = server.arg("otaPassword");
  deviceConfig.timezone = server.arg("timezone");
  deviceConfig.stationDisplaySec = server.arg("stationDisplaySec").toInt();
  deviceConfig.calendarDisplaySec = server.arg("calendarDisplaySec").toInt();
  deviceConfig.tankerApiKey = server.arg("tankerApiKey");
  deviceConfig.stationId = server.arg("stationId");
  deviceConfig.icsUrl = server.arg("icsUrl");
  deviceConfig.calendarFetchIntervalMin = server.arg("calendarFetchIntervalMin").toInt();
  deviceConfig.calendarScrollMs = server.arg("calendarScrollMs").toInt();
  deviceConfig.calendarDateColor = server.arg("calendarDateColor");
  deviceConfig.calendarTextColor = server.arg("calendarTextColor");
  
  saveDeviceConfig();
  delay(3000);
  ESP.restart();
}

// Handler für die Hauptseite (liefert das HTML-Formular aus)
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE HTML><html><head>
<title>Panelclock Konfiguration</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<style>
  body { font-family: Arial, sans-serif; background-color: #282c34; color: #abb2bf; }
  form { background-color: #353a45; padding: 20px; border-radius: 10px; max-width: 600px; margin: 20px auto; }
  h2, h3 { text-align: center; color: #61afef; border-bottom: 1px solid #4f5666; padding-bottom: 10px; }
  label { display: block; margin-top: 15px; color: #98c379; }
  input, select { width: 100%; padding: 8px; margin-top: 5px; border-radius: 5px; border: 1px solid #4f5666; background-color: #2c313a; color: #abb2bf; box-sizing: border-box; }
  button { width: 100%; padding: 10px; margin-top: 20px; border: none; border-radius: 5px; background-color: #61afef; color: #282c34; font-size: 16px; cursor: pointer; }
  button:hover { background-color: #528bce; }
  .section { margin-top: 20px; }
</style>
</head><body>
<form action="/save" method="POST">
  <h2>Panelclock Konfiguration</h2>
  
  <div class="section">
    <h3>WiFi & Allgemein</h3>
    <label for="ssid">WLAN Name (SSID)</label>
    <input type="text" id="ssid" name="ssid" value=")rawliteral";
  html += deviceConfig.ssid;
  html += R"rawliteral(">
    <label for="password">WLAN Passwort</label>
    <input type="password" id="password" name="password" value=")rawliteral";
  html += deviceConfig.password;
  html += R"rawliteral(">
    <label for="hostname">Hostname</label>
    <input type="text" id="hostname" name="hostname" value=")rawliteral";
  html += deviceConfig.hostname;
  html += R"rawliteral(">
    <label for="otaPassword">OTA Update Passwort</label>
    <input type="password" id="otaPassword" name="otaPassword" value=")rawliteral";
  html += deviceConfig.otaPassword;
  html += R"rawliteral(">
  </div>

  <div class="section">
    <h3>Anzeige & Zeitzone</h3>
    <label for="timezone">Zeitzone</label>
    <select id="timezone" name="timezone">)rawliteral";
  
  for (const auto& tz : timezones) {
    html += "<option value=\"" + String(tz.posix) + "\"";
    if (String(tz.posix) == deviceConfig.timezone) {
      html += " selected";
    }
    html += ">" + String(tz.friendlyName) + "</option>";
  }

  html += R"rawliteral(
    </select>
    <label for="stationDisplaySec">Anzeigedauer Tankpreise (Sekunden)</label>
    <input type="number" id="stationDisplaySec" name="stationDisplaySec" value=")rawliteral";
  html += String(deviceConfig.stationDisplaySec);
  html += R"rawliteral(">
    <label for="calendarDisplaySec">Anzeigedauer Kalender (Sekunden)</label>
    <input type="number" id="calendarDisplaySec" name="calendarDisplaySec" value=")rawliteral";
  html += String(deviceConfig.calendarDisplaySec);
  html += R"rawliteral(">
  </div>

  <div class="section">
    <h3>Tankerkönig</h3>
    <label for="tankerApiKey">API Key</label>
    <input type="text" id="tankerApiKey" name="tankerApiKey" value=")rawliteral";
  html += deviceConfig.tankerApiKey;
  html += R"rawliteral(">
    <label for="stationId">Tankstellen-ID</label>
    <input type="text" id="stationId" name="stationId" value=")rawliteral";
  html += deviceConfig.stationId;
  html += R"rawliteral(">
  </div>

  <div class="section">
    <h3>Kalender (iCal)</h3>
    <label for="icsUrl">ICS URL</label>
    <input type="text" id="icsUrl" name="icsUrl" value=")rawliteral";
  html += deviceConfig.icsUrl;
  html += R"rawliteral(">
    <label for="calendarFetchIntervalMin">Abrufintervall (Minuten)</label>
    <input type="number" id="calendarFetchIntervalMin" name="calendarFetchIntervalMin" value=")rawliteral";
  html += String(deviceConfig.calendarFetchIntervalMin);
  html += R"rawliteral(">
    <label for="calendarScrollMs">Scroll-Geschwindigkeit (ms)</label>
    <input type="number" id="calendarScrollMs" name="calendarScrollMs" value=")rawliteral";
  html += String(deviceConfig.calendarScrollMs);
  html += R"rawliteral(">
    <label for="calendarDateColor">Farbe Datum (z.B. #FFFF00)</label>
    <input type="text" id="calendarDateColor" name="calendarDateColor" value=")rawliteral";
  html += deviceConfig.calendarDateColor;
  html += R"rawliteral(">
    <label for="calendarTextColor">Farbe Text (z.B. #FFFFFF)</label>
    <input type="text" id="calendarTextColor" name="calendarTextColor" value=")rawliteral";
  html += deviceConfig.calendarTextColor;
  html += R"rawliteral(">
  </div>
  
  <button type="submit">Speichern & Neustarten</button>
</form>
</body></html>)rawliteral";

  server.send(200, "text/html", html);
}

// Startet den Webserver und definiert die Routen
void setupWebServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  // Leite alle anderen Anfragen auf die Hauptseite um (für Captive Portal)
  server.onNotFound([]() {
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
}

// Startet den Konfigurations-Access-Point
void startConfigPortal() {
  const char* ap_ssid = "Panelclock-Setup";
  
  // Extern deklarierte Funktion aus Panelclock.ino, um Status auf dem Display anzuzeigen
  void displayStatus(const char* msg); 
  displayStatus(ap_ssid);

  WiFi.softAP(ap_ssid);
  IPAddress apIP = WiFi.softAPIP();
  
  // Starte den DNS-Server nur im Portal-Modus
  dnsServer.start(53, "*", apIP);
}

// Behandelt Web-Anfragen im Loop
void handleWebServer() {
  // Verarbeite DNS-Anfragen nur im Portal-Modus
  if (portalRunning) {
    dnsServer.processNextRequest();
  }
  server.handleClient();
}

#endif