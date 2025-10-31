#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <functional>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <ArduinoJson.h>
#include "DrawableModule.hpp"
#include "webconfig.hpp" // Für den Zugriff auf DeviceConfig

class WebClientModule;
class GeneralTimeConverter;

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

enum class PriceTrend { TREND_STABLE, TREND_RISING, TREND_FALLING };

struct TrendStatus {
    PsramString stationId;
    PriceTrend e5_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend e5_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend e10_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend e10_max_trend = PriceTrend::TREND_STABLE;
    PriceTrend diesel_min_trend = PriceTrend::TREND_STABLE;
    PriceTrend diesel_max_trend = PriceTrend::TREND_STABLE;
};

/**
 * @brief Verwaltet die Daten und die Anzeige für das Tankstellen-Modul.
 * 
 * Dieses Modul ruft Preise von der Tankerkönig-API ab, speichert eine Preis-Historie,
 * berechnet Trends sowie Durchschnittspreise und stellt alles auf dem Display dar.
 */
class DataModule : public DrawableModule {
public:
    /**
     * @brief Konstruktor für das DataModule.
     * @param u8g2 Referenz auf die U8G2-Bibliothek für die Textdarstellung.
     * @param canvas Referenz auf die GFX-Canvas für die Grafikausgabe.
     * @param timeConverter Referenz auf den Zeitumrechner für lokale Zeit.
     * @param topOffset Y-Offset für die Zeichnung auf dem Display.
     * @param webClient Zeiger auf das WebClientModule für API-Anfragen.
     * @param config Zeiger auf die globale Gerätekongfiguration.
     */
    DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter, int topOffset, WebClientModule* webClient, DeviceConfig* config);
    ~DataModule();

    /**
     * @brief Initialisiert das Modul und lädt Daten aus dem Dateisystem.
     */
    void begin();

    /**
     * @brief Registriert eine Callback-Funktion, die bei Daten-Updates aufgerufen wird.
     * @param callback Die aufzurufende Funktion.
     */
    void onUpdate(std::function<void()> callback);

    /**
     * @brief Konfiguriert das Modul mit den notwendigen Parametern.
     * @param apiKey Der API-Key für Tankerkönig.
     * @param stationIds Eine kommaseparierte Liste von Tankstellen-IDs.
     * @param fetchIntervalMinutes Das Abrufintervall in Minuten.
     * @param pageDisplaySec Die Anzeigedauer pro Tankstelle in Sekunden.
     */
    void setConfig(const PsramString& apiKey, const PsramString& stationIds, int fetchIntervalMinutes, unsigned long pageDisplaySec);

    /**
     * @brief Stellt eine Anfrage zum Abrufen neuer Daten in die Warteschlange des WebClients.
     */
    void queueData();

    /**
     * @brief Verarbeitet heruntergeladene Daten, falls vorhanden.
     */
    void processData();

    /**
     * @brief Gibt eine thread-sichere Kopie des Stations-Caches zurück.
     * @return Eine PsramVector<StationData> mit den gecachten Stationsinformationen.
     */
    PsramVector<StationData> getStationCache();

    /**
     * @brief Ruft die Preis-Historie für eine bestimmte Tankstelle ab.
     * @param stationId Die ID der Tankstelle.
     * @return Eine Kopie der StationPriceHistory für die angegebene ID. Wenn nicht gefunden, ist das Objekt leer.
     */
    StationPriceHistory getStationPriceHistory(const PsramString& stationId);

    /**
     * @brief Zeichnet den aktuellen Zustand des Moduls auf die Canvas.
     */
    void draw() override;

    /**
     * @brief Wird periodisch aufgerufen, um Paging und andere zeitgesteuerte Aktionen zu handhaben.
     */
    void tick() override;

    /**
     * @brief Gibt die gesamte Anzeigedauer für alle Seiten des Moduls zurück.
     * @return Die Dauer in Millisekunden.
     */
    unsigned long getDisplayDuration() override;

    /**
     * @brief Gibt zurück, ob das Modul aktiviert ist.
     * @return true, wenn aktiviert, sonst false.
     */
    bool isEnabled() override;

    /**
     * @brief Setzt das Paging auf die erste Seite zurück.
     */
    void resetPaging() override;

private:
    /// @brief Referenz auf die U8G2-Bibliothek für die Textdarstellung.
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    /// @brief Referenz auf die GFX-Canvas für die Grafikausgabe.
    GFXcanvas16 &canvas;
    /// @brief Referenz auf den Zeitumrechner für lokale Zeit.
    GeneralTimeConverter& timeConverter;
    /// @brief Zeiger auf die globale Gerätekongfiguration.
    DeviceConfig* _deviceConfig;
    /// @brief Der persönliche API-Key für den Tankerkönig-Dienst.
    PsramString api_key;
    /// @brief Eine kommaseparierte Liste aller Tankstellen-IDs für die Anzeige.
    PsramString station_ids;
    /// @brief Der Y-Offset für die Zeichnung auf dem Display.
    int top_offset;
    /// @brief Vektor, der die aktuell anzuzeigenden Stationsdaten (inkl. Preise) enthält.
    PsramVector<StationData> station_data_list;
    /// @brief Vektor, der die Stammdaten aller jemals gefundenen Tankstellen zwischenspeichert.
    PsramVector<StationData> station_cache;
    /// @brief Zeiger auf das WebClientModule für API-Anfragen.
    WebClientModule* webClient;
    /// @brief Callback-Funktion, die bei Daten-Updates aufgerufen wird.
    std::function<void()> updateCallback;
    /// @brief Mutex zur Synchronisierung des Zugriffs auf die Moduldaten.
    SemaphoreHandle_t dataMutex;
    /// @brief Puffer zum Zwischenspeichern der heruntergeladenen JSON-Daten.
    char* pending_buffer = nullptr;
    /// @brief Größe der Daten im pending_buffer.
    size_t buffer_size = 0;
    /// @brief Flag, das anzeigt, ob neue Daten zur Verarbeitung bereitstehen.
    volatile bool data_pending = false;
    /// @brief Die vollständige URL für die Tankerkönig-API-Anfrage.
    PsramString resource_url;
    /// @brief Zeitstempel des letzten erfolgreich verarbeiteten Daten-Updates.
    time_t last_processed_update;
    /// @brief Vektor, der die Preis-Historie für alle Tankstellen enthält.
    PsramVector<StationPriceHistory> price_statistics;

    /// @brief Die aktuell angezeigte Seite (Tankstelle), wenn mehrere konfiguriert sind.
    int currentPage = 0;
    /// @brief Die Gesamtanzahl der Seiten (konfigurierte Tankstellen).
    int totalPages = 1;
    /// @brief Die konfigurierte Anzeigedauer pro Tankstelle in Millisekunden.
    unsigned long _pageDisplayDuration = 10000;
    /// @brief Zeitstempel des letzten Seitenwechsels für das Paging.
    unsigned long lastPageSwitchTime = 0;
    /// @brief Gibt an, ob das Modul aktiviert und korrekt konfiguriert ist.
    bool _isEnabled = false;

    /// @brief Cache für die zuletzt bekannten Preise geschlossener Tankstellen.
    PsramVector<LastPriceCache> _lastPriceCache;
    /// @brief Cache für die berechneten Preistrends der aktuell angezeigten Tankstellen.
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
    void updatePriceCache(const PsramString& stationId, float e5, float e10, float diesel, time_t lastChange);
    bool getPriceFromCache(const PsramString& stationId, float& e5, float& e10, float& diesel, time_t& lastChange);
    void cleanupOldPriceCacheEntries();

    PriceTrend calculateTrend(const PsramVector<float>& x_values, const PsramVector<float>& y_values);
    void updateAndDetermineTrends(const PsramString& stationId);

    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b);
    uint16_t calcColor(float value, float low, float high);
    int drawPrice(int x, int y, float price, uint16_t color);
    void drawPriceLine(int y, const char* label, float current, float min, float max, PriceTrend minTrend, PriceTrend maxTrend);
    void drawTrendArrow(int x, int y, PriceTrend trend);
};

#endif // DATAMODULE_HPP