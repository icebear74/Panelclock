#ifndef APPLICATION_HPP
#define APPLICATION_HPP

// Alle notwendigen Header für die Module und Manager
#include "HardwareConfig.hpp"
#include "webconfig.hpp"
#include "ConnectionManager.hpp" // *** UM BENANNT ***
#include "GeneralTimeConverter.hpp"
#include "WebServerManager.hpp"
#include "WebClientModule.hpp"
#include "MwaveSensorModule.hpp"
#include "ClockModule.hpp"
#include "DataModule.hpp"
#include "CalendarModule.hpp"
#include "DartsRankingModule.hpp"
#include "FritzboxModule.hpp"

// *** KORRIGIERT: Notwendige Header direkt einbinden statt fehlerhafter Vorwärtsdeklaration ***
#include <U8g2_for_Adafruit_GFX.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>

class Application {
public:
    Application() {}

    void begin() {
        // --- WIRD IN PHASE 4 IMPLEMENTIERT ---
    }

    void update() {
        // --- WIRD IN PHASE 4 IMPLEMENTIERT ---
    }

private:
    HardwareConfig* hardwareConfig = nullptr;
    DeviceConfig* deviceConfig = nullptr;
    
    WebServer* server = nullptr;
    DNSServer* dnsServer = nullptr;

    ConnectionManager* connectionManager = nullptr; // *** UM BENANNT ***
    GeneralTimeConverter* timeConverter = nullptr;
    WebClientModule* webClient = nullptr;
    MwaveSensorModule* mwaveSensorModule = nullptr;

    ClockModule* clockMod = nullptr;
    DataModule* dataMod = nullptr;
    CalendarModule* calendarMod = nullptr;
    DartsRankingModule* dartsMod = nullptr;
    FritzboxModule* fritzMod = nullptr;

    U8G2_FOR_ADAFRUIT_GFX* u8g2 = nullptr;
    GFXcanvas16* canvasTime = nullptr;
    GFXcanvas16* canvasData = nullptr;
    MatrixPanel_I2S_DMA* dma_display = nullptr;
    VirtualMatrixPanel_T<CHAIN_TOP_LEFT_DOWN>* virtualDisp = nullptr; 
};

#endif // APPLICATION_HPP