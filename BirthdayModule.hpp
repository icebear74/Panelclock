#ifndef BIRTHDAYMODULE_HPP
#define BIRTHDAYMODULE_HPP

#include "DrawableModule.hpp"
#include "WebClientModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "PsramUtils.hpp"
#include "RRuleParser.hpp"
#include <U8G2_FOR_ADAFRUIT_GFX.h>
#include <Adafruit_GFX.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct DeviceConfig;

/**
 * @brief Structure to hold birthday event data
 */
struct BirthdayEvent {
    PsramString name;           // Name of the person
    time_t birthEpoch;          // Original birth date as epoch (can be negative for pre-1970)
    int birthYear;              // Original birth year
    int birthMonth;             // Birth month (1-12)
    int birthDay;               // Birth day of month
    int birthHour;              // Birth hour (0-23)
    int birthMinute;            // Birth minute (0-59)
    int birthSecond;            // Birth second (0-59)
    bool hasTime;               // Whether birth time is known (not all-day event)
};

/**
 * @brief Structure to hold calculated age information
 */
struct AgeInfo {
    int years;
    int months;
    int days;
    int hours;
    int minutes;
    int seconds;
    
    // Total values (for Variant B display)
    int64_t totalDays;
    int64_t totalHours;
    int64_t totalMinutes;
    int64_t totalSeconds;
};

using PsramBirthdayEventVector = std::vector<BirthdayEvent, PsramAllocator<BirthdayEvent>>;

/**
 * @brief BirthdayModule displays age information from a birthday calendar (ICS).
 * 
 * Reads birthday events from an ICS file (typically YEARLY recurring events)
 * and calculates the current age in various formats:
 * - Line 1: Human-readable format "Du bist X Jahre, Y Monate, Z Tage, hh:mm:ss alt"
 * - Line 2: Total values format showing total days/hours/minutes/seconds
 */
class BirthdayModule : public DrawableModule {
public:
    BirthdayModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, 
                   const GeneralTimeConverter& timeConverter, 
                   WebClientModule* webClient, DeviceConfig* config);
    ~BirthdayModule();

    void begin();
    void setConfig(const PsramString& url, unsigned long fetchMinutes, 
                   unsigned long displaySec, const PsramString& headerColor,
                   const PsramString& textColor);
    void queueData();
    void processData();
    void onUpdate(std::function<void()> callback);

    // DrawableModule interface
    const char* getModuleName() const override { return "BirthdayModule"; }
    const char* getModuleDisplayName() const override { return "Geburtstag"; }
    int getCurrentPage() const override { return _currentPage; }
    int getTotalPages() const override { return _birthdayEvents.empty() ? 1 : _birthdayEvents.size(); }
    void draw() override;
    void tick() override;
    void periodicTick() override;
    void logicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;
    bool isFinished() const override { return _isFinished; }
    
    /**
     * @brief Indicates this module should NOT be in the normal playlist rotation.
     * It only shows via interrupts when there's a birthday today.
     */
    bool canBeInPlaylist() const override { return true; }

private:
    U8G2_FOR_ADAFRUIT_GFX& _u8g2;
    GFXcanvas16& _canvas;
    const GeneralTimeConverter& _timeConverter;
    WebClientModule* _webClient;
    DeviceConfig* _deviceConfig;
    std::function<void()> _updateCallback;
    
    PsramString _icsUrl;
    uint32_t _fetchIntervalMinutes = 60;
    unsigned long _displayDuration = 30000;
    
    uint16_t _headerColor = 0xFFE0;  // Yellow default
    uint16_t _textColor = 0xFFFF;    // White default
    
    PsramBirthdayEventVector _birthdayEvents;
    PsramBirthdayEventVector _rawEvents;
    
    SemaphoreHandle_t _dataMutex;
    char* _pendingBuffer = nullptr;
    size_t _bufferSize = 0;
    time_t _lastProcessedUpdate = 0;
    volatile bool _dataPending = false;
    
    bool _isEnabled = false;
    bool _isFinished = false;
    int _currentPage = 0;
    uint32_t _logicTicksSinceStart = 0;
    
    /**
     * @brief Parse ICS data and extract birthday events
     */
    void parseICS(char* icsBuffer, size_t size);
    
    /**
     * @brief Process parsed events and filter for today's birthdays
     */
    void onSuccessfulUpdate();
    
    /**
     * @brief Calculate age from birth date to now
     * @param event The birthday event with birth date info
     * @return AgeInfo structure with calculated values
     */
    AgeInfo calculateAge(const BirthdayEvent& event) const;
    
    /**
     * @brief Parse a single VEVENT block for birthday information
     */
    void parseVEventForBirthday(const char* veventBlock, size_t len, BirthdayEvent& event);
    
    /**
     * @brief Parse date/time from ICS DTSTART line, handling pre-1970 dates
     */
    bool parseBirthdayDateTime(const char* line, size_t len, BirthdayEvent& event);
    
    /**
     * @brief Check if an event is a birthday for today
     */
    bool isBirthdayToday(const BirthdayEvent& event) const;
    
    /**
     * @brief Convert hex color string to RGB565
     */
    static uint16_t hexColorTo565(const PsramString& hex);
    
    /**
     * @brief Draw a single birthday event page
     */
    void drawBirthdayPage(const BirthdayEvent& event, const AgeInfo& age);
};

#endif // BIRTHDAYMODULE_HPP
