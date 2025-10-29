#ifndef FRITZBOXMODULE_HPP
#define FRITZBOXMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <WiFiClient.h> // Include for WiFiClient declaration

// Vorwärtsdeklaration
class WebClientModule;

#define FRITZ_PORT 1012

class FritzboxModule {
public:
    FritzboxModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, WebClientModule* webClient);
    ~FritzboxModule();

    void begin(BaseType_t core);
    void setConfig(bool isEnabled, const PsramString& ip);
    
    bool isEnabled();
    bool isCallActive();
    bool redrawNeeded();
    void draw();

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    WebClientModule* webClient;
    SemaphoreHandle_t dataMutex;

    volatile bool enabled;
    volatile bool callActive;
    volatile bool needsRedraw;
    unsigned long lastConnectionAttempt;

    PsramString fritzIp;
    PsramString callerNumber;
    PsramString callerName;
    PsramString callerLocation;
    
    WiFiClient client;

    // Statische Task-Funktion
    static void taskRunner(void *pvParameters);

    // Private Methoden
    void taskLoop();
    void parseCallMonitorLine(const String& line);
    void queryCallerInfo(const PsramString& number);
    void parseCallerInfo(const char* buffer, size_t size);
};

#endif // FRITZBOXMODULE_HPP