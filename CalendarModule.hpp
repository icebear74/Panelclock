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
#include "DrawableModule.hpp"

// Konstanten für die intermittierende Anzeige
#define URGENT_EVENT_INTERVAL (2 * 60 * 1000UL) // 2 Minuten
#define URGENT_EVENT_DURATION (20 * 1000UL)     // 20 Sekunden
#define FAILSAFE_BUFFER_MS 10000 // 10 Sekunden Puffer

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

uint16_t hexColorTo565(const PsramString& hex);

class CalendarModule : public DrawableModule {
public:
    CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient);
    ~CalendarModule();

    void onUpdate(std::function<void()> callback);
    void setConfig(const PsramString& url, unsigned long fetchMinutes, unsigned long displaySec, unsigned long scrollMs, const PsramString& dateColor, const PsramString& textColor);
    
    void queueData();
    void processData();
    uint32_t getScrollStepInterval() const;

    // =================================================================
    // =========== UMSTELLUNG AUF MODERNE SCHNITTSTELLE ================
    // =================================================================
    void draw() override;
    void tick() override;
    void periodicTick() override;
    bool isEnabled() override;

    // --- Neue Metadaten-Methoden ---
    const char* getModuleName() const override { return "CalendarModule"; }
    const char* getModuleDisplayName() const override { return "Kalender"; }

    // --- VERALTETE METHODEN (werden nicht mehr genutzt) ---
    unsigned long getDisplayDuration() override { return 0; } // Muss implementiert, aber kann 0 zurückgeben
    void resetPaging() override {} // Muss implementiert, aber kann leer sein

protected:
    // --- Neue Start-Methode (ersetzt resetPaging) ---
    void onActivate() override;

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

    bool _isEnabled = false;
    unsigned long _displayDuration = 30000;
    unsigned long _startTime = 0; // NEU: für die Zeitmessung im modernen Modus

    bool _isUrgentViewActive = false;
    bool _priorityRequested = false;

    unsigned long _lastUrgentDisplayTime = 0;
    unsigned long _urgentViewStartTime = 0;
    unsigned long _lastPeriodicCheck = 0;

    uint16_t dimColor(uint16_t color, float brightness);
    void parseICS(char* icsBuffer, size_t size);
    void onSuccessfulUpdate();
    void addSingleEvent(const Event& ev);
    PsramCalendarEventVector getUpcomingEvents(int maxCount);
    PsramString fitTextToPixelWidth(const char* text, int maxPixel);
    void resetScroll();
    void ensureScrollPos(const PsramCalendarEventVector& upcomming, int maxTextPixel);
    
    void drawUrgentView();
};
#endif // CALENDARMODULE_HPP