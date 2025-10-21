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

using PsramEventVector = std::vector<Event, PsramAllocator<Event>>;
using PsramTimeVector = std::vector<time_t, PsramAllocator<time_t>>;
using PsramEventPair = std::pair<time_t, Event>;
using PsramEventPairVector = std::vector<PsramEventPair, PsramAllocator<PsramEventPair>>;
using PsramCalendarEventVector = std::vector<CalendarEvent, PsramAllocator<CalendarEvent>>;
using PsramOccurrenceVector = std::vector<Occurrence, PsramAllocator<Occurrence>>;

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
                for (auto& s : scrollPos) if (s.maxScroll > 0) s.offset = (s.offset + 1) % s.maxScroll;
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
        u8g2.setCursor(xStart, y);
        u8g2.print("Start");
        u8g2.setCursor(xEndZeit, y);
        u8g2.print("Ende/Zeit");
        u8g2.setCursor(xTermin, y);
        u8g2.print("Termin");
        y += fontH;
        
        auto upcomming = getUpcomingEvents(6);
        if (upcomming.empty()) {
            u8g2.setForegroundColor(textColor);
            u8g2.setCursor(2, y);
            u8g2.print("Keine Termine");
            xSemaphoreGive(dataMutex);
            return;
        }

        int maxSummaryPixel = canvas.width() - xTermin - 2;
        ensureScrollPos(upcomming, maxSummaryPixel);
        for (size_t i = 0; i < upcomming.size(); ++i) {
            const auto& ev = upcomming[i];
            struct tm tStart, tEnd;
            
            time_t localStartEpoch = timeConverter.toLocal(ev.startEpoch);
            time_t localEndEpoch = timeConverter.toLocal(ev.endEpoch);

            localtime_r(&localStartEpoch, &tStart);
            localtime_r(&localEndEpoch, &tEnd);
            char buf[12];
            
            u8g2.setForegroundColor(dateColor);
            
            strftime(buf, sizeof(buf), "%d.%m.%y", &tStart);
            u8g2.setCursor(xStart, y);
            u8g2.print(buf);

            // ====================================================================================
            // Display logic for different event types
            // ====================================================================================
            if (ev.isAllDay && ev.startEpoch == ev.endEpoch) {
                // Single-day all-day event, "Ende/Zeit" column remains empty.
            } else if (ev.isAllDay) {
                // Multi-day all-day event, show end date.
                strftime(buf, sizeof(buf), "%d.%m.%y", &tEnd);
                u8g2.setCursor(xEndZeit, y);
                u8g2.print(buf);
            } else {
                // Timed event, show start time.
                strftime(buf, sizeof(buf), "%H:%M", &tStart);
                u8g2.setCursor(xEndZeit, y);
                u8g2.print(buf);
            }

            u8g2.setForegroundColor(textColor);
            u8g2.setCursor(xTermin, y);

            const char* shownText = ev.summary.c_str();
            PsramString visiblePart = fitTextToPixelWidth(shownText, maxSummaryPixel);
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
    PsramCalendarEventVector raw_events;
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
        raw_events.clear();
        PsramEventVector parsedEvents;
        parsedEvents.reserve(256);
        size_t idx = 0;
        const PsramString beginTag("BEGIN:VEVENT"), endTag("END:VEVENT");
        while (true) {
            size_t pos = ics.find(beginTag, idx); if (pos == PsramString::npos) break;
            size_t endPos = ics.find(endTag, pos); if (endPos == PsramString::npos) break;
            PsramString veventBlock = ics.substr(pos, (endPos + endTag.length()) - pos);
            Event parsedEvent;
            parseVEvent(veventBlock.c_str(), veventBlock.length(), parsedEvent);
            parsedEvents.push_back(std::move(parsedEvent));
            idx = endPos + endTag.length();
        }
        if (parsedEvents.empty()) return;
        std::sort(parsedEvents.begin(), parsedEvents.end(), [](const Event& a, const Event& b){ return a.uid < b.uid; });
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
                PsramOccurrenceVector occurrences;
                occurrences.reserve(64);
                parseRRule(masterEvent, occurrences);
                
                // Convert occurrences to events, handling exceptions
                PsramEventPairVector final_series_vec;
                final_series_vec.reserve(occurrences.size() + exceptions.size());
                
                for (const auto& occ : occurrences) {
                    Event series_event = masterEvent;
                    series_event.summary = occ.summary;
                    series_event.dtstart = occ.startEpoch;
                    series_event.dtend = occ.endEpoch;
                    series_event.isAllDay = occ.isAllDay;
                    final_series_vec.emplace_back(occ.startEpoch, std::move(series_event));
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
        if (raw_events.empty()) { resetScroll(); return; }
        
        events.clear();
        
        // Sort events by start time
        std::sort(raw_events.begin(), raw_events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
            return a.startEpoch < b.startEpoch;
        });
        
        // Consolidate multiple distinct single-day all-day events on the same date
        std::map<time_t, std::vector<size_t>, std::less<time_t>, PsramAllocator<std::pair<const time_t, std::vector<size_t>>>> allDayByDate;
        std::vector<bool, PsramAllocator<bool>> processed(raw_events.size(), false);
        
        // First pass: identify single-day all-day events and group by date
        for (size_t i = 0; i < raw_events.size(); ++i) {
            const auto& ev = raw_events[i];
            if (ev.isAllDay && ev.startEpoch == ev.endEpoch) {
                // Single-day all-day event
                allDayByDate[ev.startEpoch].push_back(i);
                processed[i] = true;
            }
        }
        
        // Second pass: consolidate single-day all-day events on same date
        for (const auto& entry : allDayByDate) {
            const auto& indices = entry.second;
            if (indices.size() == 1) {
                // Only one event on this date, add it as-is
                events.push_back(raw_events[indices[0]]);
            } else {
                // Multiple events on same date, combine summaries
                CalendarEvent combined = raw_events[indices[0]];
                PsramString combinedSummary = combined.summary;
                for (size_t i = 1; i < indices.size(); ++i) {
                    combinedSummary += ", ";
                    combinedSummary += raw_events[indices[i]].summary;
                }
                combined.summary = combinedSummary;
                events.push_back(combined);
            }
        }
        
        // Third pass: add all other events (timed events and multi-day all-day events)
        for (size_t i = 0; i < raw_events.size(); ++i) {
            if (!processed[i]) {
                events.push_back(raw_events[i]);
            }
        }
        
        // Final sort by start time
        std::sort(events.begin(), events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
            return a.startEpoch < b.startEpoch;
        });

        resetScroll();
    }



    void addSingleEvent(const Event& ev) {
        if (ev.dtstart == 0) return;
        CalendarEvent ce;
        ce.summary = ev.summary.c_str();
        ce.startEpoch = ev.dtstart;
        ce.isAllDay = ev.isAllDay;
        
        // If dtend is set, use it
        if (ev.dtend > 0) {
            // For all-day events, adjust for iCal's exclusive end date convention
            if (ev.isAllDay && ev.dtend > ev.dtstart) {
                // iCal DTEND is exclusive (day after last day)
                // Check if this is a single-day event (dtend = dtstart + 1 day)
                if (ev.dtend == ev.dtstart + 86400) {
                    // Single-day all-day event: set endEpoch = startEpoch
                    ce.endEpoch = ce.startEpoch;
                } else {
                    // Multi-day all-day event: subtract 1 day to get the last day
                    ce.endEpoch = ev.dtend - 86400;
                }
            } else {
                // For timed events or edge cases
                ce.endEpoch = ev.dtend;
            }
        } else {
            // No dtend provided, default to same as start (timed event) or start + 1 day (all-day)
            if (ev.isAllDay) {
                // Treat as single-day all-day event
                ce.endEpoch = ce.startEpoch;
            } else {
                // Timed event without duration
                ce.endEpoch = ev.dtstart;
            }
        }
        
        raw_events.push_back(ce);
    }

    PsramCalendarEventVector getUpcomingEvents(int maxCount) {
        PsramCalendarEventVector result;
        result.reserve(maxCount);
        time_t now_utc; time(&now_utc);
        for (const auto& ev : events) {
            // For single-day all-day events: endEpoch == startEpoch, show if startEpoch >= now
            // For multi-day all-day events: show if endEpoch >= now
            // For timed events: show if startEpoch >= now
            time_t compareTime = (ev.isAllDay && ev.endEpoch > ev.startEpoch) ? ev.endEpoch : ev.startEpoch;
            if (compareTime >= now_utc) {
                result.push_back(ev);
            }
            if ((int)result.size() >= maxCount) break;
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
        if (scrollPos.size() != upcomming.size()) scrollPos.resize(upcomming.size());
    }
};
#endif