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
#include "webconfig.h"

const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;
const char *time_zone = "CET-1CEST,M3.5.0,M10.5.0/3";

float diesel;
float e5;
float e10;
float dieselLow;
float dieselHigh;
float e5Low;
float e5High;
float e10Low;
float e10High;

String stationname;

// API keys will be taken from deviceConfig at runtime if available
String TankerkoenigApiKey = "";
String TankstellenID = "";

// Abfragetimer, laut Nutzungsbedingungen darf die API alle 5 Minuten abgefragt werden
unsigned long lastTime = 0;
// 6 Minuten (360000)
unsigned long timerDelay = 360000;
// Um die ersten 5 Minuten zu ueberspringen
bool skiptimer = true;

String jsonBuffer;

// RootCA von Tankerkoenig (wie vorher)
const char *root_ca =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

#define PANEL_RES_X 64
#define PANEL_RES_Y 32

#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3

#define PANEL_CHAIN_LEN (VDISP_NUM_ROWS * VDISP_NUM_COLS)
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN
#define SPIRAM_FRAMEBUFFER 1

#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF
#define LGREEN 0x37e0
#define DGREEN 0x0280
#define DBLUE 0x000b
#define DRED 0x6000

const char *hostname = "Panelclock";
struct tm timeinfo;

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel_T<PANEL_CHAIN_TYPE> *virtualDisp = nullptr;

U8G2_FOR_ADAFRUIT_GFX u8g2;

GFXcanvas16 canvas(PANEL_RES_X *VDISP_NUM_COLS, PANEL_RES_Y *VDISP_NUM_ROWS);

uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r / 8) << 11) | ((g / 4) << 5) | (b / 8);
}

void savePriceHistory() {
    StaticJsonDocument<512> doc;
    doc["e5Low"] = e5Low;
    doc["e5High"] = e5High;
    doc["e10Low"] = e10Low;
    doc["e10High"] = e10High;
    doc["dieselLow"] = dieselLow;
    doc["dieselHigh"] = dieselHigh;
    File file = LittleFS.open("/preise.json", "w");
    if (!file) {
        Serial.println("Fehler beim Öffnen der Datei zum Schreiben.");
        return;
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println("Fehler beim Schreiben der JSON-Daten in die Datei.");
    } else {
        Serial.println("Preis-Historie erfolgreich gespeichert.");
    }
    file.close();
}

void loadPriceHistory() {
    File file = LittleFS.open("/preise.json", "r");
    if (!file) {
        Serial.println("Keine Preis-Historie gefunden. Verwende Initialwerte.");
        e5Low = 99.999;
        e5High = 0.0;
        e10Low = 99.999;
        e10High = 0.0;
        dieselLow = 99.999;
        dieselHigh = 0.0;
        return;
    }
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, file);
    if (error) {
        Serial.print(F("Fehler beim Deserialisieren der Preise: "));
        Serial.println(error.f_str());
        file.close();
        return;
    }
    e5Low = doc["e5Low"] | 99.999;
    e5High = doc["e5High"] | 0.0;
    e10Low = doc["e10Low"] | 99.999;
    e10High = doc["e10High"] | 0.0;
    dieselLow = doc["dieselLow"] | 99.999;
    dieselHigh = doc["dieselHigh"] | 0.0;
    Serial.println("Preis-Historie erfolgreich geladen.");
    skiptimer = false;
    file.close();
}

void printData() {
  canvas.drawRect(0, 0, 191, 28, RGB565(50, 50, 50));
  u8g2.setCursor(2, 24);
  u8g2.setFontMode(0);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(MAGENTA);
  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_fub20_tf);
  u8g2.print(&timeinfo, "%H:%M:%S");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(123, 17);
  u8g2.setForegroundColor(YELLOW);
  u8g2.print(&timeinfo, "%d.%m.%Y");
  u8g2.setForegroundColor(RGB565(0, 255, 0));
  u8g2.setCursor(123, 9);
  switch (timeinfo.tm_wday) {
    case 0: u8g2.print("Sonntag"); break;
    case 1: u8g2.print("Montag"); break;
    case 2: u8g2.print("Dienstag"); break;
    case 3: u8g2.print("Mittwoch"); break;
    case 4: u8g2.print("Donnerstag"); break;
    case 5: u8g2.print("Freitag"); break;
    case 6: u8g2.print("Samstag"); break;
    default: break;
  }
  u8g2.setCursor(123, 25);
  u8g2.setForegroundColor(CYAN);
  u8g2.print(&timeinfo, "T:%j KW:%W");
}

String httpGETRequest(const char *serverName) {
  WiFiClient client;
  HTTPClient http;
  http.begin(serverName, root_ca);
  int httpResponseCode = http.GET();
  String payload = "{}";
  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  return payload;
}

void clearLine(int xpos, int ypos) {
  int x, y;
  for (y = ypos; y <= ypos + 7; y++) {
    for (x = xpos; x < 128; x++) {
      virtualDisp->drawPixel(x, y, BLACK);
    }
  }
}

void printTankstelle() {
  byte rval;
  byte gval;
  byte diff;
  int e5farbe = WHITE;
  int e10farbe = WHITE;
  int dieselfarbe = WHITE;
  if ((millis() - lastTime) > timerDelay || skiptimer == true) {
    if (WiFi.status() == WL_CONNECTED) {
      String serverPath = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + TankstellenID + "&apikey=" + TankerkoenigApiKey;
      jsonBuffer = httpGETRequest(serverPath.c_str());
      Serial.println(jsonBuffer);
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, jsonBuffer);
      if (error) { Serial.print(F("deserializeJson() failed: ")); Serial.println(error.f_str()); return; }
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
      if (e5 < e5Low) { e5Low = e5; }
      if (e5 > e5High) { e5High = e5; }
      if (e10 < e10Low) { e10Low = e10; }
      if (e10 > e10High) { e10High = e10; }
      if (diesel < dieselLow) { dieselLow = diesel; }
      if (diesel > dieselHigh) { dieselHigh = diesel; }
      savePriceHistory();
    }
    skiptimer = false;
    lastTime = millis();
  }
  if (e5Low != e5High) {
    diff = round(((e5High - e5) / (e5High - e5Low) * 100));
    if (diff <= 50) { rval = 255; gval = map(diff, 0, 50, 0, 255); }
    else { gval = 255; rval = map(diff, 50, 100, 255, 0); }
    e5farbe = RGB565(rval, gval, 0);
  }
  if (e10Low != e10High) {
    diff = round(((e10High - e10) / (e10High - e10Low) * 100));
    if (diff <= 50) { rval = 255; gval = map(diff, 0, 50, 0, 255); }
    else { gval = 255; rval = map(diff, 50, 100, 255, 0); }
    e10farbe = RGB565(rval, gval, 0);
  }
  if (dieselLow != dieselHigh) {
    diff = round(((dieselHigh - diesel) / (dieselHigh - dieselLow) * 100));
    if (diff <= 50) { rval = 255; gval = map(diff, 0, 50, 0, 255); }
    else { gval = 255; rval = map(diff, 50, 100, 255, 0); }
    dieselfarbe = RGB565(rval, gval, 0);
  }
  u8g2.setFontMode(1);
  u8g2.setForegroundColor(WHITE);
  u8g2.setFont(u8g2_font_8x13_tf);
  u8g2.setCursor(40, 38);
  u8g2.print("Benzinpreise");
  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
  u8g2.setCursor(0, 45);
  u8g2.setForegroundColor(RGB565(128, 128, 128));
  u8g2.print(stationname);
  u8g2.setForegroundColor(YELLOW);
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(2, 58);
  u8g2.println("E5");
  u8g2.setCursor(50, 58);
  u8g2.setForegroundColor(GREEN);
  u8g2.print(e5Low, 3);
  u8g2.print(" €");
  u8g2.setCursor(100, 58);
  u8g2.setForegroundColor(e5farbe);
  u8g2.print(e5, 3);
  u8g2.print(" €");
  u8g2.setCursor(150, 58);
  u8g2.setForegroundColor(RED);
  u8g2.print(e5High, 3);
  u8g2.print(" €");
  u8g2.setForegroundColor(YELLOW);
  u8g2.setCursor(2, 71);
  u8g2.println("E10");
  u8g2.setCursor(50, 71);
  u8g2.setForegroundColor(GREEN);
  u8g2.print(e10Low, 3);
  u8g2.print(" €");
  u8g2.setCursor(100, 71);
  u8g2.setForegroundColor(e10farbe);
  u8g2.print(e10, 3);
  u8g2.print(" €");
  u8g2.setCursor(150, 71);
  u8g2.setForegroundColor(RED);
  u8g2.print(e10High, 3);
  u8g2.print(" €");
  u8g2.setForegroundColor(YELLOW);
  u8g2.setCursor(2, 84);
  u8g2.println("Diesel");
  u8g2.setCursor(50, 84);
  u8g2.setForegroundColor(GREEN);
  u8g2.print(dieselLow, 3);
  u8g2.print(" €");
  u8g2.setCursor(100, 84);
  u8g2.setForegroundColor(dieselfarbe);
  u8g2.print(diesel, 3);
  u8g2.print(" €");
  u8g2.setCursor(150, 84);
  u8g2.setForegroundColor(RED);
  u8g2.print(dieselHigh, 3);
  u8g2.print(" €");
  u8g2.setForegroundColor(YELLOW);
}

void getAllData() {
  getLocalTime(&timeinfo);
}

uint8_t oldsec = 61;

void printAll() {
  if (oldsec != timeinfo.tm_sec) {
    oldsec = timeinfo.tm_sec;
    canvas.fillScreen(0);
    printData();
    printTankstelle();
    virtualDisp->drawRGBBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height());
  }
}

void setup() {

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

  Serial.begin(9600);

  // --- LittleFS Initialisierung ---
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed! Trying to format...");
    if (LittleFS.format()) {
      Serial.println("LittleFS formatted and mounted.");
    } else {
      Serial.println("LittleFS Formatting Failed! Continuing without FS.");
    }
  } else {
    Serial.println("LittleFS Mounted Successfully.");
    loadPriceHistory();
  }

  // Display / DMA init
  HUB75_I2S_CFG::i2s_pins _pins = { RL1, GL1, BL1, RL2, GL2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK };
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN_LEN, _pins);
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(80);
  dma_display->clearScreen();

  virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
  virtualDisp->setDisplay(*dma_display);

  u8g2.begin(canvas);
  virtualDisp->println("Starting...");
  Serial.println("Display initialized.");

  // Start config portal if no stored WiFi credentials
  startConfigPortalIfNeeded();

  // If deviceConfig contains WiFi credentials, try to connect
  if (deviceConfig.ssid.length() > 0) {
    WiFi.setHostname(deviceConfig.hostname.c_str());
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str());
    int counter = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (virtualDisp) virtualDisp->print(".");
      counter++;
      if (counter == 40) { // ~20s timeout
        if (virtualDisp) {
          virtualDisp->println("Wifi Err");
        }
        Serial.println("Wifi Connect Error, will keep config portal active if started.");
        break;
      }
    }
    if (WiFi.status() == WL_CONNECTED) {
      // adopt config values
      TankerkoenigApiKey = deviceConfig.tankerApiKey;
      TankstellenID = deviceConfig.stationId;
      if (deviceConfig.otaPassword.length() > 0) ArduinoOTA.setPassword(deviceConfig.otaPassword.c_str());
      ArduinoOTA.setHostname(deviceConfig.hostname.c_str());
      // ArduinoOTA callbacks
      ArduinoOTA
        .onStart([]() {
          String type;
          if (virtualDisp) {
            virtualDisp->clearScreen();
            virtualDisp->setTextSize(2);
            virtualDisp->setCursor(0, 0);
            virtualDisp->println("OTA UPDATE");
          }
          if (ArduinoOTA.getCommand() == U_FLASH) type = "sketch"; else type = "filesystem";
          Serial.println("Start updating " + type);
        })
        .onEnd([]() { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
        });
      ArduinoOTA.begin();
      virtualDisp->println("");
      virtualDisp->println(WiFi.localIP());
      Serial.print("Connected, IP: ");
      Serial.println(WiFi.localIP());
    }
  } else {
    Serial.println("No WiFi configured; config portal should be active.");
  }

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);
  delay(500);
  virtualDisp->clearScreen();
}

void loop() {
  // Wenn Konfig-Portal aktiv ist, bediene es
  handleConfigPortal();

  // OTA
  ArduinoOTA.handle();

  // Wenn verbunden, führe normale Logik aus
  if (WiFi.status() == WL_CONNECTED) {
    getAllData();
    printAll();
  } else {
    // Portal/scan/connect reagieren lassen
    delay(50);
  }
  delay(50);
}