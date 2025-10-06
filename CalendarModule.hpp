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

// Forward declaration
struct Event;

struct CalendarEvent {
  PsramString summary;
  time_t startEpoch;
  time_t endEpoch;
  bool isAllDay;
};

// Debug- und Hilfsfunktionen
static inline void printMemoryStats(const char* stage, int counter = -1) {
    Serial.printf("\n--- Memory @ %s ", stage);
    if (counter != -1) { Serial.printf("[%d] ", counter); }
    Serial.println("---");
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t min_internal  = ESP.getMinFreeHeap();
    Serial.printf("Heap:  Free: %lu, Min Free: %lu\n", (unsigned long)free_internal, (unsigned long)min_internal);
    if (psramFound()) {
        size_t free_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        Serial.printf("PSRAM: Free: %lu\n", (unsigned long)free_spiram);
    } else {
        Serial.println("PSRAM not found.");
    }
    Serial.println("------------------------------------");
}

static uint16_t hexColorTo565(const String& hex) {
  if (hex.length() != 7 || hex[0] != '#') return 0xFFFF;
  long r = strtol(hex.substring(1,3).c_str(), NULL, 16);
  long g = strtol(hex.substring(3,5).c_str(), NULL, 16);
  long b = strtol(hex.substring(5,7).c_str(), NULL, 16);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

class CalendarModule {
public:
    CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient)
    : u8g2(u8g2), canvas(canvas), timeConverter(converter), webClient(webClient) {}

    void onUpdate(std::function<void()> callback) {
        updateCallback = callback;
    }

    void setICSUrl(const String &url) { icsUrl = url; }
    String getICSUrl() const { return icsUrl; }
    void setFetchIntervalMinutes(uint32_t minutes) { fetchInterval = minutes * 60000UL; }
    uint32_t getFetchIntervalMillis() const { return fetchInterval; }
    void setScrollStepInterval(uint32_t ms) { scrollStepInterval = ms; resetScroll(); }
    uint32_t getScrollStepInterval() const { return scrollStepInterval; }
    void setColors(const String& dateColorHex, const String& textColorHex) {
        dateColor = hexColorTo565(dateColorHex);
        textColor = hexColorTo565(textColorHex);
    }

    // Request calendar: WebClientModule liefert PSRAM buffer via onSuccess callback
    void update() {
        if (!webClient || icsUrl.length() == 0) return;
        WebRequest request;
        request.url = icsUrl;

        request.onSuccess = [this](char* buffer, size_t size) {
            printMemoryStats("onSuccess start (calendar)");
            if (buffer && size > 0) {
                this->parseICS(buffer, size);
            } else {
                this->onFailedUpdate(-100);
            }
            // free PSRAM buffer (ownership returned by WebClientModule)
            if (buffer) free(buffer);

            this->onSuccessfulUpdate();
            Serial.println("[CalendarModule] Callback: Update successful.");
            if (updateCallback) updateCallback();
        };

        request.onFailure = [this](int httpCode) {
            this->onFailedUpdate(httpCode);
            Serial.printf("[CalendarModule] Callback: Update failed, code: %d\n", httpCode);
            if (updateCallback) updateCallback();
        };

        webClient->addRequest(request);
    }

    void tickScroll() {
        if (scrollStepInterval == 0) return;
        unsigned long now = millis();
        if (now - lastScrollStep >= scrollStepInterval) {
            lastScrollStep = now;
            for (auto& s : scrollPos) {
                if (s.maxScroll > 0) {
                    s.offset = (s.offset + 1) % s.maxScroll;
                }
            }
        }
    }

    void draw() {
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
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    const GeneralTimeConverter& timeConverter;
    WebClientModule* webClient;
    std::function<void()> updateCallback;
    String icsUrl;
    uint32_t fetchInterval = 900000;
    std::vector<CalendarEvent, PsramAllocator<CalendarEvent>> events;
    struct ScrollState { int offset = 0; int maxScroll = 0; };
    std::vector<ScrollState> scrollPos;
    unsigned long lastScrollStep = 0;
    uint32_t scrollStepInterval = 150;
    uint16_t dateColor = 0xFFE0;
    uint16_t textColor = 0xFFFF;

    // Parse ICS from PSRAM buffer (char*). Uses PsramString for large temporaries.
    // This implementation collects parsed events in a single PSRAM vector,
    // sorts by UID and processes contiguous groups. That avoids node-based maps
    // and reduces internal-heap fragmentation.
    void parseICS(char* icsBuffer, size_t size) {
        printMemoryStats("Start of parseICS (PSRAM buffer)");
        if (!icsBuffer || size == 0) return;

        // Create Psram-backed string view of the buffer (no internal-heap copy)
        PsramString ics(icsBuffer, size);

        events.clear();

        using PsramEventVector = std::vector<Event, PsramAllocator<Event>>;
        PsramEventVector parsedEvents;
        parsedEvents.reserve(256); // reserve to reduce reallocations; tune if needed

        printMemoryStats("Before parsing VEVENTs");

        size_t idx = 0;
        int veventCounter = 0;
        const PsramString beginTag("BEGIN:VEVENT");
        const PsramString endTag("END:VEVENT");

        // Collect parsed events in a flat vector (PSRAM-backed) to avoid map node allocs.
        while (true) {
            size_t pos = ics.find(beginTag, idx);
            if (pos == PsramString::npos) break;
            veventCounter++;
            size_t endPos = ics.find(endTag, pos);
            if (endPos == PsramString::npos) break;
            size_t blockLen = (endPos + endTag.length()) - pos;
            PsramString veventBlock = ics.substr(pos, blockLen);

            Event parsedEvent;
            parseVEvent(veventBlock.c_str(), veventBlock.length(), parsedEvent);
            parsedEvents.push_back(std::move(parsedEvent));

            idx = endPos + endTag.length();
            if (veventCounter % 50 == 0) { printMemoryStats("Grouping VEVENTs", veventCounter); }
        }
        printMemoryStats("Finished parsing VEVENTs");

        if (parsedEvents.empty()) {
            printMemoryStats("No VEVENTs found");
            return;
        }

        // Sort parsed events by UID (PSRAM strings) to group them without map allocations
        std::sort(parsedEvents.begin(), parsedEvents.end(), [](const Event& a, const Event& b){
            return a.uid < b.uid;
        });
        printMemoryStats("After sorting");

        // Scan contiguous groups with same UID
        size_t i = 0;
        int groupCounter = 0;
        while (i < parsedEvents.size()) {
            size_t j = i + 1;
            while (j < parsedEvents.size() && parsedEvents[j].uid == parsedEvents[i].uid) ++j;

            groupCounter++;
            // parsedEvents[i..j-1] form one group for the same UID
            Event masterEvent;
            std::vector<Event, PsramAllocator<Event>> exceptions;
            exceptions.reserve(j - i);

            for (size_t k = i; k < j; ++k) {
                const Event& ev = parsedEvents[k];
                if (!ev.rrule.empty()) masterEvent = ev;
                else exceptions.push_back(ev);
            }

            if (masterEvent.rrule.empty()) {
                // no RRULE -> treat exceptions as single events
                for (const auto& singleEvent : exceptions) addSingleEvent(singleEvent);
            } else {
                // Expand master event -> occurrences
                std::vector<time_t, PsramAllocator<time_t>> occurrences;
                occurrences.reserve(64);
                parseRRule(masterEvent, occurrences);

                // Build a flat final_series vector (time,event) instead of map
                using Pair = std::pair<time_t, Event>;
                std::vector<Pair, PsramAllocator<Pair>> final_series_vec;
                final_series_vec.reserve(occurrences.size() + exceptions.size());

                // add occurrences
                for (time_t start : occurrences) {
                    Event series_event = masterEvent;
                    series_event.dtstart = start;
                    final_series_vec.emplace_back(start, std::move(series_event));
                }

                // apply exceptions (replace existing by recurrence_id or append)
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

                // sort by time and unique by time (keep one entry per time)
                std::sort(final_series_vec.begin(), final_series_vec.end(), [](const Pair& a, const Pair& b){ return a.first < b.first; });
                final_series_vec.erase(std::unique(final_series_vec.begin(), final_series_vec.end(),
                    [](const Pair& a, const Pair& b){ return a.first == b.first; }), final_series_vec.end());

                // add to events
                for (auto const& pr : final_series_vec) addSingleEvent(pr.second);
            }

            if (groupCounter % 20 == 0) { printMemoryStats("Processing UID groups", groupCounter); }

            i = j;
        }

        printMemoryStats("Finished processing all UID groups", groupCounter);
    }

    void onSuccessfulUpdate() {
        printMemoryStats("Start of onSuccessfulUpdate");
        if (events.empty()) {
            resetScroll();
            printMemoryStats("End of onSuccessfulUpdate (empty)");
            return;
        }
        std::sort(events.begin(), events.end(), [](const CalendarEvent &a, const CalendarEvent &b) {
            if (a.startEpoch != b.startEpoch) return a.startEpoch < b.startEpoch;
            return a.summary < b.summary;
        });
        printMemoryStats("After sorting");

        std::vector<CalendarEvent, PsramAllocator<CalendarEvent>> mergedEvents;
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
        printMemoryStats("After merging");
        resetScroll();
    }

    void onFailedUpdate(int httpCode) {
        events.clear();
        CalendarEvent err;
        if (httpCode == -1) { err.summary = PsramString("Keine URL/WLAN"); }
        else {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "ICS HTTP: %d", httpCode);
            err.summary = PsramString(tmp);
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
        ce.summary = PsramString(ev.summary.c_str());
        ce.startEpoch = ev.dtstart;
        ce.endEpoch = ev.dtstart + duration;
        if (ce.isAllDay && duration == 86400) {
            ce.endEpoch -= 1;
        }
        ce.isAllDay = ev.isAllDay;
        events.push_back(ce);
    }

    std::vector<CalendarEvent, PsramAllocator<CalendarEvent>> getUpcomingEvents(int maxCount) {
        std::vector<CalendarEvent, PsramAllocator<CalendarEvent>> result;
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
    void ensureScrollPos(const std::vector<CalendarEvent, PsramAllocator<CalendarEvent>>& upcomming, int maxTextPixel) {
        if (scrollPos.size() != upcomming.size()) {
            scrollPos.resize(upcomming.size());
        }
    }
};

#endif