#include "CalendarModule.hpp"

uint16_t hexColorTo565(const PsramString& hex) {
  if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
  long r = strtol(hex.substr(1,2).c_str(), NULL, 16);
  long g = strtol(hex.substr(3,2).c_str(), NULL, 16);
  long b = strtol(hex.substr(5,2).c_str(), NULL, 16);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

CalendarModule::CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient)
    : u8g2(u8g2), canvas(canvas), timeConverter(converter), webClient(webClient), last_processed_update(0) {
    dataMutex = xSemaphoreCreateMutex();
    
    // Standardwerte für konfigurierbare Parameter setzen
    highlightThresholdMinutes = 120;
    urgentViewThresholdMinutes = 60;
    urgentViewDurationMs = 20 * 1000UL;
    urgentViewIntervalMs = 2 * 60 * 1000UL;
}

CalendarModule::~CalendarModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
    if (pending_buffer) free(pending_buffer);
}

void CalendarModule::onUpdate(std::function<void()> callback) { 
    updateCallback = callback; 
}

// =================================================================
// KORREKTUR: Vereinfachte Implementierung ohne Prüfung auf 0
// =================================================================
void CalendarModule::setConfig(const PsramString& url, unsigned long fetchMinutes, unsigned long displaySec, unsigned long scrollMs, 
                               const PsramString& dateColorHex, const PsramString& textColorHex,
                               uint16_t highlightMin, uint16_t urgentMin, uint16_t urgentDurationSec, uint16_t urgentIntervalMin) {
    this->icsUrl = url;
    this->_isEnabled = !url.empty();
    
    this->fetchIntervalMinutes = fetchMinutes > 0 ? fetchMinutes : 60;
    this->_displayDuration = displaySec > 0 ? displaySec * 1000UL : 30000;
    this->scrollStepInterval = scrollMs > 0 ? scrollMs : 250;
    
    this->dateColor = hexColorTo565(dateColorHex);
    this->textColor = hexColorTo565(textColorHex);

    // Direkte Zuweisung der (optionalen) Parameter
    this->highlightThresholdMinutes = highlightMin;
    this->urgentViewThresholdMinutes = urgentMin;
    this->urgentViewDurationMs = urgentDurationSec * 1000UL;
    this->urgentViewIntervalMs = urgentIntervalMin * 60 * 1000UL;

    this->maxRuntimeMs = _displayDuration + FAILSAFE_BUFFER_MS;

    if (_isEnabled) {
        webClient->registerResource(String(icsUrl.c_str()), this->fetchIntervalMinutes, root_ca_pem);
    }
}

void CalendarModule::onActivate() {
    _startTime = millis();
    lastScrollStep = _startTime;
    resetScroll();
}

void CalendarModule::tick() {
    unsigned long now = millis();

    if (now - _startTime > _displayDuration) {
        _isFinished = true;
        return;
    }

    if (scrollStepInterval > 0 && now - lastScrollStep >= scrollStepInterval) {
        lastScrollStep = now;
        bool scrolled = false;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            for (auto& s : scrollPos) {
                if (s.maxScroll > 0) {
                    s.offset = (s.offset + 1) % s.maxScroll;
                    scrolled = true;
                }
            }
            xSemaphoreGive(dataMutex);
        }
        if (scrolled && updateCallback) {
            updateCallback();
        }
    }
}

void CalendarModule::periodicTick() {
    if (!_isEnabled) return;
    
    unsigned long now = millis();
    if (now - _lastPeriodicCheck < 1000) return;
    _lastPeriodicCheck = now;

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) != pdTRUE) return;
    
    time_t now_utc;
    time(&now_utc);

    bool isAnyEventUrgent = false;
    for (const auto& ev : events) {
        if (ev.isAllDay) continue;
        if (ev.startEpoch > now_utc) {
            if ((ev.startEpoch - now_utc) < (urgentViewThresholdMinutes * 60)) {
                isAnyEventUrgent = true;
            }
            break; 
        }
    }

    if (isAnyEventUrgent && !_isUrgentViewActive && (now - _lastUrgentDisplayTime > urgentViewIntervalMs)) {
        _isUrgentViewActive = true;
        _priorityRequested = true;
        _urgentViewStartTime = now;
        _lastUrgentDisplayTime = now;
        requestPriority();
        Serial.println("[Calendar] Dringender Termin Intervall erreicht. Fordere Priorität an.");
    } 
    else if (_isUrgentViewActive && (now - _urgentViewStartTime > urgentViewDurationMs)) {
        _isUrgentViewActive = false;
        _priorityRequested = false;
        releasePriority();
        Serial.println("[Calendar] Anzeigedauer für dringenden Termin abgelaufen. Gebe Priorität frei.");
    }
    else if (!isAnyEventUrgent && _isUrgentViewActive) {
        _isUrgentViewActive = false;
        _priorityRequested = false;
        releasePriority();
        Serial.println("[Calendar] Kein dringender Termin mehr vorhanden. Gebe Priorität frei.");
    }

    xSemaphoreGive(dataMutex);
}

bool CalendarModule::isEnabled() {
    return _isEnabled;
}

void CalendarModule::queueData() {
    if (icsUrl.empty() || !webClient) return;
    webClient->accessResource(String(icsUrl.c_str()), [this](const char* buffer, size_t size, time_t last_update, bool is_stale){
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

void CalendarModule::processData() {
    if (data_pending) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            this->parseICS(pending_buffer, buffer_size);
            this->onSuccessfulUpdate();
            free(pending_buffer);
            pending_buffer = nullptr;
            data_pending = false;
            xSemaphoreGive(dataMutex);
            if (this->updateCallback) this->updateCallback();
        }
    }
}

void CalendarModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    if (_isUrgentViewActive) {
        drawUrgentView();
    } else {
        canvas.fillScreen(0);
        u8g2.begin(canvas);
        const uint8_t* font = u8g2_font_5x8_tf;
        u8g2.setFont(font);
        int fontH = 8, y = fontH + 1;
        
        int xStart = 2, xEndZeit = 44, xTermin = 88;

        u8g2.setForegroundColor(dateColor);
        u8g2.setCursor(xStart, y); u8g2.print("Start");
        u8g2.setCursor(xEndZeit, y); u8g2.print("Ende/Zeit");
        u8g2.setCursor(xTermin, y); u8g2.print("Termin");
        y += fontH;
        
        auto upcomming = getUpcomingEvents(6);
        if (upcomming.empty()) {
            u8g2.setForegroundColor(textColor);
            u8g2.setCursor(2, y); u8g2.print("Keine Termine");
            xSemaphoreGive(dataMutex);
            return;
        }

        time_t now_utc;
        time(&now_utc);
        
        time_t now_local = timeConverter.toLocal(now_utc);
        struct tm tm_local_now;
        gmtime_r(&now_local, &tm_local_now);
        tm_local_now.tm_hour = 0; tm_local_now.tm_min = 0; tm_local_now.tm_sec = 0;
        
        time_t today_start_local_epoch = timegm(&tm_local_now);
        
        int currentOffset = timeConverter.isDST(today_start_local_epoch) ? timeConverter.getDstOffsetSec() : timeConverter.getStdOffsetSec();
        time_t today_start_utc = today_start_local_epoch - currentOffset;
        time_t tomorrow_start_utc = today_start_utc + 86400;

        int maxSummaryPixel = canvas.width() - xTermin - 2;
        ensureScrollPos(upcomming, maxSummaryPixel);

        for (size_t i = 0; i < upcomming.size(); ++i) {
            const auto& ev = upcomming[i];
            struct tm tStart;
            time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
            gmtime_r(&localStartEpoch, &tStart);
            char buf[12];
            
            uint16_t currentTextColor = textColor;
            uint16_t currentDateColor = dateColor;
            
            bool isToday = (ev.startEpoch < tomorrow_start_utc) && ((ev.startEpoch + ev.duration) > today_start_utc);
            long minutesUntilStart = (ev.startEpoch > now_utc) ? (ev.startEpoch - now_utc) / 60 : -1;

            if (isToday) {
                float pulse_period_ms = (!ev.isAllDay && minutesUntilStart >= 0 && minutesUntilStart < highlightThresholdMinutes) ? 1000.0f : 2000.0f;
                const float min_brightness = 0.25f;
                float cos_input = (millis() % (int)pulse_period_ms) / pulse_period_ms * 2.0f * PI;
                float pulseFactor = min_brightness + (1.0f - min_brightness) * (cos(cos_input) + 1.0f) / 2.0f;
                
                currentTextColor = dimColor(textColor, pulseFactor);
                currentDateColor = dimColor(dateColor, pulseFactor);
            }
            
            u8g2.setForegroundColor(currentDateColor);
            strftime(buf, sizeof(buf), "%d.%m.%y", &tStart);
            u8g2.setCursor(xStart, y); u8g2.print(buf);

            if (ev.isAllDay) {
                long days = (ev.duration + 43200) / 86400;
                if (days > 1) {
                    struct tm tEnd;
                    time_t end_date_epoch = ev.startEpoch + ev.duration - 86400;
                    time_t localEndEpoch = timeConverter.toLocal(end_date_epoch);
                    gmtime_r(&localEndEpoch, &tEnd);
                    strftime(buf, sizeof(buf), "%d.%m.%y", &tEnd);
                    u8g2.setCursor(xEndZeit, y); u8g2.print(buf);
                }
            } else {
                strftime(buf, sizeof(buf), "%H:%M", &tStart);
                u8g2.setCursor(xEndZeit, y); u8g2.print(buf);
            }

            u8g2.setForegroundColor(currentTextColor);
            
            const char* src_cstr = ev.summary.c_str();
            int src_len = strlen(src_cstr);
            PsramString visiblePart = fitTextToPixelWidth(src_cstr, maxSummaryPixel);
            int visibleChars = visiblePart.length();
            
            size_t idx = i < scrollPos.size() ? i : 0;
            
            if (src_len > visibleChars) {
                PsramString scrollText = PsramString(src_cstr) + "     ";
                scrollPos[idx].maxScroll = scrollText.length();
                int offset = scrollPos[idx].offset;
                
                PsramString part_to_display = "";
                for(int k=0; k < visibleChars; ++k) {
                    part_to_display += scrollText[(offset + k) % scrollText.length()];
                }
                u8g2.setCursor(xTermin, y);
                u8g2.print(part_to_display.c_str());

            } else {
                scrollPos[idx].maxScroll = 0;
                scrollPos[idx].offset = 0;
                u8g2.setCursor(xTermin, y);
                u8g2.print(src_cstr);
            }
            y += fontH;
        }
    }
    xSemaphoreGive(dataMutex);
}

void CalendarModule::drawUrgentView() {
    canvas.fillScreen(0);
    u8g2.begin(canvas);

    time_t now_utc;
    time(&now_utc);

    PsramCalendarEventVector urgentEvents;
    for (const auto& ev : events) {
        if (ev.isAllDay) continue;
        if (ev.startEpoch > now_utc) {
            if ((ev.startEpoch - now_utc) < (urgentViewThresholdMinutes * 60)) {
                urgentEvents.push_back(ev);
                if (urgentEvents.size() >= 2) {
                    break;
                }
            }
        }
    }

    if (urgentEvents.empty()) {
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setForegroundColor(textColor);
        u8g2.setCursor(2, 20); u8g2.print("Kein dringender Termin");
        return;
    }

    int y = 12; 
    for (size_t i = 0; i < urgentEvents.size(); ++i) {
        const auto& ev = urgentEvents[i];
        
        u8g2.setFont(u8g2_font_7x14_tf);
        u8g2.setForegroundColor(dateColor);

        struct tm tStart, tEnd;
        time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
        time_t localEndEpoch = timeConverter.toLocal(ev.startEpoch + ev.duration);
        gmtime_r(&localStartEpoch, &tStart);
        gmtime_r(&localEndEpoch, &tEnd);

        char dateTimeStr[40];
        char endTimeStr[6];
        strftime(endTimeStr, sizeof(endTimeStr), "%H:%M", &tEnd);

        if (tStart.tm_year == tEnd.tm_year && tStart.tm_yday == tEnd.tm_yday) {
             strftime(dateTimeStr, sizeof(dateTimeStr), "%d.%m. %H:%M - ", &tStart);
        } else {
             strftime(dateTimeStr, sizeof(dateTimeStr), "%d.%m. %H:%M - %d.%m. ", &tStart);
        }
        
        PsramString finalDateTimeStr = dateTimeStr;
        finalDateTimeStr += endTimeStr;
        
        int width = u8g2.getUTF8Width(finalDateTimeStr.c_str());
        u8g2.setCursor((canvas.width() - width) / 2, y);
        u8g2.print(finalDateTimeStr.c_str());
        y += 12;

        y += 5;

        u8g2.setFont(u8g2_font_helvB12_tf);
        
        u8g2.setForegroundColor(textColor);
        width = u8g2.getUTF8Width(ev.summary.c_str());
        u8g2.setCursor((canvas.width() - width) / 2, y);
        u8g2.print(ev.summary.c_str());
        y += 16;
    }
}

uint32_t CalendarModule::getScrollStepInterval() const { 
    return scrollStepInterval; 
}

uint16_t CalendarModule::dimColor(uint16_t color, float brightness) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    
    return (r << 11) | (g << 5) | b;
}

void CalendarModule::parseICS(char* icsBuffer, size_t size) {
    if (!icsBuffer || size == 0) return;
    PsramString ics(icsBuffer, size);
    
    raw_events.clear();
    raw_events.reserve(1024);

    PsramEventVector parsedEvents;
    parsedEvents.reserve(512);
    size_t idx = 0;
    const PsramString beginTag("BEGIN:VEVENT"), endTag("END:VEVENT");
    while (true) {
        size_t pos = ics.find(beginTag, idx); if (pos == PsramString::npos) break;
        size_t endPos = ics.find(endTag, pos); if (endPos == PsramString::npos) break;
        PsramString veventBlock = ics.substr(pos, (endPos + endTag.length()) - pos);
        Event parsedEvent;
        parseVEvent(veventBlock.c_str(), veventBlock.length(), parsedEvent);
        if(parsedEvent.dtstart > 0) parsedEvents.push_back(std::move(parsedEvent));
        idx = endPos + endTag.length();
        if (parsedEvents.size() % 50 == 0) delay(1);
    }
    if (parsedEvents.empty()) return;
    
    std::sort(parsedEvents.begin(), parsedEvents.end(), [](const Event& a, const Event& b){ return a.uid < b.uid; });
    delay(1);
    
    size_t i = 0;
    while (i < parsedEvents.size()) {
        if (i > 0 && i % 50 == 0) delay(1);
        size_t j = i;
        while (j < parsedEvents.size() && parsedEvents[j].uid == parsedEvents[i].uid) j++;
        
        Event masterEvent;
        bool masterFound = false;
        PsramEventVector exceptions;
        PsramEventVector singleEvents;

        for(size_t k = i; k < j; ++k) {
            if(!parsedEvents[k].rrule.empty()) {
                masterEvent = parsedEvents[k];
                masterFound = true;
            } else if(parsedEvents[k].recurrence_id != 0) {
                exceptions.push_back(parsedEvents[k]);
            } else {
                singleEvents.push_back(parsedEvents[k]);
            }
        }

        if (!masterFound) {
            for(const auto& single : singleEvents) addSingleEvent(single);
        } else {
            PsramTimeVector occurrences;
            occurrences.reserve(128);
            parseRRule(masterEvent, occurrences);
            
            PsramEventPairVector final_series_vec;
            final_series_vec.reserve(occurrences.size() + exceptions.size());
            for (time_t start : occurrences) {
                Event series_event = masterEvent;
                series_event.dtstart = start;
                final_series_vec.emplace_back(start, std::move(series_event));
            }
            for (const auto& ex : exceptions) {
                bool replaced = false;
                for (auto &pr : final_series_vec) {
                    if (pr.first == ex.recurrence_id) { pr.second = ex; replaced = true; break; }
                }
                if (!replaced) final_series_vec.emplace_back((ex.recurrence_id != 0) ? ex.recurrence_id : ex.dtstart, ex);
            }
            std::sort(final_series_vec.begin(), final_series_vec.end(), [](const PsramEventPair& a, const PsramEventPair& b){ return a.first < b.first; });
            final_series_vec.erase(std::unique(final_series_vec.begin(), final_series_vec.end(), [](const PsramEventPair& a, const PsramEventPair& b){ return a.first == b.first; }), final_series_vec.end());
            for (auto const& pr : final_series_vec) addSingleEvent(pr.second);
        }
        i = j;
    }
}

void CalendarModule::onSuccessfulUpdate() {
    if (raw_events.empty()) {
        events.clear();
        resetScroll();
        return;
    }
    
    std::sort(raw_events.begin(), raw_events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
        return a.startEpoch < b.startEpoch;
    });
    
    events = raw_events;
    resetScroll();
}

void CalendarModule::addSingleEvent(const Event& ev) {
    if (ev.dtstart == 0) return;
    
    CalendarEvent ce;
    ce.summary = ev.summary.c_str();
    ce.startEpoch = ev.dtstart;
    ce.duration = ev.duration;
    ce.isAllDay = ev.isAllDay;
    
    raw_events.push_back(ce);
}

PsramCalendarEventVector CalendarModule::getUpcomingEvents(int maxCount) {
    PsramCalendarEventVector result;
    result.reserve(maxCount);
    
    time_t now_utc;
    time(&now_utc);

    auto it = std::lower_bound(events.begin(), events.end(), now_utc - (7 * 86400));

    for (; it != events.end(); ++it) {
        if (it->startEpoch + it->duration > now_utc) {
            result.push_back(*it);
        }
        if (result.size() >= (size_t)maxCount) {
            break;
        }
    }
    return result;
}

PsramString CalendarModule::fitTextToPixelWidth(const char* text, int maxPixel) {
    int lastOk = 0;
    PsramString p_text(text);
    for (int i = 1; i <= (int)p_text.length(); ++i) {
        if (u8g2.getUTF8Width(p_text.substr(0, i).c_str()) <= (maxPixel - 1)) lastOk = i;
        else break;
    }
    return p_text.substr(0, lastOk);
}

void CalendarModule::resetScroll() { 
    scrollPos.clear(); 
}

void CalendarModule::ensureScrollPos(const PsramCalendarEventVector& upcomming, int maxTextPixel) {
    if (scrollPos.size() != upcomming.size()) {
        scrollPos.assign(upcomming.size(), ScrollState());
    }
}