#ifndef THEMEPARKMODULE_HPP
#define THEMEPARKMODULE_HPP

#include "DrawableModule.hpp"
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include <functional>
#include <map>

class WebClientModule;
struct DeviceConfig;

struct Attraction {
    PsramString name;
    int waitTime;  // in minutes
    bool isOpen;
    
    Attraction() : waitTime(0), isOpen(false) {}
};

struct ThemeParkData {
    PsramString id;
    PsramString name;
    PsramString country;
    float crowdLevel;  // 0-100 scale from API (was int 0-10)
    PsramVector<Attraction> attractions;
    time_t lastUpdate;
    bool isOpen;
    PsramString openingTime;   // e.g. "09:00"
    PsramString closingTime;   // e.g. "18:00"
    int attractionPages;  // Number of pages needed to show all attractions
    
    ThemeParkData() : crowdLevel(0.0f), lastUpdate(0), isOpen(false), attractionPages(1) {}
};

struct AvailablePark {
    PsramString id;
    PsramString name;
    PsramString country;
    
    AvailablePark() {}
    AvailablePark(const PsramString& parkId, const PsramString& parkName) 
        : id(parkId), name(parkName) {}
    AvailablePark(const PsramString& parkId, const PsramString& parkName, const PsramString& parkCountry) 
        : id(parkId), name(parkName), country(parkCountry) {}
};

class ThemeParkModule : public DrawableModule {
public:
    ThemeParkModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, WebClientModule* webClient);
    ~ThemeParkModule();

    void begin();
    void setConfig(const DeviceConfig* config);
    void queueData();
    void processData();
    void onUpdate(std::function<void()> callback);
    
    // DrawableModule Interface
    const char* getModuleName() const override { return "ThemeParkModule"; }
    const char* getModuleDisplayName() const override { return "Freizeitparks"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    void resetPaging() override;
    bool isEnabled() override;
    unsigned long getDisplayDuration() override;
    int getCurrentPage() const override { return _currentPage; }
    int getTotalPages() const override { return _totalPages; }
    
    void configure(const ModuleConfig& config) override;
    void onActivate() override;
    
    // Public methods for web configuration
    PsramVector<AvailablePark> getAvailableParks();
    void parseAvailableParks(const char* jsonBuffer, size_t size);
    void checkAndUpdateParksList();  // Check if parks list needs daily update

private:
    U8G2_FOR_ADAFRUIT_GFX& _u8g2;
    GFXcanvas16& _canvas;
    WebClientModule* _webClient;
    const DeviceConfig* _config;
    SemaphoreHandle_t _dataMutex;
    ModuleConfig _modConfig;

    PsramVector<ThemeParkData> _parkData;
    PsramVector<AvailablePark> _availableParks;
    PsramVector<PsramString> _parkIds;  // Configured park IDs
    
    // Track last processed update time for each park to avoid reprocessing same data
    std::map<PsramString, time_t> _lastProcessedWaitTimes;
    std::map<PsramString, time_t> _lastProcessedOpeningTimes;
    std::map<PsramString, time_t> _lastProcessedCrowdLevel;
    
    int _currentPage;
    int _currentParkIndex;  // Index of current park being displayed
    int _currentAttractionPage;  // Current attraction page within park
    int _totalPages;
    uint32_t _logicTicksSincePageSwitch;
    unsigned long _pageDisplayDuration;
    
    time_t _lastUpdate;
    time_t _lastParksListUpdate;  // Track when parks list was last updated
    time_t _lastParkDetailsUpdate;  // Track when park details (name, hours) were updated
    std::function<void()> _updateCallback;
    
    // Scrolling support for park name
    int _parkNameScrollOffset;
    int _parkNameMaxScroll;
    unsigned long _lastScrollStep;  // Track last scroll update time
    
    void parseWaitTimes(const char* jsonBuffer, size_t size, const PsramString& parkId);
    void parseCrowdLevel(const char* jsonBuffer, size_t size, const PsramString& parkId);
    void parseOpeningTimes(const char* jsonBuffer, size_t size, const PsramString& parkId);
    bool shouldDisplayPark(const ThemeParkData& park) const;
    int calculateTotalPages() const;  // Calculate total pages considering closed/open parks
    void drawParkPage(int parkIndex, int attractionPage);
    void drawNoDataPage();
    uint16_t getCrowdLevelColor(float level);
    PsramString truncateString(const PsramString& text, int maxWidth);
    void drawScrollingText(const PsramString& text, int x, int y, int maxWidth);
    PsramString fitTextToPixelWidth(const PsramString& text, int maxPixel);
    
    // Cache management
    void loadParkCache();
    void saveParkCache();
    PsramString getParkNameFromCache(const PsramString& parkId);
    PsramString getParkCountryFromCache(const PsramString& parkId);
};

#endif // THEMEPARKMODULE_HPP
