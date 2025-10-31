#ifndef TANKERKOENIGMODULE_HPP
#define TANKERKOENIGMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <functional>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ArduinoJson.h>
#include "DrawableModule.hpp"
#include "webconfig.hpp"

class WebClientModule;
class GeneralTimeConverter;

#define STATION_PRICE_STATS_VERSION 3 

struct DailyPriceStats {
    PsramString date;
    float e5_low = 0.0; float e5_high = 0.0;
    float e10_low = 0.0; float e10_high = 0.0;
    float diesel_low = 0.0; float diesel_high = 0.0;
};
struct StationPriceHistory {
    PsramString stationId;
    PsramVector<DailyPriceStats> dailyStats;
};
struct AveragePrices {
    float avgE5Low = 0.0; float avgE5High = 0.0; float avgE10Low = 0.0;
    float avgE10High = 0.0; float avgDieselLow = 0.0; float avgDieselHigh = 0.0;
    int count = 0;
};
struct StationData {
    PsramString id; PsramString name; PsramString brand; PsramString street;
    PsramString houseNumber; PsramString postCode; PsramString place;
    float e5; float e10; float diesel; bool isOpen;
    time_t lastPriceChange = 0;
    StationData();
};
struct LastPriceCache {
    PsramString stationId; float e5; float e10; float diesel; time_t timestamp;
};
enum class PriceTrend { TREND_STABLE, TREND_RISING, TREND_FALLING };
struct TrendStatus {
    PsramString stationId;
    PriceTrend e5_min_trend = PriceTrend::TREND_STABLE; PriceTrend e5_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend e10_min_trend = PriceTrend::TREND_STABLE; PriceTrend e10_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend diesel_min_trend = PriceTrend::TREND_STABLE; PriceTrend diesel_max_trend = PriceTrend::TREND_STABLE;
};

class TankerkoenigModule : public DrawableModule {
public:
    TankerkoenigModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient, DeviceConfig* config);
    ~TankerkoenigModule();

    void begin();
    void onUpdate(std::function<void()> callback);
    void setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes, unsigned long pageDisplaySec);
    void queueData();
    void processData();
    PsramVector<StationData> getStationCache();
    StationPriceHistory getStationPriceHistory(const PsramString& stationId);

    // --- MODERN-SCHNITTSTELLE ---
    void draw() override;
    void tick() override;
    bool isEnabled() override;
    const char* getModuleName() const override { return "TankerkoenigModule"; }
    const char* getModuleDisplayName() const override { return "Tankstellenpreise"; }
    int getCurrentPage() const override { return currentPage; }
    int getTotalPages() const override { return totalPages; }
    void configure(const ModuleConfig& config) override;
    JsonObject backup(JsonDocument& doc) override;
    void restore(JsonObject& obj) override;

    // --- DUMMY-IMPLEMENTIERUNGEN für Kompatibilität ---
    unsigned long getDisplayDuration() override { return 0; }
    void resetPaging() override {}

protected:
    void onActivate() override;

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    GeneralTimeConverter& timeConverter;
    DeviceConfig* _deviceConfig;
    PsramString api_key;
    PsramString station_ids;
    int top_offset;
    WebClientModule* webClient;
    std::function<void()> updateCallback;
    SemaphoreHandle_t dataMutex;
    PsramVector<StationData> station_data_list;
    PsramVector<StationData> station_cache;
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
    PsramVector<TrendStatus> _trendStatusCache;
    ModuleConfig _modConfig;

    void updateFailsafeTimeout();
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
    void updatePriceCache(const PsramString& stationId, float e5, float e10, float diesel, time_t lastChange);
    bool getPriceFromCache(const PsramString& stationId, float& e5, float& e10, float& diesel, time_t& lastChange);
    void cleanupOldPriceCacheEntries();
    PriceTrend calculateTrend(const PsramVector<float>& x_values, const PsramVector<float>& y_values);
    void updateAndDetermineTrends(const PsramString& stationId);

    // =================================================================
    // ======================== TABU-ZONE ============================
    // =================================================================
    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    uint16_t calcColor(float value, float low, float high);
    int drawPrice(int x, int y, float price, uint16_t color);
    void drawPriceLine(int y, const char* label, float current, float min, float max, PriceTrend minTrend, PriceTrend maxTrend);
    void drawTrendArrow(int x, int y, PriceTrend trend);
    // ===================== ENDE DER TABU-ZONE ======================
};

#endif // TANKERKOENIGMODULE_HPP