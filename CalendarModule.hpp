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
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "DrawableModule.hpp"
#include "PixelScroller.hpp"

// Konstanten für dringende Termine (Fallbacks)
#define URGENT_EVENT_INTERVAL (2 * 60 * 1000UL) // 2 Minuten zwischen Anzeigen (Fallback)
#define URGENT_EVENT_DURATION (20 * 1000UL)     // 20 Sekunden Anzeigedauer (Fallback)
#define URGENT_EVENT_UID_BASE 1000              // Basis-UID für dringende Termine

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

struct DeviceConfig;

uint16_t hexColorTo565(const PsramString& hex);

class CalendarModule : public DrawableModule {
public:
    CalendarModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, const GeneralTimeConverter& converter, WebClientModule* webClient, DeviceConfig* config);
    ~CalendarModule();

    void onUpdate(std::function<void()> callback);
    void setConfig(const PsramString& url, unsigned long fetchMinutes, unsigned long displaySec, unsigned long scrollMs, const PsramString& dateColor, const PsramString& textColor);

    // Neue Funktion, um Urgent-View-Parameter zu setzen (von Anwendung / Webinterface)
    void setUrgentParams(int fastBlinkHours, int urgentThresholdHours, int urgentDurationSec, int urgentRepeatMin);
    
    void queueData();
    void processData();
    uint32_t getScrollStepInterval() const;

    // DrawableModule Interface
    const char* getModuleName() const override { return "CalendarModule"; }
    const char* getModuleDisplayName() const override { return "Kalender"; }
    void draw() override;
    void tick() override;
    void periodicTick() override;
    void logicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    const GeneralTimeConverter& timeConverter;
    WebClientModule* webClient;
    DeviceConfig* _deviceConfig;
    std::function<void()> updateCallback;
    PsramString icsUrl;
    uint32_t fetchIntervalMinutes = 60;
    PsramCalendarEventVector events;
    PsramCalendarEventVector raw_events;
    
    // PixelScroller für pixelweises Scrolling
    PixelScroller* _pixelScroller = nullptr;
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

    // Urgent Event Handling
    bool _isUrgentViewActive = false;
    uint32_t _currentUrgentUID = 0;
    uint32_t _logicTicksSinceStart = 0;
    unsigned long _urgentViewStartTime = 0;
    unsigned long _lastUrgentDisplayTime = 0;
    unsigned long _lastPeriodicCheck = 0;

    // Konfigurierbare Urgent-Parameter (Defaults stimmen mit Makros überein)
    int _fastBlinkHours = 2; // wenn innerhalb dieses Zeitraums vor Termin -> schnelleres pulsen
    int _urgentThresholdHours = 1; // ab wieviel Stunden vor Termin wird UrgentView gezeigt
    unsigned long _urgentDurationMs = URGENT_EVENT_DURATION; // wie lange die Urgent-View angezeigt wird
    unsigned long _urgentRepeatMs = URGENT_EVENT_INTERVAL; // Pause bis zur nächsten Anzeige (wiederholung)

    void parseICS(char* icsBuffer, size_t size);
    void onSuccessfulUpdate();
    void addSingleEvent(const Event& ev);
    PsramCalendarEventVector getUpcomingEvents(int maxCount);
    PsramString fitTextToPixelWidth(const char* text, int maxPixel);
    void resetScroll();
    
    void drawUrgentView();
};
#endif // CALENDARMODULE_HPP