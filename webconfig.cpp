#include "webconfig.h"
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

DeviceConfig deviceConfig;

static WebServer server(80);
static const char* CONFIG_PATH = "/config.json";
static bool portalActive = false;

bool loadConfig() {
  if (!LittleFS.exists(CONFIG_PATH)) return false;
  File f = LittleFS.open(CONFIG_PATH, "r");
  if (!f) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  deviceConfig.ssid = String(doc["ssid"] | "");
  deviceConfig.password = String(doc["password"] | "");
  deviceConfig.tankerApiKey = String(doc["tankerApiKey"] | "");
  deviceConfig.stationId = String(doc["stationId"] | "");
  deviceConfig.otaPassword = String(doc["otaPassword"] | "");
  deviceConfig.hostname = String(doc["hostname"] | "Panelclock");
  return true;
}

bool saveConfig() {
  StaticJsonDocument<512> doc;
  doc["ssid"] = deviceConfig.ssid;
  doc["password"] = deviceConfig.password;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["otaPassword"] = deviceConfig.otaPassword;
  doc["hostname"] = deviceConfig.hostname;
  File f = LittleFS.open(CONFIG_PATH, "w");
  if (!f) return false;
  if (serializeJson(doc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  return true;
}

String pageHeader(const char* title) {
  String s;
  s += R"rawliteral(<!doctype html><html lang='de'><head><meta charset='utf-8'/>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>)rawliteral";
  s += title;
  s += R"rawliteral(</title>
<style>body{font-family:Arial,Helvetica,sans-serif;margin:10px;padding:0}label{display:block;margin-top:8px}input[type=text],input[type=password],textarea{width:100%;padding:6px}button{margin-top:12px;padding:8px 12px}footer{margin-top:12px;color:#666;font-size:0.9em}.netlist{list-style:none;padding:0;margin:6px 0} .netlist li{padding:6px;border:1px solid #ddd;margin-bottom:6px;border-radius:4px;cursor:pointer} .netlist li:hover{background:#f7f7f7}</style>
</head><body><h3>)rawliteral";
  s += title;
  s += R"rawliteral(</h3>)rawliteral";
  return s;
}

void handleRoot() {
  String html = pageHeader("Panelclock - Konfiguration");
  html += R"rawliteral(<div><button onclick="scan()">Netzwerke suchen</button> <button onclick="location.reload()">Seite neu laden</button></div>
<div style='display:flex;gap:16px;margin-top:12px'>
<div style='flex:1'>
<h4>Verfügbare WLANs</h4>
<ul id='networks' class='netlist'><li>Keine Daten. Bitte 'Netzwerke suchen' klicken.</li></ul>
</div>
<div style='flex:1'>
<h4>Konfiguration</h4>
<form id='cfg' method='POST' action='/save' accept-charset='utf-8'>
<label>WLAN SSID:<input type='text' id='ssid' name='ssid' value=')rawliteral";
  html += deviceConfig.ssid;
  html += R"rawliteral('></label>
<label>WLAN Passwort:<input type='password' id='password' name='password' value=')rawliteral";
  html += deviceConfig.password;
  html += R"rawliteral('></label>
<label>Tankerkoenig API-Key:<input type='text' id='tankerApiKey' name='tankerApiKey' value=')rawliteral";
  html += deviceConfig.tankerApiKey;
  html += R"rawliteral('></label>
<label>Tankstellen ID:<input type='text' id='stationId' name='stationId' value=')rawliteral";
  html += deviceConfig.stationId;
  html += R"rawliteral('></label>
<label>OTA Passwort:<input type='password' id='otaPassword' name='otaPassword' value=')rawliteral";
  html += deviceConfig.otaPassword;
  html += R"rawliteral('></label>
<label>Hostname:<input type='text' id='hostname' name='hostname' value=')rawliteral";
  html += deviceConfig.hostname;
  html += R"rawliteral('></label>
<div style='display:flex;gap:6px'><button type='button' onclick="connectSelected()">Mit ausgewähltem Netzwerk verbinden</button><button type='submit'>Speichern & Neustart</button></div>
</form>
<div id='status' style='margin-top:8px;color:#006;'></div>
</div>
</div>
)rawliteral";

  html += R"rawliteral(
<script>
function scan(){
  document.getElementById('status').innerText='Suche...';
  fetch('/api/scan').then(r=>r.json()).then(j=>{
    let ul=document.getElementById('networks'); ul.innerHTML='';
    if(!j.networks||j.networks.length==0){ ul.innerHTML='<li>Keine Netze gefunden</li>'; document.getElementById('status').innerText='Keine Netze gefunden'; return; }
    j.networks.forEach(n=>{let li=document.createElement('li'); li.innerText = n.ssid + ' ('+n.rssi+' dBm) ' + (n.secured? '🔒':''); li.onclick=function(){ document.getElementById('ssid').value=n.ssid; document.getElementById('password').focus(); }; ul.appendChild(li);});
    document.getElementById('status').innerText='Scan abgeschlossen';
  }).catch(e=>{ document.getElementById('status').innerText='Fehler beim Scan'; });
}

function connectSelected(){
  let s=document.getElementById('ssid').value; let p=document.getElementById('password').value;
  if(!s||s.length==0){ alert('Bitte SSID auswählen oder eingeben'); return; }
  document.getElementById('status').innerText='Verbinde...';
  fetch('/api/connect',{method:'POST',headers:{'Content-Type':'application/json; charset=utf-8'},body:JSON.stringify({ssid:s,password:p})}).then(r=>r.json()).then(j=>{ if(j.result=='ok'){ document.getElementById('status').innerText='Verbunden, speichere...'; setTimeout(()=>{ location.reload(); },1500); } else { document.getElementById('status').innerText='Verbindung fehlgeschlagen: '+(j.error||'unknown'); } }).catch(e=>{ document.getElementById('status').innerText='Fehler: '+e; });
}
</script>
)rawliteral";

  html += R"rawliteral(<footer>Hinweis: Diese Seite ist unverschlüsselt (HTTP). Für produktive Nutzung bitte Absicherung verwenden.</footer>
</body></html>)rawliteral";

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  if (server.hasArg("ssid")) deviceConfig.ssid = server.arg("ssid");
  if (server.hasArg("password")) deviceConfig.password = server.arg("password");
  if (server.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = server.arg("tankerApiKey");
  if (server.hasArg("stationId")) deviceConfig.stationId = server.arg("stationId");
  if (server.hasArg("otaPassword")) deviceConfig.otaPassword = server.arg("otaPassword");
  if (server.hasArg("hostname")) deviceConfig.hostname = server.arg("hostname");

  bool ok = saveConfig();
  if (ok) {
    String msg = R"rawliteral(<!doctype html><html lang='de'><head><meta charset='utf-8'/><title>Gespeichert</title></head><body><h3>Erfolgreich gespeichert.</h3><p>Das Gerät startet in 3 Sekunden neu.</p></body></html>)rawliteral";
    server.sendHeader("Content-Type", "text/html; charset=utf-8");
    server.send(200, "text/html; charset=utf-8", msg);
    delay(3000);
    ESP.restart();
  } else {
    server.send(500, "text/plain; charset=utf-8", "Fehler beim Speichern der Konfiguration");
  }
}

void handleApiGetConfig() {
  StaticJsonDocument<512> doc;
  doc["ssid"] = deviceConfig.ssid;
  doc["tankerApiKey"] = deviceConfig.tankerApiKey;
  doc["stationId"] = deviceConfig.stationId;
  doc["hostname"] = deviceConfig.hostname;
  String out;
  serializeJson(doc, out);
  server.sendHeader("Content-Type", "application/json; charset=utf-8");
  server.send(200, "application/json; charset=utf-8", out);
}

void handleApiPostConfig() {
  if (server.hasArg("plain") && server.arg("plain").length() > 0) {
    String body = server.arg("plain");
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
      server.send(400, "application/json; charset=utf-8", "{\"error\":\"invalid_json\"}");
      return;
    }
    deviceConfig.ssid = String(doc["ssid"] | deviceConfig.ssid);
    deviceConfig.password = String(doc["password"] | deviceConfig.password);
    deviceConfig.tankerApiKey = String(doc["tankerApiKey"] | deviceConfig.tankerApiKey);
    deviceConfig.stationId = String(doc["stationId"] | deviceConfig.stationId);
    deviceConfig.otaPassword = String(doc["otaPassword"] | deviceConfig.otaPassword);
    deviceConfig.hostname = String(doc["hostname"] | deviceConfig.hostname);
    if (!saveConfig()) {
      server.send(500, "application/json; charset=utf-8", "{\"error\":\"save_failed\"}");
      return;
    }
    server.send(200, "application/json; charset=utf-8", "{\"result\":\"ok\",\"restart\":\"true\"}");
    delay(1000);
    ESP.restart();
    return;
  }
  server.send(400, "application/json; charset=utf-8", "{\"error\":\"no_body\"}");
}

void handleApiScan() {
  server.sendHeader("Content-Type", "application/json; charset=utf-8");
  int n = WiFi.scanNetworks(true, true);
  if (n == WIFI_SCAN_RUNNING) {
    delay(1000);
    n = WiFi.scanComplete();
  }
  if (n <= 0) {
    n = WiFi.scanNetworks();
  }
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("networks");
  for (int i = 0; i < n; ++i) {
    JsonObject item = arr.createNestedObject();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["enc"] = WiFi.encryptionType(i);
    item["secured"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json; charset=utf-8", out);
}

bool tryConnectAndSave(const String& ssid, const String& password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  unsigned long start = millis();
  while (millis() - start < 10000) {
    if (WiFi.status() == WL_CONNECTED) {
      deviceConfig.ssid = ssid;
      deviceConfig.password = password;
      if (!saveConfig()) return false;
      return true;
    }
    delay(500);
  }
  return false;
}

void handleApiConnect() {
  if (!(server.hasArg("plain") && server.arg("plain").length() > 0)) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"no_body\"}");
    return;
  }
  String body = server.arg("plain");
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"invalid_json\"}");
    return;
  }
  String ssid = String(doc["ssid"] | "");
  String password = String(doc["password"] | "");
  if (ssid.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"no_ssid\"}");
    return;
  }
  bool ok = tryConnectAndSave(ssid, password);
  if (ok) {
    server.send(200, "application/json; charset=utf-8", "{\"result\":\"ok\"}");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "application/json; charset=utf-8", "{\"error\":\"connect_failed\"}");
  }
}

void beginConfigServer() {
  if (portalActive) return; // already running
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/api/config", HTTP_GET, handleApiGetConfig);
  server.on("/api/config", HTTP_POST, handleApiPostConfig);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/connect", HTTP_POST, handleApiConnect);
  server.onNotFound([](){ server.send(404, "text/plain; charset=utf-8", "Nicht gefunden"); });
  server.begin();
  portalActive = true;
  Serial.println("Konfig-Server gestartet (HTTP)");
}

// Safe startup: set WiFi mode and start server only after WiFi mode/softAP/STA is initialized.
// If there is a stored config we try to connect (STA); the HTTP server will be reachable via STA-IP.
// If no stored config, we start SoftAP and the HTTP server will be reachable via AP-IP.
void startConfigPortalIfNeeded() {
  bool haveConfig = loadConfig();

  if (haveConfig && deviceConfig.ssid.length() > 0) {
    // Try to start STA first, then bring up HTTP server (server will be reachable via STA-IP).
    WiFi.mode(WIFI_STA);
    if (deviceConfig.hostname.length()) WiFi.setHostname(deviceConfig.hostname.c_str());
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());
    // Wait briefly for the stack to initialize / connect (non-blocking-ish)
    unsigned long start = millis();
    while (millis() - start < 5000) { // wait up to 5 seconds for connection
      if (WiFi.status() == WL_CONNECTED) break;
      delay(200);
    }
    // Ensure WiFi stack settled before starting HTTP server
    delay(150);
    beginConfigServer();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Verbunden zum WLAN. Config-Server erreichbar unter %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("Keine WLAN-Verbindung nach Start; Config-Server läuft lokal. Falls gewünscht, kann SoftAP gestartet werden.");
    }
    return;
  }

  // No config: start SoftAP + HTTP server so user can connect and configure.
  const char* apName = "Panelclock-Setup";
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  // small delay to let mode switch happen
  delay(150);
  bool ok = WiFi.softAP(apName);
  delay(200);
  beginConfigServer();
  if (ok) {
    Serial.printf("Konfigurations-AP gestartet: %s\n", apName);
    Serial.printf("Verbinde mit http://%s/ um die Konfiguration vorzunehmen\n", WiFi.softAPIP().toString().c_str());
  } else {
    Serial.println("SoftAP konnte nicht gestartet werden. Config-Server läuft eventuell über STA-Interface.");
  }
}

void handleConfigPortal() {
  if (portalActive) {
    server.handleClient();
  }
}