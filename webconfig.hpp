#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Header-only webconfig for Panelclock
// - verifies that config file is actually written (read-back verification)
// - presents a dropdown of scanned WLANs in the portal so user can pick SSID
// - keeps the simple save form and restarts on successful save+verify

struct DeviceConfig {
  String ssid;
  String password;
  String hostname;
  String tankerApiKey;
  String stationId;
  String otaPassword;
};

static DeviceConfig deviceConfig;    // header-only instance
static bool configPortalActive = false;
static WebServer portalServer(80);

// Try to mount LittleFS if not mounted. Returns true if mounted.
inline bool ensureLittleFSMounted() {
  // LittleFS.begin(true) prints "Already Mounted" if already mounted; call guard to reduce noise
  // There's no direct API for "is mounted", so just call begin() and return its value.
  return LittleFS.begin(true);
}

// Load deviceConfig from LittleFS (/device_config.json)
inline void loadDeviceConfig() {
  if (!ensureLittleFSMounted()) {
    // keep defaults
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
  if (!ensureLittleFSMounted()) {
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
  // ensure file is visible to subsequent reads
  return true;
}

// Read back the file and verify fields match deviceConfig (basic verification)
inline bool verifySavedConfig() {
  if (!ensureLittleFSMounted()) return false;
  File f = LittleFS.open("/device_config.json", "r");
  if (!f) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  // Compare key fields -- exact match required
  String ssid = String((const char*)(doc["ssid"] | ""));
  String passwd = String((const char*)(doc["password"] | ""));
  String host = String((const char*)(doc["hostname"] | ""));
  String tanker = String((const char*)(doc["tankerApiKey"] | ""));
  String station = String((const char*)(doc["stationId"] | ""));
  String ota = String((const char*)(doc["otaPassword"] | ""));
  return (ssid == deviceConfig.ssid) &&
         (passwd == deviceConfig.password) &&
         (host == deviceConfig.hostname) &&
         (tanker == deviceConfig.tankerApiKey) &&
         (station == deviceConfig.stationId) &&
         (ota == deviceConfig.otaPassword);
}

// Return HTML <option> list of scanned networks (ssid values escaped)
inline String getNetworksOptionsHtml() {
  String html;
  // Do a scan (blocking). This can take a couple seconds.
  // Ensure WiFi in station mode for scanning; we keep current mode but call scan directly.
  int n = WiFi.scanNetworks();
  if (n <= 0) {
    html += "<option value=\"\">(keine Netzwerke gefunden)</option>";
    return html;
  }
  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    bool enc = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    // Escape single quotes in SSID for embedding in value=''
    String esc = ssid;
    esc.replace("'", "&#39;");
    String label = esc + " (" + String(rssi) + " dBm";
    if (enc) label += ", gesichert";
    label += ")";
    html += "<option value='" + esc + "'>" + label + "</option>";
  }
  return html;
}

// simple HTML form for portal; includes a SSID dropdown populated by scanned networks
inline String portalPageHtml() {
  String html = "<!doctype html><html><head><meta charset='utf-8'><title>Panelclock Setup</title></head><body>";
  html += "<h2>Panelclock Konfiguration</h2>";
  html += "<form action=\"/save\" method=\"post\">";
  html += "WLAN SSID:<br>";
  // include a dropdown filled by getNetworksOptionsHtml(); add a text input fallback
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
  html += "<br><input type='submit' value='Speichern'></form>";
  html += "<p>Nach Speichern startet das Gerät neu.</p>";
  html += "<p>Hinweis: Wenn das gewünschte WLAN nicht angezeigt wird, tragen Sie die SSID bitte manuell ein.</p>";
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
  // If user selected from dropdown, prefer that value if provided
  if (portalServer.hasArg("ssid_select") && portalServer.arg("ssid_select").length() > 0) {
    deviceConfig.ssid = portalServer.arg("ssid_select");
  }
  if (portalServer.hasArg("ssid")) deviceConfig.ssid = portalServer.arg("ssid");
  if (portalServer.hasArg("password")) deviceConfig.password = portalServer.arg("password");
  if (portalServer.hasArg("hostname")) deviceConfig.hostname = portalServer.arg("hostname");
  if (portalServer.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = portalServer.arg("tankerApiKey");
  if (portalServer.hasArg("stationId")) deviceConfig.stationId = portalServer.arg("stationId");
  if (portalServer.hasArg("otaPassword")) deviceConfig.otaPassword = portalServer.arg("otaPassword");

  // attempt save
  bool ok = saveDeviceConfig();
  if (!ok) {
    portalServer.send(500, "text/plain", "Fehler: Konfigurationsdatei konnte nicht geschrieben werden (LittleFS Fehler).");
    return;
  }

  // verify read-back
  bool verified = verifySavedConfig();
  if (!verified) {
    portalServer.send(500, "text/plain", "Fehler: Gespeicherte Konfiguration konnte nicht verifiziert werden. Schreib-/Leseproblem.");
    return;
  }

  // success
  portalServer.send(200, "text/html", "<html><body><h3>Gespeichert</h3><p>Gerät startet neu...</p></body></html>");
  delay(600);
  ESP.restart(); // reboot so new config takes effect immediately
}

// Start a lightweight config portal if no wifi is configured or if forced.
// This function is safe to call multiple times; it will only start the portal once.
inline void startConfigPortalIfNeeded(bool force = false) {
  // ensure config loaded
  loadDeviceConfig();

  if (!force && deviceConfig.ssid.length() > 0) {
    // already configured and not forced -> do nothing
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