#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include "time.h"
#include <ESPmDNS.h>
#include <NetworkUdp.h>
#include <ArduinoOTA.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <FS.h>
#include <LittleFS.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <vector>

// Eigene Header-Dateien
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "webconfig.hpp"
#include "HardwareConfig.hpp"
#include "MwaveSensorModule.hpp" 
#include "WebServerManager.hpp"
#include "PsramUtils.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebClientModule.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include "RRuleParser.hpp"
#include "DartsRankingModule.hpp"
#include "FritzboxModule.hpp"
#include "certs.hpp"
#include "MemoryLogger.hpp"
#include "ConnectionManager.hpp"
#include "Application.hpp"

// Definition des globalen Mutex für den Logger
SemaphoreHandle_t serialMutex;

// --- Display-Konfiguration (MUSS GANZ OBEN STEHEN) ---
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN
const int FULL_WIDTH = PANEL_RES_X * VDISP_NUM_COLS;
const int FULL_HEIGHT = PANEL_RES_Y * VDISP_NUM_ROWS;
const int TIME_AREA_H = 30;
const int DATA_AREA_H = FULL_HEIGHT - TIME_AREA_H;

// --- Globale Objekte jetzt als Zeiger ---
WebServer* server = nullptr;
DNSServer* dnsServer = nullptr;
DeviceConfig* deviceConfig = nullptr;
HardwareConfig* hardwareConfig = nullptr;
GeneralTimeConverter* timeConverter = nullptr;
WebClientModule* webClient = nullptr;
MwaveSensorModule* mwaveSensorModule = nullptr;
U8G2_FOR_ADAFRUIT_GFX* u8g2 = nullptr;
ConnectionManager* connectionManager = nullptr;

HardwareSerial sensorSerial(1);

MatrixPanel_I2S_DMA* dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp = nullptr;
GFXcanvas16* canvasTime = nullptr;
GFXcanvas16* canvasData = nullptr;

ClockModule* clockMod = nullptr;
DataModule* dataMod = nullptr;
CalendarModule* calendarMod = nullptr;
DartsRankingModule* dartsMod = nullptr;
FritzboxModule* fritzMod = nullptr;

// --- Globale Variablen ---
bool portalRunning = false;
struct tm timeinfo;
unsigned long calendarDisplayMs = 30000;
unsigned long stationDisplayMs = 30000;
unsigned long dartsDisplayMs = 30000;
unsigned long lastSwitch = 0;
unsigned long lastClockUpdate = 0;
unsigned long lastLoopDebugTime = 0;

enum DataAreaMode {
    MODE_STATION,
    MODE_CALENDAR,
    MODE_DARTS_OOM,
    MODE_DARTS_PRO_TOUR
};
DataAreaMode currentDataMode = MODE_STATION;

volatile bool calendarScrollNeedsRedraw = false;
volatile bool dataNeedsRedraw = false;
volatile bool dartsOomNeedsRedraw = false;
volatile bool dartsProTourNeedsRedraw = false;
volatile bool configNeedsApplying = false;

// --- FUNKTIONEN ---

void displayStatus(const char* msg) {
  if (!dma_display || !virtualDisp || !u8g2) return;
  dma_display->clearScreen();
  u8g2->begin(*virtualDisp);
  u8g2->setFont(u8g2_font_6x13_tf);
  u8g2->setForegroundColor(0xFFFF);
  u8g2->setBackgroundColor(0x0000);
  
  PsramString text(msg);
  int maxPixel = FULL_WIDTH - 8;
  std::vector<PsramString, PsramAllocator<PsramString>> lines;
  PsramString currentLine;

  while (text.length() > 0) {
    int newlinePos = text.find('\n');
    if (newlinePos == 0) {
        text.erase(0, 1);
        continue;
    }

    PsramString segment = (newlinePos == -1) ? text : text.substr(0, newlinePos);
    if (newlinePos != -1) {
        text.erase(0, newlinePos + 1);
    } else {
        text = "";
    }

    int spacePos = 0;
    while(spacePos < segment.length()){
        int nextSpace = segment.find(' ', spacePos);
        if (nextSpace == spacePos) {
            spacePos++;
            continue;
        }
        
        PsramString word = (nextSpace == -1) ? segment.substr(spacePos) : segment.substr(spacePos, nextSpace - spacePos);
        PsramString testLine = currentLine.length() > 0 ? currentLine + " " + word : word;

        if (u8g2->getUTF8Width(testLine.c_str()) <= maxPixel) {
            currentLine = testLine;
        } else {
            if (currentLine.length() > 0) {
                lines.push_back(currentLine);
            }
            currentLine = word;
            while(u8g2->getUTF8Width(currentLine.c_str()) > maxPixel){
                lines.push_back(currentLine.substr(0, currentLine.length() -1));
                currentLine.erase(0, currentLine.length() -1);
            }
        }
        
        if (nextSpace == -1) {
            break;
        }
        spacePos = nextSpace + 1;
    }
    
    if (currentLine.length() > 0) {
        lines.push_back(currentLine);
        currentLine = "";
    }
  }

  int totalHeight = lines.size() * 14;
  int y = (FULL_HEIGHT - totalHeight) / 2 + 12;
  if (y < 12) y = 12;
  for (const auto& l : lines) {
    int x = (FULL_WIDTH - u8g2->getUTF8Width(l.c_str())) / 2;
    u8g2->setCursor(x, y);
    u8g2->print(l.c_str());
    y += 14;
  }
  dma_display->flipDMABuffer();
}


void displayOtaStatus(const String& line1, const String& line2 = "", const String& line3 = "") {
  if (!dma_display || !virtualDisp || !u8g2) return;
  dma_display->clearScreen();
  u8g2->begin(*virtualDisp);
  u8g2->setFont(u8g2_font_6x13_tf);
  u8g2->setForegroundColor(0x07E0);
  int y = 18;
  if (!line1.isEmpty()) { u8g2->setCursor(4, y); u8g2->print(line1); y += 15; }
  if (!line2.isEmpty()) { u8g2->setCursor(4, y); u8g2->print(line2); y += 15; }
  if (!line3.isEmpty()) { u8g2->setCursor(4, y); u8g2->print(line3); }
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
      case OTA_AUTH_ERROR: msg = "Auth Fehler"; break; 
      case OTA_BEGIN_ERROR: msg = "Begin Fehler"; break; 
      case OTA_CONNECT_ERROR: msg = "Verbindungsfehler"; break; 
      case OTA_RECEIVE_ERROR: msg = "Empfangsfehler"; break; 
      case OTA_END_ERROR: msg = "End Fehler"; break; 
      default: msg = "Unbekannter Fehler"; break;
    }
    displayOtaStatus("OTA FEHLER:", msg);
    delay(3000);
  });
}

void CalendarScrollTask(void* param) {
  while (true) {
    if (portalRunning || !calendarMod) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    calendarMod->tickScroll();
    calendarScrollNeedsRedraw = true;
    vTaskDelay(pdMS_TO_TICKS(calendarMod->getScrollStepInterval()));
  }
}

void applyLiveConfig() {
  configNeedsApplying = true;
  Serial.println("[Config] Live-Konfiguration angefordert. Wird im nächsten Loop-Durchlauf angewendet.");
}

void executeApplyLiveConfig() {
  LOG_MEMORY_DETAILED("Vor executeApplyLiveConfig");
  if (!clockMod || !dataMod || !calendarMod || !dartsMod || !fritzMod || !timeConverter || !deviceConfig) return;
  Serial.println("[Config] Wende Live-Konfiguration an...");
  
  if (!timeConverter->setTimezone(deviceConfig->timezone.c_str())) {
    timeConverter->setTimezone("UTC");
  }
  
  dataMod->setConfig(deviceConfig->tankerApiKey, deviceConfig->tankerkoenigStationIds, deviceConfig->stationFetchIntervalMin);
  
  calendarMod->setFetchIntervalMinutes(deviceConfig->calendarFetchIntervalMin);
  calendarMod->setICSUrl(deviceConfig->icsUrl);
  dartsMod->applyConfig(deviceConfig->dartsOomEnabled, deviceConfig->dartsProTourEnabled, 5);
  
  fritzMod->setConfig(deviceConfig->fritzboxEnabled, deviceConfig->fritzboxIp);
  
  calendarMod->setScrollStepInterval(deviceConfig->calendarScrollMs);
  calendarMod->setColors(deviceConfig->calendarDateColor, deviceConfig->calendarTextColor);
  
  stationDisplayMs = deviceConfig->stationDisplaySec * 1000UL;
  dataMod->setPageDisplayTime(stationDisplayMs);

  calendarDisplayMs = deviceConfig->calendarDisplaySec * 1000UL;
  
  dartsDisplayMs = deviceConfig->dartsDisplaySec * 1000UL;
  dartsMod->setPageDisplayTime(dartsDisplayMs);
  
  dartsMod->setTrackedPlayers(deviceConfig->trackedDartsPlayers);

  lastSwitch = 0; 
  Serial.println("[Config] Live-Konfiguration angewendet.");
  LOG_MEMORY_DETAILED("Nach executeApplyLiveConfig");
}

void updateDisplay() {
    if (!mwaveSensorModule) return;
    if (!mwaveSensorModule->isDisplayOn()) {
        if(dma_display) {
            dma_display->clearScreen();
            dma_display->flipDMABuffer();
        }
        return;
    }
    
    if (!virtualDisp || !dma_display || !canvasTime || !canvasData) return;
    
    virtualDisp->drawRGBBitmap(0, 0, canvasTime->getBuffer(), canvasTime->width(), canvasTime->height());
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, canvasData->getBuffer(), canvasData->width(), canvasData->height());
    dma_display->flipDMABuffer();
}

void drawClockArea() {
    if (!clockMod || !mwaveSensorModule) return;
    clockMod->setTime(timeinfo);
    clockMod->setSensorState(mwaveSensorModule->isDisplayOn(), mwaveSensorModule->getLastOnTime(), mwaveSensorModule->getLastOffTime(), mwaveSensorModule->getOnPercentage());
    clockMod->tick();
    clockMod->draw();
}

void drawDataArea() {
    if (fritzMod && fritzMod->isCallActive()) {
        fritzMod->draw();
        return;
    }

    if (!dataMod || !calendarMod || !dartsMod) return;
    switch(currentDataMode) {
        case MODE_STATION:
            dataMod->draw();
            break;
        case MODE_CALENDAR:
            calendarMod->draw();
            break;
        case MODE_DARTS_OOM:
            dartsMod->draw(DartsRankingType::ORDER_OF_MERIT);
            break;
        case MODE_DARTS_PRO_TOUR:
            dartsMod->draw(DartsRankingType::PRO_TOUR);
            break;
    }
}


void setup() {
  Serial.begin(115200);
  delay(1000);

  serialMutex = xSemaphoreCreateMutex();

  Serial.println("\n=== Panelclock startup ===");
  LOG_MEMORY_STRATEGIC("Setup: Start");

  // --- PHASE 1: GRUNDLAGEN (PSRAM, FS, Hardware Config) ---
  if (!psramInit()) { Serial.println("FATAL: PSRAM konnte nicht initialisiert werden!"); delay(5000); ESP.restart(); }
  if (!LittleFS.begin(true)) { Serial.println("FATAL: LittleFS konnte nicht initialisiert werden."); delay(5000); ESP.restart(); }
  hardwareConfig = new HardwareConfig();
  loadHardwareConfig();
  
  // --- PHASE 2: DISPLAY INITIALISIERUNG ---
  u8g2 = new U8G2_FOR_ADAFRUIT_GFX();
  uint16_t* timeBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * TIME_AREA_H * sizeof(uint16_t));
  uint16_t* dataBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * DATA_AREA_H * sizeof(uint16_t));
  if (!timeBuffer || !dataBuffer) { Serial.println("FATAL: PSRAM-Allokation für Canvases fehlgeschlagen!"); delay(5000); ESP.restart(); }
  canvasTime = new GFXcanvas16(FULL_WIDTH, TIME_AREA_H, timeBuffer);
  canvasData = new GFXcanvas16(FULL_WIDTH, DATA_AREA_H, dataBuffer);
  HUB75_I2S_CFG::i2s_pins _pins = { 
    static_cast<int8_t>(hardwareConfig->R1), static_cast<int8_t>(hardwareConfig->G1), static_cast<int8_t>(hardwareConfig->B1), 
    static_cast<int8_t>(hardwareConfig->R2), static_cast<int8_t>(hardwareConfig->G2), static_cast<int8_t>(hardwareConfig->B2), 
    static_cast<int8_t>(hardwareConfig->A), static_cast<int8_t>(hardwareConfig->B), static_cast<int8_t>(hardwareConfig->C), static_cast<int8_t>(hardwareConfig->D), static_cast<int8_t>(hardwareConfig->E), 
    static_cast<int8_t>(hardwareConfig->LAT), static_cast<int8_t>(hardwareConfig->OE), static_cast<int8_t>(hardwareConfig->CLK) 
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, VDISP_NUM_ROWS * VDISP_NUM_COLS, _pins);
  mxconfig.double_buff = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;
  mxconfig.clkphase = false;
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);
  dma_display->clearScreen();
  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
  virtualDisp->setDisplay(*dma_display);
  Serial.println("Display-Initialisierung abgeschlossen.");
  displayStatus("Systemstart...");

  // --- PHASE 3: ERSTELLE ALLE OBJEKTE ---
  displayStatus("Erstelle Module...");
  deviceConfig = new DeviceConfig();
  loadDeviceConfig();
  connectionManager = new ConnectionManager(*deviceConfig);
  timeConverter = new GeneralTimeConverter();
  mwaveSensorModule = new MwaveSensorModule(*deviceConfig, *hardwareConfig, sensorSerial);
  webClient = new WebClientModule();
  clockMod = new ClockModule(*u8g2, *canvasTime, *timeConverter);
  dataMod = new DataModule(*u8g2, *canvasData, *timeConverter, TIME_AREA_H, webClient);
  calendarMod = new CalendarModule(*u8g2, *canvasData, *timeConverter, webClient);
  dartsMod = new DartsRankingModule(*u8g2, *canvasData, webClient);
  fritzMod = new FritzboxModule(*u8g2, *canvasData, webClient);
  dnsServer = new DNSServer();
  server = new WebServer(80);
  
  // --- PHASE 4: VERBINDUNG HERSTELLEN & MODULE INITIALISIEREN ---
  if (connectionManager->begin()) { // Dieser Aufruf blockiert bis WLAN und ZEIT bereit sind
    portalRunning = false;
    LOG_MEMORY_DETAILED("Nach WiFi & NTP");

    // *** KORREKTE REIHENFOLGE: Starte ALLE Modul-Tasks ERST JETZT ***
    BaseType_t app_core = xPortGetCoreID();
    BaseType_t network_core = (app_core == 0) ? 1 : 0;
    
    Serial.println("Initialisiere Module (Post-Net)...");
    mwaveSensorModule->begin();
    dataMod->begin();
    webClient->begin();
    fritzMod->begin(network_core);

    WiFi.setHostname(deviceConfig->hostname.c_str());
    if (!deviceConfig->otaPassword.empty()) ArduinoOTA.setPassword(deviceConfig->otaPassword.c_str());
    ArduinoOTA.setHostname(deviceConfig->hostname.c_str());
    setupOtaDisplayStatus();
    ArduinoOTA.begin();
    LOG_MEMORY_DETAILED("Nach OTA-Setup");
    
    setupWebServer(portalRunning);
    LOG_MEMORY_DETAILED("Nach WebServer-Setup");
    
  } else { // Fallback, wenn keine WLAN-Verbindung möglich ist
    const char* apSsid = "Panelclock-Setup";
    displayStatus(("Kein WLAN, starte\nKonfig-Portal:\n" + String(apSsid)).c_str());
    WiFi.softAP(apSsid);
    portalRunning = true;
    LOG_MEMORY_DETAILED("Nach AP-Start");
    
    // Im AP-Modus starten wir nur die Module, die keine Netzwerkverbindung brauchen
    mwaveSensorModule->begin();
    // dataMod->begin() hier NICHT aufrufen, da es Caches laden könnte, die evtl. Netzwerk erfordern
    
    setupWebServer(portalRunning);
    LOG_MEMORY_DETAILED("Nach WebServer-Setup (AP Mode)");
  }
  LOG_MEMORY_STRATEGIC("Nach Netzwerk-Stack Init");

  // --- PHASE 5: ABSCHLUSS ---
  executeApplyLiveConfig(); 
  
  dataMod->onUpdate([](){ dataNeedsRedraw = true; });
  calendarMod->onUpdate([](){ dataNeedsRedraw = true; });
  dartsMod->onUpdate([](DartsRankingType type){
      if (type == DartsRankingType::ORDER_OF_MERIT) dartsOomNeedsRedraw = true;
      if (type == DartsRankingType::PRO_TOUR) dartsProTourNeedsRedraw = true;
  });
  
  if (!portalRunning) {
    if (deviceConfig && !deviceConfig->icsUrl.empty()) {
      LOG_MEMORY_DETAILED("Vor CalendarScrollTask Start");
      BaseType_t app_core = xPortGetCoreID();
      BaseType_t network_core = (app_core == 0) ? 1 : 0;
      xTaskCreatePinnedToCore(CalendarScrollTask, "CalScroll", 1536, NULL, 2, NULL, network_core);
      LOG_MEMORY_DETAILED("Nach CalendarScrollTask Start");
    }
  }
  LOG_MEMORY_STRATEGIC("Nach Anwendungslogik Init");
  
  displayStatus("Startvorgang abgeschlossen.");
  Serial.println("Setup abgeschlossen.");
  LOG_MEMORY_STRATEGIC("Setup: Ende");
}


void loop() {
  unsigned long now_ms = millis();
  if (now_ms - lastLoopDebugTime >= 10000) {
    LOG_MEMORY_DETAILED("Loop-Heartbeat");
    lastLoopDebugTime = now_ms;
  }

  if (configNeedsApplying) {
      executeApplyLiveConfig();
      configNeedsApplying = false;
  }

  if (server) server->handleClient();
  if (portalRunning && dnsServer) dnsServer->processNextRequest();


  if (portalRunning) {
      delay(10);
      return;
  }

  if (connectionManager) connectionManager->update();

  time_t now_utc;
  time(&now_utc);

  if(mwaveSensorModule) mwaveSensorModule->update(now_utc);

  if (!portalRunning) {
    ArduinoOTA.handle();

    if (!fritzMod || !fritzMod->isCallActive()) {
        if(dataMod) dataMod->queueData();
        if(dartsMod) dartsMod->queueData();
        if(calendarMod) calendarMod->queueData();
    }

    if(dataMod) dataMod->processData();
    if(dartsMod) dartsMod->processData();
    if(calendarMod) calendarMod->processData();

    if (currentDataMode == MODE_STATION) {
        if(dataMod) dataMod->tick();
    } else if (currentDataMode == MODE_DARTS_OOM) {
        if(dartsMod) dartsMod->tick(DartsRankingType::ORDER_OF_MERIT);
    } else if (currentDataMode == MODE_DARTS_PRO_TOUR) {
        if(dartsMod) dartsMod->tick(DartsRankingType::PRO_TOUR);
    }
    
    if(timeConverter) {
        time_t local_epoch = timeConverter->toLocal(now_utc);
        localtime_r(&local_epoch, &timeinfo);
    } else {
        localtime_r(&now_utc, &timeinfo);
    }

    bool clockNeedsRedraw = false;
    if (now_ms - lastClockUpdate >= 1000) {
      lastClockUpdate = now_ms;
      drawClockArea();
      clockNeedsRedraw = true;
    }
    
    if (mwaveSensorModule && !mwaveSensorModule->isDisplayOn()) {
        if (clockNeedsRedraw) {
            updateDisplay();
        }
        delay(100);
        return;
    }
    
    bool dataAreaNeedsRedraw = false;

    if (fritzMod && fritzMod->redrawNeeded()) {
        dataAreaNeedsRedraw = true;
    }

    if (!fritzMod || !fritzMod->isCallActive()) {
        unsigned long currentDisplayDuration = 0;
        switch(currentDataMode) {
            case MODE_STATION: 
                currentDisplayDuration = dataMod ? dataMod->getRequiredDisplayDuration() : 0;
                break;
            case MODE_CALENDAR: 
                currentDisplayDuration = calendarDisplayMs; 
                break;
            case MODE_DARTS_OOM:
                currentDisplayDuration = dartsMod ? dartsMod->getRequiredDisplayDuration(DartsRankingType::ORDER_OF_MERIT) : 0;
                break;
            case MODE_DARTS_PRO_TOUR:
                currentDisplayDuration = dartsMod ? dartsMod->getRequiredDisplayDuration(DartsRankingType::PRO_TOUR) : 0;
                break;
        }

        if (now_ms - lastSwitch > currentDisplayDuration && currentDisplayDuration > 0) {
          DataAreaMode initialMode = currentDataMode;
          do {
            currentDataMode = static_cast<DataAreaMode>((static_cast<int>(currentDataMode) + 1) % 4);
            if (currentDataMode == MODE_STATION && deviceConfig && !deviceConfig->tankerkoenigStationIds.empty()) {
                if(dataMod) dataMod->resetPaging();
                break;
            }
            if (currentDataMode == MODE_CALENDAR && deviceConfig && !deviceConfig->icsUrl.empty()) break;
            if (currentDataMode == MODE_DARTS_OOM && deviceConfig && deviceConfig->dartsOomEnabled) {
                if(dartsMod) dartsMod->resetPaging();
                break;
            }
            if (currentDataMode == MODE_DARTS_PRO_TOUR && deviceConfig && deviceConfig->dartsProTourEnabled) {
                if(dartsMod) dartsMod->resetPaging();
                break;
            }
          } while (currentDataMode != initialMode);
          lastSwitch = now_ms;
          dataAreaNeedsRedraw = true; 
        }
    }
    
    if (dataNeedsRedraw && currentDataMode == MODE_STATION) {
        dataAreaNeedsRedraw = true;
        dataNeedsRedraw = false;
    }
    if (calendarScrollNeedsRedraw && currentDataMode == MODE_CALENDAR) {
        dataAreaNeedsRedraw = true;
        calendarScrollNeedsRedraw = false;
    }
    if (dartsOomNeedsRedraw || dartsProTourNeedsRedraw) {
        if ((currentDataMode == MODE_DARTS_OOM && dartsOomNeedsRedraw) || (currentDataMode == MODE_DARTS_PRO_TOUR && dartsProTourNeedsRedraw)) {
             dataAreaNeedsRedraw = true;
        }
        dartsOomNeedsRedraw = false;
        dartsProTourNeedsRedraw = false;
    }

    if (dataAreaNeedsRedraw) {
      drawDataArea();
    }

    if (clockNeedsRedraw || dataAreaNeedsRedraw) {
      updateDisplay();
    }
  }
  delay(10);
}