#ifndef FRITZBOXMODULE_HPP
#define FRITZBOXMODULE_HPP

#include <U8g2_for_Adafruit_GFX.h>
#include <WiFiClient.h>
#include "PsramUtils.hpp"
#include "DrawableModule.hpp"

// Vorwärtsdeklaration
class WebClientModule;

#define FRITZ_PORT 1012
#define FRITZBOX_MAX_DURATION_MS 900000 // 15 Minuten
#define FRITZBOX_CALL_UID 1 // Feste UID für Anruf-Interrupts

class FritzboxModule : public DrawableModule {
public:
    FritzboxModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, WebClientModule* webClient);
    ~FritzboxModule();

    void begin(BaseType_t core);
    void setConfig(bool isEnabled, const PsramString& ip);
    
    // --- Implementierung der Interface-Methoden ---
    void draw() override;
    const char* getModuleName() const override { return "FritzboxModule"; }
    const char* getModuleDisplayName() const override { return "Fritzbox"; }
    
    // Modul-Eigenschaften
    unsigned long getDisplayDuration() override { return 10000; } // Fallback, wird nicht genutzt
    bool isEnabled() override;
    void resetPaging() override {} // Keine Seiten
    bool canBeInPlaylist() const override { return false; } // Nur Interrupts

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    WebClientModule* webClient;
    SemaphoreHandle_t dataMutex;

    volatile bool enabled;
    volatile bool callActive;
    unsigned long lastConnectionAttempt;

    PsramString fritzIp;
    PsramString callerNumber;
    PsramString callerName;
    PsramString callerLocation;
    
    WiFiClient client;

    static void taskRunner(void *pvParameters);
    void taskLoop();
    void parseCallMonitorLine(const String& line);
    void queryCallerInfo(const PsramString& number);
    void parseCallerInfo(const char* buffer, size_t size);
};

#endif // FRITZBOXMODULE_HPP