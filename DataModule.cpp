#include "DataModule.hpp"
#include "WebClientModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "certs.hpp"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <algorithm>
#include <esp_heap_caps.h>

#define PRICE_CACHE_FILENAME "/last_prices.json"
#define AVG_CACHE_FILENAME "/avg_price_trends.json" // Veraltet

StationData::StationData() : e5(0.0f), e10(0.0f), diesel(0.0f), isOpen(false), lastPriceChange(0) {}

/**
 * @brief Konstruktor für das DataModule.
 * @param u8g2 Referenz auf die U8G2-Bibliothek für die Textdarstellung.
 * @param canvas Referenz auf die GFX-Canvas für die Grafikausgabe.
 * @param timeConverter Referenz auf den Zeitumrechner für lokale Zeit.
 * @param topOffset Y-Offset für die Zeichnung auf dem Display.
 * @param webClient Zeiger auf das WebClientModule für API-Anfragen.
 * @param config Zeiger auf die globale Gerätekongfiguration.
 */
DataModule::DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient, DeviceConfig* config)
     : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), _deviceConfig(config), top_offset(topOffset), webClient(webClient), last_processed_update(0) {
    dataMutex = xSemaphoreCreateMutex();
}

DataModule::~DataModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (pending_buffer) free(pending_buffer);
}

/**
 * @brief Initialisiert das Modul und lädt Daten aus dem Dateisystem.
 * 
 * Bereinigt veraltete Cache-Dateien und lädt die Preis-Statistiken,
 * den Stations-Cache und den letzten Preis-Cache.
 */
void DataModule::begin() {
    if (LittleFS.exists("/price_history.json")) {
        Serial.println("[DataModule] Altes Preis-Format 'price_history.json' gefunden. Wird gelöscht.");
        LittleFS.remove("/price_history.json");
    }
    if (LittleFS.exists(AVG_CACHE_FILENAME)) {
        Serial.println("[DataModule] Veraltete Cache-Datei 'avg_price_trends.json' gefunden. Wird gelöscht.");
        LittleFS.remove(AVG_CACHE_FILENAME);
    }
    loadPriceStatistics();
    loadStationCache();
    loadPriceCache();
    cleanupOldPriceCacheEntries();
}

/**
 * @brief Registriert eine Callback-Funktion, die bei Daten-Updates aufgerufen wird.
 * @param callback Die aufzurufende Funktion.
 */
void DataModule::onUpdate(std::function<void()> callback) {
    updateCallback = callback;
}

/**
 * @brief Konfiguriert das Modul mit den notwendigen Parametern.
 * @param apiKey Der API-Key für Tankerkönig.
 * @param stationIds Eine kommaseparierte Liste von Tankstellen-IDs.
 * @param fetchIntervalMinutes Das Abrufintervall in Minuten.
 * @param pageDisplaySec Die Anzeigedauer pro Tankstelle in Sekunden.
 */
void DataModule::setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes, unsigned long pageDisplaySec) {
    this->api_key = apiKey;
    this->station_ids = stationIds;
    this->_pageDisplayDuration = pageDisplaySec > 0 ? pageDisplaySec * 1000UL : 10000;
    
    _isEnabled = !apiKey.empty() && !stationIds.empty();

    if (_isEnabled) {
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

        auto remove_by_id = [&](const auto& entry){
            for(const auto& id : current_id_list) { if (entry.stationId == id) return false; }
            return true;
        };

        size_t original_stats_size = price_statistics.size();
        price_statistics.erase(std::remove_if(price_statistics.begin(), price_statistics.end(), remove_by_id), price_statistics.end());
        if(price_statistics.size() < original_stats_size) savePriceStatistics();

        size_t original_cache_size = _lastPriceCache.size();
        _lastPriceCache.erase(std::remove_if(_lastPriceCache.begin(), _lastPriceCache.end(), remove_by_id), _lastPriceCache.end());
        if(_lastPriceCache.size() < original_cache_size) savePriceCache();
        
        xSemaphoreGive(dataMutex);
    }
}

/**
 * @brief Gibt die gesamte Anzeigedauer für alle Seiten des Moduls zurück.
 * @return Die Dauer in Millisekunden.
 */
unsigned long DataModule::getDisplayDuration() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return _pageDisplayDuration;
    int num_stations = station_data_list.size();
    xSemaphoreGive(dataMutex);
    return (_pageDisplayDuration * (num_stations > 0 ? num_stations : 1));
}

/**
 * @brief Gibt zurück, ob das Modul aktiviert ist.
 * @return true, wenn aktiviert, sonst false.
 */
bool DataModule::isEnabled() { return _isEnabled; }

/**
 * @brief Setzt das Paging auf die erste Seite zurück.
 */
void DataModule::resetPaging() { currentPage = 0; lastPageSwitchTime = millis(); }

/**
 * @brief Wird periodisch aufgerufen, um Paging und andere zeitgesteuerte Aktionen zu handhaben.
 */
void DataModule::tick() {
    unsigned long now = millis();
    if (_pageDisplayDuration > 0 && now - lastPageSwitchTime > _pageDisplayDuration) {
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

/**
 * @brief Stellt eine Anfrage zum Abrufen neuer Daten in die Warteschlange des WebClients.
 */
void DataModule::queueData() {
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

/**
 * @brief Verarbeitet heruntergeladene Daten, falls vorhanden.
 */
void DataModule::processData() {
    if (data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            parseAndProcessJson(pending_buffer, buffer_size);
            free(pending_buffer);
            pending_buffer = nullptr;
            data_pending = false;
            xSemaphoreGive(dataMutex);
            if (this->updateCallback) this->updateCallback();
        }
    }
}

void DataModule::parseAndProcessJson(const char* buffer, size_t size) {
    JsonDocument doc;
    PsramVector<StationData> new_station_data_list;
    DeserializationError error = deserializeJson(doc, buffer, size);

    if (!error && doc["ok"] == true) {
        JsonObject prices = doc["prices"];
        PsramVector<PsramString> id_list;
        PsramString temp_ids = station_ids;
        char* strtok_ctx;
        char* id_token = strtok_r((char*)temp_ids.c_str(), ",", &strtok_ctx);
        while(id_token != nullptr) { id_list.push_back(id_token); id_token = strtok_r(nullptr, ",", &strtok_ctx); }

        _trendStatusCache.clear();
        for (const auto& id : id_list) {
            if (prices[id.c_str()].is<JsonObject>()) {
                JsonObject station_json = prices[id.c_str()];
                StationData new_data;
                new_data.id = id;
                
                for(const auto& cache_entry : station_cache) {
                    if (cache_entry.id == id) {
                        new_data.name = cache_entry.name; new_data.brand = cache_entry.brand;
                        new_data.street = cache_entry.street; new_data.houseNumber = cache_entry.houseNumber;
                        new_data.postCode = cache_entry.postCode; new_data.place = cache_entry.place;
                        break;
                    }
                }

                bool is_open = (station_json["status"] == "open");
                new_data.isOpen = is_open;

                const StationData* old_data_ptr = nullptr;
                for(const auto& old_data : station_data_list) {
                    if (old_data.id == new_data.id) { old_data_ptr = &old_data; break; }
                }

                if (is_open) {
                    new_data.e5 = station_json["e5"].as<float>();
                    new_data.e10 = station_json["e10"].as<float>();
                    new_data.diesel = station_json["diesel"].as<float>();
                    
                    if (old_data_ptr && (old_data_ptr->e5 != new_data.e5 || old_data_ptr->e10 != new_data.e10 || old_data_ptr->diesel != new_data.diesel)) {
                        new_data.lastPriceChange = last_processed_update;
                    } else if (old_data_ptr) {
                        new_data.lastPriceChange = old_data_ptr->lastPriceChange;
                    } else {
                        new_data.lastPriceChange = last_processed_update;
                    }
                    updatePriceStatistics(new_data.id, new_data.e5, new_data.e10, new_data.diesel);
                } else {
                    if (old_data_ptr && old_data_ptr->isOpen) { // Gerade geschlossen
                        updatePriceCache(new_data.id, old_data_ptr->e5, old_data_ptr->e10, old_data_ptr->diesel, old_data_ptr->lastPriceChange);
                    }
                    
                    getPriceFromCache(new_data.id, new_data.e5, new_data.e10, new_data.diesel, new_data.lastPriceChange);
                }
                new_station_data_list.push_back(new_data);
                updateAndDetermineTrends(id);
            }
        }
        station_data_list = new_station_data_list;
    } else { Serial.printf("[DataModule] JSON-Parsing-Fehler: %s\n", error.c_str()); }
}

/**
 * @brief Zeichnet den aktuellen Zustand des Moduls auf die Canvas.
 */
void DataModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    
    totalPages = station_data_list.size() > 0 ? station_data_list.size() : 1;
    if (currentPage >= totalPages) currentPage = 0;

    canvas.fillScreen(0);
    u8g2.begin(canvas);

    if (station_data_list.empty()) {
         u8g2.setFont(u8g2_font_7x14_tf); u8g2.setForegroundColor(0xFFFF);
         const char* text = "Keine Tankstelle konfiguriert.";
         u8g2.setCursor((canvas.width() - u8g2.getUTF8Width(text)) / 2, 30);
         u8g2.print(text);
         xSemaphoreGive(dataMutex);
         return;
    }
    
    const StationData& data = station_data_list[currentPage];

    // --- OBERER BEREICH ---
    const int PADDING = 10;
    const int LEFT_MARGIN = 5;
    const int RIGHT_MARGIN = 5;
    const int totalWidth = canvas.width() - LEFT_MARGIN - RIGHT_MARGIN;

    u8g2.setFont(u8g2_font_helvB14_tf); 
    PsramString brandText = data.brand;
    int brandWidth = u8g2.getUTF8Width(brandText.c_str());

    u8g2.setFont(u8g2_font_5x8_tf);
    PsramString line1 = data.street;
    if (!data.houseNumber.empty()) { line1 += " "; line1 += data.houseNumber; }
    PsramString line2 = data.postCode;
    if (!data.place.empty()) { line2 += " "; line2 += data.place; }
    int addressWidth = std::max(u8g2.getUTF8Width(line1.c_str()), u8g2.getUTF8Width(line2.c_str()));

    if (brandWidth + addressWidth + PADDING > totalWidth) {
        int maxPartWidth = (totalWidth - PADDING) / 2;
        
        u8g2.setFont(u8g2_font_helvB14_tf);
        brandText = truncateString(brandText, maxPartWidth);

        u8g2.setFont(u8g2_font_5x8_tf);
        line1 = truncateString(line1, maxPartWidth);
        line2 = truncateString(line2, maxPartWidth);
    }

    u8g2.setFont(u8g2_font_helvB14_tf);
    u8g2.setForegroundColor(0xFFFF);
    u8g2.setCursor(LEFT_MARGIN, 15);
    u8g2.print(brandText.c_str());

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setCursor(canvas.width() - u8g2.getUTF8Width(line1.c_str()) - RIGHT_MARGIN, 6);
    u8g2.print(line1.c_str());
    u8g2.setCursor(canvas.width() - u8g2.getUTF8Width(line2.c_str()) - RIGHT_MARGIN, 16);
    u8g2.print(line2.c_str());
    
    // --- TRENNLINIE ---
    canvas.drawFastHLine(0, 17, canvas.width(), rgb565(128, 128, 128));

    // --- PREISTABELLE ---
    AveragePrices averages = calculateAverages(data.id); 
    if (averages.avgE5Low == 0.0) averages.avgE5Low = data.e5 > 0 ? data.e5 - 0.05 : 0;
    if (averages.avgE5High == 0.0) averages.avgE5High = data.e5 > 0 ? data.e5 + 0.05 : 0;
    if (averages.avgE10Low == 0.0) averages.avgE10Low = data.e10 > 0 ? data.e10 - 0.05 : 0;
    if (averages.avgE10High == 0.0) averages.avgE10High = data.e10 > 0 ? data.e10 + 0.05 : 0;
    if (averages.avgDieselLow == 0.0) averages.avgDieselLow = data.diesel > 0 ? data.diesel - 0.15 : 0;
    if (averages.avgDieselHigh == 0.0) averages.avgDieselHigh = data.diesel > 0 ? data.diesel + 0.15 : 0;

    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.setForegroundColor(rgb565(0, 255, 255));
    int y_pos = 25;

    if (averages.count > 0 && _deviceConfig) {
        char count_str[12];
        snprintf(count_str, sizeof(count_str), "(%d/%d)", averages.count, _deviceConfig->movingAverageDays);
        u8g2.setCursor(188 - u8g2.getUTF8Width(count_str), y_pos);
        u8g2.print(count_str);
    }

    if (data.lastPriceChange > 0) {
        time_t local_change_time = timeConverter.toLocal(data.lastPriceChange);
        struct tm timeinfo; localtime_r(&local_change_time, &timeinfo);
        char time_str[6]; strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
        u8g2.setCursor(84 + (50 - u8g2.getUTF8Width(time_str)) / 2, y_pos);
        u8g2.print(time_str);
    } else {
        const char* no_time_str = "--:--";
        u8g2.setCursor(84 + (50 - u8g2.getUTF8Width(no_time_str)) / 2, y_pos);
        u8g2.print(no_time_str);
    }
    
    TrendStatus currentTrend;
    for(const auto& ts : _trendStatusCache) {
        if(ts.stationId == data.id) { currentTrend = ts; break; }
    }

    drawPriceLine(37, "E5", data.e5, averages.avgE5Low, averages.avgE5High, currentTrend.e5_min_trend, currentTrend.e5_max_trend);
    drawPriceLine(50, "E10", data.e10, averages.avgE10Low, averages.avgE10High, currentTrend.e10_min_trend, currentTrend.e10_max_trend);
    drawPriceLine(63, "Dies.", data.diesel, averages.avgDieselLow, averages.avgDieselHigh, currentTrend.diesel_min_trend, currentTrend.diesel_max_trend);
    
    if (!data.isOpen) {
        uint16_t red = rgb565(255, 0, 0);
        canvas.drawLine(0, 17, canvas.width() - 1, canvas.height() - 1, red);
        canvas.drawLine(canvas.width() - 1, 17, 0, canvas.height() - 1, red);
    }
    xSemaphoreGive(dataMutex);
}

void DataModule::drawPriceLine(int y, const char* label, float current, float min, float max, PriceTrend minTrend, PriceTrend maxTrend) {
    u8g2.setFont(u8g2_font_7x14_tf);
    u8g2.setForegroundColor(0xFFFF);
    u8g2.setCursor(2, y);
    u8g2.print(label);
    
    int x_min = 42, x_current = 92, x_max = 142;

    int min_width = drawPrice(x_min, y, min, rgb565(0, 255, 0));
    if(min > 0) drawTrendArrow(x_min + min_width + 2, y - 5, minTrend);

    drawPrice(x_current, y, current, calcColor(current, min, max));
    
    int max_width = drawPrice(x_max, y, max, rgb565(255, 0, 0));
    if(max > 0) drawTrendArrow(x_max + max_width + 2, y - 5, maxTrend);
}

int DataModule::drawPrice(int x, int y, float price, uint16_t color) {
    if (price <= 0) return 0;
    u8g2.setForegroundColor(color);
    String priceStr = String(price, 3);
    priceStr.replace('.', ',');
    String mainPricePart = priceStr.substring(0, priceStr.length() - 1);
    char last_digit_char = priceStr.charAt(priceStr.length() - 1);
    char last_digit_str[2] = {last_digit_char, '\0'};
    
    u8g2.setFont(u8g2_font_7x14_tf);
    int mainPriceWidth = u8g2.getUTF8Width(mainPricePart.c_str());
    u8g2.setCursor(x, y);
    u8g2.print(mainPricePart);
    
    int superscriptX = x + mainPriceWidth + 1;
    u8g2.setFont(u8g2_font_5x8_tf);
    int superscriptWidth = u8g2.getUTF8Width(last_digit_str);
    u8g2.setCursor(superscriptX, y - 4);
    u8g2.print(last_digit_str);

    u8g2.setFont(u8g2_font_6x13_me);
    int euroWidth = u8g2.getUTF8Width("€");
    u8g2.setCursor(superscriptX + superscriptWidth + 1, y);
    u8g2.print("€");

    return mainPriceWidth + 1 + superscriptWidth + 1 + euroWidth;
}

void DataModule::drawTrendArrow(int x, int y, PriceTrend trend) {
    switch (trend) {
        case PriceTrend::TREND_RISING:
            canvas.fillTriangle(x, y, x + 2, y + 3, x - 2, y + 3, rgb565(255, 0, 0));
            break;
        case PriceTrend::TREND_FALLING:
            canvas.fillTriangle(x, y + 3, x + 2, y, x - 2, y, rgb565(0, 255, 0));
            break;
        case PriceTrend::TREND_STABLE:
            canvas.fillCircle(x, y + 1, 1, 0xFFFF);
            break;
    }
}

PsramString DataModule::truncateString(const PsramString& text, int maxWidth) {
    if (u8g2.getUTF8Width(text.c_str()) <= maxWidth) { return text; }
    PsramString truncated = text;
    while (!truncated.empty() && u8g2.getUTF8Width((truncated + "...").c_str()) > maxWidth) {
        truncated.pop_back();
    }
    return truncated + "...";
}

void DataModule::loadPriceCache() {
    if (LittleFS.exists(PRICE_CACHE_FILENAME)) {
        File file = LittleFS.open(PRICE_CACHE_FILENAME, "r");
        if (file) {
            JsonDocument doc;
            if (deserializeJson(doc, file) == DeserializationError::Ok) {
                _lastPriceCache.clear();
                for (JsonObject obj : doc.as<JsonArray>()) {
                    _lastPriceCache.push_back({
                        obj["id"].as<const char*>(), obj["e5"], obj["e10"], obj["diesel"], obj["ts"]
                    });
                }
            }
            file.close();
        }
    }
}

void DataModule::savePriceCache() {
    File file = LittleFS.open(PRICE_CACHE_FILENAME, "w");
    if (file) {
        JsonDocument doc;
        JsonArray arr = doc.to<JsonArray>();
        for (const auto& entry : _lastPriceCache) {
            JsonObject obj = arr.add<JsonObject>();
            obj["id"] = entry.stationId; obj["e5"] = entry.e5; obj["e10"] = entry.e10;
            obj["diesel"] = entry.diesel; obj["ts"] = entry.timestamp;
        }
        serializeJson(doc, file);
        file.close();
    }
}

void DataModule::updatePriceCache(const PsramString& stationId, float e5, float e10, float diesel, time_t lastChange) {
    bool found = false;
    for (auto& entry : _lastPriceCache) {
        if (entry.stationId == stationId) {
            entry.e5 = e5; entry.e10 = e10; entry.diesel = diesel; entry.timestamp = lastChange;
            found = true; break;
        }
    }
    if (!found) { _lastPriceCache.push_back({stationId, e5, e10, diesel, lastChange}); }
    savePriceCache();
}

bool DataModule::getPriceFromCache(const PsramString& stationId, float& e5, float& e10, float& diesel, time_t& lastChange) {
    for (const auto& entry : _lastPriceCache) {
        if (entry.stationId == stationId) {
            e5 = entry.e5; e10 = entry.e10; diesel = entry.diesel;
            lastChange = entry.timestamp;
            return true;
        }
    }
    e5 = e10 = diesel = 0.0f;
    lastChange = 0;
    return false;
}

void DataModule::cleanupOldPriceCacheEntries() {
    size_t original_size = _lastPriceCache.size();
    _lastPriceCache.erase(
        std::remove_if(_lastPriceCache.begin(), _lastPriceCache.end(),
            [](const LastPriceCache& entry) { return entry.timestamp == 0; }),
        _lastPriceCache.end()
    );
    if (_lastPriceCache.size() < original_size) { savePriceCache(); }
}

/**
 * @brief Berechnet den Preistrend mittels linearer Regression.
 * 
 * @param x_values Vektor der Zeitwerte (Tage in der Vergangenheit, z.B. -7, -6, ..., 0).
 * @param y_values Vektor der zugehörigen Preiswerte.
 * @return PriceTrend Gibt TREND_RISING, TREND_FALLING oder TREND_STABLE zurück.
 */
PriceTrend DataModule::calculateTrend(const PsramVector<float>& x_values, const PsramVector<float>& y_values) {
    int n = x_values.size();
    if (n < 2) {
        return PriceTrend::TREND_STABLE;
    }

    double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x_squared = 0.0;
    for (int i = 0; i < n; ++i) {
        sum_x += x_values[i];
        sum_y += y_values[i];
        sum_xy += x_values[i] * y_values[i];
        sum_x_squared += x_values[i] * x_values[i];
    }

    const double SLOPE_DENOMINATOR_THRESHOLD = 0.000001;
    double denominator = (n * sum_x_squared) - (sum_x * sum_x);
    if (abs(denominator) < SLOPE_DENOMINATOR_THRESHOLD) {
        return PriceTrend::TREND_STABLE;
    }

    double slope = (n * sum_xy - sum_x * sum_y) / denominator;

    const double STABILITY_THRESHOLD = 0.001; 
    if (slope > STABILITY_THRESHOLD) {
        return PriceTrend::TREND_RISING;
    } else if (slope < -STABILITY_THRESHOLD) {
        return PriceTrend::TREND_FALLING;
    } else {
        return PriceTrend::TREND_STABLE;
    }
}

void DataModule::updateAndDetermineTrends(const PsramString& stationId) {
    if (!_deviceConfig) return;

    StationPriceHistory* history = nullptr;
    for (auto& h : price_statistics) {
        if (h.stationId == stationId) {
            history = &h;
            break;
        }
    }

    if (!history || history->dailyStats.size() < 2) {
        return;
    }
    
    PsramVector<float> x_values, y_e5_low, y_e5_high, y_e10_low, y_e10_high, y_diesel_low, y_diesel_high;

    time_t now_utc; time(&now_utc);
    
    for (const auto& s : history->dailyStats) {
        struct tm tm_stat = {0};
        sscanf(s.date.c_str(), "%d-%d-%d", &tm_stat.tm_year, &tm_stat.tm_mon, &tm_stat.tm_mday);
        tm_stat.tm_year -= 1900;
        tm_stat.tm_mon -= 1;
        time_t stat_time = mktime(&tm_stat);
        
        double days_diff = difftime(now_utc, stat_time) / (60 * 60 * 24.0);

        if (days_diff < _deviceConfig->trendAnalysisDays) {
            x_values.push_back(-days_diff);
            if(s.e5_low > 0) y_e5_low.push_back(s.e5_low);
            if(s.e5_high > 0) y_e5_high.push_back(s.e5_high);
            if(s.e10_low > 0) y_e10_low.push_back(s.e10_low);
            if(s.e10_high > 0) y_e10_high.push_back(s.e10_high);
            if(s.diesel_low > 0) y_diesel_low.push_back(s.diesel_low);
            if(s.diesel_high > 0) y_diesel_high.push_back(s.diesel_high);
        }
    }

    TrendStatus trends;
    trends.stationId = stationId;

    trends.e5_min_trend = calculateTrend(x_values, y_e5_low);
    trends.e5_max_trend = calculateTrend(x_values, y_e5_high);
    trends.e10_min_trend = calculateTrend(x_values, y_e10_low);
    trends.e10_max_trend = calculateTrend(x_values, y_e10_high);
    trends.diesel_min_trend = calculateTrend(x_values, y_diesel_low);
    trends.diesel_max_trend = calculateTrend(x_values, y_diesel_high);

    bool trend_found = false;
    for(auto& ts : _trendStatusCache) {
        if(ts.stationId == stationId) { ts = trends; trend_found = true; break; }
    }
    if(!trend_found) { _trendStatusCache.push_back(trends); }
}


void DataModule::updatePriceStatistics(const PsramString& stationId, float currentE5, float currentE10, float currentDiesel) {
    time_t now_utc; time(&now_utc);
    struct tm timeinfo; localtime_r(&now_utc, &timeinfo);
    char date_str[11]; strftime(date_str, sizeof(date_str), "%Y-%m-%d", &timeinfo);
    PsramString today_date(date_str);

    StationPriceHistory* history = nullptr;
    for (auto& h : price_statistics) {
        if (h.stationId == stationId) { history = &h; break; }
    }
    if (!history) {
        price_statistics.push_back({stationId, {}});
        history = &price_statistics.back();
    }

    DailyPriceStats* today_stats = nullptr;
    for (auto& s : history->dailyStats) {
        if (s.date == today_date) { today_stats = &s; break; }
    }

    bool changed = false;
    if (!today_stats) {
        DailyPriceStats new_stats; new_stats.date = today_date;
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

    if (changed) { savePriceStatistics(); }
}

void DataModule::trimPriceStatistics(StationPriceHistory& history) {
    if (!_deviceConfig) return;
    time_t now_utc; time(&now_utc);
    time_t cutoff_epoch = now_utc - (_deviceConfig->movingAverageDays * 86400L);
    struct tm cutoff_tm; localtime_r(&cutoff_epoch, &cutoff_tm);
    char cutoff_date_str_buf[11]; strftime(cutoff_date_str_buf, sizeof(cutoff_date_str_buf), "%Y-%m-%d", &cutoff_tm);
    PsramString cutoff_date_str(cutoff_date_str_buf);

    history.dailyStats.erase(
        std::remove_if(history.dailyStats.begin(), history.dailyStats.end(), 
            [&](const DailyPriceStats& stats) { return stats.date < cutoff_date_str; }), 
        history.dailyStats.end());
}

void DataModule::trimAllPriceStatistics() {
    for (auto& history : price_statistics) { trimPriceStatistics(history); }
    savePriceStatistics();
}

AveragePrices DataModule::calculateAverages(const PsramString& stationId) {
    AveragePrices averages;
    float sumE5Low = 0.0, sumE5High = 0.0, sumE10Low = 0.0, sumE10High = 0.0, sumDieselLow = 0.0, sumDieselHigh = 0.0;
    int count = 0;

    for (const auto& h : price_statistics) {
        if (h.stationId == stationId) {
            for (const auto& s : h.dailyStats) {
                if(s.e5_low > 0) { sumE5Low += s.e5_low; sumE5High += s.e5_high; }
                if(s.e10_low > 0) { sumE10Low += s.e10_low; sumE10High += s.e10_high; }
                if(s.diesel_low > 0) { sumDieselLow += s.diesel_low; sumDieselHigh += s.diesel_high; }
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

bool DataModule::migratePriceStatistics(JsonDocument& doc) {
    if (!doc["version"].is<int>() || doc["version"] == STATION_PRICE_STATS_VERSION) { return true; }
    return false;
}

void DataModule::savePriceStatistics() {
    JsonDocument doc;
    doc["version"] = STATION_PRICE_STATS_VERSION;
    JsonObject prices = doc["prices"].to<JsonObject>();

    for (const auto& history : price_statistics) {
        JsonArray stats_array = prices[history.stationId.c_str()].to<JsonArray>();
        for (const auto& stats : history.dailyStats) {
            JsonObject obj = stats_array.add<JsonObject>();
            obj["date"] = stats.date;
            obj["e5_low"] = stats.e5_low; obj["e5_high"] = stats.e5_high;
            obj["e10_low"] = stats.e10_low; obj["e10_high"] = stats.e10_high;
            obj["diesel_low"] = stats.diesel_low; obj["diesel_high"] = stats.diesel_high;
        }
    }

    File file = LittleFS.open("/station_price_stats.json", "w");
    if (file) { serializeJson(doc, file); file.close(); }
}

void DataModule::loadPriceStatistics() {
    if (!LittleFS.exists("/station_price_stats.json")) { return; }
    File file = LittleFS.open("/station_price_stats.json", "r");
    if(file) {
        JsonDocument doc;
        if (deserializeJson(doc, file) == DeserializationError::Ok) {
            if (migratePriceStatistics(doc)) {
                price_statistics.clear();
                JsonObject prices = doc["prices"];
                for (JsonPair kv : prices) {
                    StationPriceHistory history;
                    history.stationId = kv.key().c_str();
                    for (JsonObject obj : kv.value().as<JsonArray>()) {
                        history.dailyStats.push_back({
                            obj["date"].as<const char*>(), obj["e5_low"], obj["e5_high"],
                            obj["e10_low"], obj["e10_high"], obj["diesel_low"], obj["diesel_high"]
                        });
                    }
                    price_statistics.push_back(history);
                }
            } else { LittleFS.remove("/station_price_stats.json"); }
        } else { LittleFS.remove("/station_price_stats.json"); }
        file.close();
    }
}

void DataModule::loadStationCache() {
    if (!LittleFS.exists("/station_cache.json")) { return; }
    File file = LittleFS.open("/station_cache.json", "r");
    if(file) {
        JsonDocument doc;
        if (deserializeJson(doc, file) == DeserializationError::Ok && doc["ok"] == true) {
            station_cache.clear();
            for(JsonObject station_json : doc["stations"].as<JsonArray>()) {
                StationData cache_entry;
                cache_entry.id = station_json["id"].as<const char*>();
                cache_entry.name = station_json["name"].as<const char*>();
                cache_entry.brand = station_json["brand"].as<const char*>();
                cache_entry.street = station_json["street"].as<const char*>();
                cache_entry.houseNumber = station_json["houseNumber"].as<const char*>();
                if (station_json["postCode"].is<int>()) {
                    cache_entry.postCode = String(station_json["postCode"].as<int>()).c_str();
                } else {
                    cache_entry.postCode = station_json["postCode"].as<const char*>();
                }
                cache_entry.place = station_json["place"].as<const char*>();
                station_cache.push_back(cache_entry);
            }
        }
        file.close();
    }
}

uint16_t DataModule::rgb565(uint8_t r, uint8_t g, uint8_t b) { 
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); 
}
    
uint16_t DataModule::calcColor(float value, float low, float high) {
    if (low >= high || value <= 0) return rgb565(255, 255, 0);

    float val = (value < low) ? low : (value > high ? high : value);
    
    int diff = (int)roundf(((high - val) / (high - low)) * 100.0f);

    uint8_t rval, gval;

    if (diff <= 50) { 
        rval = 255;
        gval = map(diff, 0, 50, 0, 255);
    } else { 
        gval = 255;
        rval = map(diff, 50, 100, 255, 0);
    }
    return rgb565(rval, gval, 0);
}

/**
 * @brief Gibt eine thread-sichere Kopie des Stations-Caches zurück.
 * @return Eine PsramVector<StationData> mit den gecachten Stationsinformationen.
 */
PsramVector<StationData> DataModule::getStationCache() {
    PsramVector<StationData> cache_copy;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        cache_copy = station_cache;
        xSemaphoreGive(dataMutex);
    }
    return cache_copy;
}

/**
 * @brief Ruft die Preis-Historie für eine bestimmte Tankstelle ab.
 * @param stationId Die ID der Tankstelle.
 * @return Eine Kopie der StationPriceHistory für die angegebene ID. Wenn nicht gefunden, ist das Objekt leer.
 */
StationPriceHistory DataModule::getStationPriceHistory(const PsramString& stationId) {
    StationPriceHistory history_copy;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (const auto& history : price_statistics) {
            if (history.stationId == stationId) {
                history_copy = history;
                break;
            }
        }
        xSemaphoreGive(dataMutex);
    }
    return history_copy;
}