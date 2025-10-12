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

// Eigene Header-Dateien
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "webconfig.hpp"
#include "HardwareConfig.hpp"
#include "WebServerManager.hpp"
#include "PsramUtils.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebClientModule.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include "RRuleParser.hpp"
#include "DartsRankingModule.hpp"
#include "certs.hpp"

// Webserver-Objekte
WebServer server(80);
DNSServer dnsServer;

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

GFXcanvas16* canvasTime = nullptr;
GFXcanvas16* canvasData = nullptr;

ClockModule* clockMod = nullptr;
DataModule* dataMod = nullptr;
CalendarModule* calendarMod = nullptr;
DartsRankingModule* dartsMod = nullptr;

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
  if (!dma_display || !virtualDisp) return;
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

bool connectToWifi() {
    if (deviceConfig.ssid.empty()) return false;
    displayStatus("Suche WLANs...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    if (n == 0) {
        displayStatus("Keine WLANs gefunden!");
        delay(2000);
        return false;
    }
    struct WifiAP { String ssid; String bssid; int32_t rssi; int32_t channel; };
    std::vector<WifiAP> matchingAPs;
    for (int i = 0; i < n; ++i) {
        if (WiFi.SSID(i) == deviceConfig.ssid.c_str()) {
            matchingAPs.push_back({WiFi.SSID(i), WiFi.BSSIDstr(i), WiFi.RSSI(i), WiFi.channel(i)});
        }
    }
    if (matchingAPs.empty()) {
        displayStatus("WLAN nicht gefunden!");
        delay(2000);
        return false;
    }
    std::sort(matchingAPs.begin(), matchingAPs.end(), [](const WifiAP& a, const WifiAP& b) { return a.rssi > b.rssi; });
    
    if (dma_display && virtualDisp) {
        dma_display->clearScreen();
        u8g2.begin(*virtualDisp);
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setForegroundColor(0xFFFF);
        int y = 14;
        String title = "APs fuer " + String(deviceConfig.ssid.c_str());
        int x = (FULL_WIDTH - u8g2.getUTF8Width(title.c_str())) / 2;
        u8g2.setCursor(x, y);
        u8g2.print(title);
        y += 16;
        u8g2.setForegroundColor(0x07E0);
        for (size_t i = 0; i < matchingAPs.size(); ++i) {
            const auto& ap = matchingAPs[i];
            String line = ap.bssid + " (" + String(ap.rssi) + "dBm)";
            if (y < FULL_HEIGHT - 5) { u8g2.setCursor(4, y); u8g2.print(line); y += 14; }
        }
        dma_display->flipDMABuffer();
        delay(2000);
    }

    const auto& bestAP = matchingAPs[0];
    displayStatus("Verbinde mit dem staerksten Signal...");
    uint8_t bssid[6];
    sscanf(bestAP.bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str(), bestAP.channel, bssid);
    int retries = 40;
    while (WiFi.status() != WL_CONNECTED && retries > 0) { delay(500); retries--; }
    if (WiFi.status() == WL_CONNECTED) {
        String msg = "Verbunden!\nIP: " + WiFi.localIP().toString();
        displayStatus(msg.c_str());
        delay(2000);
        return true;
    } else {
        displayStatus("WLAN fehlgeschlagen!");
        WiFi.disconnect();
        delay(2000);
        return false;
    }
}

void applyLiveConfig() {
  configNeedsApplying = true;
  Serial.println("[Config] Live-Konfiguration angefordert. Wird im nächsten Loop-Durchlauf angewendet.");
}

void executeApplyLiveConfig() {
  if (!clockMod || !dataMod || !calendarMod || !dartsMod) return;
  Serial.println("[Config] Wende Live-Konfiguration an...");
  
  if (!timeConverter.setTimezone(deviceConfig.timezone.c_str())) {
    timeConverter.setTimezone("UTC");
  }
  
  dataMod->setConfig(deviceConfig.tankerApiKey, deviceConfig.stationId, deviceConfig.stationFetchIntervalMin);
  calendarMod->setFetchIntervalMinutes(deviceConfig.calendarFetchIntervalMin);
  calendarMod->setICSUrl(deviceConfig.icsUrl);
  dartsMod->applyConfig(deviceConfig.dartsOomEnabled, deviceConfig.dartsProTourEnabled, 5);
  
  calendarMod->setScrollStepInterval(deviceConfig.calendarScrollMs);
  calendarMod->setColors(deviceConfig.calendarDateColor, deviceConfig.calendarTextColor);
  stationDisplayMs = deviceConfig.stationDisplaySec * 1000UL;
  calendarDisplayMs = deviceConfig.calendarDisplaySec * 1000UL;
  
  dartsDisplayMs = deviceConfig.dartsDisplaySec * 1000UL;
  dartsMod->setPageDisplayTime(dartsDisplayMs);
  
  dartsMod->setTrackedPlayers(deviceConfig.trackedDartsPlayers);

  lastSwitch = 0; 
  Serial.println("[Config] Live-Konfiguration angewendet.");
}

void handleNotFound() {
    if (portalRunning && !server.hostHeader().equals(WiFi.softAPIP().toString())) {
        server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        server.send(302, "text/plain", "");
        return;
    }
    PsramString page = String(FPSTR(HTML_PAGE_HEADER)).c_str();
    page += "<h1>404 - Seite nicht gefunden</h1><div class='footer-link'><a href='/'>Zurueck zum Hauptmenue</a></div>";
    page += (const char*)FPSTR(HTML_PAGE_FOOTER);
    server.send(404, "text/html", page.c_str());
}

void updateDisplay() {
    if (!virtualDisp || !dma_display || !canvasTime || !canvasData) return;
    
    virtualDisp->drawRGBBitmap(0, 0, canvasTime->getBuffer(), canvasTime->width(), canvasTime->height());
    virtualDisp->drawRGBBitmap(0, TIME_AREA_H, canvasData->getBuffer(), canvasData->width(), canvasData->height());
    dma_display->flipDMABuffer();
}

void drawClockArea() {
    if (!clockMod) return;
    clockMod->setTime(timeinfo);
    clockMod->tick();
    clockMod->draw();
}

void drawDataArea() {
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
  Serial.println("\n=== Panelclock startup ===");

  logMemoryUsage("Setup Start");

  BaseType_t app_core = xPortGetCoreID();
  BaseType_t network_core = (app_core == 0) ? 1 : 0;
  Serial.printf("App läuft auf Core %d, Netzwerk-Tasks werden auf Core %d ausgeführt.\n", app_core, network_core);

  logMemoryUsage("Vor Canvas PSRAM Allokation");
  uint16_t* timeBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * TIME_AREA_H * sizeof(uint16_t));
  uint16_t* dataBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * DATA_AREA_H * sizeof(uint16_t));

  if (!timeBuffer || !dataBuffer) {
      Serial.println("FATAL: PSRAM-Allokation für Canvases fehlgeschlagen!");
      delay(5000); ESP.restart();
  }
  logMemoryUsage("Nach Canvas PSRAM Allokation");
  
  canvasTime = new GFXcanvas16(FULL_WIDTH, TIME_AREA_H, timeBuffer);
  canvasData = new GFXcanvas16(FULL_WIDTH, DATA_AREA_H, dataBuffer);
  logMemoryUsage("Nach GFXcanvas16 Objekten");
  
  if (!LittleFS.begin(true)) {
    Serial.println("FATAL: LittleFS konnte nicht initialisiert werden. Neustart in 5s.");
    delay(5000);
    ESP.restart();
  }
  logMemoryUsage("Nach LittleFS.begin()");
  
  loadDeviceConfig();
  loadHardwareConfig();
  logMemoryUsage("Nach Configs laden");

  webClient.begin();
  logMemoryUsage("Nach webClient.begin()");

  HUB75_I2S_CFG::i2s_pins _pins = { 
    static_cast<int8_t>(hardwareConfig.R1), static_cast<int8_t>(hardwareConfig.G1), static_cast<int8_t>(hardwareConfig.B1), 
    static_cast<int8_t>(hardwareConfig.R2), static_cast<int8_t>(hardwareConfig.G2), static_cast<int8_t>(hardwareConfig.B2), 
    static_cast<int8_t>(hardwareConfig.A), static_cast<int8_t>(hardwareConfig.B), static_cast<int8_t>(hardwareConfig.C), static_cast<int8_t>(hardwareConfig.D), static_cast<int8_t>(hardwareConfig.E), 
    static_cast<int8_t>(hardwareConfig.LAT), static_cast<int8_t>(hardwareConfig.OE), static_cast<int8_t>(hardwareConfig.CLK) 
  };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, VDISP_NUM_ROWS * VDISP_NUM_COLS, _pins);
  
  mxconfig.double_buff = false;
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;
  mxconfig.clkphase = false;
  
  logMemoryUsage("Vor dma_display Init");
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(128);
  dma_display->clearScreen();
  logMemoryUsage("Nach dma_display Init");
  
  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
  virtualDisp->setDisplay(*dma_display);
  logMemoryUsage("Nach virtualDisp Init");
  
  Serial.println("Display-Initialisierung abgeschlossen.");

  displayStatus("Starte Module...");
  
  logMemoryUsage("Vor Modul-Objekten");
  clockMod = new ClockModule(u8g2, *canvasTime);
  dataMod = new DataModule(u8g2, *canvasData, TIME_AREA_H, &webClient);
  calendarMod = new CalendarModule(u8g2, *canvasData, timeConverter, &webClient);
  dartsMod = new DartsRankingModule(u8g2, *canvasData, &webClient);
  logMemoryUsage("Nach Modul-Objekten");

  dataMod->begin();
  executeApplyLiveConfig(); 
  
  dataMod->onUpdate([](){ dataNeedsRedraw = true; });
  calendarMod->onUpdate([](){ dataNeedsRedraw = true; });
  dartsMod->onUpdate([](DartsRankingType type){
      if (type == DartsRankingType::ORDER_OF_MERIT) dartsOomNeedsRedraw = true;
      if (type == DartsRankingType::PRO_TOUR) dartsProTourNeedsRedraw = true;
  });

  logMemoryUsage("Vor WiFi-Verbindung");
  if (connectToWifi()) {
    portalRunning = false;
    logMemoryUsage("Nach WiFi-Verbindung");
    
    configTime(0, 0, "192.168.188.1", "de.pool.ntp.org");
    Serial.println("NTP-Synchronisation gestartet.");

    WiFi.setHostname(deviceConfig.hostname.c_str());
    if (!deviceConfig.otaPassword.empty()) ArduinoOTA.setPassword(deviceConfig.otaPassword.c_str());
    ArduinoOTA.setHostname(deviceConfig.hostname.c_str());
    setupOtaDisplayStatus();
    ArduinoOTA.begin();
    logMemoryUsage("Nach OTA-Setup");
    
  } else {
    const char* apSsid = "Panelclock-Setup";
    displayStatus(("Kein WLAN, starte\nKonfig-Portal:\n" + String(apSsid)).c_str());
    WiFi.softAP(apSsid);
    portalRunning = true;
    logMemoryUsage("Nach AP-Start");
  }

  setupWebServer(portalRunning);
  logMemoryUsage("Nach WebServer-Setup");
  
  if (!portalRunning) {
    if (!deviceConfig.icsUrl.empty()) {
      xTaskCreatePinnedToCore(CalendarScrollTask, "CalScroll", 2048, NULL, 2, NULL, network_core);
    }
  }
  Serial.println("Setup abgeschlossen.");
  logMemoryUsage("Setup Ende");
}

void loop() {
  unsigned long now_ms = millis();
  if (now_ms - lastLoopDebugTime >= 1000) {
    lastLoopDebugTime = now_ms;
  }

  if (configNeedsApplying) {
      executeApplyLiveConfig();
      configNeedsApplying = false;
  }

  handleWebServer(portalRunning);

  if (portalRunning && !dma_display) {
      delay(100);
      return;
  }

  time_t now_utc;
  time(&now_utc);
  if (now_utc < 1609459200) {
    displayStatus("Warte auf Uhrzeit...");
    delay(500);
    return;
  }

  if (!portalRunning) {
    ArduinoOTA.handle();

    dataMod->queueData();
    dartsMod->queueData();
    calendarMod->queueData();

    dataMod->processData();
    dartsMod->processData();
    calendarMod->processData();

    if (currentDataMode == MODE_DARTS_OOM) {
        dartsMod->tick(DartsRankingType::ORDER_OF_MERIT);
    } else if (currentDataMode == MODE_DARTS_PRO_TOUR) {
        dartsMod->tick(DartsRankingType::PRO_TOUR);
    }
    
    time_t local_epoch = timeConverter.toLocal(now_utc);
    localtime_r(&local_epoch, &timeinfo);

    bool clockNeedsRedraw = false;
    if (now_ms - lastClockUpdate >= 1000) {
      lastClockUpdate = now_ms;
      drawClockArea();
      clockNeedsRedraw = true;
    }
    
    unsigned long currentDisplayDuration = 0;
    switch(currentDataMode) {
        case MODE_STATION: currentDisplayDuration = stationDisplayMs; break;
        case MODE_CALENDAR: currentDisplayDuration = calendarDisplayMs; break;
        case MODE_DARTS_OOM:
            currentDisplayDuration = dartsMod->getRequiredDisplayDuration(DartsRankingType::ORDER_OF_MERIT);
            break;
        case MODE_DARTS_PRO_TOUR:
            currentDisplayDuration = dartsMod->getRequiredDisplayDuration(DartsRankingType::PRO_TOUR);
            break;
    }

    bool dataAreaNeedsRedraw = false;
    if (now_ms - lastSwitch > currentDisplayDuration && currentDisplayDuration > 0) {
      DataAreaMode initialMode = currentDataMode;
      do {
        currentDataMode = static_cast<DataAreaMode>((static_cast<int>(currentDataMode) + 1) % 4);
        if (currentDataMode == MODE_STATION && !deviceConfig.stationId.empty()) break;
        if (currentDataMode == MODE_CALENDAR && !deviceConfig.icsUrl.empty()) break;
        if (currentDataMode == MODE_DARTS_OOM && deviceConfig.dartsOomEnabled) {
            dartsMod->resetPaging();
            break;
        }
        if (currentDataMode == MODE_DARTS_PRO_TOUR && deviceConfig.dartsProTourEnabled) {
            dartsMod->resetPaging();
            break;
        }
      } while (currentDataMode != initialMode);
      lastSwitch = now_ms;
      dataAreaNeedsRedraw = true; 
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