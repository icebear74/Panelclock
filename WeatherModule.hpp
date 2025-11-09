#ifndef WEATHERMODULE_HPP
#define WEATHERMODULE_HPP

#include "DrawableModule.hpp"
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include "GeneralTimeConverter.hpp"
#include "WeatherIcons.hpp" 

class WebClientModule;
struct DeviceConfig;

struct WeatherCurrentData {
    float temp, feels_like, dew_point, uvi, wind_speed, wind_gust, pop;
    int humidity, clouds;
    time_t sunrise, sunset;
    PsramString icon_name;
};

struct WeatherHourlyData {
    time_t dt;
    float temp, feels_like, pop, rain_1h, snow_1h;
    PsramString icon_name;
};

struct WeatherDailyData {
    time_t dt;
    float temp_min, temp_max, pop, rain, snow;
    time_t sunrise, sunset; 
    PsramString icon_name;
};

struct WeatherAlertData {
    PsramString event;
    time_t start, end;
};

enum class WeatherPageType {
    TODAY_OVERVIEW,
    TODAY_DETAILS,
    WEEK_OVERVIEW,
    PRECIPITATION,
    COMFORT_INDEX,
    HOURLY_FORECAST,
    DAILY_FORECAST,
    ALERT
};

struct WeatherPage {
    WeatherPageType type;
    int index; 
};

class WeatherModule : public DrawableModule {
public:
    WeatherModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, const GeneralTimeConverter& timeConverter, WebClientModule* webClient);
    ~WeatherModule();

    void begin();
    void logicTick() override;
    void draw() override;
    void onUpdate(std::function<void()> callback);
    bool isEnabled() override;
    unsigned long getDisplayDuration() override;
    void resetPaging() override;
    void activateModule(uint32_t uid) override;

    void setConfig(const DeviceConfig* config);
    void queueData();
    void processData();
    void periodicTick();

private:
    U8G2_FOR_ADAFRUIT_GFX& _u8g2;
    GFXcanvas16& _canvas;
    const GeneralTimeConverter& _timeConverter;
    WebClientModule* _webClient = nullptr;
    const DeviceConfig* _config = nullptr;

    SemaphoreHandle_t _dataMutex = nullptr;
    
    PsramString _forecastApiUrl;
    PsramString _climateApiUrl;

    char* _pendingForecastBuffer = nullptr;
    size_t _forecastBufferSize = 0;
    bool _forecastDataPending = false;
    
    char* _pendingClimateBuffer = nullptr;
    size_t _climateBufferSize = 0;
    bool _climateDataPending = false;
    
    float _historicalMonthlyAvgTemp = 10.0;
    time_t _lastForecastUpdate = 0;
    time_t _lastClimateUpdate = 0;
    bool _dataAvailable = false;

    WeatherCurrentData _currentWeather;
    PsramVector<WeatherHourlyData> _hourlyForecast;
    PsramVector<WeatherDailyData> _dailyForecast;
    PsramVector<WeatherAlertData> _alerts;

    PsramVector<WeatherPage> _pages;
    int _currentPageIndex = 0;
    unsigned int _pageTicks = 0;

    bool _isUrgentViewActive = false;
    uint32_t _currentUrgentUID = 0;
    unsigned long _lastUrgentDisplayTime = 0;
    unsigned long _lastPeriodicCheck = 0;

    std::function<void()> _onUpdateCallback = nullptr;

    void buildApiUrls();
    void parseForecastData(char* jsonBuffer, size_t size);
    void parseClimateData(char* jsonBuffer, size_t size);
    
    uint16_t getClimateColorSmooth(float temp);
    PsramString mapWeatherCodeToIcon(int code, bool is_day);

    void buildPages();
    void getDayName(char* buf, size_t buf_len, time_t epoch);
    void formatTime(char* buf, size_t buf_len, time_t epoch);
    uint16_t dimColor(uint16_t color, float brightness);
    
    const WeatherIconSet& getIconSet(const PsramString& icon_name);
    void drawWeatherIcon(int x, int y, int size, const PsramString& icon_name);

    void drawTodayOverviewPage();
    void drawTodayDetailsPage();
    void drawWeekOverviewPage();
    void drawPrecipitationPage();
    void drawComfortIndexPage();
    void drawComfortBar(const char* label, int score, int y);
    void drawHourlyForecastPage();
    void drawMultiDayForecastPage();
    void drawAlertPage(int index);
    void drawNoDataPage();
};

#endif // WEATHERMODULE_HPP