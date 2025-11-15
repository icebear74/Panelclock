#ifndef CURIOUSHOLIDAYSMODULE_HPP
#define CURIOUSHOLIDAYSMODULE_HPP

#include "DrawableModule.hpp"
#include "WebClientModule.hpp"
#include "GeneralTimeConverter.hpp"
#include "PsramUtils.hpp"
#include <U8G2_FOR_ADAFRUIT_GFX.h>
#include <Adafruit_GFX.h>

struct HolidayEntry {
    PsramString name;
    PsramString description;
};

struct DeviceConfig;

class CuriousHolidaysModule : public DrawableModule {
public:
    CuriousHolidaysModule(U8G2_FOR_ADAFRUIT_GFX& u8g2, GFXcanvas16& canvas, GeneralTimeConverter& timeConverter, WebClientModule* webClient, DeviceConfig* config);
    ~CuriousHolidaysModule();

    void begin();
    void setConfig();
    void queueData();
    void processData();
    void onUpdate(std::function<void()> callback);

    const char* getModuleName() const override { return "CuriousHolidaysModule"; }
    const char* getModuleDisplayName() const override { return "Kuriose Feiertage"; }
    int getCurrentPage() const override { return currentPage; }
    int getTotalPages() const override { return pageIndices.size(); }
    void draw() override;
    unsigned long getDisplayDuration() override { return pageDisplayDuration * (getTotalPages() > 0 ? getTotalPages() : 1); }
    void resetPaging() override;
    bool isEnabled() override;
    bool isFinished() const override { return _isFinished; }
    void timeIsUp() override { /* Optional */ }

protected:
    virtual void onActivate() override;
    virtual void logicTick() override;

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    GeneralTimeConverter& timeConverter;
    WebClientModule* webClient;
    DeviceConfig* config;

    PsramVector<HolidayEntry> holidaysToday;
    PsramVector<HolidayEntry> holidaysTomorrow;
    PsramString resourceUrl;
    SemaphoreHandle_t dataMutex;
    std::function<void()> updateCallback;
    char* pendingBuffer = nullptr;
    size_t bufferSize = 0;
    time_t lastProcessedUpdate = 0;
    bool dataPending = false;

    int currentPage = 0;
    unsigned long pageDisplayDuration = 10000;
    int _logicTicksSincePageSwitch = 0;
    int _currentTicksPerPage = 100;
    bool _isFinished = false;
    int lastMonth = -1;
    int _lastCheckedDay; 

    PsramVector<PsramVector<int>> pageIndices;

    void parseAndProcessHtml(const char* buffer, size_t size);
    void handleDayChange();
    void calculatePages();
};

#endif // CURIOUSHOLIDAYSMODULE_HPP