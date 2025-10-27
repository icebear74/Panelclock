#ifndef CALENDARMODULE_HPP
#define CALENDARMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <vector>
#include <algorithm>
#include <time.h>
#include <map>
#include <string>
#include <functional>
#include <math.h> 
#include "RRuleParser.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebClientModule.hpp"
#include "PsramUtils.hpp"
#include "certs.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct CalendarEvent {
  PsramString summary;
  time_t startEpoch;
  time_t duration;
  bool isAllDay;

  bool operator<(const time_t& t) const {
    return startEpoch < t;
  }
};

using PsramEventVector = std::vector<Event, PsramAllocator<Event>>;
using PsramTimeVector = std::vector<time_t, PsramAllocator<time_t>>;
using PsramEventPair = std::pair<time_t, Event>;
using PsramEventPairVector = std::vector<PsramEventPair, PsramAllocator<PsramEventPair>>;
using PsramCalendarEventVector = std::vector<CalendarEvent, PsramAllocator<CalendarEvent>>;

static uint16_t hexColorTo565(const PsramString& hex) {
  if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
  long r = strtol(hex.substr(1,2).c_str(), NULL, 16);
  long g = strtol(hex.substr(3,2).c_str(), NULL, 16);
  long b = strtol(hex.substr(5,2).c_str(), NULL, 16);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class CalendarModule {
public:
    CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient)
    : u8g2(u8g2), canvas(canvas), timeConverter(converter), webClient(webClient), last_processed_update(0) {
        dataMutex = xSemaphoreCreateMutex();
    }

    ~CalendarModule() {
        if (dataMutex) vSemaphoreDelete(dataMutex);
        if (pending_buffer) free(pending_buffer);
    }

    void onUpdate(std::function<void()> callback) { updateCallback = callback; }
    void setICSUrl(const PsramString &url) {
        this->icsUrl = url;
        if (!icsUrl.empty()) webClient->registerResource(String(icsUrl.c_str()), fetchIntervalMinutes, root_ca_pem);
    }
    void setFetchIntervalMinutes(uint32_t minutes) { fetchIntervalMinutes = minutes > 0 ? minutes : 60; }
    
    void queueData() {
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

    void processData() {
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

    void tickScroll() {
        if (scrollStepInterval == 0) return;
        unsigned long now = millis();
        if (now - lastScrollStep >= scrollStepInterval) {
            lastScrollStep = now;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                for (auto& s : scrollPos) {
                    if (s.maxScroll > 0) {
                        s.offset = (s.offset + 1) % s.maxScroll;
                    }
                }
                xSemaphoreGive(dataMutex);
            }
        }
    }

    void draw() {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

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

        const float pulse_period_ms = 2000.0f;
        const float min_brightness = 0.25f;
        float cos_input = (millis() % (int)pulse_period_ms) / pulse_period_ms * 2.0f * PI;
        float pulseFactor = min_brightness + (1.0f - min_brightness) * (cos(cos_input) + 1.0f) / 2.0f;

        time_t now_utc;
        time(&now_utc);
        
        // *** FINALE KORREKTUR: Sichere, UTC-basierte Berechnung der Tagesgrenzen ***
        time_t local_now = timeConverter.toLocal(now_utc);
        struct tm tm_local_now;
        localtime_r(&local_now, &tm_local_now);
        tm_local_now.tm_hour = 0; tm_local_now.tm_min = 0; tm_local_now.tm_sec = 0;
        // Konvertiere den Anfang des lokalen Tages zurück nach UTC, um eine UTC-Tagesgrenze zu haben
        time_t today_start_local_epoch = mktime(&tm_local_now);
        time_t today_start_utc = today_start_local_epoch - (timeConverter.isDST(today_start_local_epoch) ? dstOffsetSec : stdOffsetSec);
        time_t tomorrow_start_utc = today_start_utc + 86400;

        int maxSummaryPixel = canvas.width() - xTermin - 2;
        ensureScrollPos(upcomming, maxSummaryPixel);

        for (size_t i = 0; i < upcomming.size(); ++i) {
            const auto& ev = upcomming[i];
            struct tm tStart;
            time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
            localtime_r(&localStartEpoch, &tStart);
            char buf[12];
            
            uint16_t currentTextColor = textColor;
            uint16_t currentDateColor = dateColor;
            
            // *** FINALE KORREKTUR: Robuste Prüfung, ob ein Termin heute aktiv ist, basierend auf UTC-Grenzen ***
            bool isToday = (ev.startEpoch < tomorrow_start_utc) && ((ev.startEpoch + ev.duration) > today_start_utc);

            if (isToday) {
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
                    localtime_r(&localEndEpoch, &tEnd);
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
        xSemaphoreGive(dataMutex);
    }

    void setScrollStepInterval(uint32_t ms) { scrollStepInterval = ms; resetScroll(); }
    uint32_t getScrollStepInterval() const { return scrollStepInterval; }
    void setColors(const PsramString& dateColorHex, const PsramString& textColorHex) {
        dateColor = hexColorTo565(dateColorHex);
        textColor = hexColorTo565(textColorHex);
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    const GeneralTimeConverter& timeConverter;
    WebClientModule* webClient;
    std::function<void()> updateCallback;
    PsramString icsUrl;
    uint32_t fetchIntervalMinutes = 60;
    PsramCalendarEventVector events;
    PsramCalendarEventVector raw_events;
    
    struct ScrollState { 
        int offset = 0; 
        int maxScroll = 0; 
    };
    std::vector<ScrollState, PsramAllocator<ScrollState>> scrollPos;
    unsigned long lastScrollStep = 0;
    uint32_t scrollStepInterval = 250;

    uint16_t dateColor = 0xFFE0;
    uint16_t textColor = 0xFFFF;
    time_t last_processed_update;
    SemaphoreHandle_t dataMutex;

    char* pending_buffer = nullptr;
    size_t buffer_size = 0;
    volatile bool data_pending = false;

    // *** Diese Member sind nötig, um die Tagesgrenzen zu berechnen, da wir nicht direkt auf die privaten Member des timeConverters zugreifen können ***
    int stdOffsetSec = 0;
    int dstOffsetSec = 0;

    uint16_t dimColor(uint16_t color, float brightness) {
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5) & 0x3F;
        uint8_t b = color & 0x1F;
        
        r = (uint8_t)(r * brightness);
        g = (uint8_t)(g * brightness);
        b = (uint8_t)(b * brightness);
        
        return (r << 11) | (g << 5) | b;
    }

    void parseICS(char* icsBuffer, size_t size) {
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

    void onSuccessfulUpdate() {
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

    void addSingleEvent(const Event& ev) {
        if (ev.dtstart == 0) return;
        
        CalendarEvent ce;
        ce.summary = ev.summary.c_str();
        ce.startEpoch = ev.dtstart;
        ce.duration = ev.duration;
        ce.isAllDay = ev.isAllDay;
        
        raw_events.push_back(ce);
    }

    PsramCalendarEventVector getUpcomingEvents(int maxCount) {
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

    PsramString fitTextToPixelWidth(const char* text, int maxPixel) {
        int lastOk = 0;
        PsramString p_text(text);
        for (int i = 1; i <= (int)p_text.length(); ++i) {
            if (u8g2.getUTF8Width(p_text.substr(0, i).c_str()) <= (maxPixel - 1)) lastOk = i;
            else break;
        }
        return p_text.substr(0, lastOk);
    }
    
    void resetScroll() { scrollPos.clear(); }
    void ensureScrollPos(const PsramCalendarEventVector& upcomming, int maxTextPixel) {
        if (scrollPos.size() != upcomming.size()) {
            scrollPos.assign(upcomming.size(), ScrollState());
        }
    }
};
#endif