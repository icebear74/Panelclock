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

// Funktionsdeklaration
uint16_t hexColorTo565(const PsramString& hex);

class CalendarModule {
public:
    CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient);
    ~CalendarModule();

    void onUpdate(std::function<void()> callback);
    void setICSUrl(const PsramString &url);
    void setFetchIntervalMinutes(uint32_t minutes);
    
    void queueData();
    void processData();
    void tickScroll();
    void draw();

    void setScrollStepInterval(uint32_t ms);
    uint32_t getScrollStepInterval() const;
    void setColors(const PsramString& dateColorHex, const PsramString& textColorHex);

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

    int stdOffsetSec = 0;
    int dstOffsetSec = 0;

    uint16_t dimColor(uint16_t color, float brightness);
    void parseICS(char* icsBuffer, size_t size);
    void onSuccessfulUpdate();
    void addSingleEvent(const Event& ev);
    PsramCalendarEventVector getUpcomingEvents(int maxCount);
    PsramString fitTextToPixelWidth(const char* text, int maxPixel);
    void resetScroll();
    void ensureScrollPos(const PsramCalendarEventVector& upcomming, int maxTextPixel);
};
#endif // CALENDARMODULE_HPP