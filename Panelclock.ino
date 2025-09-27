/**
  Panelclock.ino
  Main firmware for the Panelclock project.

  Uses:
    - ClockModule.hpp (header-only)
    - DataModule.hpp (header-only)
  Contains:
    - PSRAM copy logic for canvas buffers (heap_caps_malloc)
    - networking/OTA/config portal hooks (reuses your existing webconfig)
*/

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
#include "webconfig.hpp"

#include "ClockModule.hpp"
#include "DataModule.hpp"

#include <esp_heap_caps.h> // heap_caps_malloc / heap_caps_free

// constants
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN

const int FULL_WIDTH = PANEL_RES_X * VDISP_NUM_COLS;
const int FULL_HEIGHT = PANEL_RES_Y * VDISP_NUM_ROWS;
const int TIME_AREA_H = 30;
const int DATA_AREA_H = FULL_HEIGHT - TIME_AREA_H;

// display
MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE> *virtualDisp = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2;

// canvases
GFXcanvas16 canvasTime(FULL_WIDTH, TIME_AREA_H);
GFXcanvas16 canvasData(FULL_WIDTH, DATA_AREA_H);

// modules
ClockModule clockMod(u8g2, canvasTime);
DataModule dataMod(u8g2, canvasData, TIME_AREA_H);

// PSRAM buffers
static uint16_t *psramBufTime = nullptr;
static uint16_t *psramBufData = nullptr;
static size_t psramBufTimeBytes = 0;
static size_t psramBufDataBytes = 0;

// runtime data (kept in main)
String TankerkoenigApiKey = "";
String TankstellenID = "";
float diesel, e5, e10;
float dieselLow, dieselHigh, e5Low, e5High, e10Low, e10High;
String stationname;

unsigned long lastTime = 0;
unsigned long timerDelay = 360000;
unsigned long initialQueryDelay = 5000;
bool firstFetchDone = false;
bool skiptimer = true;

struct tm timeinfo;

// helpers & functions (HTTP, history, fetch)
String httpGETRequest(const char *serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin(serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode > 0) payload = http.getString();
  http.end();
  return payload;
}

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

void fetchStationDataIfNeeded() {
  unsigned long effectiveDelay = firstFetchDone ? timerDelay : initialQueryDelay;
  if ((millis() - lastTime) > effectiveDelay || skiptimer == true) {
    if (WiFi.status() == WL_CONNECTED) {
      String serverPath = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + TankstellenID + "&apikey=" + TankerkoenigApiKey;
      String jsonBuffer = httpGETRequest(serverPath.c_str());
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonBuffer);
      if (error) { Serial.print(F("deserializeJson() failed: " )); Serial.println(error.f_str()); return; }
      bool isOk = doc["ok"];
      if (!isOk) { String msg = doc["message"]; Serial.println(msg); return; }
      JsonObject station = doc["station"];
      e5 = station["e5"]; e10 = station["e10"]; diesel = station["diesel"]; stationname = station["name"].as<String>();
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
    }
    skiptimer = false;
    lastTime = millis();
  }
}

// Compose & draw (with PSRAM copy if available)
void composeAndDraw() {
  // draw into canvases using modules
  clockMod.setTime(timeinfo);
  clockMod.draw();
  dataMod.setData(stationname, e5, e10, diesel, e5Low, e5High, e10Low, e10High, dieselLow, dieselHigh);
  dataMod.draw();

  // compute bytes
  psramBufTimeBytes = (size_t)canvasTime.width() * (size_t)canvasTime.height() * sizeof(uint16_t);
  psramBufDataBytes = (size_t)canvasData.width() * (size_t)canvasData.height() * sizeof(uint16_t);

  // allocate PSRAM buffers lazily
  if (!psramBufTime) {
    psramBufTime = (uint16_t*) heap_caps_malloc(psramBufTimeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (psramBufTime) Serial.printf("PSRAM: allocated time buffer %u bytes\n", (unsigned)psramBufTimeBytes);
  }
  if (!psramBufData) {
    psramBufData = (uint16_t*) heap_caps_malloc(psramBufDataBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (psramBufData) Serial.printf("PSRAM: allocated data buffer %u bytes\n", (unsigned)psramBufDataBytes);
  }

  if (psramBufTime) {
    memcpy(psramBufTime, canvasTime.getBuffer(), psramBufTimeBytes);
    virtualDisp->drawRGBBitmap(0, 0, psramBufTime, canvasTime.width(), canvasTime.height());
  } else {
    virtualDisp->drawRGBBitmap(0, 0, canvasTime.getBuffer(), canvasTime.width(), canvasTime.height());
  }

  if (psramBufData) {
    memcpy(psramBufData, canvasData.getBuffer(), psramBufDataBytes);
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, psramBufData, canvasData.width(), canvasData.height());
  } else {
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, canvasData.getBuffer(), canvasData.width(), canvasData.height());
  }
}

uint8_t oldsec = 255;

void setup() {
  Serial.begin(9600);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed! Trying to format...");
    if (LittleFS.format()) Serial.println("LittleFS formatted and mounted.");
    else Serial.println("LittleFS Formatting Failed! Continuing without FS.");
  } else {
    Serial.println("LittleFS Mounted Successfully.");
    loadPriceHistory();
  }

  // Display init (pins)
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

  virtualDisp->println("Starting...");
  Serial.println("Display initialized.");

  startConfigPortalIfNeeded();

  if (deviceConfig.ssid.length() > 0) {
    WiFi.setHostname(deviceConfig.hostname.c_str());
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (virtualDisp) virtualDisp->print(".");
      counter++;
      if (counter == 40) {
        if (virtualDisp) virtualDisp->println("Wifi Err");
        Serial.println("Wifi Connect Error, will keep config portal active if started.");
        break;
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      TankerkoenigApiKey = deviceConfig.tankerApiKey;
      TankstellenID = deviceConfig.stationId;
      if (deviceConfig.otaPassword.length() > 0) ArduinoOTA.setPassword(deviceConfig.otaPassword.c_str());
      ArduinoOTA.setHostname(deviceConfig.hostname.c_str());
      ArduinoOTA
        .onStart([]() { if (virtualDisp) { virtualDisp->clearScreen(); virtualDisp->setTextSize(2); virtualDisp->setCursor(0,0); virtualDisp->println("OTA UPDATE"); } })
        .onEnd([](){ Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total){ Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error){ Serial.printf("Error[%u]: ", error); });
      ArduinoOTA.begin();
    }
  } else {
    Serial.println("No WiFi configured; config portal should be active.");
  }

  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  delay(500);
  virtualDisp->clearScreen();
}

void loop() {
  handleConfigPortal();
  ArduinoOTA.handle();

  if (WiFi.status() == WL_CONNECTED) {
    getLocalTime(&timeinfo);
    if (oldsec != timeinfo.tm_sec) {
      oldsec = timeinfo.tm_sec;
      fetchStationDataIfNeeded();
      composeAndDraw();
    }
  } else {
    delay(50);
  }
  delay(50);
}