#ifndef APPLICATION_HPP
#define APPLICATION_HPP

#include "PanelManager.hpp"
#include "HardwareConfig.hpp"
#include "webconfig.hpp"
#include "ConnectionManager.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebServerManager.hpp"
#include "WebClientModule.hpp"
#include "MwaveSensorModule.hpp"
#include "OtaManager.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include "DartsRankingModule.hpp"
#include "FritzboxModule.hpp"
#include "MemoryLogger.hpp"

void applyLiveConfig();
void displayStatus(const char* msg); // Deklariere die globale Funktion

class Application {
public:
    Application();
    ~Application();

    void begin();
    void update();

    // Mache die globalen Funktionen zu Freunden
    friend void applyLiveConfig();
    friend void displayStatus(const char* msg);

private:
    static Application* _instance;
    static void calendarScrollTask(void* param);

    void executeApplyLiveConfig();

    PanelManager* _panelManager = nullptr;
    
    ClockModule* _clockMod = nullptr;
    DataModule* _dataMod = nullptr;
    CalendarModule* _calendarMod = nullptr;
    DartsRankingModule* _dartsMod = nullptr;
    FritzboxModule* _fritzMod = nullptr;

    volatile bool _configNeedsApplying = false;
    volatile bool _redrawRequest = false;
    unsigned long _lastClockUpdate = 0;
};

// ... (externe Deklarationen bleiben gleich)
extern HardwareConfig* hardwareConfig;
extern DeviceConfig* deviceConfig;
extern ConnectionManager* connectionManager;
extern GeneralTimeConverter* timeConverter;
extern WebServer* server;
extern DNSServer* dnsServer;
extern WebClientModule* webClient;
extern MwaveSensorModule* mwaveSensorModule;
extern OtaManager* otaManager;
extern bool portalRunning;

#endif // APPLICATION_HPP