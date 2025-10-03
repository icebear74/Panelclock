#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

#if __has_include("gfxcanvas.h")
  #include "gfxcanvas.h"
#elif __has_include(<gfxcanvas.h>)
  #include <gfxcanvas.h>
#elif __has_include("GFXcanvas.h")
  #include "GFXcanvas.h"
#elif __has_include(<GFXcanvas.h>)
  #include <GFXcanvas.h>
#elif __has_include("Adafruit_GFX.h")
  #include <Adafruit_GFX.h>
#else
  #error "gfxcanvas header not found. Please install the Adafruit GFX library (provides GFXcanvas16 / gfxcanvas.h)."
#endif

#include <cmath>

class DataModule {
public:
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, int timeAreaHeight)
        : u8g2(u8g2), canvas(canvas), timeAreaH(timeAreaHeight) {}

    void begin() {
        loadPriceHistory();
    }

    void setConfig(const String& apiKey, const String& stationId, unsigned long updateIntervalMinutes) {
        this->tankerkoenigApiKey = apiKey;
        this->tankstellenId = stationId;
        this->timerDelay = updateIntervalMinutes * 60 * 1000;
    }

    void update() {
        // Decide if it's time to fetch data
        unsigned long effectiveDelay = firstFetchDone ? timerDelay : initialQueryDelay;
        if (millis() - lastTime < effectiveDelay && !forceUpdate) {
            return;
        }

        forceUpdate = false;
        lastTime = millis();

        // Pre-flight checks
        if (tankerkoenigApiKey.length() == 0 || tankstellenId.length() == 0) {
            setError("Fehler: API/Station fehlt");
            if (!firstFetchDone) {
                e5Low = e5High = e5;
                e10Low = e10High = e10;
                dieselLow = dieselHigh = diesel;
            }
            firstFetchDone = true;
            return;
        }

        if (WiFi.status() != WL_CONNECTED) {
            setError("Kein WLAN");
            return;
        }

        // Fetch data with retries
        fetchStationData();
    }

    void draw() {
        canvas.fillScreen(0);
        canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), rgb565(50,50,50));
        u8g2.begin(canvas);

        // heading
        u8g2.setFont(u8g2_font_8x13_tf);
        u8g2.setForegroundColor(WHITE);
        u8g2.setCursor(40, 40 - timeAreaH);
        u8g2.print("Benzinpreise");

        // station / status or error
        u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
        if (hasError) {
            u8g2.setForegroundColor(RED);
            u8g2.setCursor(2, 47 - timeAreaH);
            String s = errorMsg;
            if (s.length() > 30) s = s.substring(0, 30) + "...";
            u8g2.print(s);
            u8g2.setForegroundColor(rgb565(200,200,200));
            u8g2.setCursor(2, 54 - timeAreaH);
            u8g2.print("Portal: 192.168.4.1");
            return; // Don't draw prices if there's an error
        } else {
            u8g2.setForegroundColor(rgb565(128,128,128));
            u8g2.setCursor(3, 47 - timeAreaH);
            u8g2.print(stationname);
        }

        // prices: baselines
        u8g2.setForegroundColor(YELLOW);
        // E5
        int e5_baseline = 60 - timeAreaH;
        u8g2.setFont(u8g2_font_6x13_me);
        u8g2.setCursor(3, e5_baseline);
        u8g2.print("E5");
        drawPrice(48, e5_baseline, e5Low, GREEN);
        uint16_t e5col = calcColor(e5, e5Low, e5High);
        drawPrice(98, e5_baseline, e5, e5col);
        drawPrice(148, e5_baseline, e5High, RED);

        // E10
        int e10_baseline = 73 - timeAreaH;
        u8g2.setForegroundColor(YELLOW);
        u8g2.setFont(u8g2_font_6x13_me);
        u8g2.setCursor(3, e10_baseline);
        u8g2.print("E10");
        drawPrice(48, e10_baseline, e10Low, GREEN);
        uint16_t e10col = calcColor(e10, e10Low, e10High);
        drawPrice(98, e10_baseline, e10, e10col);
        drawPrice(148, e10_baseline, e10High, RED);

        // Diesel
        int diesel_baseline = 86 - timeAreaH;
        u8g2.setForegroundColor(YELLOW);
        u8g2.setFont(u8g2_font_6x13_me);
        u8g2.setCursor(3, diesel_baseline);
        u8g2.print("Diesel");
        drawPrice(48, diesel_baseline, dieselLow, GREEN);
        uint16_t dcol = calcColor(diesel, dieselLow, dieselHigh);
        drawPrice(98, diesel_baseline, diesel, dcol);
        drawPrice(148, diesel_baseline, dieselHigh, RED);
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    int timeAreaH;

    // Configuration
    String tankerkoenigApiKey;
    String tankstellenId;

    // Data
    String stationname;
    float e5=0, e10=0, diesel=0;
    float e5Low=99.999, e5High=0, e10Low=99.999, e10High=0, dieselLow=99.999, dieselHigh=0;

    // State
    bool hasError = false;
    String errorMsg;
    unsigned long lastTime = 0;
    unsigned long timerDelay = 360000; // default 6 minutes
    unsigned long initialQueryDelay = 5000; // 5 seconds
    bool firstFetchDone = false;
    bool forceUpdate = true; // force update on first run

    // Constants
    static constexpr uint16_t WHITE = 0xFFFF;
    static constexpr uint16_t YELLOW = 0xFFE0;
    static constexpr uint16_t GREEN = 0x07E0;
    static constexpr uint16_t RED = 0xF800;
    static constexpr int HTTP_RETRIES = 3;
    static constexpr int HTTP_RETRY_DELAY_MS = 10000;
    static constexpr int HTTP_TIMEOUT_MS = 4000;

    void setError(const String &msg) {
        hasError = true;
        errorMsg = msg;
    }
    
    void clearError() {
        hasError = false;
        errorMsg = "";
    }

    void fetchStationData() {
        int httpRetries = 0;
        int httpResponseCode = -1;
        String jsonBuffer;
        bool httpSuccess = false;

        while (httpRetries < HTTP_RETRIES && !httpSuccess) {
            HTTPClient http;
            String serverPath = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + tankstellenId + "&apikey=" + tankerkoenigApiKey;
            http.begin(serverPath);
            http.setTimeout(HTTP_TIMEOUT_MS);
            httpResponseCode = http.GET();
            if (httpResponseCode == 200) {
                jsonBuffer = http.getString();
                httpSuccess = true;
            } else {
                httpRetries++;
                if (httpRetries < HTTP_RETRIES) delay(HTTP_RETRY_DELAY_MS);
            }
            http.end();
        }

        if (!httpSuccess) {
            setError("HTTP Fehler: " + String(httpResponseCode));
            return;
        }

        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, jsonBuffer);
        if (error) {
            setError("JSON Fehler");
            return;
        }

        bool isOk = doc["ok"];
        if (!isOk) {
            String msg = doc.containsKey("message") ? String((const char*)doc["message"]) : "Unbekannt";
            setError("API Fehler: " + msg);
            return;
        }

        JsonObject station = doc["station"];
        if (!station["e5"].is<float>() || !station["e10"].is<float>() || !station["diesel"].is<float>()) {
            setError("API: unvollst. Daten");
            return;
        }

        // Success, clear old errors and update data
        clearError();
        e5 = station["e5"];
        e10 = station["e10"];
        diesel = station["diesel"];
        stationname = station["name"].as<String>();
        
        // Update price history
        if (!firstFetchDone) { // On first successful fetch, initialize history
            e5Low = e5; e5High = e5;
            e10Low = e10; e10High = e10;
            dieselLow = diesel; dieselHigh = diesel;
        } else { // Update history with new values
            if (e5 < e5Low) e5Low = e5;
            if (e5 > e5High) e5High = e5;
            if (e10 < e10Low) e10Low = e10;
            if (e10 > e10High) e10High = e10;
            if (diesel < dieselLow) dieselLow = diesel;
            if (diesel > dieselHigh) dieselHigh = diesel;
        }
        
        savePriceHistory();
        firstFetchDone = true;
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
        if (serializeJson(doc, file) == 0) Serial.println("Fehler beim Schreiben der JSON-Daten in die Datei.");
        file.close();
    }

    void loadPriceHistory() {
        File file = LittleFS.open("/preise.json", "r");
        if (!file) {
            // No history, use default values
            return;
        }
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, file);
        if (error) {
            file.close();
            return;
        }
        e5Low = doc["e5Low"] | 99.999;
        e5High = doc["e5High"] | 0.0;
        e10Low = doc["e10Low"] | 99.999;
        e10High = doc["e10High"] | 0.0;
        dieselLow = doc["dieselLow"] | 99.999;
        dieselHigh = doc["dieselHigh"] | 0.0;
        forceUpdate = false; // History loaded, don't force update if not needed
        file.close();
    }

    static inline uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
        return ((r/8)<<11)|((g/4)<<5)|(b/8);
    }

    static uint16_t calcColor(float value, float low, float high) {
        if (value <= low) return GREEN;
        if (value >= high) return RED;
        if (low == high) return WHITE;

        float fraction = (value - low) / (high - low);
        if (fraction < 0.5) { // From green to yellow
            uint8_t r = (uint8_t)(fraction * 2.0 * 255.0);
            return rgb565(r, 255, 0);
        } else { // From yellow to red
            uint8_t g = (uint8_t)((1.0 - fraction) * 2.0 * 255.0);
            return rgb565(255, g, 0);
        }
    }

    void drawPrice(int x, int baselineY, float price, uint16_t color) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.3f", price);
        String s = String(buf);
        int dotPos = s.indexOf('.');
        String mainPart;
        String thirdDigit;
        if (dotPos >= 0 && (int)s.length() >= dotPos + 4) {
            mainPart = s.substring(0, dotPos + 3);
            thirdDigit = s.substring(dotPos + 3, dotPos + 4);
        } else {
            mainPart = s;
            thirdDigit = "";
        }
        const int MAIN_W = 6;
        const int SMALL_W = 4;
        const int SMALL_SUPER_OFFSET = 6;
        u8g2.setForegroundColor(color);
        u8g2.setFont(u8g2_font_6x13_me);
        u8g2.setCursor(x, baselineY);
        u8g2.print(mainPart);
        int wMain = mainPart.length() * MAIN_W;
        if (thirdDigit.length() > 0) {
            u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
            u8g2.setCursor(x + wMain, baselineY - SMALL_SUPER_OFFSET);
            u8g2.print(thirdDigit);
        }
        int wSmall = (thirdDigit.length() > 0) ? (thirdDigit.length() * SMALL_W) : 0;
        u8g2.setFont(u8g2_font_6x13_me);
        u8g2.setCursor(x + wMain + wSmall + 2, baselineY);
        u8g2.print(" €");
    }
};

#endif // DATAMODULE_HPP