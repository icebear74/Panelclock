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

/*
  IMPORTANT: Target hardware requirement

  This firmware is intended to run on ESP32 boards with PSRAM (e.g. ESP32-S3 N8R8 or equivalent).
  A runtime check only verifies presence of PSRAM (not size). If PSRAM is not detected the firmware
  halts to avoid instability (HUB75 DMA framebuffers + LittleFS + WebServer require PSRAM).
*/

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

// Abfragetimer: danach wird der normale Intervall verwendet
unsigned long lastTime = 0;
// 6 Minuten (360000 ms) regulärer Abstand
unsigned long timerDelay = 360000;
// initial schnellerer Abfrage-Intervall (z.B. 5 Sekunden)
unsigned long initialQueryDelay = 5000;
// Flag: erste schnelle Abfrage noch nicht erfolgt
bool firstFetchDone = false;

// Um die ersten 5 Minuten zu ueberspringen (initial min/max init)
bool skiptimer = true;

String jsonBuffer;

// RootCA von Tankerkoenig
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

// helper: draw price with 3 decimals; third decimal rendered small & superscript
static void drawPrice(int x, int baselineY, float price, uint16_t color) {
  // Format to exactly 3 decimals
  char buf[32];
  // Use C formatting to avoid locale surprises
  snprintf(buf, sizeof(buf), "%.3f", price);
  String s = String(buf);
  int dotPos = s.indexOf('.');
  String mainPart;
  String thirdDigit;
  if (dotPos >= 0 && (int)s.length() >= dotPos + 4) {
    // mainPart includes two decimals: up to dotPos+3 (exclusive)
    mainPart = s.substring(0, dotPos + 3); // e.g. "1.45"
    thirdDigit = s.substring(dotPos + 3, dotPos + 4); // e.g. "9"
  } else {
    // fallback: just show as-is
    mainPart = s;
    thirdDigit = "";
  }

  // Font metrics (fixed for these fonts):
  const int MAIN_W = 6;  // u8g2_font_6x13_me approx width per char
  const int SMALL_W = 4; // u8g2_font_tom_thumb_4x6_tf approx width per char
  const int SMALL_SUPER_OFFSET = 6; // pixels to raise the small digit above baseline

  // Draw main part (numbers with two decimals)
  u8g2.setForegroundColor(color);
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(x, baselineY);
  u8g2.print(mainPart);

  int wMain = mainPart.length() * MAIN_W;

  // Draw third decimal as small, superscripted
  if (thirdDigit.length() > 0) {
    u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
    u8g2.setCursor(x + wMain, baselineY - SMALL_SUPER_OFFSET);
    u8g2.print(thirdDigit);
  }

  int wSmall = (thirdDigit.length() > 0) ? (thirdDigit.length() * SMALL_W) : 0;

  // Draw euro sign aligned to main baseline (slightly separated)
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(x + wMain + wSmall + 2, baselineY);
  u8g2.print(" €");
}

// Computes ISO-8601 week number (1..53) for the given tm.
static int isoWeekNumber(const struct tm &t) {
  struct tm tmp = t; // make a modifiable copy
  time_t tt = mktime(&tmp);
  if (tt == (time_t)-1) return 0;
  struct tm normalized;
  localtime_r(&tt, &normalized);

  // Convert Sunday(0)..Saturday(6) to ISO weekday 1..7
  int wday = normalized.tm_wday == 0 ? 7 : normalized.tm_wday;

  // Find Thursday of current week
  int daysToThursday = 4 - wday;
  time_t thursday_tt = tt + (time_t)daysToThursday * 24 * 3600;
  struct tm thursday_tm;
  localtime_r(&thursday_tt, &thursday_tm);

  int isoYear = thursday_tm.tm_year + 1900;

  // Jan 1 of isoYear
  struct tm jan1 = {0};
  jan1.tm_year = isoYear - 1900;
  jan1.tm_mon = 0;
  jan1.tm_mday = 1;
  jan1.tm_hour = 0;
  jan1.tm_min = 0;
  jan1.tm_sec = 0;
  time_t jan1_tt = mktime(&jan1);
  if (jan1_tt == (time_t)-1) return 0;
  struct tm jan1_tm;
  localtime_r(&jan1_tt, &jan1_tm);
  int jan1_wday = jan1_tm.tm_wday == 0 ? 7 : jan1_tm.tm_wday; // 1=Mon..7=Sun

  // week1 starts on the Monday of the week that contains Jan 4:
  int week1_monday_offset = (jan1_wday <= 4) ? (1 - jan1_wday) : (8 - jan1_wday);
  time_t week1_monday_tt = jan1_tt + (time_t)week1_monday_offset * 24 * 3600;

  int weekno = (int)((thursday_tt - week1_monday_tt) / (7 * 24 * 3600)) + 1;
  return weekno;
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
  // enlarge top frame height by 2 pixels (was 28, now 30)
  canvas.drawRect(0, 0, 191, 30, RGB565(50, 50, 50));

  // Time: moved 1 pixel down (was y=24)
  u8g2.setCursor(2, 25);
  u8g2.setFontMode(0);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(MAGENTA);
  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_fub20_tf);
  u8g2.print(&timeinfo, "%H:%M:%S");

  // Date: moved 1 pixel down (was y=17)
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setCursor(123, 18);
  u8g2.setForegroundColor(YELLOW);
  u8g2.print(&timeinfo, "%d.%m.%Y");

  u8g2.setForegroundColor(RGB565(0, 255, 0));
  // Weekday label remains at y=9 (unchanged)
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

  // Day-of-year and KW: moved 2 pixels down (was y=25 -> now y=27)
  u8g2.setCursor(123, 27);
  u8g2.setForegroundColor(CYAN);
  u8g2.print(&timeinfo, "T:%j ");
  {
    char wkbuf[8];
    int kw = isoWeekNumber(timeinfo);
    if (kw < 1) kw = 1; // safety
    snprintf(wkbuf, sizeof(wkbuf), "KW:%02d", kw);
    u8g2.print(wkbuf);
  }
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

  // choose effective delay: initialQueryDelay until firstFetchDone, otherwise timerDelay
  unsigned long effectiveDelay = firstFetchDone ? timerDelay : initialQueryDelay;

  if ((millis() - lastTime) > effectiveDelay || skiptimer == true) {
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

      // mark that initial fast fetch has occurred
      firstFetchDone = true;
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

  // Benzinpreise heading: moved 2 pixels down (was y=38 -> now 40)
  u8g2.setCursor(40, 40);
  u8g2.print("Benzinpreise");

  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);

  // Station name: moved 2 pixels down (was y=45 -> now 47)
  u8g2.setCursor(0, 47);
  u8g2.setForegroundColor(RGB565(128, 128, 128));
  u8g2.print(stationname);

  u8g2.setForegroundColor(YELLOW);
  // Use drawPrice for amounts with superscript third decimal (baseline-aligned)
  // E5: baseline y = 60
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(2, 60);
  u8g2.print("E5");
  drawPrice(50, 60, e5Low, GREEN);
  drawPrice(100, 60, e5, e5farbe);
  drawPrice(150, 60, e5High, RED);

  // E10: baseline y = 73
  u8g2.setForegroundColor(YELLOW);
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(2, 73);
  u8g2.print("E10");
  drawPrice(50, 73, e10Low, GREEN);
  drawPrice(100, 73, e10, e10farbe);
  drawPrice(150, 73, e10High, RED);

  // Diesel: baseline y = 86
  u8g2.setForegroundColor(YELLOW);
  u8g2.setFont(u8g2_font_6x13_me);
  u8g2.setCursor(2, 86);
  u8g2.print("Diesel");
  drawPrice(50, 86, dieselLow, GREEN);
  drawPrice(100, 86, diesel, dieselfarbe);
  drawPrice(150, 86, dieselHigh, RED);

  u8g2.setForegroundColor(YELLOW);
}

void getAllData() {
  getLocalTime(&timeinfo);
}

uint8_t oldsec = 61;

// Show a short "connected" message and IP on the matrix panel and wait ~5s.
// While waiting, service the config portal and OTA so device remains responsive.
static void showIPOnPanel(const String &ip) {
  if (!virtualDisp) return;

  virtualDisp->clearScreen();
  virtualDisp->setTextSize(2);
  virtualDisp->setCursor(0, 0);
  virtualDisp->println("WLAN");
  virtualDisp->println("verbunden");

  virtualDisp->setTextSize(1);
  virtualDisp->println("");
  virtualDisp->println(ip);

  unsigned long start = millis();
  while (millis() - start < 5000) {
    handleConfigPortal();
    ArduinoOTA.handle();
    delay(10);
  }
  virtualDisp->clearScreen();
}

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

  Serial.begin(9600); // Baudrate

  // --- PSRAM-Prüfung: nur Vorhandensein prüfen (direkt nach Serial.begin)
  {
    size_t psram_sz = 0;
    #if defined(ESP_HAS_PSRAM) || defined(ESP32)
      psram_sz = ESP.getPsramSize();
    #endif
    Serial.printf("PSRAM detected: %s\n", (psram_sz > 0) ? "yes" : "no");
    if (psram_sz == 0) {
      Serial.println();
      Serial.println("********************************************************");
      Serial.println("ERROR: PSRAM not detected. This firmware requires an ESP32 board with PSRAM.");
      Serial.println("Recommended target: ESP32-S3 N8R8 (or equivalent) with PSRAM enabled.");
      Serial.println("Halting to prevent runtime instability.");
      Serial.println("********************************************************");
      while (true) { delay(1000); } // stop here on unsupported hardware
    }
  }

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
  dma_display->setBrightness8(128);
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

      // Show connected message and IP on the matrix for 5 seconds
      showIPOnPanel(WiFi.localIP().toString());

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
  static int lastWiFiStatus = WL_DISCONNECTED;

  // Falls das Konfig-Portal aktiv ist, bediene es
  handleConfigPortal();

  // OTA Handler (funktioniert nur wenn ArduinoOTA.begin() vorher aufgerufen wurde)
  ArduinoOTA.handle();

  // Detect transition to connected to show IP on panel (if connection happens after boot)
  int st = WiFi.status();
  if (st == WL_CONNECTED && lastWiFiStatus != WL_CONNECTED) {
    // show IP message for 5 seconds (services handled inside)
    showIPOnPanel(WiFi.localIP().toString());
  }
  lastWiFiStatus = st;

  // Wenn verbunden, führe normale Logik aus
  if (WiFi.status() == WL_CONNECTED) {
    getAllData();
    printAll();
  } else {
    // Optional: kurze Ruhe, damit Portal/Scan/Connect reagieren kann
    delay(50);
  }
  delay(50);
}