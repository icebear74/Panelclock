#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <WiFi.h>
#include "time.h"
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <FS.h>
#include <LittleFS.h>
#include "BerlinTime.hpp"
#include "webconfig.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include <esp_heap_caps.h>

// Display-Konstanten
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN

const int FULL_WIDTH = PANEL_RES_X * VDISP_NUM_COLS;
const int FULL_HEIGHT = PANEL_RES_Y * VDISP_NUM_ROWS;
const int TIME_AREA_H = 30;
const int DATA_AREA_H = FULL_HEIGHT - TIME_AREA_H;

// Display-Objekte
MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE> *virtualDisp = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2;
GFXcanvas16 canvasTime(FULL_WIDTH, TIME_AREA_H);
GFXcanvas16 canvasData(FULL_WIDTH, DATA_AREA_H);

// Module
ClockModule clockMod(u8g2, canvasTime);
DataModule dataMod(u8g2, canvasData, TIME_AREA_H);
CalendarModule calendarMod(u8g2, canvasData);

// PSRAM Buffers
static uint16_t *psramBufTime = nullptr;
static uint16_t *psramBufData = nullptr;
static size_t psramBufTimeBytes = 0;
static size_t psramBufDataBytes = 0;

// Laufzeitdaten
String TankerkoenigApiKey = "";
String TankstellenID = "";
float diesel = 0, e5 = 0, e10 = 0;
float dieselLow = 99.999, dieselHigh = 0, e5Low = 99.999, e5High = 0, e10Low = 99.999, e10High = 0;
String stationname;
unsigned long lastTime = 0;
unsigned long timerDelay = 360000;
unsigned long initialQueryDelay = 5000;
bool firstFetchDone = false;
bool skiptimer = true;
struct tm timeinfo; // Enthält immer Berlin-Zeit!

// Anzeige-Wechsel-Logik
unsigned long calendarDisplayMs = 30000;
unsigned long stationDisplayMs = 30000;
unsigned long lastSwitch = 0;
bool showCalendar = false;

volatile bool calendarScrollNeedsRedraw = false;

// --- Hilfsfunktionen für Preishistorie ---
void savePriceHistory() {
  StaticJsonDocument<512> doc;
  doc["e5Low"] = e5Low; doc["e5High"] = e5High;
  doc["e10Low"] = e10Low; doc["e10High"] = e10High;
  doc["dieselLow"] = dieselLow; doc["dieselHigh"] = dieselHigh;
  File file = LittleFS.open("/preise.json", "w");
  if (!file) { Serial.println("Fehler beim Öffnen der Datei zum Schreiben."); return; }
  if (serializeJson(doc, file) == 0) Serial.println("Fehler beim Schreiben der JSON-Daten in die Datei.");
  file.close();
}

void loadPriceHistory() {
  File file = LittleFS.open("/preise.json", "r");
  if (!file) { e5Low = 99.999; e5High = 0.0; e10Low = 99.999; e10High = 0.0; dieselLow = 99.999; dieselHigh = 0.0; return; }
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error) { file.close(); return; }
  e5Low = doc["e5Low"] | 99.999; e5High = doc["e5High"] | 0.0;
  e10Low = doc["e10Low"] | 99.999; e10High = doc["e10High"] | 0.0;
  dieselLow = doc["dieselLow"] | 99.999; dieselHigh = doc["dieselHigh"] | 0.0;
  skiptimer = false;
  file.close();
}

// --------- Hilfsfunktionen ---------

void displayStatus(const char* msg) {
  if (!dma_display || !virtualDisp) return;
  dma_display->clearScreen();
  u8g2.begin(*virtualDisp);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setForegroundColor(0xFFFF);

  String text(msg);
  int maxPixel = FULL_WIDTH - 8;
  std::vector<String> lines;
  while (text.length()) {
    int len = text.length();
    int cut = len;
    while (cut > 0 && u8g2.getUTF8Width(text.substring(0, cut).c_str()) > maxPixel) cut--;
    if (cut == 0) cut = 1;
    lines.push_back(text.substring(0, cut));
    text = text.substring(cut);
    text.trim();
  }
  int totalHeight = lines.size() * 14;
  int y = (FULL_HEIGHT - totalHeight) / 2 + 12;
  for (const auto& l : lines) {
    int x = (FULL_WIDTH - u8g2.getUTF8Width(l.c_str())) / 2;
    u8g2.setCursor(x, y);
    u8g2.print(l);
    y += 14;
  }
  dma_display->flipDMABuffer();
}

// OTA-Statusanzeige
void displayOtaStatus(const String& line1, const String& line2 = "", const String& line3 = "") {
    dma_display->clearScreen();
    u8g2.begin(*virtualDisp);
    u8g2.setFont(u8g2_font_6x13_tf);
    u8g2.setForegroundColor(0x07E0); // grün
    int y = 18;
    if (!line1.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line1); y += 15; }
    if (!line2.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line2); y += 15; }
    if (!line3.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line3); }
    dma_display->flipDMABuffer();
}

void setupOtaDisplayStatus() {
    ArduinoOTA.onStart([]() {
        String type = ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Filesystem";
        displayOtaStatus("OTA Update:", type + " wird geladen", "Bitte warten...");
    });

    ArduinoOTA.onEnd([]() {
        displayOtaStatus("OTA Update:", "Fertig.", "Neustart...");
        delay(1500);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        int percent = (progress * 100) / total;
        String line2 = "Fortschritt: " + String(percent) + "%";
        displayOtaStatus("OTA Update", line2);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        String msg;
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "Auth Fehler"; break;
            case OTA_BEGIN_ERROR:   msg = "Begin Fehler"; break;
            case OTA_CONNECT_ERROR: msg = "Connect Fehler"; break;
            case OTA_RECEIVE_ERROR: msg = "Receive Fehler"; break;
            case OTA_END_ERROR:     msg = "End Fehler"; break;
            default: msg = "Unbekannter Fehler"; break;
        }
        displayOtaStatus("OTA Update Fehler", msg);
        delay(2000);
    });
}

// Robustere Tankerkönig-Abfrage
void fetchStationDataIfNeeded() {
  unsigned long effectiveDelay = firstFetchDone ? timerDelay : initialQueryDelay;
  if ((millis() - lastTime) > effectiveDelay || skiptimer == true) {
    if (TankerkoenigApiKey.length() == 0 || TankstellenID.length() == 0) {
      dataMod.setError("Fehler: API/Station fehlt");
      if (!firstFetchDone) {
        e5Low = e5High = e5;
        e10Low = e10High = e10;
        dieselLow = dieselHigh = diesel;
      }
      firstFetchDone = true;
      skiptimer = false;
      lastTime = millis();
      return;
    }

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String serverPath = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + TankstellenID + "&apikey=" + TankerkoenigApiKey;
      http.begin(serverPath);
      http.setTimeout(4000); // 4s Timeout
      int httpResponseCode = http.GET();
      if (httpResponseCode != 200) {
        dataMod.setError("HTTP Fehler: " + String(httpResponseCode));
        http.end();
        skiptimer = false;
        lastTime = millis();
        return;
      }
      String jsonBuffer = http.getString();
      http.end();

      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonBuffer);
      if (error) {
        dataMod.setError("JSON Fehler");
        skiptimer = false;
        lastTime = millis();
        return;
      }
      bool isOk = doc["ok"];
      if (!isOk) {
        String msg = doc.containsKey("message") ? String((const char*)doc["message"]) : String("API Fehler");
        dataMod.setError("API Fehler: " + msg);
        skiptimer = false;
        lastTime = millis();
        return;
      }
      JsonObject station = doc["station"];
      if (!station["e5"].is<float>() || !station["e10"].is<float>() || !station["diesel"].is<float>()) {
        dataMod.setError("API: unvollst. Daten");
        skiptimer = false;
        lastTime = millis();
        return;
      }
      e5 = station["e5"]; e10 = station["e10"]; diesel = station["diesel"]; stationname = station["name"].as<String>();
      dataMod.setData(stationname, e5, e10, diesel, e5Low, e5High, e10Low, e10High, dieselLow, dieselHigh);

      if (skiptimer == true) {
        if (e5Low > e5High) e5Low = e5;
        if (e5High < e5Low) e5High = e5;
        if (e10Low > e10High) e10Low = e10;
        if (e10High < e10Low) e10High = e10;
        if (dieselLow > dieselHigh) dieselLow = diesel;
        if (dieselHigh < dieselLow) dieselHigh = diesel;
      }
      if (e5 < e5Low) e5Low = e5;
      if (e5 > e5High) e5High = e5;
      if (e10 < e10Low) e10Low = e10;
      if (e10 > e10High) e10High = e10;
      if (diesel < dieselLow) dieselLow = diesel;
      if (diesel > dieselHigh) dieselHigh = diesel;
      savePriceHistory();

      firstFetchDone = true;
    } else {
      dataMod.setError("Kein WLAN");
    }
    skiptimer = false;
    lastTime = millis();
  }
}

// Compose und draw wie gehabt
void composeAndDraw(bool showCalendarNow) {
  clockMod.setTime(timeinfo);
  clockMod.tick();
  clockMod.draw();
  if (showCalendarNow) {
    calendarMod.draw();
  } else {
    dataMod.draw();
  }

  psramBufTimeBytes = (size_t)canvasTime.width() * (size_t)canvasTime.height() * sizeof(uint16_t);
  psramBufDataBytes = (size_t)canvasData.width() * (size_t)canvasData.height() * sizeof(uint16_t);

  if (!psramBufTime) {
    psramBufTime = (uint16_t*) heap_caps_malloc(psramBufTimeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }
  if (!psramBufData) {
    psramBufData = (uint16_t*) heap_caps_malloc(psramBufDataBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  uint16_t *bufTime = canvasTime.getBuffer();
  uint16_t *bufData = canvasData.getBuffer();

  if (psramBufTime) {
    memcpy(psramBufTime, bufTime, psramBufTimeBytes);
    virtualDisp->drawRGBBitmap(0, 0, psramBufTime, canvasTime.width(), canvasTime.height());
  } else {
    virtualDisp->drawRGBBitmap(0, 0, bufTime, canvasTime.width(), canvasTime.height());
  }

  if (psramBufData) {
    memcpy(psramBufData, bufData, psramBufDataBytes);
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, psramBufData, canvasData.width(), canvasData.height());
  } else {
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, bufData, canvasData.width(), canvasData.height());
  }
}

// WLAN: immer stärksten AP nehmen
bool connectToBestWifi(const String& ssid, const String& password) {
  displayStatus("Suche WLAN...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  int n = WiFi.scanNetworks();
  int best_rssi = -1000;
  int best_net = -1;
  uint8_t best_bssid[6] = {0};
  for (int i = 0; i < n; ++i) {
    if (WiFi.SSID(i) == ssid && WiFi.RSSI(i) > best_rssi) {
      best_rssi = WiFi.RSSI(i);
      best_net = i;
      memcpy(best_bssid, WiFi.BSSID(i), 6);
    }
  }
  if (best_net == -1) {
    displayStatus("WLAN nicht gefunden!");
    delay(3000);
    return false;
  }
  displayStatus("WLAN verbinden...");
  WiFi.begin(ssid.c_str(), password.c_str(), 0, best_bssid);
  for (int i = 0; i < 40; ++i) {
    if (WiFi.status() == WL_CONNECTED) break;
    delay(500);
    displayStatus("Verbinde WLAN...");
  }
  if (WiFi.status() == WL_CONNECTED) {
    String msg = "WLAN OK: " + WiFi.localIP().toString();
    displayStatus(msg.c_str());
    delay(1000);
    return true;
  } else {
    displayStatus("WLAN fehlgeschlagen!");
    delay(3000);
    return false;
  }
}

// Warte bis gültige Zeit empfangen wurde
bool waitForTime() {
  displayStatus("Warte auf Uhrzeit...");
  setenv("TZ", "UTC", 1); tzset();
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  for (int i = 0; i < 30; ++i) { // max. 15s warten
    struct tm t;
    if (getLocalTime(&t) && t.tm_year > 120) {
      timeinfo = t; // NEU: immer Berlin-Zeit!
      return true; // Jahr > 2020
    }
    delay(500);
  }
  displayStatus("Keine Zeit erhalten!");
  delay(3000);
  return false;
}

void applyLiveConfig() {
  if (deviceConfig.icsUrl.length()) {
    calendarMod.setICSUrl(deviceConfig.icsUrl);
  }
  calendarMod.setFetchIntervalMinutes(deviceConfig.calendarFetchIntervalMin);
  calendarMod.setScrollStepInterval(deviceConfig.calendarScrollMs);
  calendarMod.setColors(deviceConfig.calendarDateColor, deviceConfig.calendarTextColor);
  calendarDisplayMs = deviceConfig.calendarDisplaySec * 1000UL;
  stationDisplayMs = deviceConfig.stationDisplaySec * 1000UL;
}

// Calendar scroll task
void CalendarScrollTask(void* param) {
  while (true) {
    calendarMod.tickScroll();
    calendarScrollNeedsRedraw = true;
    vTaskDelay(pdMS_TO_TICKS(calendarMod.getScrollStepInterval()));
  }
}

// Calendar ICS Download robust
void robustCalendarUpdateTask(void* param) {
  while (true) {
    calendarMod.robustUpdateIfDue();
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("=== Panelclock startup ===");

  displayStatus("Starte Panelclock...");

  if (!LittleFS.begin(true)) {
    displayStatus("LittleFS Fehler!");
    delay(2000);
  } else {
    loadPriceHistory();
  }

  loadDeviceConfig();

#define RL1 1
#define GL1 2
#define BL1 4
#define RL2 5
#define GL2 6
#define BL2 7
#define CH_A 15
#define CH_B 16
#define CH_C 17
#define CH_D 18
#define CH_E 3
#define CLK 19
#define LAT 20
#define OE 21

  HUB75_I2S_CFG::i2s_pins _pins = { RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, VDISP_NUM_ROWS * VDISP_NUM_COLS, _pins);
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);
  dma_display->clearScreen();

  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
  virtualDisp->setDisplay(*dma_display);

  u8g2.begin(canvasTime);

  bool wifiOK = false;
  if (deviceConfig.ssid.length() > 0) {
    WiFi.setHostname(deviceConfig.hostname.c_str());
    wifiOK = connectToBestWifi(deviceConfig.ssid, deviceConfig.password);
    if (wifiOK) {
      TankerkoenigApiKey = deviceConfig.tankerApiKey;
      TankstellenID = deviceConfig.stationId;
      if (deviceConfig.otaPassword.length() > 0) ArduinoOTA.setPassword(deviceConfig.otaPassword.c_str());
      ArduinoOTA.setHostname(deviceConfig.hostname.c_str());
      setupOtaDisplayStatus();
      ArduinoOTA.begin();
      startConfigPortalIfNeeded(false);
    } else {
      startConfigPortalIfNeeded(true);
    }
  } else {
    startConfigPortalIfNeeded();
  }

  applyLiveConfig();

  bool timeOK = waitForTime();

  if (wifiOK && timeOK) {
    displayStatus("Panelclock Start...");
    delay(2000);
    dma_display->clearScreen();
    composeAndDraw(false);
    lastSwitch = millis();
  }

  xTaskCreatePinnedToCore(CalendarScrollTask, "CalScroll", 2048, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(robustCalendarUpdateTask, "CalICS", 4096, NULL, 2, NULL, 1);
}

void loop() {
  handleConfigPortal();
  ArduinoOTA.handle();
  fetchStationDataIfNeeded();

  unsigned long now = millis();
  unsigned long nextSwitchInterval = showCalendar ? calendarDisplayMs : stationDisplayMs;
  if (now - lastSwitch > nextSwitchInterval) {
    showCalendar = !showCalendar;
    lastSwitch = now;
    composeAndDraw(showCalendar);
  }

  if (calendarScrollNeedsRedraw) {
    calendarScrollNeedsRedraw = false;
    composeAndDraw(showCalendar);
  }

  struct tm utcNow;
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&utcNow)) {
    timeinfo = utcNow; // NEU: immer Berlin-Zeit!
  }

  if (timeinfo.tm_year < 120) {
    displayStatus("Warte auf Uhrzeit...");
    delay(500);
    return;
  }

  delay(10);
}