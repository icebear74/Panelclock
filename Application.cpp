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
#include "AnimationsModule.hpp"
#include "CountdownModule.hpp"
#include "Version.hpp"
#include "FragmentationMonitor.hpp"

// --- Konstanten ---
constexpr uint16_t OTA_PORT = 3232; // Standard ArduinoOTA port

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
BackupManager* backupManager = nullptr;
bool portalRunning = false;

TankerkoenigModule* tankerkoenigModule = nullptr;
ThemeParkModule* themeParkModule = nullptr;
SofaScoreLiveModule* sofascoreMod = nullptr;
FritzboxModule* fritzboxModule = nullptr;  // Exposed for cleanup before restart
CountdownModule* countdownModule = nullptr;  // Exposed for web control

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

void prepareForRestart() {
    if (Application::_instance) {
        Application::_instance->prepareForRestart();
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
    delete backupManager;
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
    delete _animationsMod;
    delete _countdownMod;
}

void Application::begin() {
    LOG_MEMORY_STRATEGIC("Application: Start");

#if ENABLE_FRAG_MONITOR
    // Initialize FragmentationMonitor VERY EARLY to track startup phase
    g_FragMonitor = new (ps_malloc(sizeof(FragmentationMonitor))) FragmentationMonitor();
    if (g_FragMonitor) {
        g_FragMonitor->begin();
        LOG_MEM_OP_FORCE("App::begin START");
    }
#endif

    LOG_MEM_OP_FORCE("Before LittleFS init");
    if (!LittleFS.begin(true)) {
        Log.println("FATAL: LittleFS konnte nicht initialisiert werden!");
        while(true) delay(1000);
    }
    LOG_MEM_OP_FORCE("LittleFS initialized");

#if ENABLE_FRAG_MONITOR
    // Clean up old log directory AFTER LittleFS is ready
    if (g_FragMonitor) {
        g_FragMonitor->cleanupDirectoryOnStartup();
        LOG_MEM_OP_FORCE("mem_debug cleanup done");
    }
#endif

    LOG_MEM_OP_FORCE("Creating HardwareConfig");
    hardwareConfig = new HardwareConfig();
    loadHardwareConfig();
    LOG_MEM_OP_FORCE("HardwareConfig loaded");
    
    LOG_MEM_OP_FORCE("Creating DeviceConfig");
    deviceConfig = new DeviceConfig();
    loadDeviceConfig();
    LOG_MEM_OP_FORCE("DeviceConfig loaded");

    LOG_MEM_OP_FORCE("Creating TimeConverter");
    timeConverter = new GeneralTimeConverter();
    LOG_MEM_OP_FORCE("TimeConverter created");
    
    LOG_MEM_OP_FORCE("Creating PanelManager");
    _panelManager = new PanelManager(*hardwareConfig, *timeConverter);
    if (!_panelManager->begin()) {
        while(true) { delay(1000); }
    }
    LOG_MEM_OP_FORCE("PanelManager initialized");
    
    // Show version on startup
    char versionMsg[64];
    snprintf(versionMsg, sizeof(versionMsg), "Panelclock\nv%s", PANELCLOCK_VERSION);
    _panelManager->displayStatus(versionMsg);
    Log.printf("[Application] Panelclock Version %s (Build: %s %s)\n", 
               PANELCLOCK_VERSION, PANELCLOCK_BUILD_DATE, PANELCLOCK_BUILD_TIME);
    delay(2000);
    
    _panelManager->displayStatus("Systemstart...");

    LOG_MEM_OP_FORCE("Creating ConnectionManager");
    connectionManager = new ConnectionManager(*deviceConfig);
    LOG_MEM_OP_FORCE("ConnectionManager created");
    
    LOG_MEM_OP_FORCE("Creating WebClientModule");
    webClient = new WebClientModule();
    LOG_MEM_OP_FORCE("WebClientModule created");
    
    LOG_MEM_OP_FORCE("Creating MwaveSensorModule");
    HardwareSerial& sensorSerial = Serial1;
    mwaveSensorModule = new MwaveSensorModule(*deviceConfig, *hardwareConfig, sensorSerial);
    LOG_MEM_OP_FORCE("MwaveSensorModule created");
    
    LOG_MEM_OP_FORCE("Creating OtaManager");
    otaManager = new OtaManager(_panelManager->getFullCanvas(), _panelManager->getDisplay(), _panelManager->getVirtualDisplay(), _panelManager->getU8g2());
    LOG_MEM_OP_FORCE("OtaManager created");
    
    LOG_MEM_OP_FORCE("Creating network servers");
    dnsServer = new DNSServer();
    server = new WebServer(80);
    LOG_MEM_OP_FORCE("Network servers created");

    _panelManager->displayStatus("Module werden\nerstellt...");
    
    LOG_MEM_OP_FORCE("Creating ClockModule");
    _clockMod = new ClockModule(*_panelManager->getU8g2(), *_panelManager->getCanvasTime(), *timeConverter);
    LOG_MEM_OP_FORCE("ClockModule created");
    
    LOG_MEM_OP_FORCE("Creating TankerkoenigModule");
    _tankerkoenigMod = new TankerkoenigModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, TIME_AREA_H, webClient, deviceConfig);
    tankerkoenigModule = _tankerkoenigMod; 
    LOG_MEM_OP_FORCE("TankerkoenigModule created");
    
    LOG_MEM_OP_FORCE("Creating CalendarModule");
    _calendarMod = new CalendarModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient, deviceConfig);
    LOG_MEM_OP_FORCE("CalendarModule created");
    
    LOG_MEM_OP_FORCE("Creating DartsRankingModule");
    _dartsMod = new DartsRankingModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient, deviceConfig);
    LOG_MEM_OP_FORCE("DartsRankingModule created");
    
    LOG_MEM_OP_FORCE("Creating SofaScoreModule");
    _sofascoreMod = new SofaScoreLiveModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient, deviceConfig);
    sofascoreMod = _sofascoreMod;  // Expose globally for web configuration
    LOG_MEM_OP_FORCE("SofaScoreModule created");
    
    LOG_MEM_OP_FORCE("Creating FritzboxModule");
    _fritzMod = new FritzboxModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient);
    fritzboxModule = _fritzMod;  // Expose globally for cleanup before restart
    LOG_MEM_OP_FORCE("FritzboxModule created");
    
    LOG_MEM_OP_FORCE("Creating CuriousHolidaysModule");
    _curiousMod = new CuriousHolidaysModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient, deviceConfig);
    LOG_MEM_OP_FORCE("CuriousHolidaysModule created");
    
    LOG_MEM_OP_FORCE("Creating WeatherModule");
    _weatherMod = new WeatherModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, webClient);
    LOG_MEM_OP_FORCE("WeatherModule created");
    
    LOG_MEM_OP_FORCE("Creating ThemeParkModule");
    _themeParkMod = new ThemeParkModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), webClient);
    themeParkModule = _themeParkMod;
    LOG_MEM_OP_FORCE("ThemeParkModule created");
    
    LOG_MEM_OP_FORCE("Creating AnimationsModule");
    _animationsMod = new AnimationsModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, deviceConfig);
    LOG_MEM_OP_FORCE("AnimationsModule created");
    
    LOG_MEM_OP_FORCE("Creating CountdownModule");
    _countdownMod = new CountdownModule(*_panelManager->getU8g2(), *_panelManager->getCanvasData(), *timeConverter, deviceConfig);
    countdownModule = _countdownMod;  // Expose globally for web control
    LOG_MEM_OP_FORCE("CountdownModule created");
    
    LOG_MEM_OP_FORCE("Registering all modules");
    _panelManager->registerClockModule(_clockMod);
    _panelManager->registerSensorModule(mwaveSensorModule);
    _panelManager->registerModule(_fritzMod);
    _panelManager->registerModule(_tankerkoenigMod);
    _panelManager->registerModule(_calendarMod);
    _panelManager->registerModule(_dartsMod);
    _panelManager->registerModule(_sofascoreMod);
    _panelManager->registerModule(_curiousMod);
    _panelManager->registerModule(_weatherMod);
    _panelManager->registerModule(_themeParkMod);
    _panelManager->registerModule(_animationsMod);
    _panelManager->registerModule(_countdownMod);
    LOG_MEM_OP_FORCE("All modules registered");

    _panelManager->displayStatus("Verbinde zu\nWLAN...");
    LOG_MEM_OP_FORCE("Starting WiFi connection");
    if (connectionManager->begin()) {
        portalRunning = false;
        LOG_MEMORY_DETAILED("Nach WiFi & NTP");
        LOG_MEM_OP_FORCE("WiFi connected");
        BaseType_t app_core = xPortGetCoreID();
        BaseType_t network_core = (app_core == 0) ? 1 : 0;
        
        _panelManager->displayStatus("Starte\nNetzwerkmodule...");
        mwaveSensorModule->begin();
        _tankerkoenigMod->begin();
        webClient->begin();
        LOG_MEM_OP("Core modules started");
        
        _fritzMod->begin(network_core);
        _curiousMod->begin();
        _weatherMod->begin();
        _themeParkMod->begin();
        _animationsMod->begin();
        LOG_MEM_OP("All network modules started");
        _countdownMod->onUpdate([this]() {
            _redrawRequest = true;
        });

        // Determine effective hostname (with fallback if empty)
        bool hostnameEmpty = deviceConfig->hostname.empty();
        if (hostnameEmpty) {
            Log.println("[Application] WARNUNG: Hostname ist leer. Verwende Standard-Hostname 'panelclock'.");
        }
        const char* effectiveHostname = hostnameEmpty ? "panelclock" : deviceConfig->hostname.c_str();
        
        // Set WiFi hostname
        WiFi.setHostname(effectiveHostname);
        
        // Initialize mDNS - required for OTA discovery in Arduino IDE
        _panelManager->displayStatus("Starte mDNS...");
        Log.println("[Application] Starte mDNS...");
        if (!MDNS.begin(effectiveHostname)) {
            Log.printf("[Application] FEHLER: mDNS-Start mit Hostname '%s' fehlgeschlagen!\n", effectiveHostname);
            displayStatus("mDNS Fehler!");
            delay(2000); // Allow user to see error message on display
        } else {
            Log.printf("[Application] mDNS gestartet: %s.local\n", effectiveHostname);
            char mdnsMsg[64];
            snprintf(mdnsMsg, sizeof(mdnsMsg), "mDNS: %s.local", effectiveHostname);
            _panelManager->displayStatus(mdnsMsg);
            delay(1000);
        }
        
        // Configure OTA with same hostname as mDNS
        _panelManager->displayStatus("Konfiguriere\nOTA-Update...");
        if (!deviceConfig->otaPassword.empty()) ArduinoOTA.setPassword(deviceConfig->otaPassword.c_str());
        ArduinoOTA.setHostname(effectiveHostname);
        
        otaManager->begin();
        ArduinoOTA.begin();
        
        // Add mDNS service for OTA
        MDNS.addService("arduino", "tcp", OTA_PORT);
        
        // Initialize BackupManager before web server setup
        _panelManager->displayStatus("Initialisiere\nBackup-System...");
        backupManager = new BackupManager(this);
        backupManager->begin();
        Log.println("[Application] BackupManager initialized");
        
        _panelManager->displayStatus("Starte\nWebserver...");
        setupWebServer(portalRunning);
        
        // Initialize Panel Streamer after WiFi is connected
        _panelManager->displayStatus("Starte\nPanel-Streamer...");
        _panelStreamer = new PanelStreamer(_panelManager);
        _panelStreamer->begin();
        Log.println("[Application] PanelStreamer initialized and started");
        
    } else {
        portalRunning = true;
        _panelManager->displayStatus("WLAN nicht\nverbunden!");
        delay(1500);
        _panelManager->displayStatus("Starte\nKonfig-Portal...");
        WiFi.softAP("Panelclock-Setup");
        mwaveSensorModule->begin();
        
        // Initialize BackupManager also in AP mode for recovery
        _panelManager->displayStatus("Initialisiere\nBackup-System...");
        backupManager = new BackupManager(this);
        backupManager->begin();
        Log.println("[Application] BackupManager initialized (AP mode)");
        
        _panelManager->displayStatus("Starte\nWebserver...");
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
    
    _sofascoreMod->onUpdate(redrawCb);

    _curiousMod->onUpdate(redrawCb);
    _weatherMod->onUpdate(redrawCb);
    _themeParkMod->onUpdate(redrawCb);
    _animationsMod->onUpdate(redrawCb);

    char completionMsg[64];
    snprintf(completionMsg, sizeof(completionMsg), "Start komplett!\nv%s", PANELCLOCK_VERSION);
    _panelManager->displayStatus(completionMsg);
    delay(2000);
    LOG_MEMORY_STRATEGIC("Application: Ende");
}

void Application::update() {
    if (_configNeedsApplying) {
        executeApplyLiveConfig();
        _configNeedsApplying = false;
    }

    // Periodic backup check (once per update cycle - checks internally if backup is needed)
    if (backupManager) {
        static unsigned long lastBackupCheck = 0;
        unsigned long now = millis();
        // Check every hour if automatic backup is needed
        if (now - lastBackupCheck >= 3600000) { // 1 hour
            lastBackupCheck = now;
            backupManager->periodicCheck();
        }
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
    if(_sofascoreMod) _sofascoreMod->queueData();
    if(_calendarMod) _calendarMod->queueData();
    if(_curiousMod) _curiousMod->queueData();
    if(_weatherMod) _weatherMod->queueData(); // HINZUGEFÜGT
    if(_themeParkMod) _themeParkMod->queueData(); // HINZUGEFÜGT
    
    // KORREKTUR: Aufrufe für das Wetter-Modul hinzugefügt
    if(_tankerkoenigMod) _tankerkoenigMod->processData();
    if(_dartsMod) _dartsMod->processData();
    if(_sofascoreMod) _sofascoreMod->processData();
    if(_calendarMod) _calendarMod->processData();
    if(_curiousMod) _curiousMod->processData();
    if(_weatherMod) _weatherMod->processData(); // HINZUGEFÜGT
    if(_themeParkMod) _themeParkMod->processData(); // HINZUGEFÜGT

    if (_panelManager) _panelManager->tick();

#if ENABLE_FRAG_MONITOR
    // Call FragmentationMonitor periodicTick every ~100ms
    if (g_FragMonitor) {
        static unsigned long lastFragMonTick = 0;
        if (millis() - lastFragMonTick >= 100) {
            g_FragMonitor->periodicTick();
            lastFragMonTick = millis();
        }
    }
#endif

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
    if (!_tankerkoenigMod || !_calendarMod || !_dartsMod || !_fritzMod || !_curiousMod || !_weatherMod || !_themeParkMod || !_animationsMod || !_countdownMod || !timeConverter || !deviceConfig) return;
    Log.println("[Config] Wende Live-Konfiguration an...");
    
    // Apply debug file logging setting (immediately active)
    Log.setDebugFileEnabled(deviceConfig->debugFileEnabled);
    
    if (!timeConverter->setTimezone(deviceConfig->timezone.c_str())) {
        timeConverter->setTimezone("UTC");
    }
    
    _tankerkoenigMod->setConfig(deviceConfig->tankerApiKey, deviceConfig->tankerkoenigStationIds, deviceConfig->stationFetchIntervalMin, deviceConfig->stationDisplaySec);
    _calendarMod->setConfig(deviceConfig->icsUrl, deviceConfig->calendarFetchIntervalMin, deviceConfig->calendarDisplaySec, deviceConfig->globalScrollSpeedMs, deviceConfig->calendarDateColor, deviceConfig->calendarTextColor);
    _calendarMod->setUrgentParams(deviceConfig->calendarFastBlinkHours, deviceConfig->calendarUrgentThresholdHours, deviceConfig->calendarUrgentDurationSec, deviceConfig->calendarUrgentRepeatMin);
    _dartsMod->setConfig(deviceConfig->dartsOomEnabled, deviceConfig->dartsProTourEnabled, 5, deviceConfig->dartsDisplaySec, deviceConfig->trackedDartsPlayers);
    _sofascoreMod->setConfig(deviceConfig->dartsSofascoreEnabled, deviceConfig->dartsSofascoreFetchIntervalMin, deviceConfig->dartsSofascoreDisplaySec, deviceConfig->dartsSofascoreTournamentIds, deviceConfig->dartsSofascoreFullscreen, deviceConfig->dartsSofascoreInterruptOnLive, deviceConfig->dartsSofascorePlayNextMinutes, deviceConfig->dartsSofascoreContinuousLive, deviceConfig->dartsSofascoreLiveCheckIntervalSec, deviceConfig->dartsSofascoreLiveDataFetchIntervalSec, deviceConfig->dartsSofascoreTournamentExcludeMode);
    _fritzMod->setConfig(deviceConfig->fritzboxEnabled, deviceConfig->fritzboxIp);
    _curiousMod->setConfig();
    _weatherMod->setConfig(deviceConfig);
    _themeParkMod->setConfig(deviceConfig);
    _animationsMod->setConfig();
    // Countdown module - always available, no config needed

    Log.println("[Config] Live-Konfiguration angewendet.");
    LOG_MEMORY_DETAILED("Nach executeApplyLiveConfig");
}

void Application::prepareForRestart() {
    Log.println("[Application] Bereite sauberes Herunterfahren vor ESP-Neustart vor...");
    
    // Rufe shutdown() für alle DrawableModule-basierten Module auf
    if (_fritzMod) {
        Log.println("[Application] Schließe Fritzbox Callmonitor...");
        _fritzMod->shutdown();
    }
    
    if (_tankerkoenigMod) {
        Log.println("[Application] Fahre TankerkoenigModule herunter...");
        _tankerkoenigMod->shutdown();
    }
    
    if (_calendarMod) _calendarMod->shutdown();
    if (_dartsMod) _dartsMod->shutdown();
    if (_curiousMod) _curiousMod->shutdown();
    if (_weatherMod) _weatherMod->shutdown();
    if (_themeParkMod) _themeParkMod->shutdown();
    if (_animationsMod) _animationsMod->shutdown();
    
    // Note: ClockModule and MwaveSensorModule don't inherit from DrawableModule,
    // so they don't have shutdown() method. They don't need cleanup anyway.
    
    // Flush LittleFS um sicherzustellen, dass alle Dateien geschrieben sind
    Log.println("[Application] Flush LittleFS...");
    // Note: LittleFS doesn't have explicit flush, but closing files handles this
    
    // Stoppe den WebClient um ausstehende Requests abzubrechen
    if (webClient) {
        Log.println("[Application] Stoppe WebClient...");
        // WebClient hat keine explizite stop-Methode, Tasks laufen weiter bis Neustart
    }
    
    // Gib anderen Tasks Zeit zum Abschluss
    delay(100);
    
    Log.println("[Application] Herunterfahren abgeschlossen, bereit für Neustart.");
}
