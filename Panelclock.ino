#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <WiFi.h>
#include "time.h"
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>

// Eigene Header-Dateien
#include "webconfig.hpp"
#include "PsramUtils.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebClientModule.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include "RRuleParser.hpp"

// Globale Objekte
GeneralTimeConverter timeConverter;
bool portalRunning = false;
WebClientModule webClient;

// Display-Konfiguration
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN
const int FULL_WIDTH = PANEL_RES_X * VDISP_NUM_COLS;
const int FULL_HEIGHT = PANEL_RES_Y * VDISP_NUM_ROWS;
const int TIME_AREA_H = 30;
const int DATA_AREA_H = FULL_HEIGHT - TIME_AREA_H;
MatrixPanel_I2S_DMA* dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp = nullptr;
U8G2_FOR_ADAFRUIT_GFX u8g2;
GFXcanvas16 canvasTime(FULL_WIDTH, TIME_AREA_H);
GFXcanvas16 canvasData(FULL_WIDTH, DATA_AREA_H);

// Module
ClockModule clockMod(u8g2, canvasTime);
DataModule dataMod(u8g2, canvasData, TIME_AREA_H, &webClient);
CalendarModule calendarMod(u8g2, canvasData, timeConverter, &webClient);

// Globale Zustandsvariablen
static uint16_t* psramBufTime = nullptr;
static uint16_t* psramBufData = nullptr;
static size_t psramBufTimeBytes = 0;
static size_t psramBufDataBytes = 0;
struct tm timeinfo;
unsigned long calendarDisplayMs = 30000;
unsigned long stationDisplayMs = 30000;
unsigned long lastSwitch = 0;
unsigned long lastClockUpdate = 0;
bool showCalendar = false;
volatile bool calendarScrollNeedsRedraw = false;
volatile bool calendarNeedsRedraw = false;
volatile bool dataNeedsRedraw = false;

void displayStatus(const char* msg) {
  if (!dma_display || !virtualDisp) return;
  dma_display->clearScreen();
  u8g2.begin(*virtualDisp);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setForegroundColor(0xFFFF);
  String text(msg);
  int maxPixel = FULL_WIDTH - 8;
  std::vector<String> lines;
  String currentLine;
  while (text.length() > 0) {
    int nextSpace = text.indexOf(' ');
    String word = (nextSpace == -1) ? text : text.substring(0, nextSpace);
    String testLine = currentLine.length() > 0 ? currentLine + " " + word : word;
    if (u8g2.getUTF8Width(testLine.c_str()) <= maxPixel) {
        currentLine = testLine;
        if (nextSpace != -1) {
            text = text.substring(nextSpace + 1);
        } else {
            text = "";
        }
    } else {
        if (currentLine.length() > 0) {
            lines.push_back(currentLine);
            currentLine = "";
        } else { 
            int cut = word.length();
            while(cut > 0 && u8g2.getUTF8Width(word.substring(0, cut).c_str()) > maxPixel) {
                cut--;
            }
            lines.push_back(word.substring(0, cut));
            text = word.substring(cut) + (nextSpace != -1 ? text.substring(nextSpace) : "");
        }
    }
  }
  if (currentLine.length() > 0) {
      lines.push_back(currentLine);
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

void displayOtaStatus(const String& line1, const String& line2 = "", const String& line3 = "") {
  dma_display->clearScreen();
  u8g2.begin(*virtualDisp);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.setForegroundColor(0x07E0);
  int y = 18;
  if (!line1.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line1); y += 15; }
  if (!line2.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line2); y += 15; }
  if (!line3.isEmpty()) { u8g2.setCursor(4, y); u8g2.print(line3); }
  dma_display->flipDMABuffer();
}

void setupOtaDisplayStatus() {
  ArduinoOTA.onStart([]() { String type = ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Filesystem"; displayOtaStatus("OTA Update:", type + " wird geladen", "Bitte warten..."); });
  ArduinoOTA.onEnd([]() { displayOtaStatus("OTA Update:", "Fertig.", "Neustart..."); delay(1500); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { int percent = (progress * 100) / total; String line2 = "Fortschritt: " + String(percent) + "%"; displayOtaStatus("OTA Update", line2); });
  ArduinoOTA.onError([](ota_error_t error) { String msg; switch (error) { case OTA_AUTH_ERROR: msg = "Auth Fehler"; break; case OTA_BEGIN_ERROR: msg = "Begin Fehler"; break; case OTA_CONNECT_ERROR: msg = "Connect Fehler"; break; case OTA_RECEIVE_ERROR: msg = "Receive Fehler"; break; case OTA_END_ERROR: msg = "End Fehler"; break; default: msg = "Unbekannter Fehler"; } displayOtaStatus("OTA Update Fehler", msg); delay(2000); });
}

void UpdateCalendarTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(10000));
  while (true) {
    if (!portalRunning) {
      calendarMod.update();
    }
    vTaskDelay(pdMS_TO_TICKS(calendarMod.getFetchIntervalMillis()));
  }
}

void UpdateDataTask(void* param) {
  vTaskDelay(pdMS_TO_TICKS(5000));
  while(true) {
    if (!portalRunning) {
      dataMod.fetch();
    }
    vTaskDelay(pdMS_TO_TICKS(dataMod.getFetchIntervalMillis()));
  }
}

void CalendarScrollTask(void* param) {
  while (true) {
    if (portalRunning) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    calendarMod.tickScroll();
    calendarScrollNeedsRedraw = true;
    vTaskDelay(pdMS_TO_TICKS(calendarMod.getScrollStepInterval()));
  }
}

bool connectToWifi() {
  if (deviceConfig.ssid.length() == 0) return false;
  displayStatus("Verbinde WLAN...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());
  int retries = 40;
  while (WiFi.status() != WL_CONNECTED && retries > 0) {
    delay(500);
    Serial.print(".");
    retries--;
  }
  if (WiFi.status() == WL_CONNECTED) {
    String msg = "Verbunden!\nIP: " + WiFi.localIP().toString();
    displayStatus(msg.c_str());
    delay(2000);
    return true;
  } else {
    displayStatus("WLAN fehlgeschlagen!");
    delay(2000);
    return false;
  }
}

bool waitForTime() {
  displayStatus("Warte auf Uhrzeit...");
  // Diese Funktion setzt nur die NTP-Server. Die Zeitzone bleibt unberührt.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  
  time_t now_utc;
  time(&now_utc);
  int retries = 30;
  while (now_utc < 1609459200 && retries > 0) {
    delay(500);
    time(&now_utc);
    retries--;
  }

  if (now_utc < 1609459200) {
    displayStatus("Keine Zeit erhalten!");
    delay(3000);
    return false;
  }
  
  // Die Zeit ist synchronisiert. Die Konvertierung in Lokalzeit passiert jetzt
  // in der loop() unter Verwendung des GeneralTimeConverter.
  return true;
}

void applyLiveConfig() {
  // Diese Funktion setzt die Zeitzonen-Regel, die von GeneralTimeConverter verwendet wird.
  if (!timeConverter.setTimezone(deviceConfig.timezone.c_str())) {
    timeConverter.setTimezone("UTC");
  }
  dataMod.setConfig(deviceConfig.tankerApiKey, deviceConfig.stationId, deviceConfig.stationFetchIntervalMin);
  if (deviceConfig.icsUrl.length()) {
    calendarMod.setICSUrl(deviceConfig.icsUrl);
  }
  calendarMod.setFetchIntervalMinutes(deviceConfig.calendarFetchIntervalMin);
  calendarMod.setScrollStepInterval(deviceConfig.calendarScrollMs);
  calendarMod.setColors(deviceConfig.calendarDateColor, deviceConfig.calendarTextColor);
  calendarDisplayMs = deviceConfig.calendarDisplaySec * 1000UL;
  stationDisplayMs = deviceConfig.stationDisplaySec * 1000UL;
}

void drawClockArea() {
    clockMod.setTime(timeinfo);
    clockMod.tick();
    clockMod.draw();
    
    psramBufTimeBytes = (size_t)canvasTime.width() * (size_t)canvasTime.height() * sizeof(uint16_t);
    if (!psramBufTime) {
        psramBufTime = (uint16_t*)heap_caps_malloc(psramBufTimeBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    
    if (psramBufTime) {
        memcpy(psramBufTime, canvasTime.getBuffer(), psramBufTimeBytes);
        virtualDisp->drawRGBBitmap(0, 0, psramBufTime, canvasTime.width(), canvasTime.height());
    } else {
        virtualDisp->drawRGBBitmap(0, 0, canvasTime.getBuffer(), canvasTime.width(), canvasTime.height());
    }
}

void drawDataArea() {
    if (showCalendar) {
        calendarMod.draw();
    } else {
        dataMod.draw();
    }

    psramBufDataBytes = (size_t)canvasData.width() * (size_t)canvasData.height() * sizeof(uint16_t);
    if (!psramBufData) {
        psramBufData = (uint16_t*)heap_caps_malloc(psramBufDataBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (psramBufData) {
        memcpy(psramBufData, canvasData.getBuffer(), psramBufDataBytes);
        virtualDisp->drawRGBBitmap(0, TIME_AREA_H, psramBufData, canvasData.width(), canvasData.height());
    } else {
        virtualDisp->drawRGBBitmap(0, TIME_AREA_H, canvasData.getBuffer(), canvasData.width(), canvasData.height());
    }
}

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("=== Panelclock startup (Your Design Restored) ===");
  webClient.begin();

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

  displayStatus("Starte...");

  if (!LittleFS.begin(true)) {
    displayStatus("LittleFS Fehler!");
    delay(2000);
    ESP.restart();
  }
  
  loadDeviceConfig();
  applyLiveConfig(); // Zeitzone wird hier gesetzt
  dataMod.begin();

  dataMod.onUpdate([](){
    dataNeedsRedraw = true;
  });

  calendarMod.onUpdate([](){
    calendarNeedsRedraw = true;
  });

  if (connectToWifi()) {
    portalRunning = false;
    
    if (waitForTime()) { // Holt nur UTC-Zeit, ohne die TZ zu ändern
      WiFi.setHostname(deviceConfig.hostname.c_str());
      if (deviceConfig.otaPassword.length() > 0) {
        ArduinoOTA.setPassword(deviceConfig.otaPassword.c_str());
      }
      ArduinoOTA.setHostname(deviceConfig.hostname.c_str());
      setupOtaDisplayStatus();
      ArduinoOTA.begin();

      displayStatus("Start...");
      delay(1000);
      dma_display->clearScreen();
      // Erste Zeitumwandlung für den initialen Draw
      time_t now_utc;
      time(&now_utc);
      time_t local_epoch = timeConverter.toLocal(now_utc);
      localtime_r(&local_epoch, &timeinfo);
      drawClockArea();
      drawDataArea();
      lastSwitch = millis();
    } else {
      displayStatus("Zeitfehler");
    }
  } else {
    portalRunning = true;
    startConfigPortal();
  }

  setupWebServer();
  
  if (!portalRunning) {
    xTaskCreatePinnedToCore(CalendarScrollTask, "CalScroll", 2048, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(UpdateCalendarTask, "CalUpdate", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(UpdateDataTask, "DataUpdate", 4096, NULL, 2, NULL, 1);
  }
}

void loop() {
  handleWebServer();

  if (!portalRunning) {
    ArduinoOTA.handle();

    time_t now_utc;
    time(&now_utc);

    if (now_utc < 1609459200) {
      displayStatus("Warte auf Zeit...");
      delay(500);
      return;
    }

    // *** IHR URSPRÜNGLICHES DESIGN: Die Zeitumwandlung findet hier statt ***
    time_t local_epoch = timeConverter.toLocal(now_utc);
    localtime_r(&local_epoch, &timeinfo);
    
    unsigned long now_ms = millis();

    if (now_ms - lastClockUpdate >= 1000) {
      lastClockUpdate = now_ms;
      drawClockArea();
    }

    bool needsDataRedraw = false;
    unsigned long nextSwitchInterval = showCalendar ? calendarDisplayMs : stationDisplayMs;
    
    if (now_ms - lastSwitch > nextSwitchInterval) {
      showCalendar = !showCalendar;
      lastSwitch = now_ms;
      needsDataRedraw = true;
    }

    if (calendarScrollNeedsRedraw && showCalendar) {
      needsDataRedraw = true;
      calendarScrollNeedsRedraw = false;
    }
    if (calendarNeedsRedraw && showCalendar) {
      needsDataRedraw = true;
      calendarNeedsRedraw = false;
    }
    if (dataNeedsRedraw && !showCalendar) {
      needsDataRedraw = true;
      dataNeedsRedraw = false;
    }

    if (needsDataRedraw) {
      drawDataArea();
    }
  }
  delay(10);
}