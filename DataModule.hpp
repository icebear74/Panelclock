#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include <functional>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ArduinoJson.h>
#include "DrawableModule.hpp"

class WebClientModule;
class GeneralTimeConverter;

#define MOVING_AVERAGE_DAYS 30 
#define STATION_PRICE_STATS_VERSION 3 

struct DailyPriceStats {
    PsramString date;
    float e5_low = 0.0;
    float e5_high = 0.0;
    float e10_low = 0.0;
    float e10_high = 0.0;
    float diesel_low = 0.0;
    float diesel_high = 0.0;
};

struct StationPriceHistory {
    PsramString stationId;
    PsramVector<DailyPriceStats> dailyStats;
};

struct AveragePrices {
    float avgE5Low = 0.0;
    float avgE5High = 0.0;
    float avgE10Low = 0.0;
    float avgE10High = 0.0;
    float avgDieselLow = 0.0;
    float avgDieselHigh = 0.0;
    int count = 0;
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
    time_t lastPriceChange = 0;
    StationData();
};

struct LastPriceCache {
    PsramString stationId;
    float e5;
    float e10;
    float diesel;
    time_t timestamp;
};

enum class PriceTrend { STABLE, RISING, FALLING };

struct TrendStatus {
    PsramString stationId;
    PriceTrend e5_min_trend = PriceTrend::STABLE;
    PriceTrend e5_max_trend = PriceTrend::STABLE;
    PriceTrend e10_min_trend = PriceTrend::STABLE;
    PriceTrend e10_max_trend = PriceTrend::STABLE;
    PriceTrend diesel_min_trend = PriceTrend::STABLE;
    PriceTrend diesel_max_trend = PriceTrend::STABLE;
};

struct LastAveragePrice {
    PsramString stationId;
    float avgE5Low = 0.0;
    float avgE5High = 0.0;
    float avgE10Low = 0.0;
    float avgE10High = 0.0;
    float avgDieselLow = 0.0;
    float avgDieselHigh = 0.0;
};

class DataModule : public DrawableModule {
public:
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient);
    ~DataModule();

    void begin();
    void onUpdate(std::function<void()> callback);
    void setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes, unsigned long pageDisplaySec);
    void queueData();
    void processData();

    void draw() override;
    void tick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;

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
    unsigned long _pageDisplayDuration = 10000;
    unsigned long lastPageSwitchTime = 0;
    bool _isEnabled = false;

    PsramVector<LastPriceCache> _lastPriceCache;
    PsramVector<LastAveragePrice> _lastAverageCache;
    PsramVector<TrendStatus> _trendStatusCache;

    PsramString truncateString(const PsramString& text, int maxWidth);
    void parseAndProcessJson(const char* buffer, size_t size);
    
    void updatePriceStatistics(const PsramString& stationId, float currentE5, float currentE10, float currentDiesel);
    void trimPriceStatistics(StationPriceHistory& history);
    void trimAllPriceStatistics();
    AveragePrices calculateAverages(const PsramString& stationId);
    bool migratePriceStatistics(JsonDocument& doc);
    void savePriceStatistics();
    void loadPriceStatistics();
    
    void loadStationCache();
    
    void loadPriceCache();
    void savePriceCache();
    void updatePriceCache(const PsramString& stationId, float e5, float e10, float diesel);
    bool getPriceFromCache(const PsramString& stationId, float& e5, float& e10, float& diesel);
    void cleanupOldPriceCacheEntries();

    void loadAverageCache();
    void saveAverageCache();
    void updateAndDetermineTrends(const PsramString& stationId);

    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    uint16_t calcColor(float value, float low, float high);
    int drawPrice(int x, int y, float price, uint16_t color);
    void drawPriceLine(int y, const char* label, float current, float min, float max, PriceTrend minTrend, PriceTrend maxTrend);
    void drawTrendArrow(int x, int y, PriceTrend trend);
};

#endif // DATAMODULE_HPP