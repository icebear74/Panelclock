#include "webconfig.h"
#include <WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>

#ifdef CONFIG_SPIRAM_SUPPORT
#include <esp_heap_caps.h>
#endif

#ifndef ARDUINO
#error "This file is for Arduino/ESP32"
#endif

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

static String humanReadableBytes(size_t bytes) {
  const char *units[] = { "B", "KB", "MB", "GB", "TB" };
  double b = (double)bytes;
  int unit = 0;
  while (b >= 1024.0 && unit < 4) {
    b /= 1024.0;
    ++unit;
  }
  char buf[32];
  if (unit == 0) {
    snprintf(buf, sizeof(buf), "%u %s", (unsigned)bytes, units[unit]);
  } else {
    snprintf(buf, sizeof(buf), "%.2f %s", b, units[unit]);
  }
  return String(buf);
}

// Fallback LittleFS summary by iterating files (used bytes). total remains 0 if unknown.
static void getLittleFSSummary(size_t &used, size_t &total) {
  used = 0;
  total = 0;
  File root = LittleFS.open("/");
  if (!root) { used = 0; total = 0; return; }
  File file = root.openNextFile();
  while (file) {
    if (!file.isDirectory()) used += file.size();
    file = root.openNextFile();
  }
  total = 0;
}

String pageHeader(const char* title) {
  String s;
  s += R"rawliteral(<!doctype html><html lang='de'><head><meta charset='utf-8'/>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>)rawliteral";
  s += title;
  s += R"rawliteral(</title>
<style>body{font-family:Arial,Helvetica,sans-serif;margin:10px;padding:0}label{display:block;margin-top:8px}input[type=text],input[type=password],textarea{width:100%;padding:6px}button{margin-top:12px;padding:8px 12px}footer{margin-top:12px;color:#666;font-size:0.9em}.netlist{list-style:none;padding:0;margin:6px 0} .netlist li{padding:6px;border:1px solid #ddd;margin-bottom:6px;border-radius:4px;cursor:pointer} .netlist li:hover{background:#f7f7f7}.topbar{display:flex;gap:8px;margin-bottom:8px} .btn{display:inline-block;padding:6px 10px;background:#0078D4;color:#fff;text-decoration:none;border-radius:4px;border:1px solid #005a9e}</style>
</head><body><div class='topbar'><a class='btn' href='/'>Home</a><a class='btn' href='/status'>Status</a><a class='btn' href='/wifi'>WLAN</a></div><h3>)rawliteral";
  s += title;
  s += R"rawliteral(</h3>)rawliteral";
  return s;
}

void handleRoot() {
  String html = pageHeader("Panelclock - Konfiguration");
  html += R"rawliteral(<div><button type='button' id='scanBtn' onclick="scan()">Netzwerke suchen</button> <button type='button' onclick="location.reload()">Seite neu laden</button></div>
<div style='display:flex;gap:16px;margin-top:12px'>
<div style='flex:1'>
<h4>Verfügbare WLANs</h4>
<ul id='networks' class='netlist'><li>Keine Daten. Bitte 'Netzwerke suchen' klicken.</li></ul>
</div>
<div style='flex:1'>
<h4>Allgemeine Konfiguration</h4>
<form id='cfg' method='POST' action='/save' accept-charset='utf-8'>
<label>Tankerkoenig API-Key:<input type='text' id='tankerApiKey' name='tankerApiKey' value=')rawliteral";
  html += deviceConfig.tankerApiKey;
  html += R"rawliteral('></label>
<label>Tankstellen ID:<input type='text' id='stationId' name='stationId' value=')rawliteral";
  html += deviceConfig.stationId;
  html += R"rawliteral('></label>
<label>Hostname:<input type='text' id='hostname' name='hostname' value=')rawliteral";
  html += deviceConfig.hostname;
  html += R"rawliteral('></label>
<div style='display:flex;gap:6px'><button type='submit'>Speichern & Neustart</button></div>
</form>
<div id='status' style='margin-top:8px;color:#006;'></div>
</div>
</div>
)rawliteral";

  // Updated JS: store scan result in sessionStorage and reload page; on load populate list from storage
  html += R"rawliteral(
<script>
function renderNetworks(j){
  let ul = document.getElementById('networks');
  ul.innerHTML = '';
  if(!j.networks || j.networks.length==0){
    ul.innerHTML = '<li>Keine Netze gefunden</li>';
    document.getElementById('status').innerText = 'Keine Netze gefunden';
    return;
  }
  j.networks.forEach(n=>{
    let li=document.createElement('li');
    li.innerText = n.ssid + ' ('+n.rssi+' dBm) ' + (n.secured? '🔒':'');
    li.onclick=function(){ document.location.href='/wifi?ssid='+encodeURIComponent(n.ssid); };
    ul.appendChild(li);
  });
  document.getElementById('status').innerText = 'Scan abgeschlossen';
}

function populateFromSession(){
  try {
    const raw = sessionStorage.getItem('scanResult');
    if(!raw) return false;
    const j = JSON.parse(raw);
    renderNetworks(j);
    sessionStorage.removeItem('scanResult');
    return true;
  } catch(e){
    console.log('populateFromSession error', e);
    return false;
  }
}

function scan(){
  const btn = document.getElementById('scanBtn');
  btn.disabled = true;
  document.getElementById('status').innerText='Suche...';
  fetch('/api/scan').then(r=>{
    if(!r.ok) throw new Error('HTTP '+r.status);
    return r.json();
  }).then(j=>{
    try {
      sessionStorage.setItem('scanResult', JSON.stringify(j));
    } catch(e){
      console.log('Failed to store scan result', e);
    }
    location.reload();
  }).catch(e=>{
    console.log('scan error', e);
    document.getElementById('status').innerText='Fehler beim Scan';
    btn.disabled = false;
  });
}

window.addEventListener('DOMContentLoaded', (event) => {
  populateFromSession();
});
</script>
)rawliteral";

  html += R"rawliteral(<footer>Hinweis: Diese Seite ist unverschlüsselt (HTTP). Für produktive Nutzung bitte Absicherung verwenden.</footer>
</body></html>)rawliteral";

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html; charset=utf-8", html);
}

void handleSave() {
  if (server.hasArg("tankerApiKey")) deviceConfig.tankerApiKey = server.arg("tankerApiKey");
  if (server.hasArg("stationId")) deviceConfig.stationId = server.arg("stationId");
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

// API: GET /api/config -> JSON mit aktuellen Einstellungen (ohne Passwort)
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

// API: POST /api/config -> JSON body with fields (ssid,password,tankerApiKey,stationId,otaPassword,hostname)
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

// API: GET /api/scan -> führt WLAN-Scan aus und gibt JSON zurück
void handleApiScan() {
  server.sendHeader("Content-Type", "application/json; charset=utf-8");

  // Remember current mode to restore after scan
  wifi_mode_t prevMode = WiFi.getMode();

  // Ensure STA is active for reliable scanning
  if (prevMode != WIFI_STA && prevMode != WIFI_AP_STA) {
    WiFi.mode(WIFI_STA);
    delay(50);
  }

  // Perform blocking scan (synchronous)
  int n = WiFi.scanNetworks(false, true);
  if (n < 0) n = 0;

  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.createNestedArray("networks");
  for (int i = 0; i < n; ++i) {
    JsonObject item = arr.createNestedObject();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
    item["enc"] = WiFi.encryptionType(i);
    item["secured"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
  }

  // Restore previous WiFi mode if changed
  if (prevMode != WiFi.getMode()) {
    WiFi.mode(prevMode);
    delay(50);
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json; charset=utf-8", out);
}

// Versuch, mit angegebenen Credentials zu verbinden und bei Erfolg zu speichern
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

// API: POST /api/connect  -> JSON body {ssid,password}
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

// ---------- Status page and API ----------

void handleApiStatus() {
  StaticJsonDocument<1024> doc;
  size_t heap_total = ESP.getHeapSize();
  size_t heap_free = ESP.getFreeHeap();

  size_t psram_total = 0;
  size_t psram_free = 0;

  // Try to get SPIRAM/PSRAM info in a portable way
  // Preferred: use esp_heap_caps if available (CONFIG_SPIRAM_SUPPORT)
  #ifdef CONFIG_SPIRAM_SUPPORT
    psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
  #else
    // Fallback to Arduino API if present
    #if defined(ESP_HAS_PSRAM)
      // Some Arduino-ESP32 cores provide ESP.getPsramSize() and ESP.getFreePsram()
      // Use getPsramSize if available; getFreePsram may not be present on all cores.
      #if defined(ESP.getPsramSize)
        psram_total = ESP.getPsramSize();
      #else
        psram_total = 0;
      #endif
      // Try common function names for free PSRAM
      #if defined(ESP.getFreePsram)
        psram_free = ESP.getFreePsram();
      #elif defined(ESP.getFreePsramSize)
        psram_free = ESP.getFreePsramSize();
      #else
        psram_free = 0; // unknown
      #endif
    #else
      psram_total = 0;
      psram_free = 0;
    #endif
  #endif

  size_t lfs_used = 0, lfs_total = 0;
  getLittleFSSummary(lfs_used, lfs_total);

  doc["heap"]["total"] = heap_total;
  doc["heap"]["free"] = heap_free;
  doc["heap"]["used"] = heap_total - heap_free;
  doc["heap"]["total_human"] = humanReadableBytes(heap_total);
  doc["heap"]["free_human"] = humanReadableBytes(heap_free);
  doc["heap"]["used_human"] = humanReadableBytes(heap_total - heap_free);

  doc["psram"]["total"] = psram_total;
  doc["psram"]["free"] = psram_free;
  doc["psram"]["used"] = (psram_total > 0) ? (psram_total - psram_free) : 0;
  doc["psram"]["total_human"] = psram_total ? humanReadableBytes(psram_total) : String("");
  doc["psram"]["free_human"] = psram_total ? humanReadableBytes(psram_free) : String("");
  doc["psram"]["used_human"] = psram_total ? humanReadableBytes((psram_total - psram_free)) : String("");

  doc["littlefs"]["used"] = lfs_used;
  doc["littlefs"]["total"] = lfs_total;
  doc["littlefs"]["used_human"] = humanReadableBytes(lfs_used);
  doc["littlefs"]["total_human"] = lfs_total ? humanReadableBytes(lfs_total) : String("");

  String out;
  serializeJson(doc, out);
  server.sendHeader("Content-Type", "application/json; charset=utf-8");
  server.send(200, "application/json; charset=utf-8", out);
}

// HTML status page
void handleStatus() {
  String html = pageHeader("Systemstatus");
  html += R"rawliteral(<div id='stat' style='font-family:monospace'></div>
<script>
// Helper: number with thousands separator (German style " . ")
function numberWithSep(n){
  if(n===undefined||n===null) return '0';
  try {
    return n.toString().replace(/\B(?=(\d{3})+(?!\d))/g, " . ");
  } catch(e) {
    return String(n);
  }
}

// Render function: accepts parsed JSON from /api/status
function renderStatus(j){
  let s = '';
  if(!j || typeof j !== 'object') {
    s += 'Keine Daten vom API<br>';
    document.getElementById('stat').innerHTML = s;
    return;
  }

  s += '<h4>Heap</h4>';
  if (j.heap) {
    if (j.heap.total_human) {
      s += 'Total: ' + (j.heap.total_human || '') + ' (' + numberWithSep(j.heap.total) + ')<br>';
      s += 'Free : ' + (j.heap.free_human || '') + ' (' + numberWithSep(j.heap.free) + ')<br>';
      s += 'Used : ' + (j.heap.used_human || '') + ' (' + numberWithSep(j.heap.used) + ')<br>';
    } else {
      s += 'Total: ' + numberWithSep(j.heap.total) + ' bytes<br>';
      s += 'Free : ' + numberWithSep(j.heap.free) + ' bytes<br>';
      s += 'Used : ' + numberWithSep(j.heap.used) + ' bytes<br>';
    }
  } else {
    s += 'Heap: keine Daten<br>';
  }

  s += '<h4>PSRAM</h4>';
  if (j.psram && j.psram.total && j.psram.total > 0) {
    s += 'Total: ' + (j.psram.total_human || '') + ' (' + numberWithSep(j.psram.total) + ')<br>';
    s += 'Free : ' + (j.psram.free_human || '') + ' (' + numberWithSep(j.psram.free) + ')<br>';
    s += 'Used : ' + (j.psram.used_human || '') + ' (' + numberWithSep(j.psram.used) + ')<br>';
  } else {
    s += 'PSRAM: nicht verfügbar auf diesem Board<br>';
  }

  s += '<h4>LittleFS</h4>';
  if (j.littlefs) {
    if (j.littlefs.total_human && j.littlefs.total_human.length > 0) {
      s += 'Total: ' + (j.littlefs.total_human || '') + ' (' + numberWithSep(j.littlefs.total) + ')<br>';
      s += 'Used : ' + (j.littlefs.used_human || '') + ' (' + numberWithSep(j.littlefs.used) + ')<br>';
      let free = (j.littlefs.total || 0) - (j.littlefs.used || 0);
      s += 'Free : ' + (free >= 0 ? numberWithSep(free) : '0') + '<br>';
    } else {
      s += 'Used: ' + numberWithSep(j.littlefs.used) + ' bytes<br>';
      s += 'Total: unbekannt<br>';
    }
  } else {
    s += 'LittleFS: keine Daten<br>';
  }

  document.getElementById('stat').innerHTML = s;
}

// Fetch /api/status and render, with error handling
function loadStatus(){
  fetch('/api/status').then(function(res){
    if(!res.ok) throw new Error('HTTP ' + res.status);
    return res.json();
  }).then(function(json){
    renderStatus(json);
  }).catch(function(err){
    console.error('Error loading /api/status:', err);
    document.getElementById('stat').innerHTML = 'Fehler beim Laden: ' + err + '<br>Prüfe /api/status direkt (z.B. curl) oder die Browser-Konsole.';
  });
}

// Run after DOM ready
window.addEventListener('DOMContentLoaded', function(){
  loadStatus();
});
</script>
)rawliteral";
  html += R"rawliteral(<footer>Seite aktualisiert sich beim Laden. Für Live‑Updates die API per JS alle x Sekunden abfragen.</footer></body></html>)rawliteral";
  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html; charset=utf-8", html);
}

// ---------- WLAN page (separate) ----------

// Mask SSID for display
static String maskSSID(const String& ssid) {
  if (ssid.length() == 0) return "(nicht gesetzt)";
  String out = ssid;
  if (out.length() <= 2) return String(out[0]) + "*";
  for (size_t i = 1; i + 1 < out.length(); ++i) out[i] = '*';
  return out;
}

void handleWifiPage() {
  String presetSSID = "";
  if (server.hasArg("ssid")) {
    presetSSID = server.arg("ssid");
  }
  String html = pageHeader("WLAN Zugangsdaten");
  html += R"rawliteral(<div><p>Aktuelle SSID: )rawliteral";
  html += maskSSID(deviceConfig.ssid);
  html += R"rawliteral(</p>
<form id='wifi' method='POST' action='/api/wifi' accept-charset='utf-8'>
<label>SSID:<input type='text' id='ssid' name='ssid' value=')rawliteral";
  html += (presetSSID.length() ? presetSSID : deviceConfig.ssid);
  html += R"rawliteral('></label>
<label>Passwort:<input type='password' id='password' name='password' value=''></label>
<label>OTA Passwort:<input type='password' id='otaPassword' name='otaPassword' value=')rawliteral";
  html += deviceConfig.otaPassword;
  html += R"rawliteral('></label>
<div style='display:flex;gap:6px'><button type='submit'>Verbinden & Speichern</button></div>
</form>
<div id='status' style='margin-top:8px;color:#006;'></div>
</div>
<script>
document.getElementById('wifi').addEventListener('submit', function(ev){
  ev.preventDefault();
  let s = document.getElementById('ssid').value;
  let p = document.getElementById('password').value;
  let o = document.getElementById('otaPassword')?document.getElementById('otaPassword').value:'';
  document.getElementById('status').innerText = 'Versuche Verbindung...';
  fetch('/api/wifi',{method:'POST',headers:{'Content-Type':'application/json; charset=utf-8'},body:JSON.stringify({ssid:s,password:p,otaPassword:o})})
    .then(r=>r.json()).then(j=>{ if (j.result=='ok') { document.getElementById('status').innerText='Verbunden; speichere und starte neu...'; setTimeout(()=>{ location.reload(); },1500); } else { document.getElementById('status').innerText = 'Fehler: '+(j.error||'unknown'); } })
    .catch(e=>{ document.getElementById('status').innerText = 'Fehler: '+e; });
});
</script>
)rawliteral";
  html += R"rawliteral(<footer>Gib SSID/Passwort ein, um Verbindung herzustellen und dauerhaft zu speichern.</footer></body></html>)rawliteral";

  server.sendHeader("Content-Type", "text/html; charset=utf-8");
  server.send(200, "text/html; charset=utf-8", html);
}

// API to save wifi credentials from /wifi page
void handleApiWifiSave() {
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
  String ota = String(doc["otaPassword"] | deviceConfig.otaPassword);

  if (ssid.length() == 0) {
    server.send(400, "application/json; charset=utf-8", "{\"error\":\"no_ssid\"}");
    return;
  }

  // Try connect
  bool ok = tryConnectAndSave(ssid, password);
  // Save otaPassword regardless
  deviceConfig.otaPassword = ota;
  saveConfig();

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
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/wifi", HTTP_GET, handleWifiPage);
  server.on("/api/wifi", HTTP_POST, handleApiWifiSave);
  server.onNotFound([](){ server.send(404, "text/plain; charset=utf-8", "Nicht gefunden"); });
  server.begin();
  portalActive = true;
  Serial.println("Konfig-Server gestartet (HTTP)");
}

// Safe startup: set WiFi mode and start server only after WiFi mode/softAP/STA is initialized.
void startConfigPortalIfNeeded() {
  bool haveConfig = loadConfig();

  if (haveConfig && deviceConfig.ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    if (deviceConfig.hostname.length()) WiFi.setHostname(deviceConfig.hostname.c_str());
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());
    unsigned long start = millis();
    while (millis() - start < 5000) {
      if (WiFi.status() == WL_CONNECTED) break;
      delay(200);
    }
    delay(150);
    beginConfigServer();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Verbunden zum WLAN. Config-Server erreichbar unter %s\n", WiFi.localIP().toString().c_str());
    } else {
      Serial.println("Keine WLAN-Verbindung nach Start; Config-Server läuft lokal. Falls gewünscht, kann SoftAP gestartet werden.");
    }
    return;
  }

  const char* apName = "Panelclock-Setup";
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
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