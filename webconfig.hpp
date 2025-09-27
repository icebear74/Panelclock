#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Simple header-only webconfig for Panelclock
// - Inline/static variables to avoid separate .cpp file
// - Provides: deviceConfig, loadDeviceConfig(), saveDeviceConfig(),
//   startConfigPortalIfNeeded(), handleConfigPortal()

struct DeviceConfig {
  String ssid;
  String password;
  String hostname;
  String tankerApiKey;
  String stationId;
  String otaPassword;
};

static DeviceConfig deviceConfig;    // header-only; internal linkage per TU
static bool configPortalActive = false;
static WebServer portalServer(80);

// Load deviceConfig from LittleFS (/device_config.json)
inline void loadDeviceConfig() {
  if (!LittleFS.begin(true)) {
    // if LittleFS not mounted here, caller may mount earlier; keep defaults
    return;
  }
  File f = LittleFS.open("/device_config.json", "r");
  if (!f) {
    // no config -> keep defaults
    return;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return;
  deviceConfig.ssid        = String((const char*)(doc["ssid"] | ""));
  deviceConfig.password    = String((const char*)(doc["password"] | ""));
  deviceConfig.hostname    = String((const char*)(doc["hostname"] | ""));
  deviceConfig.tankerApiKey= String((const char*)(doc["tankerApiKey"] | ""));
  deviceConfig.stationId   = String((const char*)(doc["stationId"] | ""));
  deviceConfig.otaPassword = String((const char*)(doc["otaPassword"] | ""));
}

// Save deviceConfig to LittleFS (/device_config.json)
inline bool saveDeviceConfig() {
  if (!LittleFS.begin(true)) {
    return false;
  }
  StaticJsonDocument<512> doc;
  doc["ssid"] = deviceConfig.ssid;
  doc["password"] = deviceConfig.password;
  doc["hostname"] = deviceConfig.hostname;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["otaPassword"] = deviceConfig.otaPassword;
  File f = LittleFS.open("/device_config.json", "w");
  if (!f) return false;
  if (serializeJson(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  return true;
}

// simple HTML form for portal
inline String portalPageHtml() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>Panelclock Setup</title></head><body>";
  html += "<h2>Panelclock Konfiguration</h2>";
  html += "<form action=\"/save\" method=\"post\">";
  html += "WLAN SSID:<br><input name='ssid' value='" + deviceConfig.ssid + "'><br>";
  html += "WLAN Passwort:<br><input name='password' value='" + deviceConfig.password + "'><br>";
  html += "Hostname:<br><input name='hostname' value='" + deviceConfig.hostname + "'><br>";
  html += "Tankerkoenig API Key:<br><input name='tankerApiKey' value='" + deviceConfig.tankerApiKey + "'><br>";
  html += "Station ID:<br><input name='stationId' value='" + deviceConfig.stationId + "'><br>";
  html += "OTA Passwort:<br><input name='otaPassword' value='" + deviceConfig.otaPassword + "'><br>";
  html += "<br><input type='submit' value='Speichern'></form>";
  html += "<p>Nach Speichern bitte das Gerät neu starten.</p>";
  html += "</body></html>";
  return html;
}

// Handler for GET /
inline void _portal_handleRoot() {
  String page = portalPageHtml();
  portalServer.send(200, "text/html", page);
}

// Handler for POST /save
inline void _portal_handleSave() {
  if (portalServer.hasArg("ssid")) deviceConfig.ssid = portalServer.arg("ssid");
  if (portalServer.hasArg("password")) deviceConfig.password = portalServer.arg("password");
  if (portalServer.hasArg("hostname")) deviceConfig.hostname = portalServer.arg("hostname");
  if (portalServer.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = portalServer.arg("tankerApiKey");
  if (portalServer.hasArg("stationId")) deviceConfig.stationId = portalServer.arg("stationId");
  if (portalServer.hasArg("otaPassword")) deviceConfig.otaPassword = portalServer.arg("otaPassword");
  bool ok = saveDeviceConfig();
  if (ok) {
    portalServer.send(200, "text/html", "<html><body><h3>Gespeichert</h3><p>Bitte das Gerät neu starten.</p></body></html>");
  } else {
    portalServer.send(500, "text/plain", "Fehler beim Speichern");
  }
  // We keep the portal running so the user can see the confirmation
}

// Start a lightweight config portal if no wifi is configured.
// This function is safe to call multiple times; it will only start the portal once.
inline void startConfigPortalIfNeeded(unsigned long apTimeoutMs = 0) {
  // ensure config loaded
  loadDeviceConfig();

  if (deviceConfig.ssid.length() > 0) {
    // already configured -> do nothing
    return;
  }

  if (configPortalActive) return;

  // start AP for configuration
  const char *apName = "Panelclock-Setup";
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apName);

  // setup webserver routes
  portalServer.on("/", HTTP_GET, _portal_handleRoot);
  portalServer.on("/save", HTTP_POST, _portal_handleSave);
  portalServer.onNotFound([](){ portalServer.send(404, "text/plain", "Not found"); });

  portalServer.begin();
  configPortalActive = true;
}

// call this regularly from loop() to serve portal requests
inline void handleConfigPortal() {
  if (!configPortalActive) return;
  portalServer.handleClient();
}

#endif // WEBCONFIG_HPP