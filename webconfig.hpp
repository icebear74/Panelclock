#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

extern void applyLiveConfig();
inline void _portal_handleRoot();
inline void _portal_handleSave();

struct DeviceConfig {
  String ssid;
  String password;
  String hostname;
  String tankerApiKey;
  String stationId;
  String otaPassword;
  String icsUrl;
  uint32_t switchIntervalSec = 30;
  uint32_t calendarFetchIntervalMin = 5;
  uint32_t calendarScrollMs = 150;
  uint32_t calendarDisplaySec = 30;
  uint32_t stationDisplaySec = 30;
  String calendarDateColor = "#FFE000";
  String calendarTextColor = "#FFFFFF";
  String timezone = "CET-1CEST,M3.5.0,M10.5.0/3"; // <--- Berlin Default NEU
};

static DeviceConfig deviceConfig;
static bool configPortalActive = false;
static WebServer portalServer(80);

inline bool ensureLittleFSMounted() {
  return LittleFS.begin(true);
}

inline void loadDeviceConfig() {
  if (!ensureLittleFSMounted()) return;
  File f = LittleFS.open("/device_config.json", "r");
  if (!f) return;
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  deviceConfig.ssid        = String((const char*)(doc["ssid"] | ""));
  deviceConfig.password    = String((const char*)(doc["password"] | ""));
  deviceConfig.hostname    = String((const char*)(doc["hostname"] | ""));
  deviceConfig.tankerApiKey= String((const char*)(doc["tankerApiKey"] | ""));
  deviceConfig.stationId   = String((const char*)(doc["stationId"] | ""));
  deviceConfig.otaPassword = String((const char*)(doc["otaPassword"] | ""));
  deviceConfig.icsUrl      = String((const char*)(doc["icsUrl"] | ""));
  deviceConfig.switchIntervalSec = doc["switchIntervalSec"] | 30;
  deviceConfig.calendarFetchIntervalMin = doc["calendarFetchIntervalMin"] | 5;
  deviceConfig.calendarScrollMs = doc["calendarScrollMs"] | 150;
  deviceConfig.calendarDisplaySec = doc["calendarDisplaySec"] | 30;
  deviceConfig.stationDisplaySec = doc["stationDisplaySec"] | 30;
  deviceConfig.calendarDateColor = String((const char*)(doc["calendarDateColor"] | "#FFE000"));
  deviceConfig.calendarTextColor = String((const char*)(doc["calendarTextColor"] | "#FFFFFF"));
  deviceConfig.timezone         = String((const char*)(doc["timezone"] | "CET-1CEST,M3.5.0,M10.5.0/3")); // NEU
}

inline bool saveDeviceConfig() {
  if (!ensureLittleFSMounted()) return false;
  StaticJsonDocument<1024> doc;
  doc["ssid"] = deviceConfig.ssid;
  doc["password"] = deviceConfig.password;
  doc["hostname"] = deviceConfig.hostname;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["otaPassword"] = deviceConfig.otaPassword;
  doc["icsUrl"] = deviceConfig.icsUrl;
  doc["switchIntervalSec"] = deviceConfig.switchIntervalSec;
  doc["calendarFetchIntervalMin"] = deviceConfig.calendarFetchIntervalMin;
  doc["calendarScrollMs"] = deviceConfig.calendarScrollMs;
  doc["calendarDisplaySec"] = deviceConfig.calendarDisplaySec;
  doc["stationDisplaySec"] = deviceConfig.stationDisplaySec;
  doc["calendarDateColor"] = deviceConfig.calendarDateColor;
  doc["calendarTextColor"] = deviceConfig.calendarTextColor;
  doc["timezone"] = deviceConfig.timezone; // NEU
  File f = LittleFS.open("/device_config.json", "w");
  if (!f) return false;
  if (serializeJson(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  return true;
}

inline bool verifySavedConfig() {
  if (!ensureLittleFSMounted()) return false;
  File f = LittleFS.open("/device_config.json", "r");
  if (!f) return false;
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  String ssid = String((const char*)(doc["ssid"] | ""));
  String passwd = String((const char*)(doc["password"] | ""));
  String host = String((const char*)(doc["hostname"] | ""));
  String tanker = String((const char*)(doc["tankerApiKey"] | ""));
  String station = String((const char*)(doc["stationId"] | ""));
  String ota = String((const char*)(doc["otaPassword"] | ""));
  String icsUrl = String((const char*)(doc["icsUrl"] | ""));
  uint32_t swInt = doc["switchIntervalSec"] | 30;
  uint32_t calFetch = doc["calendarFetchIntervalMin"] | 5;
  uint32_t calScroll = doc["calendarScrollMs"] | 150;
  uint32_t calDisp = doc["calendarDisplaySec"] | 30;
  uint32_t staDisp = doc["stationDisplaySec"] | 30;
  String calDateColor = String((const char*)(doc["calendarDateColor"] | "#FFE000"));
  String calTextColor = String((const char*)(doc["calendarTextColor"] | "#FFFFFF"));
  String tz = String((const char*)(doc["timezone"] | "CET-1CEST,M3.5.0,M10.5.0/3")); // NEU
  return (ssid == deviceConfig.ssid) &&
         (passwd == deviceConfig.password) &&
         (host == deviceConfig.hostname) &&
         (tanker == deviceConfig.tankerApiKey) &&
         (station == deviceConfig.stationId) &&
         (ota == deviceConfig.otaPassword) &&
         (icsUrl == deviceConfig.icsUrl) &&
         (swInt == deviceConfig.switchIntervalSec) &&
         (calFetch == deviceConfig.calendarFetchIntervalMin) &&
         (calScroll == deviceConfig.calendarScrollMs) &&
         (calDisp == deviceConfig.calendarDisplaySec) &&
         (staDisp == deviceConfig.stationDisplaySec) &&
         (calDateColor == deviceConfig.calendarDateColor) &&
         (calTextColor == deviceConfig.calendarTextColor) &&
         (tz == deviceConfig.timezone);
}

inline String getNetworksOptionsHtml() {
  String html;
  int n = WiFi.scanNetworks();
  if (n <= 0) { html += "<option value=\"\">(keine Netzwerke gefunden)</option>"; return html; }
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    bool enc = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    String esc = ssid; esc.replace("'", "&#39;");
    String label = esc + " (" + String(rssi) + " dBm";
    if (enc) label += ", gesichert";
    label += ")";
    html += "<option value='" + esc + "'>" + label + "</option>";
  }
  return html;
}

inline String portalPageHtml() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>Panelclock Setup</title></head><body>";
  html += "<h2>Panelclock Konfiguration</h2>";
  html += "<form action=\"/save\" method=\"post\">";
  html += "WLAN SSID:<br>";
  html += "<select name='ssid_select' id='ssid_select' onchange=\"document.getElementById('ssid').value=this.value\">";
  html += "<option value=''>-- Netzwerk wählen --</option>";
  html += getNetworksOptionsHtml();
  html += "</select><br>";
  html += "<input type='text' id='ssid' name='ssid' placeholder='SSID' value='" + deviceConfig.ssid + "' style='width:300px'><br>";
  html += "WLAN Passwort:<br><input type='password' name='password' value='" + deviceConfig.password + "' style='width:300px'><br>";
  html += "Hostname:<br><input name='hostname' value='" + deviceConfig.hostname + "' style='width:300px'><br>";
  html += "Tankerkoenig API Key:<br><input name='tankerApiKey' value='" + deviceConfig.tankerApiKey + "' style='width:300px'><br>";
  html += "Station ID:<br><input name='stationId' value='" + deviceConfig.stationId + "' style='width:300px'><br>";
  html += "OTA Passwort:<br><input name='otaPassword' value='" + deviceConfig.otaPassword + "' style='width:300px'><br>";
  html += "Google Kalender ICS-URL:<br><input name='icsUrl' value='" + deviceConfig.icsUrl + "' style='width:300px'><br>";
  html += "Zeitzone (TZ-String, z.B. CET-1CEST,M3.5.0,M10.5.0/3):<br><input name='timezone' value='" + deviceConfig.timezone + "' style='width:300px'><br>"; // NEU
  html += "Anzeigedauer Kalender (Sekunden):<br><input name='calendarDisplaySec' type='number' min='5' max='600' value='" + String(deviceConfig.calendarDisplaySec) + "' style='width:80px'><br>";
  html += "Anzeigedauer Tankstelle (Sekunden):<br><input name='stationDisplaySec' type='number' min='5' max='600' value='" + String(deviceConfig.stationDisplaySec) + "' style='width:80px'><br>";
  html += "Wechselintervall (Sekunden, deprecated):<br><input name='switchIntervalSec' type='number' min='5' max='600' value='" + String(deviceConfig.switchIntervalSec) + "' style='width:80px'><br>";
  html += "Kalender-Update-Intervall (Minuten):<br><input name='calendarFetchIntervalMin' type='number' min='1' max='120' value='" + String(deviceConfig.calendarFetchIntervalMin) + "' style='width:80px'><br>";
  html += "Scrollgeschwindigkeit Kalender (ms pro Schritt, 0 = aus):<br><input name='calendarScrollMs' type='number' min='0' max='2000' value='" + String(deviceConfig.calendarScrollMs) + "' style='width:80px'><br>";
  html += "Kalender-Datum-Farbe:<br><input name='calendarDateColor' type='color' value='" + deviceConfig.calendarDateColor + "' style='width:80px'><br>";
  html += "Kalender-Text-Farbe:<br><input name='calendarTextColor' type='color' value='" + deviceConfig.calendarTextColor + "' style='width:80px'><br>";
  html += "<br><input type='submit' value='Speichern'></form>";
  html += "<p>Nach Speichern werden die meisten Änderungen sofort aktiv.<br>Nur bei WLAN/OTA/Hostname ist ein Neustart nötig.</p>";
  html += "<p>Hinweis: Wenn das gewünschte WLAN nicht angezeigt wird, tragen Sie die SSID bitte manuell ein.</p>";
  html += "</body></html>";
  return html;
}

inline void _portal_handleRoot() { String page = portalPageHtml(); portalServer.send(200, "text/html", page); }

inline void _portal_handleSave() {
  String oldSsid = deviceConfig.ssid;
  String oldPwd = deviceConfig.password;
  String oldHost = deviceConfig.hostname;
  String oldOta = deviceConfig.otaPassword;

  if (portalServer.hasArg("ssid_select") && portalServer.arg("ssid_select").length() > 0) deviceConfig.ssid = portalServer.arg("ssid_select");
  if (portalServer.hasArg("ssid")) deviceConfig.ssid = portalServer.arg("ssid");
  if (portalServer.hasArg("password")) deviceConfig.password = portalServer.arg("password");
  if (portalServer.hasArg("hostname")) deviceConfig.hostname = portalServer.arg("hostname");
  if (portalServer.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = portalServer.arg("tankerApiKey");
  if (portalServer.hasArg("stationId")) deviceConfig.stationId = portalServer.arg("stationId");
  if (portalServer.hasArg("otaPassword")) deviceConfig.otaPassword = portalServer.arg("otaPassword");
  if (portalServer.hasArg("icsUrl")) deviceConfig.icsUrl = portalServer.arg("icsUrl");
  if (portalServer.hasArg("timezone")) deviceConfig.timezone = portalServer.arg("timezone"); // NEU
  if (portalServer.hasArg("switchIntervalSec")) deviceConfig.switchIntervalSec = portalServer.arg("switchIntervalSec").toInt();
  if (portalServer.hasArg("calendarFetchIntervalMin")) deviceConfig.calendarFetchIntervalMin = portalServer.arg("calendarFetchIntervalMin").toInt();
  if (portalServer.hasArg("calendarScrollMs")) deviceConfig.calendarScrollMs = portalServer.arg("calendarScrollMs").toInt();
  if (portalServer.hasArg("calendarDisplaySec")) deviceConfig.calendarDisplaySec = portalServer.arg("calendarDisplaySec").toInt();
  if (portalServer.hasArg("stationDisplaySec")) deviceConfig.stationDisplaySec = portalServer.arg("stationDisplaySec").toInt();
  if (portalServer.hasArg("calendarDateColor")) deviceConfig.calendarDateColor = portalServer.arg("calendarDateColor");
  if (portalServer.hasArg("calendarTextColor")) deviceConfig.calendarTextColor = portalServer.arg("calendarTextColor");

  bool ok = saveDeviceConfig();
  if (!ok) { portalServer.send(500, "text/plain", "Fehler: Konfigurationsdatei konnte nicht geschrieben werden (LittleFS Fehler).\n"); return; }
  bool verified = verifySavedConfig();
  if (!verified) { portalServer.send(500, "text/plain", "Fehler: Gespeicherte Konfiguration konnte nicht verifiziert werden. Schreib-/Leseproblem.\n"); return; }

  bool needsRestart = (deviceConfig.ssid != oldSsid) || (deviceConfig.password != oldPwd) || (deviceConfig.hostname != oldHost) || (deviceConfig.otaPassword != oldOta);

  if (needsRestart) {
    portalServer.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='2;url=/'></head><body><h3>Gespeichert</h3><p>Gerät startet neu...</p></body></html>");
    delay(600);
    ESP.restart();
  } else {
    applyLiveConfig();
    portalServer.send(200, "text/html", "<html><head><meta http-equiv='refresh' content='3;url=/'></head><body><h3>Gespeichert</h3><p>Änderungen übernommen! Zurück zur Hauptseite...</p></body></html>");
  }
}

inline void startConfigPortalIfNeeded(bool force = false) {
  loadDeviceConfig();
  if (configPortalActive) { Serial.println("Config portal already active"); return; }
  if (!force && WiFi.status() == WL_CONNECTED) {
    portalServer.on("/", HTTP_GET, _portal_handleRoot);
    portalServer.on("/save", HTTP_POST, _portal_handleSave);
    portalServer.onNotFound([](){ portalServer.send(404, "text/plain", "Not found"); });
    portalServer.begin();
    configPortalActive = true;
    Serial.printf("Config portal started on STA IP: %s\n", WiFi.localIP().toString().c_str());
    return;
  }
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.scanNetworks();
  delay(200);
  const char *apName = "Panelclock-Setup";
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName);
  portalServer.on("/", HTTP_GET, _portal_handleRoot);
  portalServer.on("/save", HTTP_POST, _portal_handleSave);
  portalServer.onNotFound([](){ portalServer.send(404, "text/plain", "Not found"); });
  portalServer.begin();
  configPortalActive = true;
  Serial.println("Config portal started on softAP; connect to the AP to configure.");
}

inline void handleConfigPortal() { if (!configPortalActive) return; portalServer.handleClient(); }

#endif // WEBCONFIG_HPP