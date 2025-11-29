#include "BirthdayModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <cstring>
#include <cstdlib>
#include <algorithm>

// Static helper function - convert hex color to RGB565
uint16_t BirthdayModule::hexColorTo565(const PsramString& hex) {
    if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
    long r = strtol(hex.substr(1, 2).c_str(), NULL, 16);
    long g = strtol(hex.substr(3, 2).c_str(), NULL, 16);
    long b = strtol(hex.substr(5, 2).c_str(), NULL, 16);
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

BirthdayModule::BirthdayModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas,
                               const GeneralTimeConverter& timeConverter,
                               WebClientModule* webClient, DeviceConfig* config)
    : _u8g2(u8g2), _canvas(canvas), _timeConverter(timeConverter),
      _webClient(webClient), _deviceConfig(config), _lastProcessedUpdate(0) {
    _dataMutex = xSemaphoreCreateMutex();
}

BirthdayModule::~BirthdayModule() {
    if (_dataMutex) vSemaphoreDelete(_dataMutex);
    if (_pendingBuffer) free(_pendingBuffer);
}

void BirthdayModule::begin() {
    // Nothing special needed at begin
}

void BirthdayModule::setConfig(const PsramString& url, unsigned long fetchMinutes,
                                unsigned long displaySec, const PsramString& headerColor,
                                const PsramString& textColor) {
    _icsUrl = url;
    _isEnabled = !url.empty();
    
    _fetchIntervalMinutes = fetchMinutes > 0 ? fetchMinutes : 60;
    _displayDuration = displaySec > 0 ? displaySec * 1000UL : 30000;
    
    _headerColor = hexColorTo565(headerColor);
    _textColor = hexColorTo565(textColor);
    
    if (_isEnabled && _webClient) {
        _webClient->registerResource(String(_icsUrl.c_str()), _fetchIntervalMinutes, nullptr);
    }
}

void BirthdayModule::queueData() {
    if (_icsUrl.empty() || !_webClient) return;
    
    _webClient->accessResource(String(_icsUrl.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
        if (buffer && size > 0 && last_update > this->_lastProcessedUpdate) {
            if (_pendingBuffer) free(_pendingBuffer);
            _pendingBuffer = (char*)ps_malloc(size + 1);
            if (_pendingBuffer) {
                memcpy(_pendingBuffer, buffer, size);
                _pendingBuffer[size] = '\0';
                _bufferSize = size;
                _lastProcessedUpdate = last_update;
                _dataPending = true;
            }
        }
    });
}

void BirthdayModule::processData() {
    if (_dataPending) {
        if (xSemaphoreTake(_dataMutex, portMAX_DELAY) == pdTRUE) {
            parseICS(_pendingBuffer, _bufferSize);
            onSuccessfulUpdate();
            free(_pendingBuffer);
            _pendingBuffer = nullptr;
            _dataPending = false;
            xSemaphoreGive(_dataMutex);
            if (_updateCallback) _updateCallback();
        }
    }
}

void BirthdayModule::onUpdate(std::function<void()> callback) {
    _updateCallback = callback;
}

void BirthdayModule::tick() {
    // Animation tick - nothing special needed
}

void BirthdayModule::periodicTick() {
    // Background tasks - nothing special needed
}

void BirthdayModule::logicTick() {
    _logicTicksSinceStart++;
    
    // Calculate ticks needed for display duration
    uint32_t ticksPerPage = _displayDuration / 100;
    if (ticksPerPage == 0) ticksPerPage = 1;
    
    if (_logicTicksSinceStart >= ticksPerPage) {
        int totalPages = getTotalPages();
        if (totalPages > 0) {
            _currentPage = (_currentPage + 1) % totalPages;
            if (_currentPage == 0) {
                _isFinished = true;
            }
        }
        _logicTicksSinceStart = 0;
    }
}

unsigned long BirthdayModule::getDisplayDuration() {
    return _displayDuration * getTotalPages();
}

bool BirthdayModule::isEnabled() {
    if (!_isEnabled) return false;
    
    bool hasEvents = false;
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        hasEvents = !_birthdayEvents.empty();
        xSemaphoreGive(_dataMutex);
    }
    return hasEvents;
}

void BirthdayModule::resetPaging() {
    _currentPage = 0;
    _logicTicksSinceStart = 0;
    _isFinished = false;
}

bool BirthdayModule::parseBirthdayDateTime(const char* line, size_t len, BirthdayEvent& event) {
    if (!line || len == 0) return false;
    
    // Find the value after the colon
    const char* value_ptr = line;
    const char* colon = (const char*)memchr(line, ':', len);
    if (colon) {
        value_ptr = colon + 1;
    }
    
    // Check for VALUE=DATE (all-day event)
    const char* semicolon = (const char*)memchr(line, ';', len);
    event.hasTime = true;
    if (semicolon && semicolon < colon) {
        const char* value_date = strstr(semicolon, "VALUE=DATE");
        if (value_date && value_date < colon) {
            event.hasTime = false;
        }
    }
    
    const char* dt_str = value_ptr;
    size_t dt_len = strlen(dt_str);
    
    // Trim trailing whitespace
    while (dt_len > 0 && !isalnum(dt_str[dt_len - 1])) {
        dt_len--;
    }
    
    if (dt_len < 8) return false;
    
    // Parse date components
    auto parseDecimal = [](const char* p, size_t len) -> int {
        int v = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = p[i];
            if (c < '0' || c > '9') break;
            v = v * 10 + (c - '0');
        }
        return v;
    };
    
    event.birthYear = parseDecimal(dt_str, 4);
    event.birthMonth = parseDecimal(dt_str + 4, 2);
    event.birthDay = parseDecimal(dt_str + 6, 2);
    event.birthHour = 0;
    event.birthMinute = 0;
    event.birthSecond = 0;
    
    // Parse time if present (format: YYYYMMDDTHHMMSS)
    if (dt_len > 8 && dt_str[8] == 'T') {
        if (dt_len >= 15) {
            event.birthHour = parseDecimal(dt_str + 9, 2);
            event.birthMinute = parseDecimal(dt_str + 11, 2);
            event.birthSecond = parseDecimal(dt_str + 13, 2);
        }
    } else {
        event.hasTime = false;
    }
    
    // Validate date
    if (event.birthYear < 1 || event.birthMonth < 1 || event.birthMonth > 12 ||
        event.birthDay < 1 || event.birthDay > 31) {
        return false;
    }
    
    // Calculate epoch (may be negative for pre-1970 dates)
    struct tm t = {0};
    t.tm_year = event.birthYear - 1900;
    t.tm_mon = event.birthMonth - 1;
    t.tm_mday = event.birthDay;
    t.tm_hour = event.birthHour;
    t.tm_min = event.birthMinute;
    t.tm_sec = event.birthSecond;
    t.tm_isdst = -1;
    
    // Use our fixed timegm that handles pre-1970 dates
    event.birthEpoch = timegm(&t);
    
    return true;
}

void BirthdayModule::parseVEventForBirthday(const char* veventBlock, size_t len, BirthdayEvent& event) {
    if (!veventBlock || len == 0) return;
    
    const char* p = veventBlock;
    const char* end = veventBlock + len;
    
    while (p < end) {
        const char* lineEnd = (const char*)memchr(p, '\n', end - p);
        size_t lineLen = 0;
        if (lineEnd) {
            lineLen = lineEnd - p;
        } else {
            lineLen = end - p;
            lineEnd = end;
        }
        if (lineLen > 0 && p[lineLen - 1] == '\r') --lineLen;
        
        // Parse SUMMARY for name
        if (lineLen > 8 && strncmp(p, "SUMMARY:", 8) == 0) {
            const char* val = p + 8;
            event.name = PsramString(val, lineLen - 8);
        }
        // Parse DTSTART for birth date
        else if (strncmp(p, "DTSTART", 7) == 0) {
            parseBirthdayDateTime(p, lineLen, event);
        }
        
        p = lineEnd + 1;
    }
}

void BirthdayModule::parseICS(char* icsBuffer, size_t size) {
    if (!icsBuffer || size == 0) return;
    
    PsramString ics(icsBuffer, size);
    _rawEvents.clear();
    _rawEvents.reserve(64);
    
    size_t idx = 0;
    const PsramString beginTag("BEGIN:VEVENT"), endTag("END:VEVENT");
    
    while (true) {
        size_t pos = ics.find(beginTag, idx);
        if (pos == PsramString::npos) break;
        
        size_t endPos = ics.find(endTag, pos);
        if (endPos == PsramString::npos) break;
        
        PsramString veventBlock = ics.substr(pos, (endPos + endTag.length()) - pos);
        
        BirthdayEvent event;
        event.birthEpoch = 0;
        event.birthYear = 0;
        event.hasTime = false;
        
        parseVEventForBirthday(veventBlock.c_str(), veventBlock.length(), event);
        
        if (event.birthYear > 0 && !event.name.empty()) {
            _rawEvents.push_back(std::move(event));
        }
        
        idx = endPos + endTag.length();
        if (_rawEvents.size() % 20 == 0) delay(1);
    }
}

bool BirthdayModule::isBirthdayToday(const BirthdayEvent& event) const {
    time_t now_utc;
    time(&now_utc);
    time_t local_now = _timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    return (event.birthMonth == (tm_now.tm_mon + 1)) && (event.birthDay == tm_now.tm_mday);
}

void BirthdayModule::onSuccessfulUpdate() {
    if (_rawEvents.empty()) {
        _birthdayEvents.clear();
        return;
    }
    
    // Filter for today's birthdays
    _birthdayEvents.clear();
    _birthdayEvents.reserve(_rawEvents.size());
    
    for (const auto& event : _rawEvents) {
        if (isBirthdayToday(event)) {
            _birthdayEvents.push_back(event);
        }
    }
    
    // Sort by name
    std::sort(_birthdayEvents.begin(), _birthdayEvents.end(),
              [](const BirthdayEvent& a, const BirthdayEvent& b) {
                  return a.name < b.name;
              });
    
    resetPaging();
}

AgeInfo BirthdayModule::calculateAge(const BirthdayEvent& event) const {
    AgeInfo age = {0};
    
    time_t now_utc;
    time(&now_utc);
    time_t local_now = _timeConverter.toLocal(now_utc);
    
    struct tm tm_now;
    localtime_r(&local_now, &tm_now);
    
    int nowYear = tm_now.tm_year + 1900;
    int nowMonth = tm_now.tm_mon + 1;
    int nowDay = tm_now.tm_mday;
    int nowHour = tm_now.tm_hour;
    int nowMinute = tm_now.tm_min;
    int nowSecond = tm_now.tm_sec;
    
    int birthYear = event.birthYear;
    int birthMonth = event.birthMonth;
    int birthDay = event.birthDay;
    int birthHour = event.hasTime ? event.birthHour : 0;
    int birthMinute = event.hasTime ? event.birthMinute : 0;
    int birthSecond = event.hasTime ? event.birthSecond : 0;
    
    // Calculate years, months, days using calendar math
    age.years = nowYear - birthYear;
    age.months = nowMonth - birthMonth;
    age.days = nowDay - birthDay;
    age.hours = nowHour - birthHour;
    age.minutes = nowMinute - birthMinute;
    age.seconds = nowSecond - birthSecond;
    
    // Handle borrowing
    if (age.seconds < 0) {
        age.seconds += 60;
        age.minutes--;
    }
    if (age.minutes < 0) {
        age.minutes += 60;
        age.hours--;
    }
    if (age.hours < 0) {
        age.hours += 24;
        age.days--;
    }
    if (age.days < 0) {
        // Get days in previous month
        int prevMonth = nowMonth - 1;
        int prevMonthYear = nowYear;
        if (prevMonth < 1) {
            prevMonth = 12;
            prevMonthYear--;
        }
        
        static const int daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int daysInPrevMonth = daysInMonth[prevMonth - 1];
        if (prevMonth == 2 && is_leap(prevMonthYear)) {
            daysInPrevMonth = 29;
        }
        
        age.days += daysInPrevMonth;
        age.months--;
    }
    if (age.months < 0) {
        age.months += 12;
        age.years--;
    }
    
    // Calculate total values
    // Use epoch difference for accurate totals (handles pre-1970)
    int64_t birthEpoch64 = event.birthEpoch;
    int64_t nowEpoch64 = local_now;
    int64_t diffSeconds = nowEpoch64 - birthEpoch64;
    
    if (diffSeconds < 0) diffSeconds = 0; // Future birth date
    
    age.totalSeconds = diffSeconds;
    age.totalMinutes = diffSeconds / 60;
    age.totalHours = diffSeconds / 3600;
    age.totalDays = diffSeconds / 86400;
    
    return age;
}

void BirthdayModule::draw() {
    if (xSemaphoreTake(_dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;
    
    _canvas.fillScreen(0);
    _u8g2.begin(_canvas);
    
    if (_birthdayEvents.empty()) {
        _u8g2.setFont(u8g2_font_7x14_tf);
        _u8g2.setForegroundColor(_textColor);
        const char* text = "Kein Geburtstag heute";
        int textWidth = _u8g2.getUTF8Width(text);
        _u8g2.setCursor((_canvas.width() - textWidth) / 2, 30);
        _u8g2.print(text);
        xSemaphoreGive(_dataMutex);
        return;
    }
    
    // Get current page event
    int pageIndex = _currentPage;
    if (pageIndex >= (int)_birthdayEvents.size()) {
        pageIndex = 0;
    }
    
    const BirthdayEvent& event = _birthdayEvents[pageIndex];
    AgeInfo age = calculateAge(event);
    
    drawBirthdayPage(event, age);
    
    xSemaphoreGive(_dataMutex);
}

void BirthdayModule::drawBirthdayPage(const BirthdayEvent& event, const AgeInfo& age) {
    int y = 12;
    
    // Header: Name
    _u8g2.setFont(u8g2_font_helvB12_tf);
    _u8g2.setForegroundColor(_headerColor);
    
    PsramString headerText = event.name;
    int headerWidth = _u8g2.getUTF8Width(headerText.c_str());
    
    // If too wide, use smaller font
    if (headerWidth > _canvas.width() - 4) {
        _u8g2.setFont(u8g2_font_7x14_tf);
        headerWidth = _u8g2.getUTF8Width(headerText.c_str());
    }
    
    _u8g2.setCursor((_canvas.width() - headerWidth) / 2, y);
    _u8g2.print(headerText.c_str());
    y += 16;
    
    // Line 1: Human-readable age format
    _u8g2.setFont(u8g2_font_6x10_tf);
    _u8g2.setForegroundColor(_textColor);
    
    char line1[100];
    if (event.hasTime) {
        snprintf(line1, sizeof(line1), "Du bist %d J, %d M, %d T, %02d:%02d:%02d alt",
                 age.years, age.months, age.days,
                 age.hours, age.minutes, age.seconds);
    } else {
        snprintf(line1, sizeof(line1), "Du bist %d Jahre, %d Monate, %d Tage alt",
                 age.years, age.months, age.days);
    }
    
    int line1Width = _u8g2.getUTF8Width(line1);
    if (line1Width > _canvas.width() - 4) {
        // Shorter format if too wide
        if (event.hasTime) {
            snprintf(line1, sizeof(line1), "%dJ %dM %dT %02d:%02d:%02d",
                     age.years, age.months, age.days,
                     age.hours, age.minutes, age.seconds);
        } else {
            snprintf(line1, sizeof(line1), "%d J, %d M, %d T alt",
                     age.years, age.months, age.days);
        }
        line1Width = _u8g2.getUTF8Width(line1);
    }
    
    _u8g2.setCursor((_canvas.width() - line1Width) / 2, y);
    _u8g2.print(line1);
    y += 12;
    
    // Line 2: Total values (Variant B)
    char line2[120];
    
    // Format with thousands separator
    auto formatNumber = [](int64_t num, char* buf, size_t bufSize) {
        if (num < 1000) {
            snprintf(buf, bufSize, "%lld", (long long)num);
            return;
        }
        
        char temp[32];
        snprintf(temp, sizeof(temp), "%lld", (long long)num);
        int len = strlen(temp);
        int commas = (len - 1) / 3;
        int newLen = len + commas;
        
        if ((size_t)newLen >= bufSize) {
            snprintf(buf, bufSize, "%lld", (long long)num);
            return;
        }
        
        buf[newLen] = '\0';
        int j = newLen - 1;
        int count = 0;
        for (int i = len - 1; i >= 0; i--) {
            buf[j--] = temp[i];
            count++;
            if (count == 3 && i > 0) {
                buf[j--] = '.';
                count = 0;
            }
        }
    };
    
    char totalDaysStr[24], totalHoursStr[24], totalMinsStr[24];
    formatNumber(age.totalDays, totalDaysStr, sizeof(totalDaysStr));
    formatNumber(age.totalHours, totalHoursStr, sizeof(totalHoursStr));
    formatNumber(age.totalMinutes, totalMinsStr, sizeof(totalMinsStr));
    
    snprintf(line2, sizeof(line2), "= %s Tage = %s Std", totalDaysStr, totalHoursStr);
    
    int line2Width = _u8g2.getUTF8Width(line2);
    _u8g2.setCursor((_canvas.width() - line2Width) / 2, y);
    _u8g2.print(line2);
    y += 12;
    
    // Line 3: More total values
    char totalSecsStr[24];
    formatNumber(age.totalSeconds, totalSecsStr, sizeof(totalSecsStr));
    
    char line3[80];
    snprintf(line3, sizeof(line3), "= %s Min = %s Sek", totalMinsStr, totalSecsStr);
    
    int line3Width = _u8g2.getUTF8Width(line3);
    
    // If too wide, split
    if (line3Width > _canvas.width() - 4) {
        snprintf(line3, sizeof(line3), "= %s Min", totalMinsStr);
        line3Width = _u8g2.getUTF8Width(line3);
        _u8g2.setCursor((_canvas.width() - line3Width) / 2, y);
        _u8g2.print(line3);
        y += 12;
        
        snprintf(line3, sizeof(line3), "= %s Sek", totalSecsStr);
        line3Width = _u8g2.getUTF8Width(line3);
        _u8g2.setCursor((_canvas.width() - line3Width) / 2, y);
        _u8g2.print(line3);
    } else {
        _u8g2.setCursor((_canvas.width() - line3Width) / 2, y);
        _u8g2.print(line3);
    }
}
