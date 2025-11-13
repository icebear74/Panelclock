#ifndef THEMEPARKMODULE_HPP
#define THEMEPARKMODULE_HPP

#include "DrawableModule.hpp"
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include <functional>

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
    int crowdLevel;  // 0-10 scale
    PsramVector<Attraction> attractions;
    time_t lastUpdate;
    bool isOpen;
    PsramString openingTime;   // e.g. "09:00"
    PsramString closingTime;   // e.g. "18:00"
    
    ThemeParkData() : crowdLevel(0), lastUpdate(0), isOpen(false) {}
};

struct AvailablePark {
    PsramString id;
    PsramString name;
    
    AvailablePark() {}
    AvailablePark(const PsramString& parkId, const PsramString& parkName) 
        : id(parkId), name(parkName) {}
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

private:
    U8G2_FOR_ADAFRUIT_GFX& _u8g2;
    GFXcanvas16& _canvas;
    WebClientModule* _webClient;
    const DeviceConfig* _config;
    SemaphoreHandle_t _dataMutex;
    ModuleConfig _modConfig;

    PsramVector<ThemeParkData> _parkData;
    PsramVector<AvailablePark> _availableParks;
    
    int _currentPage;
    int _totalPages;
    uint32_t _logicTicksSincePageSwitch;
    unsigned long _pageDisplayDuration;
    
    char* _pendingParksListBuffer;
    size_t _parksListBufferSize;
    bool _parksListDataPending;
    
    char* _pendingWaitTimesBuffer;
    size_t _waitTimesBufferSize;
    bool _waitTimesDataPending;
    
    time_t _lastUpdate;
    std::function<void()> _updateCallback;
    
    void parseAvailableParks(const char* jsonBuffer, size_t size);
    void parseWaitTimes(const char* jsonBuffer, size_t size);
    void drawParkPage(int pageIndex);
    void drawNoDataPage();
    uint16_t getCrowdLevelColor(int level);
    PsramString truncateString(const PsramString& text, int maxWidth);
};

#endif // THEMEPARKMODULE_HPP
