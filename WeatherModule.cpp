#include "WeatherModule.hpp"
#include "MultiLogger.hpp"
#include "webconfig.hpp" 
#include "WebClientModule.hpp"
#include <ArduinoJson.h>
#include <time.h>

#define CLIMATE_COLOR_RANGE 5.0f

// TEST: Small test array to verify PROGMEM reading works
const uint8_t test_icon_data[] PROGMEM = {
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};
const WeatherIcon test_icon = { test_icon_data, 5, 1 };

// Allocator für PSRAM
struct SpiRamAllocator : ArduinoJson::Allocator {
  void* allocate(size_t size) override { return ps_malloc(size); }
  void deallocate(void* pointer) override { free(pointer); }
  void* reallocate(void* ptr, size_t new_size) override { return ps_realloc(ptr, new_size); }
};

namespace {
    // Helferfunktion zum zentrierten Zeichnen von Text
    void drawCenteredString(U8G2_FOR_ADAFRUIT_GFX& u8g2, int x, int y, int width, const char* text) {
        int textWidth = u8g2.getUTF8Width(text);
        u8g2.setCursor(x + (width - textWidth) / 2, y);
        u8g2.print(text);
    }
    
    // Helferfunktion zum Parsen von ISO 8601 DateTime Strings zu UTC epoch
    time_t parseISODateTime(const char* iso_string) {
        if (!iso_string) return 0;
        
        struct tm tm_time = {};
        int year, month, day, hour = 0, minute = 0, second = 0;
        
        // Parse ISO 8601 format: "2025-01-09T14:30:00" oder "2025-01-09"
        if (sscanf(iso_string, "%d-%d-%dT%d:%d:%d", 
                   &year, &month, &day, &hour, &minute, &second) >= 3) {
            tm_time.tm_year = year - 1900;
            tm_time.tm_mon = month - 1;
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;
            tm_time.tm_isdst = -1;
            
            // Use timegm to get UTC timestamp (from GeneralTimeConverter)
            return timegm(&tm_time);
        }
        
        return 0;
    }
    
    // Helferfunktion für Komfort-Farben (1-5 Skala)
    uint16_t getComfortColor(int score) {
        switch(score) {
            case 1: return 0xF800;  // Dunkelrot
            case 2: return 0xFD20;  // Hellrot
            case 3: return 0xFFE0;  // Gelb
            case 4: return 0x9FE0;  // Hellgrün
            case 5: return 0x07E0;  // Dunkelgrün
            default: return 0x8410; // Grau
        }
    }
    
    // RGB565-Konvertierung: 8-Bit RGB → 16-Bit RGB565
    // (übernommen aus TankerkoenigModule zur fließenden Farbdarstellung)
    uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) { 
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); 
    }
    
    // Fließende Farbberechnung von Grün → Gelb → Rot basierend auf Wert im Bereich [low..high]
    // (übernommen aus TankerkoenigModule für POP-basierte Niederschlagsfärbung)
    uint16_t calcColor(float value, float low, float high) {
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
}

WeatherModule::WeatherModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, const GeneralTimeConverter& timeConverter, WebClientModule* webClient)
    : _u8g2(u8g2), _canvas(canvas), _timeConverter(timeConverter), _webClient(webClient) {
    _dataMutex = xSemaphoreCreateMutex();
}

WeatherModule::~WeatherModule() {
    if (_dataMutex) vSemaphoreDelete(_dataMutex);
    if (_pendingForecastBuffer) free(_pendingForecastBuffer);
    if (_pendingClimateBuffer) free(_pendingClimateBuffer);
}

void WeatherModule::begin() {
    registerWeatherIcons();            // Main-Icons ins Registry
    registerSpecialIcons(globalWeatherIconSet); // Special-Icons ins Registry
}

void WeatherModule::setConfig(const DeviceConfig* config) {
    _config = config;
    if (isEnabled()) {
        buildApiUrls();
        if (_webClient) {
            if (!_forecastApiUrl.empty()) {
                _webClient->registerResource(String(_forecastApiUrl.c_str()), _config->weatherFetchIntervalMin, nullptr);
            }
            if (!_climateApiUrl.empty()) {
                _webClient->registerResource(String(_climateApiUrl.c_str()), 60 * 24, nullptr);
            }
        }
    }
}

void WeatherModule::onUpdate(std::function<void()> callback) { 
    _onUpdateCallback = callback; 
}

bool WeatherModule::isEnabled() { 
    return _config && _config->weatherEnabled; 
}

void WeatherModule::queueData() {
    if (!isEnabled() || !_webClient) return;
    
    time_t now_utc = time(nullptr);
    
    // Check if day has changed (using GeneralTimeConverter for local time comparison)
    bool dayChanged = false;
    if (_lastUrlBuildTime == 0) {
        dayChanged = true; // First time
    } else {
        dayChanged = !_timeConverter.isSameDay(_lastUrlBuildTime, now_utc);
    }
    
    // Rebuild URLs and update resources if day changed
    if (dayChanged) {
        buildApiUrls(); // Re-build URL to include new dates
        
        // Re-register resources to update URLs in WebClient
        if (!_forecastApiUrl.empty()) {
            _webClient->registerResource(String(_forecastApiUrl.c_str()), _config->weatherFetchIntervalMin, nullptr);
        }
        if (!_climateApiUrl.empty()) {
            _webClient->registerResource(String(_climateApiUrl.c_str()), 60 * 24, nullptr);
        }
    }
    
    _webClient->accessResource(String(_forecastApiUrl.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
        if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (buffer && size > 0 && last_update > this->_lastForecastUpdate) {
                if (_pendingForecastBuffer) free(_pendingForecastBuffer);
                _pendingForecastBuffer = (char*)ps_malloc(size + 1);
                if (_pendingForecastBuffer) {
                    memcpy(_pendingForecastBuffer, buffer, size);
                    _pendingForecastBuffer[size] = '\0';
                    _forecastBufferSize = size;
                    _lastForecastUpdate = last_update;
                    _forecastDataPending = true;
                }
            }
            xSemaphoreGive(_dataMutex);
        }
    });
    
    if (now_utc - _lastClimateUpdate > (60 * 60 * 24)) {
        _webClient->accessResource(String(_climateApiUrl.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
            if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                if (buffer && size > 0 && last_update > this->_lastClimateUpdate) {
                    if (_pendingClimateBuffer) free(_pendingClimateBuffer);
                    _pendingClimateBuffer = (char*)ps_malloc(size + 1);
                    if (_pendingClimateBuffer) {
                        memcpy(_pendingClimateBuffer, buffer, size);
                        _pendingClimateBuffer[size] = '\0';
                        _climateBufferSize = size;
                        _lastClimateUpdate = last_update;
                        _climateDataPending = true;
                    }
                }
                xSemaphoreGive(_dataMutex);
            }
        });
    }
}

void WeatherModule::processData() {
    bool something_processed = false;
    if (_forecastDataPending) {
        if (xSemaphoreTake(_dataMutex, portMAX_DELAY) == pdTRUE) {
            parseForecastData(_pendingForecastBuffer, _forecastBufferSize);
            free(_pendingForecastBuffer);
            _pendingForecastBuffer = nullptr;
            _forecastDataPending = false;
            something_processed = true;
            xSemaphoreGive(_dataMutex);
        }
    }
    if (_climateDataPending) {
        if (xSemaphoreTake(_dataMutex, portMAX_DELAY) == pdTRUE) {
            parseClimateData(_pendingClimateBuffer, _climateBufferSize);
            free(_pendingClimateBuffer);
            _pendingClimateBuffer = nullptr;
            _climateDataPending = false;
            something_processed = true;
            xSemaphoreGive(_dataMutex);
        }
    }
    if (something_processed && this->_onUpdateCallback) {
        this->_onUpdateCallback();
    }
}

void WeatherModule::periodicTick() {
    if (!isEnabled() || !_config || !_config->weatherAlertsEnabled) return;
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 2000) return;
    _lastPeriodicCheck = now;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    bool hasActiveAlertsNow = !_alerts.empty();
    if (hasActiveAlertsNow) {
        unsigned long minInterval = (_lastUrgentDisplayTime == 0) ? 0 : (unsigned long)_config->weatherAlertsRepeatMin * 60 * 1000;
        if (!_isUrgentViewActive && (now - _lastUrgentDisplayTime > minInterval)) {
            _currentUrgentUID = 2000 + (_alerts.front().start % 1000);
            unsigned long safeDuration = (unsigned long)_config->weatherAlertsDisplaySec * 1000 + 5000;
            if (requestPriorityEx(Priority::High, _currentUrgentUID, safeDuration)) {
                _isUrgentViewActive = true;
            }
        }
    } else if (_isUrgentViewActive) {
        releasePriorityEx(_currentUrgentUID);
        _isUrgentViewActive = false;
    }
    xSemaphoreGive(_dataMutex);
}

void WeatherModule::logicTick() {
    _pageTicks++;
    unsigned long ticksNeeded = (_isUrgentViewActive) ? ((unsigned long)_config->weatherAlertsDisplaySec * 10) : (_config ? (unsigned long)_config->weatherDisplaySec * 10 : 100);
    if (_pageTicks >= ticksNeeded) {
        _pageTicks = 0;
        if (_isUrgentViewActive) {
            releasePriorityEx(_currentUrgentUID);
            _isUrgentViewActive = false;
            _lastUrgentDisplayTime = millis();
            _isFinished = true;
            return;
        }
        _currentPageIndex++;
        if (_currentPageIndex >= _pages.size()) {
            _isFinished = true;
        } else if (_onUpdateCallback) {
            _onUpdateCallback();
        }
    }
}

void WeatherModule::draw() {
    if (!isEnabled() || xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    _canvas.fillScreen(0);
    if (_isUrgentViewActive && !_alerts.empty()) {
        drawAlertPage(0);
    } else if (_dataAvailable && !_pages.empty() && _currentPageIndex < _pages.size()) {
        const auto& page = _pages[_currentPageIndex];
        switch (page.type) {
            case WeatherPageType::CURRENT_WEATHER:     drawCurrentWeatherPage();           break;
            case WeatherPageType::TODAY_PART1:         drawTodayPart1Page();               break;
            case WeatherPageType::TODAY_PART2:         drawTodayPart2Page();               break;
            case WeatherPageType::PRECIPITATION_CHART: drawPrecipitationChartPage();       break;
            case WeatherPageType::TEMPERATURE_CHART:   drawTemperatureChartPage();         break;
            case WeatherPageType::HOURLY_FORECAST:     drawHourlyForecastPage(page.index); break;
            case WeatherPageType::DAILY_FORECAST:      drawDailyForecastPage(page.index);  break;
            case WeatherPageType::ALERT:               drawAlertPage(page.index);          break;
            default:                                   drawNoDataPage();                   break;
        }
    } else {
        drawNoDataPage();
    }
    xSemaphoreGive(_dataMutex);
}

void WeatherModule::resetPaging() { 
    _currentPageIndex = 0; 
    _pageTicks = 0; 
    _isFinished = false; 
}

void WeatherModule::activateModule(uint32_t uid) {
    DrawableModule::activateModule(uid);
    resetPaging();
    buildPages();
    if (_onUpdateCallback) _onUpdateCallback();
}

unsigned long WeatherModule::getDisplayDuration() {
    if (!_config || _pages.empty()) return 5000;
    return (unsigned long)_config->weatherDisplaySec * 1000 * _pages.size();
}

void WeatherModule::buildApiUrls() {
    if (!isEnabled() || !_config) return;
    
    // Convert floats to strings in PSRAM
    char latBuf[16], lonBuf[16];
    snprintf(latBuf, sizeof(latBuf), "%.6f", _config->userLatitude);
    snprintf(lonBuf, sizeof(lonBuf), "%.6f", _config->userLongitude);

    char startDate[11], endDate[11];
    time_t now_utc = time(nullptr);
    
    // Convert to local time using GeneralTimeConverter
    time_t now_local = _timeConverter.toLocal(now_utc);
    struct tm timeinfo;
    
    // Use local time for date calculations
    gmtime_r(&now_local, &timeinfo);
    strftime(startDate, sizeof(startDate), "%Y-%m-%d", &timeinfo);
    
    // Calculate end date (today + forecast days in local time)
    time_t end_time_local = now_local + (_config->weatherDailyForecastDays * 24 * 60 * 60);
    gmtime_r(&end_time_local, &timeinfo);
    strftime(endDate, sizeof(endDate), "%Y-%m-%d", &timeinfo);

    // Build forecast URL with only the necessary parameters and correct date range
    _forecastApiUrl = "https://api.open-meteo.com/v1/forecast?latitude=";
    _forecastApiUrl += latBuf; 
    _forecastApiUrl += "&longitude="; 
    _forecastApiUrl += lonBuf;
    _forecastApiUrl += "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m,wind_gusts_10m,uv_index";
    _forecastApiUrl += "&hourly=temperature_2m,apparent_temperature,precipitation_probability,precipitation,rain,snowfall,weather_code";
    _forecastApiUrl += "&daily=weather_code,temperature_2m_max,temperature_2m_min,temperature_2m_mean,sunrise,sunset,precipitation_sum,rain_sum,snowfall_sum,precipitation_probability_max,uv_index_max,cloud_cover_mean,wind_speed_10m_max,sunshine_duration";
    _forecastApiUrl += "&start_date="; 
    _forecastApiUrl += startDate;
    _forecastApiUrl += "&end_date="; 
    _forecastApiUrl += endDate;
    _forecastApiUrl += "&timezone=UTC";  // Use UTC for all times

    // Build climate URL for historical data
    gmtime_r(&now_local, &timeinfo);
    int current_year = timeinfo.tm_year + 1900;
    char climate_start_date[20];  // Increased buffer size to avoid truncation warning
    snprintf(climate_start_date, sizeof(climate_start_date), "%d-%02d-%02d", current_year - 5, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    _climateApiUrl = "https://archive-api.open-meteo.com/v1/archive?latitude=";
    _climateApiUrl += latBuf; 
    _climateApiUrl += "&longitude="; 
    _climateApiUrl += lonBuf;
    _climateApiUrl += "&start_date="; 
    _climateApiUrl += climate_start_date; 
    _climateApiUrl += "&end_date="; 
    _climateApiUrl += startDate;
    _climateApiUrl += "&daily=temperature_2m_mean&timezone=UTC";  // Use UTC
    
    // Update timestamp when URLs were built
    _lastUrlBuildTime = now_utc;
}

void WeatherModule::parseForecastData(char* jsonBuffer, size_t size) {
    using namespace ArduinoJson;
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    if (error) { return; }

    JsonObject current = doc["current"];
    _currentWeather.temp = current["temperature_2m"];
    _currentWeather.feels_like = current["apparent_temperature"];
    _currentWeather.humidity = current["relative_humidity_2m"];
    _currentWeather.clouds = current["cloud_cover"];
    _currentWeather.wind_speed = current["wind_speed_10m"];
    _currentWeather.wind_gust = current["wind_gusts_10m"];
    _currentWeather.uvi = current["uv_index"] | 0.0f;
    _currentWeather.icon_name = mapWeatherCodeToIcon(current["weather_code"], current["is_day"].as<bool>());

    JsonObject daily = doc["daily"];
    _dailyForecast.clear();
    JsonArray daily_time = daily["time"];
    if (!daily_time.isNull()) {
        JsonArray d_code = daily["weather_code"];
        JsonArray d_max = daily["temperature_2m_max"];
        JsonArray d_min = daily["temperature_2m_min"];
        JsonArray d_mean = daily["temperature_2m_mean"];
        JsonArray d_pop = daily["precipitation_probability_max"];
        JsonArray d_rain = daily["rain_sum"];
        JsonArray d_snow = daily["snowfall_sum"];
        JsonArray d_sunrise = daily["sunrise"];
        JsonArray d_sunset = daily["sunset"];
        JsonArray d_cloud = daily["cloud_cover_mean"];
        JsonArray d_wind = daily["wind_speed_10m_max"];
        JsonArray d_sunshine = daily["sunshine_duration"];
        
        for (int i = 0; i < daily_time.size(); ++i) {
            _dailyForecast.push_back({
                parseISODateTime(daily_time[i].as<const char*>()),  // Parse UTC time
                d_min[i].as<float>(), 
                d_max[i].as<float>(), 
                d_mean[i] | 0.0f,
                d_pop[i].as<float>() / 100.0f, 
                d_rain[i].as<float>(), 
                d_snow[i].as<float>(), 
                d_wind[i] | 0.0f,
                d_sunshine[i] | 0.0f,
                static_cast<int>(d_cloud[i] | 0),
                parseISODateTime(d_sunrise[i].as<const char*>()),  // Parse UTC time
                parseISODateTime(d_sunset[i].as<const char*>()),   // Parse UTC time
                mapWeatherCodeToIcon(d_code[i], true)
            });
        }
        if (!_dailyForecast.empty()) {
            _currentWeather.sunrise = _dailyForecast[0].sunrise;
            _currentWeather.sunset = _dailyForecast[0].sunset;
        }
    } else {
        _dataAvailable = false; 
        return;
    }

    _hourlyForecast.clear();
    JsonArray hourly_time = doc["hourly"]["time"];
    if (!hourly_time.isNull()) {
        JsonArray h_temp = doc["hourly"]["temperature_2m"];
        JsonArray h_feels = doc["hourly"]["apparent_temperature"];
        JsonArray h_pop = doc["hourly"]["precipitation_probability"];
        JsonArray h_precip = doc["hourly"]["precipitation"];
        JsonArray h_rain = doc["hourly"]["rain"];
        JsonArray h_snow = doc["hourly"]["snowfall"];
        JsonArray h_code = doc["hourly"]["weather_code"];
        
        for (int i = 0; i < hourly_time.size(); ++i) {
            time_t dt = parseISODateTime(hourly_time[i].as<const char*>());  // Parse UTC time
            int day_index = -1;
            struct tm tm_hourly;
            gmtime_r(&dt, &tm_hourly);

            for(size_t j=0; j < _dailyForecast.size(); ++j) {
                struct tm tm_daily;
                gmtime_r(&_dailyForecast[j].dt, &tm_daily);
                if(tm_hourly.tm_yday == tm_daily.tm_yday && tm_hourly.tm_year == tm_daily.tm_year) {
                    day_index = j;
                    break;
                }
            }

            bool is_day = true;
            if(day_index != -1){
                 is_day = (dt > _dailyForecast[day_index].sunrise && dt < _dailyForecast[day_index].sunset);
            }
            
            float rain_val = h_rain[i] | 0.0f;
            float precip_val = h_precip[i] | 0.0f;
            // Use precipitation as fallback if rain is 0 but precip > 0
            if (rain_val == 0 && precip_val > 0) {
                rain_val = precip_val;
            }
            
            _hourlyForecast.push_back({
                dt, 
                h_temp[i], 
                h_feels[i], 
                h_pop[i].as<float>() / 100.0f, 
                rain_val,
                h_snow[i], 
                mapWeatherCodeToIcon(h_code[i], is_day)
            });
        }
    } else {
        _dataAvailable = false; 
        return;
    }
    
    _alerts.clear(); 
    _dataAvailable = true; 
    buildPages();
}

void WeatherModule::parseClimateData(char* jsonBuffer, size_t size) {
    using namespace ArduinoJson;
    SpiRamAllocator allocator;
    JsonDocument doc(&allocator);
    DeserializationError error = deserializeJson(doc, jsonBuffer);
    if (error) { return; }
    JsonArray daily_temps = doc["daily"]["temperature_2m_mean"];
    if (daily_temps.size() > 0) {
        float sum = 0; int count = 0;
        for (JsonVariant v : daily_temps) { if (!v.isNull()) { sum += v.as<float>(); count++; } }
        if (count > 0) { _historicalMonthlyAvgTemp = sum / count; }
    }
}

PsramString WeatherModule::mapWeatherCodeToIcon(int code, bool is_day) {
    auto it = wmo_code_to_icon.find(code);
    if (it == wmo_code_to_icon.end()) return PsramString("unknown");
    // Konvertiere std::string zu PsramString
    return PsramString(it->second.c_str());
}

bool WeatherModule::isNightTime(time_t timestamp) const {
    // If we don't have sunrise/sunset data, assume day
    if (_dailyForecast.empty()) return false;
    
    // Find the day this timestamp belongs to
    struct tm tm_check;
    gmtime_r(&timestamp, &tm_check);
    
    for (const auto& day : _dailyForecast) {
        struct tm tm_day;
        gmtime_r(&day.dt, &tm_day);
        
        // Check if same day
        if (tm_check.tm_year == tm_day.tm_year && 
            tm_check.tm_yday == tm_day.tm_yday) {
            // Check if timestamp is before sunrise or after sunset
            return (timestamp < day.sunrise || timestamp > day.sunset);
        }
    }
    
    // Fallback: check current weather's sunrise/sunset
    if (_currentWeather.sunrise > 0 && _currentWeather.sunset > 0) {
        return (timestamp < _currentWeather.sunrise || timestamp > _currentWeather.sunset);
    }
    
    return false;
}

void WeatherModule::buildPages() {
    _pages.clear();
    if (!_config || !_dataAvailable) return;
    
    // Current weather pages according to new requirements
    if (_config->weatherShowCurrent) { 
        // Page 1: Current weather (NOW)
        _pages.push_back({WeatherPageType::CURRENT_WEATHER, 0}); 
        
        // Page 2: Today Part 1 (min/max, sunrise/sunset)
        _pages.push_back({WeatherPageType::TODAY_PART1, 0}); 
        
        // Page 3: Today Part 2 (cloud coverage, precipitation, wind, mean temp)
        _pages.push_back({WeatherPageType::TODAY_PART2, 0}); 
        
        // Page 4: Precipitation chart for next 24 hours (if rain/snow expected or likely)
        bool precipitationExpected = false;
        time_t now_utc = time(nullptr);
        time_t end_time = now_utc + (24 * 60 * 60);  // 24 hours from now
        
        for (const auto& hour : _hourlyForecast) {
            if (hour.dt >= now_utc && hour.dt <= end_time) {
                // Show chart if there's actual precipitation or significant probability (>20%)
                if (hour.rain_1h > 0 || hour.snow_1h > 0 || hour.pop > 0.2f) {
                    precipitationExpected = true;
                    break;
                }
            }
        }
        
        if (precipitationExpected) {
            _pages.push_back({WeatherPageType::PRECIPITATION_CHART, 0});
        }
        
        // Page 5: Temperature chart for next 24 hours (always show)
        _pages.push_back({WeatherPageType::TEMPERATURE_CHART, 0});
    }
    
    // Page 6+: Hourly forecast for next 24 hours (divided by 8 = 3 hour intervals)
    if (_config->weatherShowHourly && _hourlyForecast.size() > 1) {
        // Show next 24 hours, divided by 8 = 8 forecasts (every ~3 hours)
        // 2 forecasts per page = 4 pages total
        int forecasts_to_show = 8;  // Show 8 hourly forecasts (24h / 8 = 3h intervals)
        int forecasts_per_page = 2;
        int pages_needed = (forecasts_to_show + forecasts_per_page - 1) / forecasts_per_page;
        
        for (int page = 0; page < pages_needed; page++) {
            _pages.push_back({WeatherPageType::HOURLY_FORECAST, page});
        }
    }
    
    // Page 7+: Daily forecast (3 days per page, starting from tomorrow)
    if (_config->weatherShowDaily && _dailyForecast.size() > 1) {
        // Skip first day (today) and show 3 days per page
        int days_available = (int)_dailyForecast.size() - 1;  // Minus today
        int pages_needed = (days_available + 2) / 3;  // Ceil division by 3
        
        for (int page = 0; page < pages_needed; page++) {
            _pages.push_back({WeatherPageType::DAILY_FORECAST, page});
        }
    }
}

uint16_t WeatherModule::getClimateColorSmooth(float temp) {
    float delta = temp - _historicalMonthlyAvgTemp, range = CLIMATE_COLOR_RANGE;
    if (delta < -range) delta = -range; if (delta > range) delta = range;
    uint8_t r = 0, g = 0, b = 0; float mid_range = range / 2.0f;
    if (delta <= 0) {
        if (delta < -mid_range) { b = 255; g = (uint8_t)map((long)(delta * 100), (long)(-range * 100), (long)(-mid_range * 100), 0, 255); } 
        else { g = 255; b = 255; r = (uint8_t)map((long)(delta * 100), (long)(-mid_range * 100), 0, 0, 255); }
    } else {
        if (delta < mid_range) { r = 255; g = 255; b = (uint8_t)map((long)(delta * 100), 0, (long)(mid_range * 100), 255, 0); } 
        else { r = 255; g = (uint8_t)map((long)(delta * 100), (long)(mid_range * 100), (long)(range * 100), 255, 0); }
    }
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Page 1: Current Weather Display (JETZT)
void WeatherModule::drawCurrentWeatherPage() {
    if (_dailyForecast.empty()) { drawNoDataPage(); return; }
    
    time_t now_utc = time(nullptr);
    
    // Left side: Weather icon (48x48)
    drawWeatherIcon(10, 9, 48, _currentWeather.icon_name, isNightTime(now_utc));
    
    _u8g2.begin(_canvas);
    const int data_x = 68;
    
    // Use consistent font throughout
    _u8g2.setFont(u8g2_font_helvR08_tr);
    
    // Title with data fetch timestamp (not current time)
    _u8g2.setForegroundColor(0xFFFF);
    char titleBuf[20];
    char timeBuf[6];
    formatTime(timeBuf, sizeof(timeBuf), _lastForecastUpdate);
    snprintf(titleBuf, sizeof(titleBuf), "JETZT %s", timeBuf);
    _u8g2.setCursor(data_x, 10);
    _u8g2.print(titleBuf);
    
    // Current temperature (colored) - use consistent °C
    char tempBuf[16];
    snprintf(tempBuf, sizeof(tempBuf), "%.1f°C", _currentWeather.temp);
    _u8g2.setForegroundColor(getClimateColorSmooth(_currentWeather.temp));
    _u8g2.setCursor(data_x, 22);
    _u8g2.print(tempBuf);
    
    // Feels like temperature (colored)
    snprintf(tempBuf, sizeof(tempBuf), "Gefuehlt %.1f°C", _currentWeather.feels_like);
    _u8g2.setForegroundColor(getClimateColorSmooth(_currentWeather.feels_like));
    _u8g2.setCursor(data_x, 34);
    _u8g2.print(tempBuf);
    
    // Humidity and clouds
    _u8g2.setForegroundColor(0xAAAA);
    char buf[30];
    snprintf(buf, sizeof(buf), "Luftf:%d%% Wolken:%d%%", _currentWeather.humidity, _currentWeather.clouds);
    _u8g2.setCursor(data_x, 46);
    _u8g2.print(buf);
    
    // Wind speed with consistent units
    if (_currentWeather.wind_speed > 0) {
        snprintf(buf, sizeof(buf), "Wind: %.0fkm/h", _currentWeather.wind_speed);
        _u8g2.setCursor(data_x, 58);
        _u8g2.print(buf);
    }
}

// Page 2: Today's Weather - Part 1 (Min/Max, Sunrise/Sunset)
void WeatherModule::drawTodayPart1Page() {
    if (_dailyForecast.empty()) { drawNoDataPage(); return; }
    const auto& today = _dailyForecast[0];
    
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvR08_tr);  // Use consistent font
    
    // Title
    _u8g2.setForegroundColor(0xFFFF);
    _u8g2.setCursor(10, 10);
    _u8g2.print("HEUTE");
    
    // Left column
    // Max temperature with icon (colored)
    drawWeatherIcon(10, 14, 16, PsramString("temp_hot"), false);
    char tempBuf[20];
    snprintf(tempBuf, sizeof(tempBuf), "Max: %.1f°C", today.temp_max);
    _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_max));
    _u8g2.setCursor(30, 24);
    _u8g2.print(tempBuf);
    
    // Min temperature with icon (colored)
    drawWeatherIcon(10, 30, 16, PsramString("temp_cold"), false);
    snprintf(tempBuf, sizeof(tempBuf), "Min: %.1f°C", today.temp_min);
    _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_min));
    _u8g2.setCursor(30, 40);
    _u8g2.print(tempBuf);
    
    // Sunrise with icon
    drawWeatherIcon(10, 46, 16, PsramString("sunrise"), false);
    _u8g2.setForegroundColor(0xFE60);  // Orange
    char timeBuf[6];
    formatTime(timeBuf, sizeof(timeBuf), today.sunrise);
    _u8g2.setCursor(30, 56);
    _u8g2.print(timeBuf);
    
    // Right column
    // Mean temperature with icon (colored)
    drawWeatherIcon(100, 14, 16, PsramString("temp_moderate"), false);
    snprintf(tempBuf, sizeof(tempBuf), "Mittel: %.1f°C", today.temp_mean);
    _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_mean));
    _u8g2.setCursor(120, 24);
    _u8g2.print(tempBuf);
    
    // Sunset with icon
    drawWeatherIcon(100, 30, 16, PsramString("sunset"), false);
    _u8g2.setForegroundColor(0xF800);  // Red
    formatTime(timeBuf, sizeof(timeBuf), today.sunset);
    _u8g2.setCursor(120, 40);
    _u8g2.print(timeBuf);
    
    // UV Index if available
    if (_currentWeather.uvi > 0) {
        drawWeatherIcon(100, 46, 16, PsramString("uv_moderate"), false);
        _u8g2.setForegroundColor(0xFFE0);  // Yellow
        char uvBuf[12];
        snprintf(uvBuf, sizeof(uvBuf), "UV:%.1f", _currentWeather.uvi);
        _u8g2.setCursor(120, 56);
        _u8g2.print(uvBuf);
    }
}

// Page 3: Today's Weather - Part 2 (Cloud, Precipitation, Wind, Humidity)
void WeatherModule::drawTodayPart2Page() {
    if (_dailyForecast.empty()) { drawNoDataPage(); return; }
    const auto& today = _dailyForecast[0];
    
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvR08_tr);  // Use consistent font
    
    // Title
    _u8g2.setForegroundColor(0xFFFF);
    _u8g2.setCursor(10, 10);
    _u8g2.print("HEUTE - Details");
    
    // Left column
    // Cloud coverage with icon
    drawWeatherIcon(10, 14, 16, PsramString("unknown"), false);
    _u8g2.setForegroundColor(0xAAAA);
    char buf[30];
    snprintf(buf, sizeof(buf), "Wolken: %d%%", today.cloud_cover);
    _u8g2.setCursor(30, 24);
    _u8g2.print(buf);
    
    // Precipitation with icon
    float total_precip = today.rain + today.snow;
    if (total_precip > 0) {
        drawWeatherIcon(10, 30, 16, PsramString("rain"), false);
    }
    snprintf(buf, sizeof(buf), "Regen: %.1fmm", total_precip);
    _u8g2.setCursor(30, 40);
    _u8g2.print(buf);
    
    // Wind speed with icon
    PsramString wind_icon = "wind_calm";
    if (today.wind_speed > 50) wind_icon = "wind_storm";
    else if (today.wind_speed > 30) wind_icon = "wind_strong";
    else if (today.wind_speed > 15) wind_icon = "wind_moderate";
    else if (today.wind_speed > 5) wind_icon = "wind_light";
    
    drawWeatherIcon(10, 46, 16, wind_icon, false);
    snprintf(buf, sizeof(buf), "Wind: %.0fkm/h", today.wind_speed);
    _u8g2.setCursor(30, 56);
    _u8g2.print(buf);
    
    // Right column
    // Humidity with icon
    PsramString humidity_icon = "humidity_moderate";
    if (_currentWeather.humidity > 70) humidity_icon = "humidity_high";
    else if (_currentWeather.humidity < 40) humidity_icon = "humidity_low";
    
    drawWeatherIcon(100, 14, 16, humidity_icon, false);
    snprintf(buf, sizeof(buf), "Luftf: %d%%", _currentWeather.humidity);
    _u8g2.setCursor(120, 24);
    _u8g2.print(buf);
    
    // Sunshine duration with icon
    if (today.sunshine_duration > 0) {
        drawWeatherIcon(100, 30, 16, PsramString("sunrise"), false);
        float hours = today.sunshine_duration / 3600.0f;
        snprintf(buf, sizeof(buf), "Sonne: %.1fh", hours);
        _u8g2.setForegroundColor(0xFE60);  // Orange
        _u8g2.setCursor(120, 40);
        _u8g2.print(buf);
    }
}

// Page 4: Precipitation Chart (Area graph for rain/snow - next 24 hours)
void WeatherModule::drawPrecipitationChartPage() {
    _u8g2.begin(_canvas);
    
    // Title
    _u8g2.setFont(u8g2_font_helvB08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    drawCenteredString(_u8g2, 0, 10, _canvas.width(), "NIEDERSCHLAG 24h");
    
    // Find hourly data for next 24 hours from now
    time_t now_utc = time(nullptr);
    time_t end_time = now_utc + (24 * 60 * 60);  // 24 hours from now
    
    PsramVector<const WeatherHourlyData*> next_24h_hours;
    for (const auto& hour : _hourlyForecast) {
        if (hour.dt >= now_utc && hour.dt <= end_time) {
            next_24h_hours.push_back(&hour);
        }
    }
    
    if (next_24h_hours.empty()) {
        drawNoDataPage();
        return;
    }
    
    // Chart dimensions - adjusted to make room for axis labels
    const int chart_x = 20;  // More space for Y-axis labels
    const int chart_y = 15;
    const int chart_w = _canvas.width() - 30;
    const int chart_h = 38;  // Slightly smaller to make room for X-axis labels
    
    // Find max precipitation for scaling
    float max_precip = 0.1f;  // Minimum scale
    for (const auto* hour : next_24h_hours) {
        float total = hour->rain_1h + hour->snow_1h;
        if (total > max_precip) max_precip = total;
    }
    
    // Draw axes
    _canvas.drawLine(chart_x, chart_y + chart_h, chart_x + chart_w, chart_y + chart_h, 0x7BEF);
    _canvas.drawLine(chart_x, chart_y, chart_x, chart_y + chart_h, 0x7BEF);
    
    // Draw Y-axis labels
    _u8g2.setFont(u8g2_font_4x6_tf);
    _u8g2.setForegroundColor(0xAAAA);
    char labelBuf[8];
    // Top label
    snprintf(labelBuf, sizeof(labelBuf), "%.1f", max_precip);
    _u8g2.setCursor(2, chart_y + 4);
    _u8g2.print(labelBuf);
    // Middle label
    snprintf(labelBuf, sizeof(labelBuf), "%.1f", max_precip / 2);
    _u8g2.setCursor(2, chart_y + chart_h / 2 + 2);
    _u8g2.print(labelBuf);
    // Bottom label (0)
    _u8g2.setCursor(2, chart_y + chart_h);
    _u8g2.print("0");
    
    // Draw precipitation as area chart (not bars)
    int num_hours = next_24h_hours.size();
    if (num_hours > 1) {
        float step_width = (float)chart_w / (num_hours - 1);
        
        // Draw rain area (blue) or probability area (lighter blue if no actual rain)
        for (int i = 0; i < num_hours - 1; i++) {
            const auto* hour1 = next_24h_hours[i];
            const auto* hour2 = next_24h_hours[i + 1];
            
            int x1 = chart_x + (int)(i * step_width);
            int x2 = chart_x + (int)((i + 1) * step_width);
            
            // Use rain amount if available, otherwise use pop * max_precip for visual representation
            float rain1 = (hour1->rain_1h > 0) ? hour1->rain_1h : (hour1->pop * max_precip * 0.3f);
            float rain2 = (hour2->rain_1h > 0) ? hour2->rain_1h : (hour2->pop * max_precip * 0.3f);
            
            int rain_y1 = chart_y + chart_h - (int)((rain1 / max_precip) * chart_h);
            int rain_y2 = chart_y + chart_h - (int)((rain2 / max_precip) * chart_h);
            
            // Fill area under the line
            if (rain1 > 0 || rain2 > 0) {
                // Berechne Farbe basierend auf mittlerer Regenwahrscheinlichkeit (POP)
                float avg_pop = (hour1->pop + hour2->pop) * 0.5f;  // POP liegt in 0..1
                uint16_t rain_color = calcColor(avg_pop, 0.0f, 1.0f);
                // Draw filled polygon (trapezoid) from baseline to line
                for (int x = x1; x <= x2; x++) {
                    float t = (float)(x - x1) / (float)(x2 - x1);
                    int y = rain_y1 + (int)(t * (rain_y2 - rain_y1));
                    _canvas.drawLine(x, y, x, chart_y + chart_h, rain_color);
                }
            }
        }
        
        // Draw snow area (cyan) on top
        for (int i = 0; i < num_hours - 1; i++) {
            const auto* hour1 = next_24h_hours[i];
            const auto* hour2 = next_24h_hours[i + 1];
            
            int x1 = chart_x + (int)(i * step_width);
            int x2 = chart_x + (int)((i + 1) * step_width);
            
            int snow_y1 = chart_y + chart_h - (int)((hour1->snow_1h / max_precip) * chart_h);
            int snow_y2 = chart_y + chart_h - (int)((hour2->snow_1h / max_precip) * chart_h);
            
            // Fill area under the line
            if (hour1->snow_1h > 0 || hour2->snow_1h > 0) {
                // Draw filled polygon (trapezoid) from baseline to line
                for (int x = x1; x <= x2; x++) {
                    float t = (float)(x - x1) / (float)(x2 - x1);
                    int y = snow_y1 + (int)(t * (snow_y2 - snow_y1));
                    _canvas.drawLine(x, y, x, chart_y + chart_h, 0x07FF);  // Cyan
                }
            }
        }
    }
    
    // Draw time labels with better distribution and positioning
    _u8g2.setFont(u8g2_font_4x6_tf);
    _u8g2.setForegroundColor(0xAAAA);
    // Show labels at better intervals
    int label_count = min(8, num_hours);  // Show max 8 labels
    int label_interval = max(1, num_hours / label_count);
    
    for (int i = 0; i < num_hours; i += label_interval) {
        const auto* hour = next_24h_hours[i];
        char timeBuf[6];
        formatTime(timeBuf, sizeof(timeBuf), hour->dt);
        // Only show hour part (HH:MM -> HH)
        timeBuf[2] = '\0';  // Truncate after HH
        
        int x = chart_x + (int)(i * ((float)chart_w / (num_hours - 1)));
        // Position labels higher up (moved from +8 to +6)
        _u8g2.setCursor(x - 4, chart_y + chart_h + 6);
        _u8g2.print(timeBuf);
    }
    // Always show last hour label
    if ((num_hours - 1) % label_interval != 0) {
        const auto* hour = next_24h_hours[num_hours - 1];
        char timeBuf[6];
        formatTime(timeBuf, sizeof(timeBuf), hour->dt);
        timeBuf[2] = '\0';
        int x = chart_x + chart_w;
        _u8g2.setCursor(x - 8, chart_y + chart_h + 6);
        _u8g2.print(timeBuf);
    }
}

// Page 5: Temperature Chart (Area graph for temperature - next 24 hours)
void WeatherModule::drawTemperatureChartPage() {
    _u8g2.begin(_canvas);
    
    // Title
    _u8g2.setFont(u8g2_font_helvB08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    drawCenteredString(_u8g2, 0, 10, _canvas.width(), "TEMPERATUR 24h");
    
    // Find hourly data for next 24 hours from now
    time_t now_utc = time(nullptr);
    time_t end_time = now_utc + (24 * 60 * 60);  // 24 hours from now
    
    PsramVector<const WeatherHourlyData*> next_24h_hours;
    for (const auto& hour : _hourlyForecast) {
        if (hour.dt >= now_utc && hour.dt <= end_time) {
            next_24h_hours.push_back(&hour);
        }
    }
    
    if (next_24h_hours.empty()) {
        drawNoDataPage();
        return;
    }
    
    // Chart dimensions - adjusted to make room for axis labels
    const int chart_x = 20;  // More space for Y-axis labels
    const int chart_y = 15;
    const int chart_w = _canvas.width() - 30;
    const int chart_h = 38;  // Slightly smaller to make room for X-axis labels
    
    // Find min/max temperatures for scaling (both actual and feels like)
    float min_temp = next_24h_hours[0]->temp;
    float max_temp = next_24h_hours[0]->temp;
    
    for (const auto* hour : next_24h_hours) {
        if (hour->temp < min_temp) min_temp = hour->temp;
        if (hour->temp > max_temp) max_temp = hour->temp;
        if (hour->feels_like < min_temp) min_temp = hour->feels_like;
        if (hour->feels_like > max_temp) max_temp = hour->feels_like;
    }
    
    // Add some padding to the scale
    float temp_range = max_temp - min_temp;
    if (temp_range < 5.0f) temp_range = 5.0f;  // Minimum range of 5 degrees
    min_temp -= temp_range * 0.1f;
    max_temp += temp_range * 0.1f;
    
    // Draw axes
    _canvas.drawLine(chart_x, chart_y + chart_h, chart_x + chart_w, chart_y + chart_h, 0x7BEF);
    _canvas.drawLine(chart_x, chart_y, chart_x, chart_y + chart_h, 0x7BEF);
    
    // Draw Y-axis labels
    _u8g2.setFont(u8g2_font_4x6_tf);
    _u8g2.setForegroundColor(0xAAAA);
    char labelBuf[8];
    // Top label
    snprintf(labelBuf, sizeof(labelBuf), "%.0f", max_temp);
    _u8g2.setCursor(2, chart_y + 4);
    _u8g2.print(labelBuf);
    // Middle label
    snprintf(labelBuf, sizeof(labelBuf), "%.0f", (max_temp + min_temp) / 2);
    _u8g2.setCursor(2, chart_y + chart_h / 2 + 2);
    _u8g2.print(labelBuf);
    // Bottom label
    snprintf(labelBuf, sizeof(labelBuf), "%.0f", min_temp);
    _u8g2.setCursor(2, chart_y + chart_h);
    _u8g2.print(labelBuf);
    
    // Draw temperature as area chart (filled)
    int num_hours = next_24h_hours.size();
    if (num_hours > 1) {
        float step_width = (float)chart_w / (num_hours - 1);
        
        // Draw actual temperature area (colored based on temperature)
        for (int i = 0; i < num_hours - 1; i++) {
            const auto* hour1 = next_24h_hours[i];
            const auto* hour2 = next_24h_hours[i + 1];
            
            int x1 = chart_x + (int)(i * step_width);
            int x2 = chart_x + (int)((i + 1) * step_width);
            
            float norm_temp1 = (hour1->temp - min_temp) / (max_temp - min_temp);
            float norm_temp2 = (hour2->temp - min_temp) / (max_temp - min_temp);
            
            int temp_y1 = chart_y + chart_h - (int)(norm_temp1 * chart_h);
            int temp_y2 = chart_y + chart_h - (int)(norm_temp2 * chart_h);
            
            // Get color based on average temperature for this segment
            float avg_temp = (hour1->temp + hour2->temp) / 2.0f;
            uint16_t temp_color = getClimateColorSmooth(avg_temp);
            
            // Draw filled polygon (trapezoid) from baseline to line
            for (int x = x1; x <= x2; x++) {
                float t = (float)(x - x1) / (float)(x2 - x1);
                int y = temp_y1 + (int)(t * (temp_y2 - temp_y1));
                _canvas.drawLine(x, y, x, chart_y + chart_h, temp_color);
            }
        }
        
        // Draw feels like temperature line (yellow line on top)
        for (int i = 0; i < num_hours - 1; i++) {
            const auto* hour1 = next_24h_hours[i];
            const auto* hour2 = next_24h_hours[i + 1];
            
            int x1 = chart_x + (int)(i * step_width);
            int x2 = chart_x + (int)((i + 1) * step_width);
            
            float norm_feels1 = (hour1->feels_like - min_temp) / (max_temp - min_temp);
            float norm_feels2 = (hour2->feels_like - min_temp) / (max_temp - min_temp);
            
            int feels_y1 = chart_y + chart_h - (int)(norm_feels1 * chart_h);
            int feels_y2 = chart_y + chart_h - (int)(norm_feels2 * chart_h);
            
            // Draw yellow line for feels like temperature
            _canvas.drawLine(x1, feels_y1, x2, feels_y2, 0xFFE0);  // Yellow
        }
    }
    
    // Draw time labels with better distribution and positioning
    _u8g2.setFont(u8g2_font_4x6_tf);
    _u8g2.setForegroundColor(0xAAAA);
    // Show labels at better intervals
    int label_count = min(8, num_hours);  // Show max 8 labels
    int label_interval = max(1, num_hours / label_count);
    
    for (int i = 0; i < num_hours; i += label_interval) {
        const auto* hour = next_24h_hours[i];
        char timeBuf[6];
        formatTime(timeBuf, sizeof(timeBuf), hour->dt);
        // Only show hour part (HH:MM -> HH)
        timeBuf[2] = '\0';  // Truncate after HH
        
        int x = chart_x + (int)(i * ((float)chart_w / (num_hours - 1)));
        // Position labels higher up
        _u8g2.setCursor(x - 4, chart_y + chart_h + 6);
        _u8g2.print(timeBuf);
    }
    // Always show last hour label
    if ((num_hours - 1) % label_interval != 0) {
        const auto* hour = next_24h_hours[num_hours - 1];
        char timeBuf[6];
        formatTime(timeBuf, sizeof(timeBuf), hour->dt);
        timeBuf[2] = '\0';
        int x = chart_x + chart_w;
        _u8g2.setCursor(x - 8, chart_y + chart_h + 6);
        _u8g2.print(timeBuf);
    }
}

// Page 6+: Hourly Forecast (2 columns per page, multiple pages - next 24 hours)
void WeatherModule::drawHourlyForecastPage(int pageIndex) {
    if (!_config || _hourlyForecast.empty()) { drawNoDataPage(); return; }
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvR08_tr);  // Use consistent font
    
    const int num_cols = 2;
    const int col_width = _canvas.width() / num_cols;
    
    // Get current time and end time (24 hours from now)
    time_t now_utc = time(nullptr);
    time_t end_time = now_utc + (24 * 60 * 60);
    
    // Collect hourly forecasts for next 24 hours
    PsramVector<const WeatherHourlyData*> next_24h_hours;
    for (const auto& hour : _hourlyForecast) {
        if (hour.dt >= now_utc && hour.dt <= end_time) {
            next_24h_hours.push_back(&hour);
        }
    }
    
    if (next_24h_hours.empty()) { drawNoDataPage(); return; }
    
    // We want to show 8 forecasts total (24h / 8 = 3h intervals approximately)
    // Calculate interval based on available data
    int total_forecasts_to_show = 8;
    int interval = max(1, (int)next_24h_hours.size() / total_forecasts_to_show);
    
    // Calculate starting position for this page (2 forecasts per page)
    int start_forecast = pageIndex * num_cols;
    
    for (int i = 0; i < num_cols; ++i) {
        int hour_index = start_forecast + i;
        int actual_index = hour_index * interval;
        
        if (actual_index >= next_24h_hours.size()) break;
        
        const auto* hour = next_24h_hours[actual_index]; 
        int x = i * col_width;
        
        // Weather icon (24x24)
        drawWeatherIcon(x + (col_width - 24) / 2, 2, 24, hour->icon_name, isNightTime(hour->dt));
        
        _u8g2.setForegroundColor(0xFFFF);
        
        // Time
        char time_buf[6]; 
        formatTime(time_buf, sizeof(time_buf), hour->dt); 
        drawCenteredString(_u8g2, x, 30, col_width, time_buf);
        
        // Temperature (colored) with consistent unit
        char temp_buf[14]; 
        snprintf(temp_buf, sizeof(temp_buf), "%.1f°C", hour->temp);
        _u8g2.setForegroundColor(getClimateColorSmooth(hour->temp)); 
        drawCenteredString(_u8g2, x, 40, col_width, temp_buf);
        
        // Feels like (colored)
        snprintf(temp_buf, sizeof(temp_buf), "Gefuehlt %.1f°C", hour->feels_like);
        _u8g2.setForegroundColor(getClimateColorSmooth(hour->feels_like)); 
        drawCenteredString(_u8g2, x, 50, col_width, temp_buf);
        
        // Rain probability and amount with unit
        if (hour->rain_1h > 0) {
            char rain_buf[16];
            snprintf(rain_buf, sizeof(rain_buf), "%.0f%% %.1fmm", hour->pop * 100, hour->rain_1h);
            _u8g2.setForegroundColor(0x001F);
            drawCenteredString(_u8g2, x, 60, col_width, rain_buf);
        } else {
            char pop_buf[10]; 
            snprintf(pop_buf, sizeof(pop_buf), "%.0f%%", hour->pop * 100); 
            _u8g2.setForegroundColor(0x7BEF);
            drawCenteredString(_u8g2, x, 60, col_width, pop_buf);
        }
    }
}

// Page 7+: Daily Forecast (3 days per page, starting from tomorrow)
void WeatherModule::drawDailyForecastPage(int pageIndex) {
    if (_dailyForecast.size() < 2) { drawNoDataPage(); return; }
    
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvR08_tr);  // Use consistent font
    
    // Skip first day (today) since we have dedicated pages
    // Show 3 days per page starting from tomorrow
    int start_day = 1 + (pageIndex * 3);  // Start from tomorrow, offset by page
    int available_days = (int)_dailyForecast.size() - start_day;
    if (available_days <= 0) { drawNoDataPage(); return; }
    
    int days_to_show = min(3, available_days);
    const int col_width = _canvas.width() / days_to_show;
    
    for (int i = 0; i < days_to_show; ++i) {
        const auto& day = _dailyForecast[start_day + i];
        int x = i * col_width;
        
        // Weather icon (24x24)
        time_t noon = day.dt + (12 * 60 * 60);
        drawWeatherIcon(x + (col_width - 24) / 2, 2, 24, day.icon_name, isNightTime(noon));
        
        // Day name
        _u8g2.setForegroundColor(0xFFFF);
        char day_buf[10];
        if (start_day + i == 1) strcpy(day_buf, "Morgen");
        else getDayName(day_buf, sizeof(day_buf), day.dt);
        drawCenteredString(_u8g2, x, 30, col_width, day_buf);
        
        // Max temperature (colored) with consistent unit
        char temp_buf[14];
        snprintf(temp_buf, sizeof(temp_buf), "%.1f°C", day.temp_max);
        _u8g2.setForegroundColor(getClimateColorSmooth(day.temp_max));
        drawCenteredString(_u8g2, x, 40, col_width, temp_buf);
        
        // Min temperature (colored) with consistent unit
        snprintf(temp_buf, sizeof(temp_buf), "%.1f°C", day.temp_min);
        _u8g2.setForegroundColor(getClimateColorSmooth(day.temp_min));
        drawCenteredString(_u8g2, x, 50, col_width, temp_buf);
        
        // Rain probability
        _u8g2.setForegroundColor(0x7BEF);
        snprintf(temp_buf, sizeof(temp_buf), "%.0f%%", day.pop * 100);
        drawCenteredString(_u8g2, x, 60, col_width, temp_buf);
    }
}

void WeatherModule::drawNoDataPage() {
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvR08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    const char* text = _dataAvailable ? "Keine Seiten konfig." : "Warte auf Wetterdaten...";
    drawCenteredString(_u8g2, 0, _canvas.height() / 2 + 4, _canvas.width(), text);
}

void WeatherModule::drawAlertPage(int index) {
    if (index >= _alerts.size()) return;
    const auto& alert = _alerts[index];
    uint16_t bgColor = dimColor(0xF800, 0.5f);
    _canvas.fillScreen(bgColor);
    float pulseFactor = 0.6f + (sin(millis() / 200.0f) + 1.0f) / 5.0f;
    uint16_t iconColor = dimColor(0xFFFF, pulseFactor);
    _u8g2.begin(_canvas);
    _u8g2.setFont(u8g2_font_helvB10_tr);
    _u8g2.setForegroundColor(iconColor);
    _u8g2.setCursor(70, 20);
    _u8g2.print("WETTERWARNUNG");
    _u8g2.setFont(u8g2_font_helvR08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    _u8g2.setCursor(70, 40);
    _u8g2.print(alert.event.c_str());
    char time_buf_start[6], time_buf_end[6];
    formatTime(time_buf_start, sizeof(time_buf_start), alert.start);
    formatTime(time_buf_end, sizeof(time_buf_end), alert.end);
    _u8g2.setCursor(70, 55);
    _u8g2.printf("Von %s bis %s Uhr", time_buf_start, time_buf_end);
    drawWeatherIcon(10, 9, 48, PsramString("warning_generic"), false);
}


void WeatherModule::drawWeatherIcon(int x, int y, int size, const PsramString& name, bool isNight) {
    // Retrieves the appropriate icon from the registry/cache, independent of Main/Special!
    // Use std::string for cache lookup (WeatherIconCache requires std::string)
    std::string iconName(name.c_str());
    
    // TEMPORARY TEST: Bypass cache and read directly from PROGMEM
    // This helps diagnose if the issue is with cache/PSRAM or PROGMEM reading
    const WeatherIcon* src = globalWeatherIconSet.getIcon(iconName, isNight);
    
    // Log missing icons only once per unique icon name (using PSRAM-backed set)
    if (!src) {
        src = globalWeatherIconSet.getIcon(iconName, false);
        if (!src) {
            // Create unique key for this missing icon using PsramString
            PsramString missingKey = name;
            missingKey += (isNight ? "_night" : "_day");
            if (_loggedMissingIcons.find(missingKey) == _loggedMissingIcons.end()) {
                // First time seeing this missing icon - log it
                Log.printf("[Weather] Missing icon: '%s' (isNight: %d)\n", name.c_str(), isNight);
                _loggedMissingIcons.insert(missingKey);
            }
            src = globalWeatherIconSet.getUnknown();
        }
    }
    
    if (!src || !src->data) {
        // This is a critical error - log every time
        Log.printf("[Weather] ERROR: No valid icon found for '%s'!\n", name.c_str());
        return;
    }
    
    // Use PSRAM cache for all icon sizes (with bilinear scaling)
    const WeatherIcon* iconPtr = globalWeatherIconCache.getScaled(iconName, size, isNight);
    if(!iconPtr || !iconPtr->data) {
        iconPtr = globalWeatherIconCache.getScaled("unknown", size, false);
        // Log fallback only once per icon using PsramString
        PsramString fallbackKey = name;
        fallbackKey += "_fallback_";
        char sizeBuf[16];
        snprintf(sizeBuf, sizeof(sizeBuf), "%d", size);
        fallbackKey += sizeBuf;
        if (_loggedMissingIcons.find(fallbackKey) == _loggedMissingIcons.end()) {
            Log.printf("[Weather] Fallback to unknown icon (icon: %s, size: %d)\n", name.c_str(), size);
            _loggedMissingIcons.insert(fallbackKey);
        }
    }
    if(!iconPtr || !iconPtr->data) return;
    
    // Draw icon pixel by pixel, reading from PSRAM cache
    for(int j=0; j<size; ++j) {
        for(int i=0; i<size; ++i) {
            int idx = (j*size+i)*3;
            // Icon data is already in PSRAM (copied by scaleBilinear), read directly
            uint8_t r = iconPtr->data[idx+0];
            uint8_t g = iconPtr->data[idx+1];
            uint8_t b = iconPtr->data[idx+2];
            // Convert RGB888 to RGB565
            uint16_t color = ((r&0xF8)<<8)|((g&0xFC)<<3)|(b>>3);
            // Skip fully transparent pixels (black = 0x00,0x00,0x00)
            if(r != 0 || g != 0 || b != 0) {
                _canvas.drawPixel(x+i, y+j, color);
            }
        }
    }
}

void WeatherModule::getDayName(char* buf, size_t buf_len, time_t epoch) {
    // Convert UTC to local time
    time_t local_epoch = _timeConverter.toLocal(epoch);
    struct tm tm_info; 
    localtime_r(&local_epoch, &tm_info);  // Use localtime_r for local time
    const char* days[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    if (tm_info.tm_wday >= 0 && tm_info.tm_wday < 7) {
        strncpy(buf, days[tm_info.tm_wday], buf_len - 1);
        buf[buf_len - 1] = '\0';
    }
}

void WeatherModule::formatTime(char* buf, size_t buf_len, time_t epoch) {
    // Convert UTC to local time
    time_t local_epoch = _timeConverter.toLocal(epoch);
    struct tm tm_info; 
    localtime_r(&local_epoch, &tm_info);  // Use localtime_r for local time
    snprintf(buf, buf_len, "%02d:%02d", tm_info.tm_hour, tm_info.tm_min);
}

uint16_t WeatherModule::dimColor(uint16_t color, float brightness) {
    uint8_t r = (color >> 11) & 0x1F, g = (color >> 5) & 0x3F, b = color & 0x1F;
    r = (uint8_t)(r * brightness); g = (uint8_t)(g * brightness); b = (uint8_t)(b * brightness);
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
}