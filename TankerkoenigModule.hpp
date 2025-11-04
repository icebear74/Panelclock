#ifndef TANKERKOENIGMODULE_HPP
#define TANKERKOENIGMODULE_HPP

#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include "DrawableModule.hpp"
#include <functional>

class GeneralTimeConverter;
class WebClientModule;
struct DeviceConfig;

#define STATION_PRICE_STATS_VERSION 1
#define TANKERKOENIG_SAVE_HISTORY true  // ← SWITCH: false = Historie wird NICHT gespeichert

enum class PriceTrend {
    TREND_RISING,
    TREND_FALLING,
    TREND_STABLE
};

struct DailyPriceStats {
    PsramString date;
    float e5_low = 0.0f, e5_high = 0.0f;
    float e10_low = 0.0f, e10_high = 0.0f;
    float diesel_low = 0.0f, diesel_high = 0.0f;
};

struct StationPriceHistory {
    PsramString stationId;
    PsramVector<DailyPriceStats> dailyStats;
};

struct StationData {
    PsramString id, name, brand, street, houseNumber, postCode, place;
    float e5, e10, diesel;
    bool isOpen;
    time_t lastPriceChange;
    StationData();
};

struct AveragePrices {
    float avgE5Low = 0.0f, avgE5High = 0.0f;
    float avgE10Low = 0.0f, avgE10High = 0.0f;
    float avgDieselLow = 0.0f, avgDieselHigh = 0.0f;
    int count = 0;
};

struct LastPriceCache {
    PsramString stationId;
    float e5, e10, diesel;
    time_t timestamp;
};

struct TrendStatus {
    PsramString stationId;
    PriceTrend e5_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend e5_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend e10_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend e10_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend diesel_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend diesel_max_trend = PriceTrend::TREND_STABLE;
};

class TankerkoenigModule : public DrawableModule {
public:
    TankerkoenigModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient, DeviceConfig* config);
    ~TankerkoenigModule();

    void begin();
    void setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes, unsigned long pageDisplaySec);
    void queueData();
    void processData();
    void onUpdate(std::function<void()> callback);
    
    // DrawableModule Interface
    const char* getModuleName() const override { return "TankerkoenigModule"; }
    const char* getModuleDisplayName() const override { return "Tankstellen"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    void resetPaging() override;
    bool isEnabled() override;
    unsigned long getDisplayDuration() override;
    int getCurrentPage() const override { return currentPage; }
    int getTotalPages() const override { return totalPages; }
    
    void configure(const ModuleConfig& config) override;
    void onActivate() override;
    JsonObject backup(JsonDocument& doc) override;
    void restore(JsonObject& obj) override;
    
    // Public Methoden für WebServer
    PsramVector<StationData> getStationCache();
    StationPriceHistory getStationPriceHistory(const PsramString& stationId);

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    GeneralTimeConverter& timeConverter;
    DeviceConfig* _deviceConfig;
    int top_offset;
    WebClientModule* webClient;
    SemaphoreHandle_t dataMutex;
    ModuleConfig _modConfig;

    PsramString api_key, station_ids, resource_url;
    bool _isEnabled = false;
    unsigned long _pageDisplayDuration = 15000;
    
    int currentPage = 0;
    int totalPages = 1;
    uint32_t _logicTicksSincePageSwitch = 0;
    const uint32_t LOGIC_TICKS_PER_PAGE = 150; // 150 * 100ms = 15 Sekunden (wird dynamisch angepasst)
    uint32_t _currentTicksPerPage = LOGIC_TICKS_PER_PAGE;

    PsramVector<StationData> station_data_list, station_cache;
    PsramVector<StationPriceHistory> price_statistics;
    PsramVector<LastPriceCache> _lastPriceCache;
    PsramVector<TrendStatus> _trendStatusCache;
    
    std::function<void()> updateCallback;
    char* pending_buffer = nullptr;
    size_t buffer_size = 0;
    time_t last_processed_update = 0;
    bool data_pending = false;

    void parseAndProcessJson(const char* buffer, size_t size);
    void drawPriceLine(int y, const char* label, float current, float min, float max, PriceTrend minTrend, PriceTrend maxTrend);
    int drawPrice(int x, int y, float price, uint16_t color);
    void drawTrendArrow(int x, int y, PriceTrend trend);
    uint16_t calcColor(float value, float low, float high);
    PsramString truncateString(const PsramString& text, int maxWidth);
    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    
    void loadPriceCache();
    void savePriceCache();
    void updatePriceCache(const PsramString& stationId, float e5, float e10, float diesel, time_t lastChange);
    bool getPriceFromCache(const PsramString& stationId, float& e5, float& e10, float& diesel, time_t& lastChange);
    void cleanupOldPriceCacheEntries();
    
    void updatePriceStatistics(const PsramString& stationId, float currentE5, float currentE10, float currentDiesel);
    void trimPriceStatistics(StationPriceHistory& history);
    void trimAllPriceStatistics();
    AveragePrices calculateAverages(const PsramString& stationId);
    bool migratePriceStatistics(JsonDocument& doc);
    void savePriceStatistics();
    void loadPriceStatistics();
    void loadStationCache();
    
    PriceTrend calculateTrend(const PsramVector<float>& x_values, const PsramVector<float>& y_values);
    void updateAndDetermineTrends(const PsramString& stationId);
    void updateFailsafeTimeout();
};

#endif // TANKERKOENIGMODULE_HPP