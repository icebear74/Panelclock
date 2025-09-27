#ifndef WEBCONFIG_HPP
#define WEBCONFIG_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Header-only webconfig for Panelclock
// - verifies that config file is actually written (read-back verification)
// - performs a WiFi scan in STA mode before starting the AP so the dropdown is populated
// - adds serial debug output so it's visible whether AP started

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
  return LittleFS.begin(true);
}

// Load deviceConfig from LittleFS (/device_config.json)
inline void loadDeviceConfig() {
  if (!ensureLittleFSMounted()) return;
  File f = LittleFS.open("/device_config.json", "r");
  if (!f) return;
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
  if (!ensureLittleFSMounted()) return false;
  StaticJsonDocument<512> doc;
  doc["ssid"] = deviceConfig.ssid;
  doc["password"] = deviceConfig.password;
  doc["hostname"] = deviceConfig.hostname;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["otaPassword"] = deviceConfig.otaPassword;
  File f = LittleFS.open("/device_config.json", "w");
  if (!f) return false;
  if (serializeJson(doc, f) == 0) { f.close(); return false; }
  f.close();
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
  String ssid = String((const char*)(doc["ssid"] | ""));
  String passwd = String((const char*)(doc["password"] | ""));
  String host = String((const char*)(doc["hostname"] | ""));
  String tanker = String((const char*)(doc["tankerApiKey"] | ""));
  String station = String((const char*)(doc["stationId"] | ""));
  String ota = String((const char*)(doc["otaPassword"] | ""));
  return (ssid == deviceConfig.ssid) && (passwd == deviceConfig.password) && (host == deviceConfig.hostname) && (tanker == deviceConfig.tankerApiKey) && (station == deviceConfig.stationId) && (ota == deviceConfig.otaPassword);
}

// Return HTML <option> list of scanned networks (ssid values escaped)
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
  html += "<br><input type='submit' value='Speichern'></form>";
  html += "<p>Nach Speichern startet das Gerät neu.</p>";
  html += "<p>Hinweis: Wenn das gewünschte WLAN nicht angezeigt wird, tragen Sie die SSID bitte manuell ein.</p>";
  html += "</body></html>";
  return html;
}

inline void _portal_handleRoot() { String page = portalPageHtml(); portalServer.send(200, "text/html", page); }

inline void _portal_handleSave() {
  if (portalServer.hasArg("ssid_select") && portalServer.arg("ssid_select").length() > 0) deviceConfig.ssid = portalServer.arg("ssid_select");
  if (portalServer.hasArg("ssid")) deviceConfig.ssid = portalServer.arg("ssid");
  if (portalServer.hasArg("password")) deviceConfig.password = portalServer.arg("password");
  if (portalServer.hasArg("hostname")) deviceConfig.hostname = portalServer.arg("hostname");
  if (portalServer.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = portalServer.arg("tankerApiKey");
  if (portalServer.hasArg("stationId")) deviceConfig.stationId = portalServer.arg("stationId");
  if (portalServer.hasArg("otaPassword")) deviceConfig.otaPassword = portalServer.arg("otaPassword");

  bool ok = saveDeviceConfig();
  if (!ok) { portalServer.send(500, "text/plain", "Fehler: Konfigurationsdatei konnte nicht geschrieben werden (LittleFS Fehler).\n"); return; }
  bool verified = verifySavedConfig();
  if (!verified) { portalServer.send(500, "text/plain", "Fehler: Gespeicherte Konfiguration konnte nicht verifiziert werden. Schreib-/Leseproblem.\n"); return; }
  portalServer.send(200, "text/html", "<html><body><h3>Gespeichert</h3><p>Gerät startet neu...</p></body></html>");
  delay(600);
  ESP.restart();
}

// Start a lightweight config portal if no wifi is configured or if forced.
// Performs scan in STA mode first to populate the dropdown and prints diagnostics
inline void startConfigPortalIfNeeded(bool force = false) {
  loadDeviceConfig();
  if (!force && deviceConfig.ssid.length() > 0) return;
  if (configPortalActive) { Serial.println("Config portal already active"); return; }

  Serial.println("Starting config portal: scanning networks (STA mode)...");
  // try a STA scan first to populate dropdown reliably
  WiFi.mode(WIFI_STA);
  delay(200);
  int n = WiFi.scanNetworks();
  Serial.printf("Scan result count: %d\n", n);
  // small delay to let scan settle on some SDKs
  delay(200);

  const char *apName = "Panelclock-Setup";
  Serial.println("Switching to AP+STA and starting softAP...");
  WiFi.mode(WIFI_AP_STA);
  bool ok = WiFi.softAP(apName);
  if (!ok) {
    Serial.println("Warning: WiFi.softAP() returned false");
  }
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("softAP IP: %s\n", apIP.toString().c_str());

  portalServer.on("/", HTTP_GET, _portal_handleRoot);
  portalServer.on("/save", HTTP_POST, _portal_handleSave);
  portalServer.onNotFound([](){ portalServer.send(404, "text/plain", "Not found"); });
  portalServer.begin();
  configPortalActive = true;
  Serial.println("Config portal started; serve on / (webserver running)");
}

inline void handleConfigPortal() { if (!configPortalActive) return; portalServer.handleClient(); }

#endif // WEBCONFIG_HPP
