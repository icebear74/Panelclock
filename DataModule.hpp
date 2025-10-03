#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>
#include <WiFiClientSecure.h>

struct PriceEntry {
    float price;
    long timestamp;
};

struct StationData {
    String name;
    String street;
    String city;
    float e5;
    float e10;
    float diesel;
    bool isOpen;
};

class DataModule {
public:
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, int topOffset)
     : u8g2(u8g2), canvas(canvas), top_offset(topOffset) {}

    void begin() {
        if (LittleFS.exists("/price_history.json")) {
            loadPriceHistory();
        }
    }

    void setConfig(const String& apiKey, const String& stationId, int fetchIntervalMinutes) {
        this->api_key = apiKey;
        this->station_id = stationId;
        this->fetch_interval_ms = fetchIntervalMinutes * 60 * 1000UL;
    }

    void update() {
        unsigned long now = millis();
        if (api_key.length() > 0 && station_id.length() > 0 && (last_fetch_ms == 0 || (now - last_fetch_ms > fetch_interval_ms))) {
            fetchStationData();
            last_fetch_ms = now;
        }
    }

    void draw() {
        canvas.fillScreen(0);
        u8g2.begin(canvas);

        if (!data.isOpen) {
             u8g2.setFont(u8g2_font_10x20_tf);
             u8g2.setForegroundColor(0xF800); // Rot
             int x = (canvas.width() - u8g2.getUTF8Width("GESCHLOSSEN")) / 2;
             u8g2.setCursor(x, 30);
             u8g2.print("GESCHLOSSEN");
             return;
        }

        // Titel "Benzinpreise"
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setForegroundColor(0xFFFF);
        int titleWidth = u8g2.getUTF8Width("Benzinpreise");
        u8g2.setCursor((canvas.width() - titleWidth) / 2, 11);
        u8g2.print("Benzinpreise");
        
        // Tankstellen-Name mit dem korrekten Font und Position
        u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
        int nameWidth = u8g2.getUTF8Width(data.name.c_str());
        u8g2.setCursor((canvas.width() - nameWidth) / 2, 19); // ** 3 PIXEL HÖHER **
        u8g2.print(data.name);

        // Finde Min/Max Preise aus dem Verlauf
        float minE5 = 10.0, maxE5 = 0.0;
        if (priceHistory.size() >= 2) {
            for (const auto& entry : priceHistory) {
                if (entry.price < minE5) minE5 = entry.price;
                if (entry.price > maxE5) maxE5 = entry.price;
            }
        } else {
            minE5 = data.e5 > 0 ? data.e5 - 0.05 : 0;
            maxE5 = data.e5 > 0 ? data.e5 + 0.05 : 0;
        }
        
        float minE10 = minE5 > 0 ? minE5 - 0.05 : 0;
        float maxE10 = maxE5 > 0 ? maxE5 - 0.05 : 0;
        float minDiesel = minE5 > 0 ? minE5 - 0.15 : 0;
        float maxDiesel = maxE5 > 0 ? maxE5 - 0.15 : 0;

        // Zeichne die Preiszeilen
        drawPriceLine(31, "E5", data.e5, minE5, maxE5);
        drawPriceLine(44, "E10", data.e10, minE10, maxE10);
        drawPriceLine(57, "Diesel", data.diesel, minDiesel, maxDiesel);
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    String api_key;
    String station_id;
    unsigned long fetch_interval_ms = 300000;
    unsigned long last_fetch_ms = 0;
    int top_offset;
    StationData data = {"", "", "", 0.0, 0.0, 0.0, false};
    std::vector<PriceEntry> priceHistory;

    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
    static uint16_t calcColor(float value, float low, float high) {
        if (low >= high) return rgb565(255, 255, 0);
        int diff = (int)roundf(((high - value) / (high - low)) * 100.0f);
        if(diff < 0) diff = 0;
        if(diff > 100) diff = 100;
        uint8_t rval = 255, gval = 255;
        if (diff <= 50) { rval = 255; gval = map(diff, 0, 50, 0, 255); }
        else { gval = 255; rval = map(diff, 50, 100, 255, 0); }
        return rgb565(rval, gval, 0);
    }

    void drawPrice(int x, int y, float price, uint16_t color) {
        if (price <= 0) return;
        
        u8g2.setForegroundColor(color);
        
        String priceStr = String(price, 3);
        priceStr.replace('.', ',');
        String mainPricePart = priceStr.substring(0, priceStr.length() - 1);
        char last_digit_char = priceStr.charAt(priceStr.length() - 1);
        char last_digit_str[2] = {last_digit_char, '\0'};
        
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setCursor(x, y);
        u8g2.print(mainPricePart);
        
        int mainPriceWidth = u8g2.getUTF8Width(mainPricePart.c_str());
        int superscriptX = x + mainPriceWidth;
        
        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.setCursor(superscriptX+1, y - 4);
        u8g2.print(last_digit_str);

        // Euro-Zeichen mit dem korrekten Font zeichnen
        int superscriptWidth = u8g2.getUTF8Width(last_digit_str);
        u8g2.setFont(u8g2_font_6x13_me); // ** FONT FÜR EURO-ZEICHEN **
        u8g2.setCursor(superscriptX + superscriptWidth + 3, y);
        u8g2.print("€");
    }

    void drawPriceLine(int y, const char* label, float current, float min, float max) {
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setForegroundColor(0xFFFF);
        u8g2.setCursor(2, y);
        u8g2.print(label);

        drawPrice(50, y, min, rgb565(0, 255, 0));
        drawPrice(100, y, current, calcColor(current, min, max));
        drawPrice(150, y, max, rgb565(255, 0, 0));
    }

    void fetchStationData() {
        if (WiFi.status() != WL_CONNECTED) return;
        
        WiFiClientSecure client;
        client.setInsecure();
        
        HTTPClient http;
        String url = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + station_id + "&apikey=" + api_key;
        http.begin(client, url);

        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error) { http.end(); return; }
            if (doc["ok"] == true) {
                JsonObject station = doc["station"];
                data.name = station["name"].as<String>();
                data.street = station["street"].as<String>();
                data.city = String(station["postCode"].as<int>()) + " " + station["place"].as<String>();
                data.e5 = station["e5"];
                data.e10 = station["e10"];
                data.diesel = station["diesel"];
                data.isOpen = station["isOpen"];
                if(data.isOpen && data.e5 > 0) {
                    addPriceToHistory(data.e5);
                }
            }
        }
        http.end();
    }

    void addPriceToHistory(float price) {
        if (priceHistory.empty() || priceHistory.back().price != price) {
            priceHistory.push_back({price, (long)time(nullptr)});
            if (priceHistory.size() > 30) {
                priceHistory.erase(priceHistory.begin());
            }
            savePriceHistory();
        }
    }

    void savePriceHistory() {
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        for(const auto& entry : priceHistory) {
            JsonObject obj = array.add<JsonObject>();
            obj["p"] = entry.price;
            obj["t"] = entry.timestamp;
        }
        File file = LittleFS.open("/price_history.json", "w");
        if (file) {
            serializeJson(doc, file);
            file.close();
        }
    }

    void loadPriceHistory() {
        File file = LittleFS.open("/price_history.json", "r");
        if(file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            if (!error) {
                JsonArray array = doc.as<JsonArray>();
                priceHistory.clear();
                for(JsonObject obj : array) {
                    priceHistory.push_back({obj["p"].as<float>(), obj["t"].as<long>()});
                }
            }
            file.close();
        }
    }
};

#endif