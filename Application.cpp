#include "Application.hpp"
#include <Arduino.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>
#include "ConnectionManager.hpp"
#include "GeneralTimeConverter.hpp"
#include "WebServerManager.hpp"
#include "webconfig.hpp"
#include "HardwareConfig.hpp"
#include "MwaveSensorModule.hpp"
#include "OtaManager.hpp"
#include "MemoryLogger.hpp"
#include "MultiLogger.hpp"
#include "PanelStreamer.hpp"

// --- Globale Variablen ---
Application* Application::_instance = nullptr;
HardwareConfig* hardwareConfig = nullptr;
DeviceConfig* deviceConfig = nullptr;
ConnectionManager* connectionManager = nullptr;
GeneralTimeConverter* timeConverter = nullptr;
WebServer* server = nullptr;
DNSServer* dnsServer = nullptr;
WebClientModule* webClient = nullptr;
MwaveSensorModule* mwaveSensorModule = nullptr;
OtaManager* otaManager = nullptr;
bool portalRunning = false;

TankerkoenigModule* tankerkoenigModule = nullptr;
ThemeParkModule* themeParkModule = nullptr;

// Forward-Deklaration, da in WebServerManager.cpp definiert
void setupWebServer(bool portalMode);
void handleWebServer(bool portalIsRunning);

void displayStatus(const char* msg) {
    if (Application::_instance && Application::_instance->getPanelManager()) {
        Application::_instance->getPanelManager()->displayStatus(msg);
    } else {
        Log.printf("[displayStatus FALLBACK]: %s\n", msg);
    }
}

void applyLiveConfig() {
    if (Application::_instance) {
        Application::_instance->_configNeedsApplying = true;
        Log.println("[Config] Live-Konfiguration angefordert. Wird im nächsten Loop-Durchlauf angewendet.");
    }
}

Application::Application() {
    _instance = this;
}

Application::~Application() {
    delete hardwareConfig;
    delete deviceConfig;
    delete connectionManager;
    delete timeConverter;
    delete server;
    delete dnsServer;
    delete webClient;
    delete mwaveSensorModule;
    delete otaManager;
    delete _panelManager;
    delete _clockMod;
    delete _tankerkoenigMod;
    delete _calendarMod;
    delete _dartsMod;
    delete _fritzMod;
    delete _curiousMod;
    delete _weatherMod;
    delete _themeParkMod;
    delete _panelStreamer;
}

void Application::begin() {
    LOG_MEMORY_STRATEGIC("Application: Start");

    if (!LittleFS.begin()) {
        Log.println("FATAL: LittleFS konnte nicht initialisiert werden!");
        while(true) delay(1000);
    }

    hardwareConfig = new HardwareConfig();
    loadHardwareConfig();
    deviceConfig = new DeviceConfig();
    loadDeviceConfig();

    timeConverter = new GeneralTimeConverter();
    _panelManager = new PanelManager(*hardwareConfig, *timeConverter);
    if (!_panelManager->begin()) {
        while(true) { delay(1000); }
    }
    _panelManager->displayStatus("Systemstart...");

    connectionManager = new ConnectionManager(*deviceConfig);
    webClient = new WebClientModule();
    HardwareSerial& sensorSerial = Serial1;
    mwaveSensorModule = new MwaveSensorModule(*deviceConfig, *hardwareConfig, sensorSerial);
    otaManager = new OtaManager(_panelManager->getFullCanvas(), _panelManager->getDisplay(), _panelManager->getVirtualDisplay(), _panelManager->getU8g2());
    dnsServer = new DNSServer();
    server = new WebServer(80);

    _panelManager->displayStatus("Erstelle Module...");
    _clockMod = new ClockModule(*_panelManager->getU8g2(), *_panelManager->getCanvasTime(), *timeConverter);
    
    _tankerkoenigMod = new TankerkoenigModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, TIME_AREA_H, webClient, deviceConfig);
    tankerkoenigModule = _tankerkoenigMod; 
    
    _calendarMod = new CalendarModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient, deviceConfig);
    _dartsMod = new DartsRankingModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient);
    _fritzMod = new FritzboxModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient);
    _curiousMod = new CuriousHolidaysModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient, deviceConfig);
    _weatherMod = new WeatherModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient);
    _themeParkMod = new ThemeParkModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient);
    themeParkModule = _themeParkMod;
    
    _panelManager->registerClockModule(_clockMod);
    _panelManager->registerSensorModule(mwaveSensorModule);
    _panelManager->registerModule(_fritzMod);
    _panelManager->registerModule(_tankerkoenigMod);
    _panelManager->registerModule(_calendarMod);
    _panelManager->registerModule(_dartsMod);
    _panelManager->registerModule(_curiousMod);
    _panelManager->registerModule(_weatherMod);
    _panelManager->registerModule(_themeParkMod);

    if (connectionManager->begin()) {
        portalRunning = false;
        LOG_MEMORY_DETAILED("Nach WiFi & NTP");
        BaseType_t app_core = xPortGetCoreID();
        BaseType_t network_core = (app_core == 0) ? 1 : 0;
        
        mwaveSensorModule->begin();
        _tankerkoenigMod->begin();
        webClient->begin();
        _fritzMod->begin(network_core);
        _curiousMod->begin();
        _weatherMod->begin();
        _themeParkMod->begin();

        WiFi.setHostname(deviceConfig->hostname.c_str());
        if (!deviceConfig->otaPassword.empty()) ArduinoOTA.setPassword(deviceConfig->otaPassword.c_str());
        ArduinoOTA.setHostname(deviceConfig->hostname.c_str());
        
        otaManager->begin();
        ArduinoOTA.begin();
        setupWebServer(portalRunning);
        
        // Initialize Panel Streamer after WiFi is connected
        _panelStreamer = new PanelStreamer(_panelManager);
        _panelStreamer->begin();
        Log.println("[Application] PanelStreamer initialized and started");
        
    } else {
        portalRunning = true;
        WiFi.softAP("Panelclock-Setup");
        mwaveSensorModule->begin();
        setupWebServer(portalRunning);
    }
    LOG_MEMORY_STRATEGIC("Nach Netzwerk-Stack Init");

    executeApplyLiveConfig();
    
    auto redrawCb = [this](){ this->_redrawRequest = true; };
    _tankerkoenigMod->onUpdate(redrawCb);
    _calendarMod->onUpdate(redrawCb);

    _dartsMod->onUpdate([this](DartsRankingType type){ 
        this->_redrawRequest = true; 
    });

    _curiousMod->onUpdate(redrawCb);
    _weatherMod->onUpdate(redrawCb);
    _themeParkMod->onUpdate(redrawCb);

    _panelManager->displayStatus("Startvorgang\nabgeschlossen.");
    delay(2000);
    LOG_MEMORY_STRATEGIC("Application: Ende");
}

void Application::update() {
    if (_configNeedsApplying) {
        executeApplyLiveConfig();
        _configNeedsApplying = false;
    }

    handleWebServer(portalRunning);

    if (portalRunning) {
        if (_panelManager) {
            _panelManager->displayStatus(("Konfig-Portal aktiv\nIP: " + WiFi.softAPIP().toString()).c_str());
        }
        delay(100);
        return;
    }

    if (connectionManager) connectionManager->update();
    
    time_t now_utc;
    time(&now_utc);
    if(mwaveSensorModule) mwaveSensorModule->update(now_utc);

    ArduinoOTA.handle();

    // KORREKTUR: Aufrufe für das Wetter-Modul hinzugefügt
    if(_tankerkoenigMod) _tankerkoenigMod->queueData();
    if(_dartsMod) _dartsMod->queueData();
    if(_calendarMod) _calendarMod->queueData();
    if(_curiousMod) _curiousMod->queueData();
    if(_weatherMod) _weatherMod->queueData(); // HINZUGEFÜGT
    if(_themeParkMod) _themeParkMod->queueData(); // HINZUGEFÜGT
    
    // KORREKTUR: Aufrufe für das Wetter-Modul hinzugefügt
    if(_tankerkoenigMod) _tankerkoenigMod->processData();
    if(_dartsMod) _dartsMod->processData();
    if(_calendarMod) _calendarMod->processData();
    if(_curiousMod) _curiousMod->processData();
    if(_weatherMod) _weatherMod->processData(); // HINZUGEFÜGT
    if(_themeParkMod) _themeParkMod->processData(); // HINZUGEFÜGT

    if (_panelManager) _panelManager->tick();

    bool needsRedraw = _redrawRequest;
    _redrawRequest = false;

    if (millis() - _lastClockUpdate >= 1000) {
        needsRedraw = true;
        _lastClockUpdate = millis();
    }
    
    if (needsRedraw) {
        if (_panelManager) _panelManager->render();
    }
  
    delay(10);
}

void Application::executeApplyLiveConfig() {
    LOG_MEMORY_DETAILED("Vor executeApplyLiveConfig");
    if (!_tankerkoenigMod || !_calendarMod || !_dartsMod || !_fritzMod || !_curiousMod || !_weatherMod || !_themeParkMod || !timeConverter || !deviceConfig) return;
    Log.println("[Config] Wende Live-Konfiguration an...");
    
    if (!timeConverter->setTimezone(deviceConfig->timezone.c_str())) {
        timeConverter->setTimezone("UTC");
    }
    
    _tankerkoenigMod->setConfig(deviceConfig->tankerApiKey, deviceConfig->tankerkoenigStationIds, deviceConfig->stationFetchIntervalMin, deviceConfig->stationDisplaySec);
    _calendarMod->setConfig(deviceConfig->icsUrl, deviceConfig->calendarFetchIntervalMin, deviceConfig->calendarDisplaySec, deviceConfig->globalScrollSpeedMs, deviceConfig->calendarDateColor, deviceConfig->calendarTextColor);
    _calendarMod->setUrgentParams(deviceConfig->calendarFastBlinkHours, deviceConfig->calendarUrgentThresholdHours, deviceConfig->calendarUrgentDurationSec, deviceConfig->calendarUrgentRepeatMin);
    _dartsMod->setConfig(deviceConfig->dartsOomEnabled, deviceConfig->dartsProTourEnabled, 5, deviceConfig->dartsDisplaySec, deviceConfig->trackedDartsPlayers);
    _fritzMod->setConfig(deviceConfig->fritzboxEnabled, deviceConfig->fritzboxIp);
    _curiousMod->setConfig();
    _weatherMod->setConfig(deviceConfig);
    _themeParkMod->setConfig(deviceConfig);

    Log.println("[Config] Live-Konfiguration angewendet.");
    LOG_MEMORY_DETAILED("Nach executeApplyLiveConfig");
}
