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

class FritzboxModule : public DrawableModule {
public:
    FritzboxModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, WebClientModule* webClient);
    ~FritzboxModule();

    void begin(BaseType_t core);
    void setConfig(bool isEnabled, const PsramString& ip);
    
    // --- Implementierung der Interface-Methoden ---
    void draw() override;
    Priority getPriority() const override { return Priority::Highest; }
    unsigned long getMaxInterruptDuration() const override { return FRITZBOX_MAX_DURATION_MS; }

    // NEU: Implementierung der fehlenden puren virtuellen Funktionen
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;

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