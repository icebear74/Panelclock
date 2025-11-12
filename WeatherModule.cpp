#include "WeatherModule.hpp"
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
    buildApiUrls(); // Re-build URL before every queue to ensure dates are current
    
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
    
    if (time(nullptr) - _lastClimateUpdate > (60 * 60 * 24)) {
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
            case WeatherPageType::TODAY_OVERVIEW:  drawTodayOverviewPage();  break;
            case WeatherPageType::TODAY_DETAILS:   drawTodayDetailsPage();   break;
            case WeatherPageType::WEEK_OVERVIEW:   drawWeekOverviewPage();   break;
            case WeatherPageType::PRECIPITATION:   drawPrecipitationPage();  break;
            case WeatherPageType::COMFORT_INDEX:   drawComfortIndexPage();   break;
            case WeatherPageType::HOURLY_FORECAST: drawHourlyForecastPage(); break;
            case WeatherPageType::DAILY_FORECAST:  drawMultiDayForecastPage(); break;
            case WeatherPageType::ALERT:           drawAlertPage(page.index);  break;
            default:                               drawNoDataPage();         break;
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
    String lat(_config->userLatitude, 6), lon(_config->userLongitude, 6);

    char startDate[11], endDate[11];
    time_t now = time(nullptr);
    struct tm timeinfo;

    gmtime_r(&now, &timeinfo);
    strftime(startDate, sizeof(startDate), "%Y-%m-%d", &timeinfo);
    
    time_t end_time = now + (_config->weatherDailyForecastDays * 24 * 60 * 60);
    gmtime_r(&end_time, &timeinfo);
    strftime(endDate, sizeof(endDate), "%Y-%m-%d", &timeinfo);

    // Build forecast URL with only the necessary parameters and correct date range
    _forecastApiUrl = "https://api.open-meteo.com/v1/forecast?latitude=";
    _forecastApiUrl += lat.c_str(); 
    _forecastApiUrl += "&longitude="; 
    _forecastApiUrl += lon.c_str();
    _forecastApiUrl += "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,precipitation,rain,showers,snowfall,weather_code,cloud_cover,wind_speed_10m,wind_gusts_10m,uv_index";
    _forecastApiUrl += "&hourly=temperature_2m,apparent_temperature,precipitation_probability,precipitation,rain,snowfall,weather_code";
    _forecastApiUrl += "&daily=weather_code,temperature_2m_max,temperature_2m_min,sunrise,sunset,precipitation_sum,rain_sum,snowfall_sum,precipitation_probability_max,uv_index_max";
    _forecastApiUrl += "&start_date="; 
    _forecastApiUrl += startDate;
    _forecastApiUrl += "&end_date="; 
    _forecastApiUrl += endDate;
    _forecastApiUrl += "&timezone=UTC";  // Use UTC for all times

    // Build climate URL for historical data
    gmtime_r(&now, &timeinfo);
    int current_year = timeinfo.tm_year + 1900;
    char climate_start_date[20];  // Increased buffer size to avoid truncation warning
    snprintf(climate_start_date, sizeof(climate_start_date), "%d-%02d-%02d", current_year - 5, timeinfo.tm_mon + 1, timeinfo.tm_mday);
    
    _climateApiUrl = "https://archive-api.open-meteo.com/v1/archive?latitude=";
    _climateApiUrl += lat.c_str(); 
    _climateApiUrl += "&longitude="; 
    _climateApiUrl += lon.c_str();
    _climateApiUrl += "&start_date="; 
    _climateApiUrl += climate_start_date; 
    _climateApiUrl += "&end_date="; 
    _climateApiUrl += startDate;
    _climateApiUrl += "&daily=temperature_2m_mean&timezone=UTC";  // Use UTC
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
        JsonArray d_pop = daily["precipitation_probability_max"];
        JsonArray d_rain = daily["rain_sum"];
        JsonArray d_snow = daily["snowfall_sum"];
        JsonArray d_sunrise = daily["sunrise"];
        JsonArray d_sunset = daily["sunset"];
        // JsonArray d_uvi = daily["uv_index_max"];  // Currently unused
        
        for (int i = 0; i < daily_time.size(); ++i) {
            _dailyForecast.push_back({
                parseISODateTime(daily_time[i].as<const char*>()),  // Parse UTC time
                d_min[i].as<float>(), 
                d_max[i].as<float>(), 
                d_pop[i].as<float>() / 100.0f, 
                d_rain[i].as<float>(), 
                d_snow[i].as<float>(), 
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
    
    // Current weather pages
    if (_config->weatherShowCurrent) { 
        _pages.push_back({WeatherPageType::TODAY_OVERVIEW, 0}); 
        _pages.push_back({WeatherPageType::TODAY_DETAILS, 0}); 
        
        // Only add precipitation page if rain is expected today
        bool rainExpected = false;
        time_t now_utc = time(nullptr);
        struct tm tm_now;
        gmtime_r(&now_utc, &tm_now);
        
        for (const auto& hour : _hourlyForecast) {
            if (hour.dt < now_utc) continue;
            struct tm tm_hour;
            gmtime_r(&hour.dt, &tm_hour);
            if (tm_hour.tm_yday != tm_now.tm_yday) break;
            if (hour.rain_1h > 0 || hour.snow_1h > 0) {
                rainExpected = true;
                break;
            }
        }
        
        if (rainExpected) {
            _pages.push_back({WeatherPageType::PRECIPITATION, 0});
        }
        
        _pages.push_back({WeatherPageType::COMFORT_INDEX, 0});
    }
    
    // Week overview - removed duplicate, only one compact overview
    if (_config->weatherShowDaily && _dailyForecast.size() > 1) {
        _pages.push_back({WeatherPageType::WEEK_OVERVIEW, 0});
    }
    
    // Hourly forecast
    if (_config->weatherShowHourly && _hourlyForecast.size() > 1) { 
        _pages.push_back({WeatherPageType::HOURLY_FORECAST, 0}); 
    }
    
    // NO daily forecast detail pages with 48x48 icons - removed as requested
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

void WeatherModule::drawTodayOverviewPage() {
    if (_dailyForecast.empty()) { drawNoDataPage(); return; }
    const auto& today = _dailyForecast[0];
    time_t now_utc = time(nullptr);
    time_t first_rain_hour = 0;
    PsramString dominant_icon = today.icon_name;
    bool rain_expected = false;
    struct tm tm_now; 
    gmtime_r(&now_utc, &tm_now);
    for (const auto& hour : _hourlyForecast) {
        if (hour.dt < now_utc) continue; 
        struct tm tm_hour; 
        gmtime_r(&hour.dt, &tm_hour);
        if (tm_hour.tm_yday != tm_now.tm_yday) break;
        if (hour.pop > 0.3f && (hour.rain_1h > 0 || hour.snow_1h > 0)) {
            dominant_icon = hour.icon_name; first_rain_hour = hour.dt; rain_expected = true; break; 
        }
    }
    if (!rain_expected) { dominant_icon = _currentWeather.icon_name; }
    drawWeatherIcon(24, (_canvas.height() - 48) / 2, 48, dominant_icon, isNightTime(now_utc));
    _u8g2.begin(_canvas);
    int right_block_x = 94, right_block_width = _canvas.width() - right_block_x;
    _u8g2.setFont(u8g2_font_helvB10_tr); _u8g2.setForegroundColor(0xFFFF);
    drawCenteredString(_u8g2, right_block_x, 12, right_block_width, "HEUTE");
    char tempBuf[10]; _u8g2.setFont(u8g2_font_logisoso16_tr);
    snprintf(tempBuf, sizeof(tempBuf), "%.0f°", today.temp_max); _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_max));
    drawCenteredString(_u8g2, right_block_x, 32, right_block_width, tempBuf);
    snprintf(tempBuf, sizeof(tempBuf), "%.0f°", today.temp_min); _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_min));
    drawCenteredString(_u8g2, right_block_x, 50, right_block_width, tempBuf);
    _u8g2.setFont(u8g2_font_helvR08_tr); _u8g2.setForegroundColor(0x7BEF);
    if (rain_expected && first_rain_hour > 0) {
        char rainBuf[30], timeBuf[6]; formatTime(timeBuf, sizeof(timeBuf), first_rain_hour); snprintf(rainBuf, sizeof(rainBuf), "Regen ab %s", timeBuf);
        drawCenteredString(_u8g2, right_block_x, 64, right_block_width, rainBuf);
    } else { drawCenteredString(_u8g2, right_block_x, 64, right_block_width, "Heute trocken"); }
}

void WeatherModule::drawTodayDetailsPage() {
    if (_dailyForecast.empty()) { drawNoDataPage(); return; }
    const auto& today = _dailyForecast[0];
    
    time_t now_utc = time(nullptr);
    drawWeatherIcon(10, (_canvas.height() - 48) / 2, 48, _currentWeather.icon_name, isNightTime(now_utc));
    _u8g2.begin(_canvas);
    
    int col1_x = 70, col2_x = 130;
    
    // "HEUTE" Überschrift
    _u8g2.setFont(u8g2_font_helvB08_tr); 
    _u8g2.setForegroundColor(0xFFFF); 
    _u8g2.setCursor(col1_x, 10); 
    _u8g2.print("HEUTE");
    
    // Aktuelle Temperatur mit Klimafarbe
    char tempBuf[20]; 
    snprintf(tempBuf, sizeof(tempBuf), "Akt: %.0f°", _currentWeather.temp);
    _u8g2.setForegroundColor(getClimateColorSmooth(_currentWeather.temp)); 
    _u8g2.setCursor(col2_x, 10); 
    _u8g2.print(tempBuf);
    
    // Max/Min Temperaturen mit Klimafarben
    _u8g2.setFont(u8g2_font_helvR08_tr);
    
    // Max Temperatur
    snprintf(tempBuf, sizeof(tempBuf), "Max: %.0f°", today.temp_max);
    _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_max)); 
    _u8g2.setCursor(col1_x, 22); 
    _u8g2.print(tempBuf);
    
    // Min Temperatur  
    snprintf(tempBuf, sizeof(tempBuf), "Min: %.0f°", today.temp_min);
    _u8g2.setForegroundColor(getClimateColorSmooth(today.temp_min)); 
    _u8g2.setCursor(col2_x, 22); 
    _u8g2.print(tempBuf);
    
    // Rest der Details
    _u8g2.setFont(u8g2_font_5x8_tf); 
    _u8g2.setForegroundColor(0xAAAA); 
    int y_pos = 32; 
    char buf[20], time_buf[6];
    
    // Sunrise 
    _u8g2.setCursor(col1_x, y_pos);
    _u8g2.print("Aufg:"); 
    formatTime(time_buf, sizeof(time_buf), _currentWeather.sunrise); 
    _u8g2.setCursor(col1_x + 25, y_pos); 
    _u8g2.print(time_buf);
    
    // Sunset
    _u8g2.setCursor(col2_x, y_pos);
    _u8g2.print("Unter:"); 
    formatTime(time_buf, sizeof(time_buf), _currentWeather.sunset); 
    _u8g2.setCursor(col2_x + 30, y_pos); 
    _u8g2.print(time_buf); 
    y_pos += 9;
    
    // Wind
    _u8g2.setCursor(col1_x, y_pos);
    _u8g2.print("Wind:"); 
    snprintf(buf, sizeof(buf), "%.0f km/h", _currentWeather.wind_speed); 
    _u8g2.setCursor(col1_x + 25, y_pos); 
    _u8g2.print(buf);
    
    // Gusts
    _u8g2.setCursor(col2_x, y_pos);
    _u8g2.print("Boe:"); 
    if (!isnan(_currentWeather.wind_gust)) { 
        snprintf(buf, sizeof(buf), "%.0f km/h", _currentWeather.wind_gust); 
    } else { 
        strcpy(buf, "---"); 
    } 
    _u8g2.setCursor(col2_x + 20, y_pos); 
    _u8g2.print(buf); 
    y_pos += 9;
    
    // Clouds
    _u8g2.setCursor(col1_x, y_pos);
    _u8g2.print("Wolken:"); 
    snprintf(buf, sizeof(buf), "%d%%", _currentWeather.clouds); 
    _u8g2.setCursor(col1_x + 35, y_pos); 
    _u8g2.print(buf);
    
    // Humidity
    _u8g2.setCursor(col2_x, y_pos);
    _u8g2.print("Luftf:"); 
    snprintf(buf, sizeof(buf), "%d%%", _currentWeather.humidity); 
    _u8g2.setCursor(col2_x + 30, y_pos); 
    _u8g2.print(buf); 
    
    // Feels like temperature mit Klimafarbe
    y_pos += 9;
    _u8g2.setCursor(col1_x, y_pos); 
    _u8g2.print("Gefuehlt:"); 
    snprintf(buf, sizeof(buf), "%.0f°", _currentWeather.feels_like); 
    _u8g2.setForegroundColor(getClimateColorSmooth(_currentWeather.feels_like)); 
    _u8g2.setCursor(col1_x + 45, y_pos); 
    _u8g2.print(buf);
    
    // UV Index if available
    if (_currentWeather.uvi > 0) {
        _u8g2.setForegroundColor(0xAAAA);
        _u8g2.setCursor(col2_x, y_pos);
        _u8g2.print("UV:"); 
        snprintf(buf, sizeof(buf), "%.1f", _currentWeather.uvi);
        _u8g2.setCursor(col2_x + 15, y_pos);
        _u8g2.print(buf);
    }
}

void WeatherModule::drawWeekOverviewPage() {
    if (_dailyForecast.size() < 2) { drawNoDataPage(); return; }
    
    _u8g2.begin(_canvas);
    
    // Show up to 6 days with 24x24 icons
    int days_to_show = min(6, (int)_dailyForecast.size());
    const int col_width = _canvas.width() / days_to_show;
    
    for (int i = 0; i < days_to_show; ++i) {
        const auto& day = _dailyForecast[i];
        int x = i * col_width;
        
        // Small weather icon - use noon for day determination
        time_t noon = day.dt + (12 * 60 * 60);
        drawWeatherIcon(x + (col_width - 24) / 2, 2, 24, day.icon_name, isNightTime(noon));
        
        // Day name - special handling for first 3 days
        _u8g2.setFont(u8g2_font_5x7_tf);
        _u8g2.setForegroundColor(0xFFFF);
        char day_buf[8];
        if (i == 0) {
            strcpy(day_buf, "Heute");
        } else if (i == 1) {
            strcpy(day_buf, "Morgen");
        } else {
            getDayName(day_buf, sizeof(day_buf), day.dt);
        }
        drawCenteredString(_u8g2, x, 32, col_width, day_buf);
        
        // Temperature range mit Klimafarben
        _u8g2.setFont(u8g2_font_4x6_tf);
        char temp_buf[12];
        
        // Max temp
        snprintf(temp_buf, sizeof(temp_buf), "%.0f", day.temp_max);
        _u8g2.setForegroundColor(getClimateColorSmooth(day.temp_max));
        _u8g2.setCursor(x + col_width/2 - 8, 42);
        _u8g2.print(temp_buf);
        
        // Separator
        _u8g2.setForegroundColor(0xFFFF);
        _u8g2.print("/");
        
        // Min temp
        snprintf(temp_buf, sizeof(temp_buf), "%.0f", day.temp_min);
        _u8g2.setForegroundColor(getClimateColorSmooth(day.temp_min));
        _u8g2.print(temp_buf);
        
        // Rain probability
        _u8g2.setForegroundColor(0x7BEF);
        snprintf(temp_buf, sizeof(temp_buf), "%.0f%%", day.pop * 100);
        drawCenteredString(_u8g2, x, 52, col_width, temp_buf);
        
        // Rain amount if significant
        if (day.pop > 0 && (day.rain + day.snow) > 0.1f) {
            snprintf(temp_buf, sizeof(temp_buf), "%.1f", day.rain + day.snow);
            drawCenteredString(_u8g2, x, 60, col_width, temp_buf);
        }
    }
}

void WeatherModule::drawPrecipitationPage() {
    _u8g2.begin(_canvas);
    
    // Title
    _u8g2.setFont(u8g2_font_helvB08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    drawCenteredString(_u8g2, 0, 10, _canvas.width(), "NIEDERSCHLAG HEUTE");
    
    // Find next 6 hours with rain data
    time_t now_utc = time(nullptr);
    struct tm tm_now;
    gmtime_r(&now_utc, &tm_now);
    
    int y_pos = 20;
    int hours_shown = 0;
    float total_rain = 0;
    
    _u8g2.setFont(u8g2_font_5x8_tf);
    
    for (const auto& hour : _hourlyForecast) {
        if (hour.dt < now_utc) continue;
        
        struct tm tm_hour;
        gmtime_r(&hour.dt, &tm_hour);
        if (tm_hour.tm_yday != tm_now.tm_yday) break;
        
        if (hours_shown >= 6) break;
        
        char time_buf[6];
        formatTime(time_buf, sizeof(time_buf), hour.dt);
        
        // Time
        _u8g2.setForegroundColor(0xAAAA);
        _u8g2.setCursor(10, y_pos);
        _u8g2.print(time_buf);
        
        // Rain bar visualization
        float rain_mm = hour.rain_1h + hour.snow_1h;
        total_rain += rain_mm;
        int bar_width = map((long)(rain_mm * 10), 0, 50, 0, 80); // Max 5mm
        if (bar_width > 0) {
            _canvas.fillRect(50, y_pos - 6, bar_width, 6, 0x001F);
        }
        
        // Rain amount
        char rain_buf[10];
        snprintf(rain_buf, sizeof(rain_buf), "%.1fmm", rain_mm);
        _u8g2.setForegroundColor(0x7BEF);
        _u8g2.setCursor(140, y_pos);
        _u8g2.print(rain_buf);
        
        y_pos += 8;
        hours_shown++;
    }
    
    // Total
    _u8g2.setFont(u8g2_font_helvR08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    char total_buf[20];
    snprintf(total_buf, sizeof(total_buf), "Gesamt: %.1fmm", total_rain);
    drawCenteredString(_u8g2, 0, 60, _canvas.width(), total_buf);
}

void WeatherModule::drawComfortIndexPage() {
    _u8g2.begin(_canvas);
    
    // Title
    _u8g2.setFont(u8g2_font_helvB08_tr);
    _u8g2.setForegroundColor(0xFFFF);
    drawCenteredString(_u8g2, 0, 10, _canvas.width(), "WOHLFUEHL-INDEX");
    
    _u8g2.setFont(u8g2_font_5x8_tf);
    
    // Calculate comfort scores (1-5 scale)
    int temp_score = 5;
    if (_currentWeather.temp < 0) temp_score = 1;
    else if (_currentWeather.temp < 10) temp_score = 2;
    else if (_currentWeather.temp < 18) temp_score = 3;
    else if (_currentWeather.temp < 25) temp_score = 4;
    else if (_currentWeather.temp > 30) temp_score = 3;
    
    int humid_score = 5;
    if (_currentWeather.humidity < 30) humid_score = 2;
    else if (_currentWeather.humidity < 40) humid_score = 4;
    else if (_currentWeather.humidity > 70) humid_score = 3;
    else if (_currentWeather.humidity > 80) humid_score = 2;
    
    int wind_score = 5;
    if (_currentWeather.wind_speed > 30) wind_score = 2;
    else if (_currentWeather.wind_speed > 20) wind_score = 3;
    else if (_currentWeather.wind_speed > 10) wind_score = 4;
    
    int uv_score = 5;
    if (_currentWeather.uvi > 8) uv_score = 1;
    else if (_currentWeather.uvi > 6) uv_score = 2;
    else if (_currentWeather.uvi > 3) uv_score = 3;
    else if (_currentWeather.uvi > 1) uv_score = 4;
    
    int overall_score = (temp_score + humid_score + wind_score + uv_score) / 4;
    
    // Draw scores with new color scheme
    drawComfortBar("Gesamt", overall_score, 20);
    drawComfortBar("Waerme", temp_score, 30);
    drawComfortBar("Feucht", humid_score, 40);
    drawComfortBar("Wind", wind_score, 50);
    if (_currentWeather.uvi > 0) {
        drawComfortBar("UV", uv_score, 60);
    }
}

void WeatherModule::drawComfortBar(const char* label, int score, int y) {
    // Label in cyan
    _u8g2.setForegroundColor(0x07FF);  // Cyan
    _u8g2.setCursor(10, y + 5);
    _u8g2.print(label);
    
    // Draw score circles with comfort colors
    for (int i = 0; i < 5; i++) {
        int x = 60 + i * 12;
        if (i < score) {
            _canvas.fillCircle(x, y + 2, 4, getComfortColor(score));
        } else {
            _canvas.drawCircle(x, y + 2, 4, 0x4208);
        }
    }
    
    // Score text with comfort color
    const char* rating = "---";
    if (score >= 5) rating = "Super";
    else if (score >= 4) rating = "Gut";
    else if (score >= 3) rating = "OK";
    else if (score >= 2) rating = "Maessig";
    else rating = "Schlecht";
    
    _u8g2.setForegroundColor(getComfortColor(score));
    _u8g2.setCursor(130, y + 5);
    _u8g2.print(rating);
}

void WeatherModule::drawHourlyForecastPage() {
    if (!_config || _hourlyForecast.empty()) { drawNoDataPage(); return; }
    _u8g2.begin(_canvas);
    const int num_cols = 4; 
    const int col_width = _canvas.width() / num_cols;
    
    // Get current local time for comparison
    time_t now_utc = time(nullptr);
    time_t now_local = _timeConverter.toLocal(now_utc);
    
    size_t current_hour_index = 0;
    for(size_t i=0; i<_hourlyForecast.size(); ++i){ 
        if(_hourlyForecast[i].dt >= now_utc){ 
            current_hour_index = i; 
            break; 
        } 
    }
    
    if (_config->weatherHourlyMode == 0) {
        int interval = _config->weatherHourlyInterval > 0 ? _config->weatherHourlyInterval : 1;
        for (int i = 0; i < num_cols; ++i) {
            size_t hour_index = current_hour_index + (interval * i); 
            if (hour_index >= _hourlyForecast.size()) break;
            const auto& hour = _hourlyForecast[hour_index]; 
            int x = i * col_width;
            
            drawWeatherIcon(x + (col_width - 24) / 2, 8, 24, hour.icon_name, isNightTime(hour.dt));
            _u8g2.setFont(u8g2_font_5x8_tf); 
            _u8g2.setForegroundColor(0xFFFF);
            
            // Check if it's a different day using local time
            time_t hour_local = _timeConverter.toLocal(hour.dt);
            struct tm tm_hour, tm_now; 
            localtime_r(&hour_local, &tm_hour); 
            localtime_r(&now_local, &tm_now);
            
            if (tm_hour.tm_yday != tm_now.tm_yday) { 
                char day_buf[6]; 
                getDayName(day_buf, sizeof(day_buf), hour.dt); 
                drawCenteredString(_u8g2, x, 6, col_width, day_buf); 
            }
            
            char time_buf[6]; 
            formatTime(time_buf, sizeof(time_buf), hour.dt); 
            drawCenteredString(_u8g2, x, 36, col_width, time_buf);
            
            // Temperature mit Klimafarbe
            char temp_buf[8]; 
            snprintf(temp_buf, sizeof(temp_buf), "%.0f°", hour.temp);
            _u8g2.setForegroundColor(getClimateColorSmooth(hour.temp)); 
            drawCenteredString(_u8g2, x, 48, col_width, temp_buf);
            
            // Show rain amount if significant
            if (hour.pop > 0.5f && hour.rain_1h > 0) {
                char rain_buf[8];
                snprintf(rain_buf, sizeof(rain_buf), "%.1fmm", hour.rain_1h);
                _u8g2.setForegroundColor(0x001F);
                drawCenteredString(_u8g2, x, 58, col_width, rain_buf);
            } else {
                char pop_buf[8]; 
                snprintf(pop_buf, sizeof(pop_buf), "%.0f%%", hour.pop * 100); 
                _u8g2.setForegroundColor(0x7BEF);
                drawCenteredString(_u8g2, x, 58, col_width, pop_buf);
            }
        }
    }
}

void WeatherModule::drawMultiDayForecastPage() {
    // This function is no longer used as we removed the 48x48 icon pages
    drawNoDataPage();
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
    // TEST: First verify PROGMEM reading works with test array
    Serial.println("\n=== PROGMEM TEST ===");
    Serial.println("Test array (should be: 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF):");
    for(int i=0; i<15; i++) {
        uint8_t val = pgm_read_byte(test_icon.data + i);
        Serial.printf("%02X ", val);
    }
    Serial.println("\n=== END TEST ===\n");
    
    // TEST: Explicitly load "unknown" icon to verify registry
    Serial.println("=== EXPLICIT UNKNOWN ICON TEST ===");
    const WeatherIcon* unknownIcon = globalWeatherIconSet.getIcon("unknown", false);
    if (unknownIcon && unknownIcon->data) {
        Serial.printf("Unknown icon pointer: %p\n", unknownIcon->data);
        Serial.printf("Unknown icon first 15 bytes: ");
        for(int i=0; i<15; i++) {
            uint8_t val = pgm_read_byte(unknownIcon->data + i);
            Serial.printf("%02X ", val);
        }
        Serial.println();
    } else {
        Serial.println("ERROR: Unknown icon is NULL or has no data!");
    }
    Serial.println("=== END UNKNOWN TEST ===\n");
    
    // Retrieves the appropriate icon from the registry/cache, independent of Main/Special!
    // Converts PsramString to std::string for cache lookup
    std::string iconName(name.c_str());
    Serial.printf("Requested icon name: '%s', isNight: %d\n", iconName.c_str(), isNight);
    
    // TEMPORARY TEST: Bypass cache and read directly from PROGMEM
    // This helps diagnose if the issue is with cache/PSRAM or PROGMEM reading
    const WeatherIcon* src = globalWeatherIconSet.getIcon(iconName, isNight);
    Serial.printf("  getIcon('%s', %d) returned: %p\n", iconName.c_str(), isNight, src);
    if (!src) {
        src = globalWeatherIconSet.getIcon(iconName, false);
        Serial.printf("  getIcon('%s', false) returned: %p\n", iconName.c_str(), src);
    }
    if (!src) {
        src = globalWeatherIconSet.getUnknown();
        Serial.printf("  getUnknown() returned: %p\n", src);
    }
    if (!src || !src->data) {
        Serial.println("ERROR: No valid icon found!");
        return;
    }
    
    // Use PSRAM cache for all icon sizes (with bilinear scaling)
    const WeatherIcon* iconPtr = globalWeatherIconCache.getScaled(iconName, size, isNight);
    if(!iconPtr || !iconPtr->data) {
        iconPtr = globalWeatherIconCache.getScaled("unknown", size, false);
        Serial.printf("  Fallback to unknown icon (size %d)\n", size);
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