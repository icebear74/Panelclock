#include "CuriousHolidaysModule.hpp"
#include <algorithm>
#include <cctype>

// Hilfsfunktionen
static PsramString trim(const PsramString& str) {
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == PsramString::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r\f\v");
    return str.substr(start, end - start + 1);
}

// Bereinigungs-Funktion, die nur erlaubte Zeichen durchlässt
static PsramString sanitizeString(const PsramString& input) {
    PsramString output = "";
    output.reserve(input.length());
    for (size_t i = 0; i < input.length(); ++i) {
        unsigned char c1 = input[i];
        
        // KORREKTUR: Der Punkt wird jetzt explizit erlaubt.
        bool isAllowed = (c1 >= 'a' && c1 <= 'z') ||
                         (c1 >= 'A' && c1 <= 'Z') ||
                         (c1 >= '0' && c1 <= '9') ||
                         c1 == ' ' || c1 == '-' || c1 == '.';

        if (isAllowed) {
            output += c1;
        } else if (c1 == 0xC3) { // UTF-8 Startbyte für Umlaute/ß
            if (i + 1 < input.length()) {
                unsigned char c2 = input[i+1];
                if (c2 == 0xA4 || c2 == 0xB6 || c2 == 0xBC || // ä, ö, ü
                    c2 == 0x84 || c2 == 0x96 || c2 == 0x9C || // Ä, Ö, Ü
                    c2 == 0x9F) { // ß
                    output += c1;
                    output += c2;
                    i++; // Zwei Bytes verarbeitet
                } else {
                    output += ' '; // Unerlaubtes UTF-8 Zeichen
                }
            }
        } else {
            output += ' '; // Alle anderen unerlaubten Zeichen
        }
    }
    return output;
}


static uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) { 
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3); 
}

static int drawAndCountLines(U8G2_FOR_ADAFRUIT_GFX& u8g2, PsramString text, int x, int& y, int maxWidth, int lineHeight, bool doDraw, bool capitalizeFirst) {
    if (text.empty()) return 0;

    text = trim(text);
    if (capitalizeFirst && !text.empty()) {
        text[0] = std::toupper(static_cast<unsigned char>(text[0]));
    }

    int lines = 0;
    if (text.length() == 0) return 0;
    
    PsramString currentLine;
    int lastSpace = -1;
    int pos = 0;

    while(pos < text.length()){
        lastSpace = text.find(' ', pos);
        if(lastSpace == PsramString::npos){
            lastSpace = text.length();
        }

        PsramString word = text.substr(pos, lastSpace - pos);
        PsramString potentialLine = currentLine.empty() ? word : currentLine + " " + word;

        if (u8g2.getUTF8Width(potentialLine.c_str()) <= maxWidth) {
            currentLine = potentialLine;
        } else {
            if (!currentLine.empty()) {
                if(doDraw) {
                    u8g2.setCursor(x, y);
                    u8g2.print(currentLine.c_str());
                    y += lineHeight;
                }
                lines++;
            }
            currentLine = word;
            if (u8g2.getUTF8Width(currentLine.c_str()) > maxWidth) {
                 if(doDraw) {
                    u8g2.setCursor(x, y);
                    u8g2.print(currentLine.c_str());
                    y += lineHeight;
                }
                lines++;
                currentLine = "";
            }
        }
        pos = lastSpace + 1;
    }

    if(!currentLine.empty()){
        if(doDraw) {
            u8g2.setCursor(x, y);
            u8g2.print(currentLine.c_str());
            y += lineHeight;
        }
        lines++;
    }
    
    return lines;
}

CuriousHolidaysModule::CuriousHolidaysModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, GeneralTimeConverter& timeConverter, WebClientModule* webClient)
    : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter), webClient(webClient), lastProcessedUpdate(0), _lastCheckedDay(-1) {
    dataMutex = xSemaphoreCreateMutex();
}

CuriousHolidaysModule::~CuriousHolidaysModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (pendingBuffer) free(pendingBuffer);
}

void CuriousHolidaysModule::begin() {
    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    _lastCheckedDay = tm_now.tm_mday;
    lastMonth = tm_now.tm_mon + 1;
    setConfig();
}

void CuriousHolidaysModule::setConfig() {
    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    
    const char* monthNames[] = {"januar", "februar", "maerz", "april", "mai", "juni", "juli", "august", "september", "oktober", "november", "dezember"};
    PsramString monthName = monthNames[tm_now.tm_mon];

    resourceUrl = "https://www.kuriose-feiertage.de/kalender/" + monthName + "/";

    if (webClient) {
        webClient->registerResource(String(resourceUrl.c_str()), 720, nullptr);
    }
}

void CuriousHolidaysModule::queueData() {
    if (resourceUrl.empty() || !webClient) return;

    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);
    int currentDay = tm_now.tm_mday;

    if (currentDay != _lastCheckedDay) {
        handleDayChange();
    }
    
    if (tm_now.tm_mon + 1 != lastMonth) {
        lastMonth = tm_now.tm_mon + 1;
        setConfig(); 
    }

    webClient->accessResource(String(resourceUrl.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale) {
        if (buffer && size > 0 && last_update > this->lastProcessedUpdate) {
            if (pendingBuffer) free(pendingBuffer);
            pendingBuffer = (char*)ps_malloc(size + 1);
            if (pendingBuffer) {
                memcpy(pendingBuffer, buffer, size);
                pendingBuffer[size] = '\0';
                bufferSize = size;
                lastProcessedUpdate = last_update;
                dataPending = true;
            }
        }
    });
}

void CuriousHolidaysModule::handleDayChange() {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        time_t now = time(nullptr);
        time_t local_time = timeConverter.toLocal(now);
        struct tm tm_now;
        localtime_r(&local_time, &tm_now);
        
        _lastCheckedDay = tm_now.tm_mday;
        
        holidaysToday = holidaysTomorrow;
        holidaysTomorrow.clear();
        
        xSemaphoreGive(dataMutex);
        
        calculatePages();
        if (updateCallback) updateCallback();
    }
}

void CuriousHolidaysModule::processData() {
    if (dataPending) {
        parseAndProcessHtml(pendingBuffer, bufferSize);
        free(pendingBuffer);
        pendingBuffer = nullptr;
        dataPending = false;
        if (updateCallback) this->updateCallback();
    }
}

void CuriousHolidaysModule::parseAndProcessHtml(const char* buffer, size_t size) {
    PsramVector<HolidayEntry> today, tomorrow;
    PsramString html(buffer, size);

    time_t now = time(nullptr);
    time_t local_now = timeConverter.toLocal(now);
    struct tm tm_today;
    localtime_r(&local_now, &tm_today);
    
    time_t tomorrow_epoch = local_now + 86400;
    struct tm tm_tomorrow;
    localtime_r(&tomorrow_epoch, &tm_tomorrow);

    const char* monthNames[] = {"Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"};
    PsramString todayMonthName = monthNames[tm_today.tm_mon];
    PsramString tomorrowMonthName = monthNames[tm_tomorrow.tm_mon];

    int h2Pos = 0;
    while ((h2Pos = indexOf(html, "<h2>", h2Pos)) != -1) {
        int h2End = indexOf(html, "</h2>", h2Pos);
        if (h2End == -1) break;

        PsramString dateFromH2 = trim(html.substr(h2Pos + 4, h2End - (h2Pos + 4)));
        if (!dateFromH2.empty() && dateFromH2.back() == ':') dateFromH2.pop_back();

        int day = atoi(dateFromH2.c_str());
        PsramString monthStr;
        int dotPos = indexOf(dateFromH2, ".");
        if(dotPos != -1) monthStr = trim(dateFromH2.substr(dotPos + 1));
        
        bool isToday = (day == tm_today.tm_mday && monthStr.find(todayMonthName.c_str()) != PsramString::npos);
        if (!isToday && tm_today.tm_mon == 2) {
             isToday = (day == tm_today.tm_mday && monthStr.find("Maerz") != PsramString::npos);
        }
        bool isTomorrow = (day == tm_tomorrow.tm_mday && monthStr.find(tomorrowMonthName.c_str()) != PsramString::npos);
        if (!isTomorrow && tm_tomorrow.tm_mon == 2) {
             isTomorrow = (day == tm_tomorrow.tm_mday && monthStr.find("Maerz") != PsramString::npos);
        }

        if (!isToday && !isTomorrow) {
            h2Pos = h2End; continue;
        }

        int ulStart = indexOf(html, "<ul class=\"lcp_catlist\"", h2End);
        if (ulStart == -1) continue;
        int ulEnd = indexOf(html, "</ul>", ulStart);
        if (ulEnd == -1) continue;
        PsramString ulContent = html.substr(ulStart, ulEnd - ulStart);

        int liPos = 0;
        while ((liPos = indexOf(ulContent, "<li>", liPos)) != -1) {
            int liEnd = indexOf(ulContent, "</li>", liPos);
            if (liEnd == -1) break;

            PsramString liContent = ulContent.substr(liPos, liEnd - liPos);
            int aStart = indexOf(liContent, "<a ");
            if (aStart == -1) { liPos = liEnd + 5; continue; }
            int aEnd = indexOf(liContent, "</a>", aStart);
            if (aEnd == -1) { liPos = liEnd + 5; continue; }

            PsramString aText = liContent.substr(aStart, aEnd - aStart);
            int textStart = indexOf(aText, ">") + 1;
            if (textStart == 0) { liPos = liEnd + 5; continue; }

            PsramString rawFullText = aText.substr(textStart);
            
            HolidayEntry entry;
            PsramString rawName, rawDescription;

            int dashPos = indexOf(rawFullText, "–");
            if (dashPos == -1) dashPos = indexOf(rawFullText, "&#8211;");

            if (dashPos != -1) {
                rawName = rawFullText.substr(0, dashPos);
                int descStart = rawFullText.find_first_not_of(" –&;#8211;", dashPos);
                if(descStart != -1) rawDescription = rawFullText.substr(descStart);

            } else {
                rawName = rawFullText;
            }

            entry.name = trim(sanitizeString(rawName));
            entry.description = trim(sanitizeString(rawDescription));

            char date_str[30];
            snprintf(date_str, sizeof(date_str), "am %d. %s", (isToday ? tm_today.tm_mday : tm_tomorrow.tm_mday), (isToday ? todayMonthName.c_str() : tomorrowMonthName.c_str()));
            size_t date_pos = entry.description.find(date_str);
            if(date_pos != PsramString::npos) {
                entry.description.erase(date_pos, strlen(date_str));
            }
             if(tm_today.tm_mon == 2 || tm_tomorrow.tm_mon == 2){ //März/Maerz
                 snprintf(date_str, sizeof(date_str), " am %d. Maerz", (isToday ? tm_today.tm_mday : tm_tomorrow.tm_mday));
                 date_pos = entry.description.find(date_str);
                 if(date_pos != PsramString::npos) {
                    entry.description.erase(date_pos, strlen(date_str));
                 }
            }
            entry.description = trim(entry.description);


            if (!entry.name.empty()) {
                if (isToday) today.push_back(entry);
                if (isTomorrow) tomorrow.push_back(entry);
            }
            liPos = liEnd + 5;
        }
        h2Pos = ulEnd;
    }

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        holidaysToday = today;
        holidaysTomorrow = tomorrow;
        xSemaphoreGive(dataMutex);
    }
    calculatePages();
}

void CuriousHolidaysModule::calculatePages() {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) != pdTRUE) return;

    pageIndices.clear();
    if (holidaysToday.empty()) {
        xSemaphoreGive(dataMutex);
        return;
    }

    const int maxWidth = canvas.width() - 10;
    const int lineHeight = 10;
    const int entrySpacing = 8;
    const int topMargin = 25;
    const int availableHeight = canvas.height() - topMargin - 5;
    int dummy_y = 0;

    u8g2.setFont(u8g2_font_5x8_tf);

    int holidayIndex = 0;
    while (holidayIndex < holidaysToday.size()) {
        std::vector<int> page;
        int currentHeight = 0;
        
        while (holidayIndex < holidaysToday.size()) {
            const HolidayEntry& entry = holidaysToday[holidayIndex];
            
            int nameLines = drawAndCountLines(u8g2, entry.name, 0, dummy_y, maxWidth, lineHeight, false, false);
            int descLines = drawAndCountLines(u8g2, entry.description, 0, dummy_y, maxWidth, lineHeight, false, false);
            int entryHeight = (nameLines + descLines) * lineHeight + entrySpacing;

            if (currentHeight + entryHeight > availableHeight && !page.empty()) break; 
            
            page.push_back(holidayIndex);
            currentHeight += entryHeight;
            holidayIndex++;
        }
        
        if (!page.empty()) pageIndices.push_back(page);
        else if (holidayIndex < holidaysToday.size()) {
            page.push_back(holidayIndex);
            pageIndices.push_back(page);
            holidayIndex++;
        }
    }

    resetPaging(); 
    xSemaphoreGive(dataMutex);
}

void CuriousHolidaysModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    canvas.fillScreen(0);
    u8g2.begin(canvas);

    if (pageIndices.empty() || holidaysToday.empty()) {
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setForegroundColor(0xFFFF);
        const char* text = "Keine Feiertage heute";
        u8g2.setCursor((canvas.width() - u8g2.getUTF8Width(text)) / 2, 30);
        u8g2.print(text);
        xSemaphoreGive(dataMutex);
        return;
    }

    time_t now = time(nullptr);
    time_t local_time = timeConverter.toLocal(now);
    struct tm tm_now;
    localtime_r(&local_time, &tm_now);

    char dateStr[32];
    strftime(dateStr, sizeof(dateStr), "%d. %B", &tm_now);

    u8g2.setFont(u8g2_font_helvB14_tf);
    u8g2.setForegroundColor(rgb565(255, 255, 0));
    int dateWidth = u8g2.getUTF8Width(dateStr);
    u8g2.setCursor((canvas.width() - dateWidth) / 2, 15);
    u8g2.print(dateStr);

    const auto& currentPageIndices = pageIndices[currentPage];
    int y = 25;
    int maxWidth = canvas.width() - 10;
    const int lineHeight = 10;
    const int entrySpacing = 8;

    u8g2.setFont(u8g2_font_5x8_tf);

    for (size_t i = 0; i < currentPageIndices.size(); ++i) {
        int idx = currentPageIndices[i];
        if (idx >= holidaysToday.size()) continue; 
        const HolidayEntry& entry = holidaysToday[idx];

        u8g2.setForegroundColor(0xFFFF);
        drawAndCountLines(u8g2, entry.name, 5, y, maxWidth, lineHeight, true, false);
        
        u8g2.setForegroundColor(rgb565(0, 255, 255));
        drawAndCountLines(u8g2, entry.description, 5, y, maxWidth, lineHeight, true, true);

        y += (entrySpacing / 2);

        if (i < currentPageIndices.size() - 1) {
            uint16_t lineColor = rgb565(128, 128, 128);
            for (int hx = 5; hx < canvas.width() - 5; hx += 2) {
                canvas.drawPixel(hx, y, lineColor);
            }
            y += (entrySpacing / 2);
        }
    }

    xSemaphoreGive(dataMutex);
}

void CuriousHolidaysModule::resetPaging() {
    currentPage = 0;
    _logicTicksSincePageSwitch = 0;
    _isFinished = false;
}

bool CuriousHolidaysModule::isEnabled() {
    bool enabled = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        enabled = !holidaysToday.empty();
        xSemaphoreGive(dataMutex);
    }
    return enabled;
}

void CuriousHolidaysModule::onActivate() {
    _logicTicksSincePageSwitch = 0;
    _currentTicksPerPage = pageDisplayDuration / 100;
    if (_currentTicksPerPage == 0) _currentTicksPerPage = 1;
}

void CuriousHolidaysModule::logicTick() {
    _logicTicksSincePageSwitch++;
    
    if (_logicTicksSincePageSwitch >= _currentTicksPerPage) {
        int totalPages = getTotalPages();
        if (totalPages > 0) {
            currentPage = (currentPage + 1) % totalPages;
            if (currentPage == 0) {
                _isFinished = true;
            } else {
                if (updateCallback) updateCallback();
            }
        }
        _logicTicksSincePageSwitch = 0;
    }
}

void CuriousHolidaysModule::onUpdate(std::function<void()> callback) {
    updateCallback = callback;
}