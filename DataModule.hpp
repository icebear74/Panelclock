#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include <functional>
#include <algorithm> // moving average
#include "WebClientModule.hpp"
#include "PsramUtils.hpp"
#include "certs.hpp"
#include <esp_heap_caps.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "GeneralTimeConverter.hpp" // moving average

// moving average
#define MOVING_AVERAGE_DAYS 14 
#define STATION_PRICE_STATS_VERSION 3 

// moving average - Erweiterte Struktur für alle drei Kraftstoffsorten
struct DailyPriceStats {
    PsramString date; // "YYYY-MM-DD"
    float e5_low = 0.0;
    float e5_high = 0.0;
    float e10_low = 0.0;
    float e10_high = 0.0;
    float diesel_low = 0.0;
    float diesel_high = 0.0;
};

// moving average - Neue Struktur zur Kapselung der Historie pro Tankstelle
struct StationPriceHistory {
    PsramString stationId;
    PsramVector<DailyPriceStats> dailyStats;
};

// moving average - Struktur für die Rückgabe der berechneten Mittelwerte
struct AveragePrices {
    float avgE5Low = 0.0;
    float avgE5High = 0.0;
    float avgE10Low = 0.0;
    float avgE10High = 0.0;
    float avgDieselLow = 0.0;
    float avgDieselHigh = 0.0;
    int count = 0; // Anzahl der Datensätze (Tage)
};

struct StationData {
    PsramString id;
    PsramString name;
    PsramString brand;
    PsramString street;
    PsramString houseNumber;
    PsramString postCode;
    PsramString place;
    float e5;
    float e10;
    float diesel;
    bool isOpen;
    time_t lastPriceChange = 0; // Zeitstempel der letzten Preisänderung

    StationData() : e5(0.0f), e10(0.0f), diesel(0.0f), isOpen(false) {}
};


class DataModule {
public:
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient)
     : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), top_offset(topOffset), webClient(webClient), last_processed_update(0) {
        dataMutex = xSemaphoreCreateMutex();
    }

    ~DataModule() {
        if (dataMutex) vSemaphoreDelete(dataMutex);
        if (pending_buffer) free(pending_buffer);
    }

    void begin() {
        if (LittleFS.exists("/price_history.json")) {
            Serial.println("[DataModule] Altes Preis-Format 'price_history.json' gefunden. Wird gelöscht.");
            LittleFS.remove("/price_history.json");
        }
        loadPriceStatistics();
        loadStationCache();
    }

    void onUpdate(std::function<void()> callback) {
        updateCallback = callback;
    }

    void setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes) {
        this->api_key = apiKey;
        this->station_ids = stationIds;
        
        if (!api_key.empty() && !station_ids.empty()) {
            this->resource_url = "https://creativecommons.tankerkoenig.de/json/prices.php?ids=";
            this->resource_url += this->station_ids;
            this->resource_url += "&apikey=";
            this->resource_url += this->api_key;
            webClient->registerResource(String(this->resource_url.c_str()), fetchIntervalMinutes, root_ca_pem);
        } else {
            this->resource_url.clear();
        }

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            PsramVector<PsramString> current_id_list;
            PsramString temp_ids = station_ids;
            char* strtok_ctx;
            char* id_token = strtok_r((char*)temp_ids.c_str(), ",", &strtok_ctx);
            while(id_token != nullptr) { current_id_list.push_back(id_token); id_token = strtok_r(nullptr, ",", &strtok_ctx); }

            price_statistics.erase(
                std::remove_if(price_statistics.begin(), price_statistics.end(), 
                    [&](const StationPriceHistory& history) {
                        for(const auto& id : current_id_list) {
                            if (history.stationId == id) return false;
                        }
                        Serial.printf("[DataModule] Entferne Preis-Statistik für nicht mehr konfigurierte Tankstelle: %s\n", history.stationId.c_str());
                        return true;
                    }), 
                price_statistics.end());
            
            trimAllPriceStatistics();
            xSemaphoreGive(dataMutex);
        }
    }

    void setPageDisplayTime(unsigned long ms) {
        pageDisplayDuration = ms > 0 ? ms : 10000;
    }

    unsigned long getRequiredDisplayDuration() {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return pageDisplayDuration;
        
        int num_stations = station_data_list.size();
        int num_pages = (num_stations > 0) ? num_stations : 1;
        
        xSemaphoreGive(dataMutex);
        return (pageDisplayDuration * num_pages) + 5000;
    }

    void resetPaging() {
        currentPage = 0;
        lastPageSwitchTime = millis();
    }

    void tick() {
        unsigned long now = millis();
        if (pageDisplayDuration > 0 && now - lastPageSwitchTime > pageDisplayDuration) {
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (totalPages > 1) {
                    currentPage = (currentPage + 1) % totalPages;
                    if (updateCallback) updateCallback();
                }
                xSemaphoreGive(dataMutex);
            }
            lastPageSwitchTime = now;
        }
    }

    void queueData() {
        if (resource_url.empty() || !webClient) return;

        webClient->accessResource(String(resource_url.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (buffer && size > 0 && last_update > this->last_processed_update) {
                if (pending_buffer) free(pending_buffer);
                pending_buffer = (char*)ps_malloc(size + 1);
                if (pending_buffer) {
                    memcpy(pending_buffer, buffer, size);
                    pending_buffer[size] = '\0';
                    buffer_size = size;
                    last_processed_update = last_update;
                    data_pending = true;
                }
            }
        });
    }

    void processData() {
        if (data_pending) {
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                Serial.println("[DataModule] Neue Tanker-Daten gefunden, starte Parsing...");
                parseAndProcessJson(pending_buffer, buffer_size);
                free(pending_buffer);
                pending_buffer = nullptr;
                data_pending = false;
                xSemaphoreGive(dataMutex);
                if (this->updateCallback) this->updateCallback();
            }
        }
    }

    void draw() {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
        
        totalPages = station_data_list.size() > 0 ? station_data_list.size() : 1;
        if (currentPage >= totalPages) currentPage = 0;

        canvas.fillScreen(0);
        u8g2.begin(canvas);

        if (station_data_list.empty()) {
             u8g2.setFont(u8g2_font_7x14_tf);
             u8g2.setForegroundColor(0xFFFF);
             const char* text = "Keine Tankstelle konfiguriert.";
             int x = (canvas.width() - u8g2.getUTF8Width(text)) / 2;
             u8g2.setCursor(x, 30);
             u8g2.print(text);
             xSemaphoreGive(dataMutex);
             return;
        }
        
        const StationData& data = station_data_list[currentPage];

        // --- HEADER DESIGN ---
        const int PADDING = 10;
        const int LEFT_MARGIN = 5;
        const int RIGHT_MARGIN = 5;
        const int totalWidth = canvas.width() - LEFT_MARGIN - RIGHT_MARGIN;

        u8g2.setFont(u8g2_font_logisoso16_tf);
        PsramString brandText = data.brand;
        int brandWidth = u8g2.getUTF8Width(brandText.c_str());

        u8g2.setFont(u8g2_font_5x8_tf);
        PsramString line1 = data.street;
        if (!data.houseNumber.empty()) { line1 += " "; line1 += data.houseNumber; }
        PsramString line2 = data.postCode;
        if (!data.place.empty()) { line2 += " "; line2 += data.place; }
        int line1Width = u8g2.getUTF8Width(line1.c_str());
        int line2Width = u8g2.getUTF8Width(line2.c_str());
        int addressWidth = std::max(line1Width, line2Width);

        if (brandWidth + addressWidth + PADDING > totalWidth) {
            bool brandIsProblem = brandWidth > totalWidth - addressWidth - PADDING;
            bool addressIsProblem = addressWidth > totalWidth - brandWidth - PADDING;

            if (brandIsProblem && addressIsProblem) {
                int brandMaxWidth = (totalWidth - PADDING) / 2;
                int addressMaxWidth = totalWidth - brandMaxWidth - PADDING;
                u8g2.setFont(u8g2_font_logisoso16_tf);
                brandText = truncateString(brandText, brandMaxWidth);
                u8g2.setFont(u8g2_font_5x8_tf);
                line1 = truncateString(line1, addressMaxWidth);
                line2 = truncateString(line2, addressMaxWidth);
            } else if (brandIsProblem) {
                int brandMaxWidth = totalWidth - addressWidth - PADDING;
                u8g2.setFont(u8g2_font_logisoso16_tf);
                brandText = truncateString(brandText, brandMaxWidth);
            } else {
                int addressMaxWidth = totalWidth - brandWidth - PADDING;
                u8g2.setFont(u8g2_font_5x8_tf);
                line1 = truncateString(line1, addressMaxWidth);
                line2 = truncateString(line2, addressMaxWidth);
            }
        }
        
        u8g2.setFont(u8g2_font_logisoso16_tf);
        u8g2.setForegroundColor(0xFFFF);
        u8g2.setCursor(LEFT_MARGIN, 17);
        u8g2.print(brandText.c_str());

        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.setCursor(canvas.width() - u8g2.getUTF8Width(line1.c_str()) - RIGHT_MARGIN, 8);
        u8g2.print(line1.c_str());
        u8g2.setCursor(canvas.width() - u8g2.getUTF8Width(line2.c_str()) - RIGHT_MARGIN, 18);
        u8g2.print(line2.c_str());
        
        canvas.drawFastHLine(0, 19, canvas.width(), rgb565(128, 128, 128));

        // --- PREIS-ANZEIGE ---
        AveragePrices averages = calculateAverages(data.id);
        if (averages.avgE5Low == 0.0) averages.avgE5Low = data.e5 > 0 ? data.e5 - 0.05 : 0;
        if (averages.avgE5High == 0.0) averages.avgE5High = data.e5 > 0 ? data.e5 + 0.05 : 0;
        if (averages.avgE10Low == 0.0) averages.avgE10Low = data.e10 > 0 ? data.e10 - 0.05 : 0;
        if (averages.avgE10High == 0.0) averages.avgE10High = data.e10 > 0 ? data.e10 + 0.05 : 0;
        if (averages.avgDieselLow == 0.0) averages.avgDieselLow = data.diesel > 0 ? data.diesel - 0.15 : 0;
        if (averages.avgDieselHigh == 0.0) averages.avgDieselHigh = data.diesel > 0 ? data.diesel + 0.15 : 0;

        u8g2.setFont(u8g2_font_5x8_tf);
        u8g2.setForegroundColor(rgb565(0, 255, 255)); // Cyan
        int y_pos = 27;

        if (averages.count > 0) {
            char count_str[12];
            snprintf(count_str, sizeof(count_str), "(%d/%d)", averages.count, MOVING_AVERAGE_DAYS);
            int count_width = u8g2.getUTF8Width(count_str);
            u8g2.setCursor(188 - count_width, y_pos);
            u8g2.print(count_str);
        }

        if (data.lastPriceChange > 0) {
            time_t local_change_time = timeConverter.toLocal(data.lastPriceChange);
            struct tm timeinfo;
            localtime_r(&local_change_time, &timeinfo);
            char time_str[6];
            strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
            int time_width = u8g2.getUTF8Width(time_str);
            u8g2.setCursor(96 + (50 - time_width) / 2, y_pos);
            u8g2.print(time_str);
        } else {
            const char* no_time_str = "--:--";
            int time_width = u8g2.getUTF8Width(no_time_str);
            u8g2.setCursor(96 + (50 - time_width) / 2, y_pos);
            u8g2.print(no_time_str);
        }

        drawPriceLine(39, "E5", data.e5, averages.avgE5Low, averages.avgE5High);
        drawPriceLine(52, "E10", data.e10, averages.avgE10Low, averages.avgE10High);
        drawPriceLine(65, "Diesel", data.diesel, averages.avgDieselLow, averages.avgDieselHigh);
        
        // Wenn geschlossen, rotes X über die Preistabelle zeichnen
        if (!data.isOpen) {
            uint16_t red = rgb565(255, 0, 0);
         // Linie von links oben nach rechts unten
            canvas.drawLine(0, 19, canvas.width() - 1, canvas.height() - 1, red);
            // Linie von rechts oben nach links unten
            canvas.drawLine(canvas.width() - 1, 19, 0, canvas.height() - 1, red);
        }
        xSemaphoreGive(dataMutex);
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    GeneralTimeConverter& timeConverter;
    PsramString api_key;
    PsramString station_ids;
    int top_offset;
    PsramVector<StationData> station_data_list;
    PsramVector<StationData> station_cache;
    WebClientModule* webClient;
    std::function<void()> updateCallback;
    SemaphoreHandle_t dataMutex;
    char* pending_buffer = nullptr;
    size_t buffer_size = 0;
    volatile bool data_pending = false;
    PsramString resource_url;
    time_t last_processed_update;
    PsramVector<StationPriceHistory> price_statistics;

    int currentPage = 0;
    int totalPages = 1;
    unsigned long pageDisplayDuration = 10000;
    unsigned long lastPageSwitchTime = 0;

    PsramString truncateString(const PsramString& text, int maxWidth) {
        if (u8g2.getUTF8Width(text.c_str()) <= maxWidth) {
            return text;
        }
        PsramString truncated = text;
        int ellipsisWidth = u8g2.getUTF8Width("...");
        while (!truncated.empty() && u8g2.getUTF8Width(truncated.c_str()) + ellipsisWidth > maxWidth) {
            truncated.pop_back();
        }
        return truncated + "...";
    }

    void parseAndProcessJson(const char* buffer, size_t size) {
        const size_t JSON_DOC_SIZE = 8192;
        void* docMem = ps_malloc(JSON_DOC_SIZE);
        if (!docMem) { Serial.println("[DataModule] FATAL: PSRAM für JSON-Parsing fehlgeschlagen!"); return; }
        
        DynamicJsonDocument* doc = new (docMem) DynamicJsonDocument(JSON_DOC_SIZE);
        
        PsramVector<StationData> new_station_data_list;
        
        DeserializationError error = deserializeJson(*doc, buffer, size);

        if (!error && (*doc)["ok"] == true) {
            JsonObject prices = (*doc)["prices"];
            PsramVector<PsramString> id_list;
            PsramString temp_ids = station_ids;
            char* strtok_ctx;
            char* id_token = strtok_r((char*)temp_ids.c_str(), ",", &strtok_ctx);
            while(id_token != nullptr) { id_list.push_back(id_token); id_token = strtok_r(nullptr, ",", &strtok_ctx); }

            for (const auto& id : id_list) {
                if (prices.containsKey(id.c_str())) {
                    JsonObject station_json = prices[id.c_str()];
                    StationData new_data;
                    new_data.id = id;
                    
                    bool found_in_cache = false;
                    for(const auto& cache_entry : station_cache) {
                        if (cache_entry.id == id) {
                            new_data.name = cache_entry.name; new_data.brand = cache_entry.brand;
                            new_data.street = cache_entry.street; new_data.houseNumber = cache_entry.houseNumber;
                            new_data.postCode = cache_entry.postCode; new_data.place = cache_entry.place;
                            found_in_cache = true;
                            break;
                        }
                    }
                    if (!found_in_cache) { new_data.name = "Unbekannt"; }

                    // Logik für offene/geschlossene Tankstellen
                    bool is_open = (station_json["status"] == "open");
                    new_data.isOpen = is_open;

                    const StationData* old_data_ptr = nullptr;
                    for(const auto& old_data : station_data_list) {
                        if (old_data.id == new_data.id) {
                            old_data_ptr = &old_data;
                            break;
                        }
                    }

                    if (is_open) {
                        // Tankstelle ist offen, Preise aus JSON lesen
                        new_data.e5 = station_json["e5"].as<float>();
                        new_data.e10 = station_json["e10"].as<float>();
                        new_data.diesel = station_json["diesel"].as<float>();
                        
                        bool price_changed = false;
                        if (old_data_ptr) {
                            if (old_data_ptr->e5 != new_data.e5 || old_data_ptr->e10 != new_data.e10 || old_data_ptr->diesel != new_data.diesel) {
                                price_changed = true;
                            }
                            new_data.lastPriceChange = price_changed ? last_processed_update : old_data_ptr->lastPriceChange;
                        } else {
                            new_data.lastPriceChange = last_processed_update;
                        }
                        updatePriceStatistics(new_data.id, new_data.e5, new_data.e10, new_data.diesel);
                    } else {
                        // Tankstelle ist geschlossen, letzte bekannte Preise übernehmen
                        if (old_data_ptr) {
                            new_data.e5 = old_data_ptr->e5;
                            new_data.e10 = old_data_ptr->e10;
                            new_data.diesel = old_data_ptr->diesel;
                            new_data.lastPriceChange = old_data_ptr->lastPriceChange;
                        }
                    }
                    
                    new_station_data_list.push_back(new_data);
                }
            }
            station_data_list = new_station_data_list;
        } else { Serial.printf("[DataModule] JSON-Parsing-Fehler oder API-Antwort nicht ok. Fehler: %s\n", error.c_str()); }

        doc->~DynamicJsonDocument();
        free(docMem);
    }
    
    void updatePriceStatistics(const PsramString& stationId, float currentE5, float currentE10, float currentDiesel) {
        time_t now_utc;
        time(&now_utc);
        time_t local_epoch = timeConverter.toLocal(now_utc);
        struct tm timeinfo;
        localtime_r(&local_epoch, &timeinfo);
        
        char date_str[11];
        strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
        PsramString today_date(date_str);

        StationPriceHistory* history = nullptr;
        for (auto& h : price_statistics) {
            if (h.stationId == stationId) {
                history = &h;
                break;
            }
        }
        if (!history) {
            price_statistics.push_back({stationId, {}});
            history = &price_statistics.back();
        }

        DailyPriceStats* today_stats = nullptr;
        for (auto& s : history->dailyStats) {
            if (s.date == today_date) {
                today_stats = &s;
                break;
            }
        }

        bool changed = false;
        if (!today_stats) {
            DailyPriceStats new_stats;
            new_stats.date = today_date;
            if (currentE5 > 0) { new_stats.e5_low = currentE5; new_stats.e5_high = currentE5; }
            if (currentE10 > 0) { new_stats.e10_low = currentE10; new_stats.e10_high = currentE10; }
            if (currentDiesel > 0) { new_stats.diesel_low = currentDiesel; new_stats.diesel_high = currentDiesel; }
            history->dailyStats.push_back(new_stats);
            changed = true;
            trimPriceStatistics(*history);
        } else {
            if (currentE5 > 0 && (today_stats->e5_low == 0 || currentE5 < today_stats->e5_low)) { today_stats->e5_low = currentE5; changed = true; }
            if (currentE5 > 0 && currentE5 > today_stats->e5_high) { today_stats->e5_high = currentE5; changed = true; }
            if (currentE10 > 0 && (today_stats->e10_low == 0 || currentE10 < today_stats->e10_low)) { today_stats->e10_low = currentE10; changed = true; }
            if (currentE10 > 0 && currentE10 > today_stats->e10_high) { today_stats->e10_high = currentE10; changed = true; }
            if (currentDiesel > 0 && (today_stats->diesel_low == 0 || currentDiesel < today_stats->diesel_low)) { today_stats->diesel_low = currentDiesel; changed = true; }
            if (currentDiesel > 0 && currentDiesel > today_stats->diesel_high) { today_stats->diesel_high = currentDiesel; changed = true; }
        }

        if (changed) {
            savePriceStatistics();
        }
    }

    void trimPriceStatistics(StationPriceHistory& history) {
        time_t now_utc;
        time(&now_utc);
        time_t local_epoch = timeConverter.toLocal(now_utc);
        time_t cutoff_epoch = local_epoch - (MOVING_AVERAGE_DAYS * 86400L);
        struct tm cutoff_tm;
        localtime_r(&cutoff_epoch, &cutoff_tm);
        char cutoff_date_str_buf[11];
        strftime(cutoff_date_str_buf, sizeof(cutoff_date_str_buf), "%Y-%m-%d", &cutoff_tm);
        PsramString cutoff_date_str(cutoff_date_str_buf);

        history.dailyStats.erase(
            std::remove_if(history.dailyStats.begin(), history.dailyStats.end(), 
                [&](const DailyPriceStats& stats) {
                    return stats.date < cutoff_date_str;
                }), 
            history.dailyStats.end());
    }

    void trimAllPriceStatistics() {
        for (auto& history : price_statistics) {
            trimPriceStatistics(history);
        }
        savePriceStatistics();
    }

    AveragePrices calculateAverages(const PsramString& stationId) {
        AveragePrices averages;
        float sumE5Low = 0.0, sumE5High = 0.0, sumE10Low = 0.0, sumE10High = 0.0, sumDieselLow = 0.0, sumDieselHigh = 0.0;
        int count = 0;

        for (const auto& h : price_statistics) {
            if (h.stationId == stationId) {
                for (const auto& s : h.dailyStats) {
                    sumE5Low += s.e5_low; sumE5High += s.e5_high;
                    sumE10Low += s.e10_low; sumE10High += s.e10_high;
                    sumDieselLow += s.diesel_low; sumDieselHigh += s.diesel_high;
                    count++;
                }
                break;
            }
        }

        if (count > 0) {
            averages.avgE5Low = sumE5Low / count; averages.avgE5High = sumE5High / count;
            averages.avgE10Low = sumE10Low / count; averages.avgE10High = sumE10High / count;
            averages.avgDieselLow = sumDieselLow / count; averages.avgDieselHigh = sumDieselHigh / count;
        }
        averages.count = count;
        return averages;
    }

    bool migratePriceStatistics(DynamicJsonDocument& doc) {
        if (!doc.containsKey("version") || doc["version"] == STATION_PRICE_STATS_VERSION) {
            return true;
        }
        int file_version = doc["version"];
        Serial.printf("[DataModule] Migration für station_price_stats.json von v%d auf v%d erforderlich.\n", file_version, STATION_PRICE_STATS_VERSION);
        if (file_version != STATION_PRICE_STATS_VERSION) {
            Serial.printf("[DataModule] Migration auf v%d nicht erfolgreich. Konnte v%d nicht erreichen.\n", STATION_PRICE_STATS_VERSION, file_version);
            return false;
        }
        Serial.println("[DataModule] Migration erfolgreich.");
        return true;
    }

    void savePriceStatistics() {
        const size_t JSON_DOC_SIZE = 32768;
        void* docMem = ps_malloc(JSON_DOC_SIZE);
        if (!docMem) { Serial.println("[DataModule] FATAL: PSRAM für Statistik-Speichern fehlgeschlagen!"); return; }
        
        DynamicJsonDocument* doc = new (docMem) DynamicJsonDocument(JSON_DOC_SIZE);
        (*doc)["version"] = STATION_PRICE_STATS_VERSION;
        JsonObject prices = doc->createNestedObject("prices");

        for (const auto& history : price_statistics) {
            JsonArray stats_array = prices.createNestedArray(history.stationId.c_str());
            for (const auto& stats : history.dailyStats) {
                JsonObject obj = stats_array.add<JsonObject>();
                obj["date"] = stats.date;
                obj["e5_low"] = stats.e5_low; obj["e5_high"] = stats.e5_high;
                obj["e10_low"] = stats.e10_low; obj["e10_high"] = stats.e10_high;
                obj["diesel_low"] = stats.diesel_low; obj["diesel_high"] = stats.diesel_high;
            }
        }

        File file = LittleFS.open("/station_price_stats.json", "w");
        if (file) {
            serializeJson(*doc, file);
            file.close();
        } else {
            Serial.println("[DataModule] Fehler beim Öffnen von station_price_stats.json zum Schreiben!");
        }

        doc->~DynamicJsonDocument();
        free(docMem);
    }

    void loadPriceStatistics() {
        if (!LittleFS.exists("/station_price_stats.json")) { return; }

        File file = LittleFS.open("/station_price_stats.json", "r");
        if(file) {
            const size_t JSON_DOC_SIZE = 32768;
            void* docMem = ps_malloc(JSON_DOC_SIZE);
            if (!docMem) { Serial.println("[DataModule] FATAL: PSRAM für Statistik-Laden fehlgeschlagen!"); file.close(); return; }

            DynamicJsonDocument* doc = new (docMem) DynamicJsonDocument(JSON_DOC_SIZE);
            DeserializationError error = deserializeJson(*doc, file);
            file.close();

            if (!error) {
                if (migratePriceStatistics(*doc)) {
                    price_statistics.clear();
                    JsonObject prices = (*doc)["prices"];
                    for (JsonPair kv : prices) {
                        StationPriceHistory history;
                        history.stationId = kv.key().c_str();
                        JsonArray stats_array = kv.value().as<JsonArray>();
                        for (JsonObject obj : stats_array) {
                            DailyPriceStats stats;
                            stats.date = obj["date"].as<const char*>();
                            stats.e5_low = obj["e5_low"]; stats.e5_high = obj["e5_high"];
                            stats.e10_low = obj["e10_low"]; stats.e10_high = obj["e10_high"];
                            stats.diesel_low = obj["diesel_low"]; stats.diesel_high = obj["diesel_high"];
                            history.dailyStats.push_back(stats);
                        }
                        price_statistics.push_back(history);
                    }
                    Serial.printf("[DataModule] %d Preis-Statistiken geladen.\n", price_statistics.size());
                } else {
                     Serial.println("[DataModule] Migration der Preis-Statistiken fehlgeschlagen. Datei wird gelöscht.");
                     LittleFS.remove("/station_price_stats.json");
                }
            } else {
                Serial.printf("[DataModule] Fehler beim Deserialisieren von station_price_stats.json (Fehler: %s). Datei wird gelöscht.\n", error.c_str());
                LittleFS.remove("/station_price_stats.json");
            }
            
            doc->~DynamicJsonDocument();
            free(docMem);
        }
    }

    void loadStationCache() {
        if (!LittleFS.exists("/station_cache.json")) { return; }
        File file = LittleFS.open("/station_cache.json", "r");
        if(file) {
            const size_t JSON_DOC_SIZE = 32768;
            void* docMem = ps_malloc(JSON_DOC_SIZE);
            if (!docMem) { Serial.println("[DataModule] FATAL: PSRAM für Cache-Laden fehlgeschlagen!"); file.close(); return; }

            DynamicJsonDocument* doc = new (docMem) DynamicJsonDocument(JSON_DOC_SIZE);
            DeserializationError error = deserializeJson(*doc, file);
            file.close();

            if (!error && (*doc)["ok"] == true) {
                JsonArray stations = (*doc)["stations"];
                station_cache.clear();
                for(JsonObject station_json : stations) {
                    StationData cache_entry;
                    const char* id_ptr = station_json["id"];
                    if (id_ptr) cache_entry.id = id_ptr;
                    const char* name_ptr = station_json["name"];
                    if (name_ptr) cache_entry.name = name_ptr;
                    const char* brand_ptr = station_json["brand"];
                    if (brand_ptr) cache_entry.brand = brand_ptr;
                    const char* street_ptr = station_json["street"];
                    if (street_ptr) cache_entry.street = street_ptr;
                    const char* houseNumber_ptr = station_json["houseNumber"];
                    if (houseNumber_ptr) cache_entry.houseNumber = houseNumber_ptr;
                    if (station_json["postCode"].is<int>()) {
                        cache_entry.postCode = String(station_json["postCode"].as<int>()).c_str();
                    } else if (station_json["postCode"].is<const char*>()) {
                        cache_entry.postCode = station_json["postCode"].as<const char*>();
                    }
                    const char* place_ptr = station_json["place"];
                    if (place_ptr) cache_entry.place = place_ptr;
                    station_cache.push_back(cache_entry);
                }
                Serial.printf("[DataModule] %d Tankstellen aus dem Cache geladen.\n", station_cache.size());
            } else { Serial.printf("[DataModule] Fehler beim Parsen von station_cache.json: %s\n", error.c_str()); }
            
            doc->~DynamicJsonDocument();
            free(docMem);
        }
    }

    static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) { return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); }
    
    static uint16_t calcColor(float value, float low, float high) {
        if (low >= high) return rgb565(255, 255, 0);
        if (value < low) { value = low; }
        if (value > high) { value = high; }
        int diff = (int)roundf(((high - value) / (high - low)) * 100.0f);
        if(diff < 0) diff = 0; if(diff > 100) diff = 100;
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
};

#endif