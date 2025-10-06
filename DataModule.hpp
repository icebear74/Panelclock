#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include <functional>
#include "WebClientModule.hpp"
#include "PsramUtils.hpp"
#include <esp_heap_caps.h>

// Minimal heap dump
static inline void dumpHeapData(const char* tag) {
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t free_spiram   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("[HEAP_DATA] %-30s ESP.getFreeHeap=%u  Min=%u  INTERNAL=%u  PSRAM=%u\n",
                  tag,
                  (unsigned)ESP.getFreeHeap(),
                  (unsigned)ESP.getMinFreeHeap(),
                  (unsigned)free_internal,
                  (unsigned)free_spiram);
}

struct PriceEntry {
    float price;
    long timestamp;
};

struct StationData {
    char name[64];
    char street[64];
    char city[96];
    float e5;
    float e10;
    float diesel;
    bool isOpen;
    StationData() {
        name[0] = street[0] = city[0] = '\0';
        e5 = e10 = diesel = 0.0f;
        isOpen = false;
    }
};

using PriceVec = std::vector<PriceEntry, PsramAllocator<PriceEntry>>;

class DataModule {
public:
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, int topOffset, WebClientModule* webClient)
     : u8g2(u8g2), canvas(canvas), top_offset(topOffset), webClient(webClient) {
        data.isOpen = false;
    }

    void begin() {
        if (LittleFS.exists("/price_history.json")) {
            loadPriceHistory();
        }
        priceHistory.reserve(32);
    }

    void onUpdate(std::function<void()> callback) {
        updateCallback = callback;
    }

    void setConfig(const String& apiKey, const String& stationId, int fetchIntervalMinutes) {
        this->api_key = apiKey;
        this->station_id = stationId;
        this->fetch_interval_ms = fetchIntervalMinutes * 60 * 1000UL;
    }

    unsigned long getFetchIntervalMillis() const {
        return fetch_interval_ms;
    }

    void fetch() {
        if (api_key.length() == 0 || station_id.length() == 0) {
            data.isOpen = false;
            return;
        }

        WebRequest request;
        request.url = "https://creativecommons.tankerkoenig.de/json/detail.php?id=" + station_id + "&apikey=" + api_key;

        request.onSuccess = [this](char* buffer, size_t size) {
            dumpHeapData("onSuccess start");

            // allocate JSON docs in PSRAM (placement-new)
            void* docMem = ps_malloc(sizeof(StaticJsonDocument<2048>));
            void* filterMem = ps_malloc(sizeof(StaticJsonDocument<256>));
            StaticJsonDocument<2048>* doc = nullptr;
            StaticJsonDocument<256>* filter = nullptr;

            if (!docMem || !filterMem) {
                Serial.println("[DataModule] FEHLER: ps_malloc für JsonDocument fehlgeschlagen!");
                if (docMem) free(docMem);
                if (filterMem) free(filterMem);
                if (buffer) free(buffer);
                dumpHeapData("onSuccess ps_malloc failure end");
                return;
            }

            doc = new (docMem) StaticJsonDocument<2048>();
            filter = new (filterMem) StaticJsonDocument<256>();

            (*filter)["ok"] = true;
            (*filter)["station"]["name"] = true;
            (*filter)["station"]["street"] = true;
            (*filter)["station"]["postCode"] = true;
            (*filter)["station"]["place"] = true;
            (*filter)["station"]["isOpen"] = true;
            (*filter)["station"]["e5"] = true;
            (*filter)["station"]["e10"] = true;
            (*filter)["station"]["diesel"] = true;

            dumpHeapData("before deserializeJson");
            DeserializationError error = deserializeJson(*doc, buffer, size, DeserializationOption::Filter(*filter));
            dumpHeapData("after deserializeJson");

            if (!error && (*doc)["ok"] == true) {
                Serial.println("[DataModule] ERFOLG: JSON-Daten erfolgreich geparst!");
                JsonObject station = (*doc)["station"];

                const char* sname = station["name"] | "";
                const char* sstreet = station["street"] | "";
                const char* spost = station["postCode"] | "";
                const char* splace = station["place"] | "";

                strncpy(this->data.name, sname, sizeof(this->data.name) - 1);
                this->data.name[sizeof(this->data.name) - 1] = '\0';

                strncpy(this->data.street, sstreet, sizeof(this->data.street) - 1);
                this->data.street[sizeof(this->data.street) - 1] = '\0';

                snprintf(this->data.city, sizeof(this->data.city), "%s %s", spost, splace);
                this->data.city[sizeof(this->data.city) - 1] = '\0';

                this->data.e5 = station["e5"];
                this->data.e10 = station["e10"];
                this->data.diesel = station["diesel"];
                this->data.isOpen = station["isOpen"];

                Serial.printf("[DataModule] -> Tankstelle: %s\n", this->data.name);
                Serial.printf("[DataModule] -> E5-Preis: %.3f\n", this->data.e5);

                if (this->data.isOpen && this->data.e5 > 0) {
                    dumpHeapData("before addPriceToHistory");
                    this->addPriceToHistory(this->data.e5);
                    dumpHeapData("after addPriceToHistory");
                }
            } else {
                Serial.println("[DataModule] FEHLER: JSON-Daten konnten nicht geparst werden.");
                Serial.printf("[DataModule] -> ArduinoJson-Fehler: %s\n", error.c_str());
                this->data.isOpen = false;
            }

            if (updateCallback) {
                updateCallback();
            }

            // clean up JSON docs and free PSRAM backing
            if (doc) {
                doc->~StaticJsonDocument<2048>();
                free(docMem);
            }
            if (filter) {
                filter->~StaticJsonDocument<256>();
                free(filterMem);
            }

            dumpHeapData("before free buffer");
            if (buffer) free(buffer);
            dumpHeapData("after free buffer");
            dumpHeapData("onSuccess end");
        };

        request.onFailure = [this](int httpCode) {
            Serial.printf("[DataModule] FEHLER: Download der Tankstellendaten fehlgeschlagen, HTTP-Code: %d\n", httpCode);
            this->data.isOpen = false;
            if (updateCallback) {
                updateCallback();
            }
            dumpHeapData("onFailure end");
        };

        webClient->addRequest(request);
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

        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setForegroundColor(0xFFFF);
        int titleWidth = u8g2.getUTF8Width("Benzinpreise");
        u8g2.setCursor((canvas.width() - titleWidth) / 2, 11);
        u8g2.print("Benzinpreise");
        
        u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
        int nameWidth = u8g2.getUTF8Width(this->data.name);
        u8g2.setCursor((canvas.width() - nameWidth) / 2, 19);
        u8g2.print(this->data.name);

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
    int top_offset;
    StationData data;
    PriceVec priceHistory;
    WebClientModule* webClient;
    std::function<void()> updateCallback;

    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
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
        int superscriptWidth = u8g2.getUTF8Width(last_digit_str);
        u8g2.setFont(u8g2_font_6x13_me);
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
        void* mem = ps_malloc(sizeof(StaticJsonDocument<1024>));
        if (!mem) return;
        StaticJsonDocument<1024>* doc = new (mem) StaticJsonDocument<1024>();
        JsonArray array = doc->to<JsonArray>();
        for(const auto& entry : priceHistory) {
            JsonObject obj = array.add<JsonObject>();
            obj["p"] = entry.price;
            obj["t"] = entry.timestamp;
        }
        File file = LittleFS.open("/price_history.json", "w");
        if (file) { serializeJson(*doc, file); file.close(); }
        doc->~StaticJsonDocument<1024>();
        free(mem);
    }

    void loadPriceHistory() {
        File file = LittleFS.open("/price_history.json", "r");
        if(file) {
            void* mem = ps_malloc(sizeof(StaticJsonDocument<1024>));
            if (!mem) { file.close(); return; }
            StaticJsonDocument<1024>* doc = new (mem) StaticJsonDocument<1024>();
            DeserializationError error = deserializeJson(*doc, file);
            if (!error) {
                JsonArray array = doc->as<JsonArray>();
                priceHistory.clear();
                for(JsonObject obj : array) {
                    priceHistory.push_back({obj["p"].as<float>(), obj["t"].as<long>()});
                }
            }
            file.close();
            doc->~StaticJsonDocument<1024>();
            free(mem);
        }
    }
};

#endif // DATAMODULE_HPP