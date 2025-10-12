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
#include "RRuleParser.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebClientModule.hpp"
#include "PsramUtils.hpp"
#include "certs.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct Event;

struct CalendarEvent {
  PsramString summary;
  time_t startEpoch;
  time_t endEpoch;
  bool isAllDay;
};

// --- NEU: Definition von Containern, die den PsramAllocator verwenden ---
using PsramEventVector = std::vector<Event, PsramAllocator<Event>>;
using PsramTimeVector = std::vector<time_t, PsramAllocator<time_t>>;
using PsramEventPair = std::pair<time_t, Event>;
using PsramEventPairVector = std::vector<PsramEventPair, PsramAllocator<PsramEventPair>>;
using PsramCalendarEventVector = std::vector<CalendarEvent, PsramAllocator<CalendarEvent>>;


static inline void printMemoryStats(const char* stage, int counter = -1) {
    // This function can be re-enabled for debugging if needed
}

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

    void onUpdate(std::function<void()> callback) {
        updateCallback = callback;
    }

    void setICSUrl(const PsramString &url) {
        this->icsUrl = url;
        if (!icsUrl.empty()) {
            webClient->registerResource(String(icsUrl.c_str()), fetchIntervalMinutes, root_ca_pem);
        }
    }
    PsramString getICSUrl() const { return icsUrl; }
    void setFetchIntervalMinutes(uint32_t minutes) { fetchIntervalMinutes = minutes > 0 ? minutes : 60; }
    uint32_t getFetchIntervalMinutes() const { return fetchIntervalMinutes; }
    
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
                Serial.println("[CalendarModule] Neue Kalender-Daten gefunden, starte Parsing...");
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
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                lastScrollStep = now;
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
        u8g2.setForegroundColor(dateColor);
        u8g2.setCursor(2, y);
        u8g2.print("Start   Ende   Zeit   Termin");
        y += fontH;
        auto upcomming = getUpcomingEvents(6);
        if (upcomming.empty()) {
            u8g2.setForegroundColor(textColor);
            u8g2.setCursor(2, y);
            u8g2.print("Keine Termine");
            xSemaphoreGive(dataMutex);
            return;
        }
        int xStart = 2, xEnd = 48, xTime = 74, xSummary = 110;
        int maxSummaryPixel = canvas.width() - xSummary - 2;
        ensureScrollPos(upcomming, maxSummaryPixel);
        for (size_t i = 0; i < upcomming.size(); ++i) {
            const auto& ev = upcomming[i];
            struct tm tStart, tEnd;
            time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
            time_t displayEndEpoch = timeConverter.toLocal(ev.endEpoch);
            localtime_r(&localStartEpoch, &tStart);
            localtime_r(&displayEndEpoch, &tEnd);
            char buf[12];
            strftime(buf, sizeof(buf), "%d.%m.%y", &tStart);
            u8g2.setForegroundColor(dateColor);
            u8g2.setCursor(xStart, y);
            u8g2.print(buf);
            bool isMultiDay = (tEnd.tm_yday != tStart.tm_yday || tEnd.tm_year != tStart.tm_year);
            if (isMultiDay) {
                strftime(buf, sizeof(buf), "%d.%m.%y", &tEnd);
                u8g2.setCursor(xEnd, y);
                u8g2.print(buf);
            }
            u8g2.setCursor(xTime, y);
            if (isMultiDay) { u8g2.print("     "); }
            else if (!ev.isAllDay) { strftime(buf, sizeof(buf), "%H:%M", &tStart); u8g2.print(buf); }
            else { u8g2.print(" ganzt."); }
            u8g2.setForegroundColor(textColor);
            u8g2.setCursor(xSummary, y);

            const char* shownText = ev.summary.c_str();
            PsramString visiblePart = fitTextToPixelWidth(shownText, maxSummaryPixel - 1);
            int visibleChars = visiblePart.length();
            PsramString pad("   ");
            PsramString src(shownText);
            PsramString scrollText = src + pad + src.substr(0, visibleChars);
            int maxScroll = (src.length() > visibleChars) ? (scrollText.length() - visibleChars + pad.length()) : 0;
            size_t idx = i < scrollPos.size() ? i : 0;
            scrollPos[idx].maxScroll = maxScroll;
            int offset = scrollPos[idx].offset;
            if (offset >= maxScroll) offset = 0;
            scrollPos[idx].offset = offset;
            PsramString part = scrollText.substr(offset, visibleChars);
            u8g2.print(part.c_str());
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
    struct ScrollState { int offset = 0; int maxScroll = 0; };
    std::vector<ScrollState, PsramAllocator<ScrollState>> scrollPos;
    unsigned long lastScrollStep = 0;
    uint32_t scrollStepInterval = 150;
    uint16_t dateColor = 0xFFE0;
    uint16_t textColor = 0xFFFF;
    time_t last_processed_update;
    SemaphoreHandle_t dataMutex;

    char* pending_buffer = nullptr;
    size_t buffer_size = 0;
    volatile bool data_pending = false;

    void parseICS(char* icsBuffer, size_t size) {
        if (!icsBuffer || size == 0) return;

        PsramString ics(icsBuffer, size);
        events.clear();
        
        PsramEventVector parsedEvents;
        parsedEvents.reserve(256);

        size_t idx = 0;
        const PsramString beginTag("BEGIN:VEVENT");
        const PsramString endTag("END:VEVENT");

        while (true) {
            size_t pos = ics.find(beginTag, idx);
            if (pos == PsramString::npos) break;
            size_t endPos = ics.find(endTag, pos);
            if (endPos == PsramString::npos) break;
            size_t blockLen = (endPos + endTag.length()) - pos;
            PsramString veventBlock = ics.substr(pos, blockLen);
            Event parsedEvent;
            parseVEvent(veventBlock.c_str(), veventBlock.length(), parsedEvent);
            parsedEvents.push_back(std::move(parsedEvent));
            idx = endPos + endTag.length();
        }

        if (parsedEvents.empty()) return;

        std::sort(parsedEvents.begin(), parsedEvents.end(), [](const Event& a, const Event& b){
            return a.uid < b.uid;
        });

        size_t i = 0;
        while (i < parsedEvents.size()) {
            size_t j = i + 1;
            while (j < parsedEvents.size() && parsedEvents[j].uid == parsedEvents[i].uid) ++j;
            Event masterEvent;
            
            PsramEventVector exceptions;
            exceptions.reserve(j - i);

            for (size_t k = i; k < j; ++k) {
                const Event& ev = parsedEvents[k];
                if (!ev.rrule.empty()) masterEvent = ev;
                else exceptions.push_back(ev);
            }
            if (masterEvent.rrule.empty()) {
                for (const auto& singleEvent : exceptions) addSingleEvent(singleEvent);
            } else {
                PsramTimeVector occurrences;
                occurrences.reserve(64);
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
                        if (pr.first == ex.recurrence_id) {
                            pr.second = ex;
                            replaced = true;
                            break;
                        }
                    }
                    if (!replaced) {
                        time_t key = (ex.recurrence_id != 0) ? ex.recurrence_id : ex.dtstart;
                        final_series_vec.emplace_back(key, ex);
                    }
                }
                std::sort(final_series_vec.begin(), final_series_vec.end(), [](const PsramEventPair& a, const PsramEventPair& b){ return a.first < b.first; });
                final_series_vec.erase(std::unique(final_series_vec.begin(), final_series_vec.end(),
                    [](const PsramEventPair& a, const PsramEventPair& b){ return a.first == b.first; }), final_series_vec.end());
                for (auto const& pr : final_series_vec) addSingleEvent(pr.second);
            }
            i = j;
        }
    }

    void onSuccessfulUpdate() {
        if (events.empty()) {
            resetScroll();
            return;
        }
        std::sort(events.begin(), events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
            if (a.startEpoch != b.startEpoch) return a.startEpoch < b.startEpoch;
            return a.summary < b.summary;
        });

        PsramCalendarEventVector mergedEvents;
        mergedEvents.reserve(events.size());
        if (!events.empty()) mergedEvents.push_back(events[0]);
        for (size_t i = 1; i < events.size(); ++i) {
            CalendarEvent& lastMerged = mergedEvents.back();
            CalendarEvent& current = events[i];
            if (current.isAllDay && lastMerged.isAllDay &&
                current.summary == lastMerged.summary &&
                current.startEpoch == (lastMerged.endEpoch + 1)) {
                lastMerged.endEpoch = current.endEpoch;
            } else {
                mergedEvents.push_back(current);
            }
        }
        events.assign(mergedEvents.begin(), mergedEvents.end());
        resetScroll();
    }

    void onFailedUpdate(int httpCode) {
        events.clear();
        CalendarEvent err;
        if (httpCode == -1) { err.summary = "Keine URL/WLAN"; }
        else {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "ICS HTTP: %d", httpCode);
            err.summary = tmp;
        }
        time_t now; time(&now);
        err.startEpoch = now > 1609459200 ? now + 1 : 0;
        err.endEpoch = err.startEpoch;
        err.isAllDay = false;
        events.push_back(err);
        resetScroll();
    }

    void addSingleEvent(const Event& ev) {
        if (ev.dtstart == 0) return;
        time_t duration = (ev.dtend > ev.dtstart) ? (ev.dtend - ev.dtstart) : (ev.isAllDay ? 86400 : 0);
        CalendarEvent ce;
        ce.summary = ev.summary.c_str();
        ce.startEpoch = ev.dtstart;
        ce.endEpoch = ev.dtstart + duration;
        if (ce.isAllDay && duration == 86400) {
            ce.endEpoch -= 1;
        }
        ce.isAllDay = ev.isAllDay;
        events.push_back(ce);
    }

    PsramCalendarEventVector getUpcomingEvents(int maxCount) {
        PsramCalendarEventVector result;
        result.reserve(maxCount);
        time_t now_utc; time(&now_utc);
        for (const auto& ev : events) {
            if (ev.endEpoch > now_utc) {
                result.push_back(ev);
            }
            if ((int)result.size() >= maxCount) break;
        }
        return result;
    }

    PsramString fitTextToPixelWidth(const char* text, int maxPixel) {
        int lastOk = 0;
        int len = strlen(text);
        PsramString p_text(text);
        for (int i = 1; i <= len; ++i) {
            if (u8g2.getUTF8Width(p_text.substr(0, i).c_str()) <= (maxPixel - 1)) {
                lastOk = i;
            } else { break; }
        }
        return p_text.substr(0, lastOk);
    }

    void resetScroll() { scrollPos.clear(); }
    void ensureScrollPos(const PsramCalendarEventVector& upcomming, int maxTextPixel) {
        if (scrollPos.size() != upcomming.size()) {
            scrollPos.resize(upcomming.size());
        }
    }
};

#endif