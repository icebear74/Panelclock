#ifndef OTAMANAGER_HPP
#define OTAMANAGER_HPP

#include <Arduino.h>

// KORREKTUR: Die vollständigen Header werden benötigt, da Template-Argumente und Enum-Werte bekannt sein müssen.
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <U8g2_for_Adafruit_GFX.h>

// Die Vorwärtsdeklarationen sind nicht mehr nötig und werden entfernt.

// Die Definition ist jetzt im Header enthalten, aber wir lassen sie zur Klarheit
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN 

class OtaManager {
public:
    // Konstruktor nimmt Zeiger auf alle benötigten Objekte entgegen
    OtaManager(GFXcanvas16* fullCanvas, MatrixPanel_I2S_DMA* dma_display, VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp, U8G2_FOR_ADAFRUIT_GFX* u8g2);

    // Diese Methode richtet die ArduinoOTA-Callbacks ein
    void begin();

private:
    // Member-Variablen zum Speichern der Zeiger
    GFXcanvas16* _fullCanvas;
    MatrixPanel_I2S_DMA* _dma_display;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2;

    // Private Hilfsfunktionen, die aus der .ino-Datei hierher verschoben wurden
    void drawProgressBar(int x, int y, int width, int height, float percentage, uint16_t borderColor, uint16_t fillColor);
    void displayOtaStatus(const String& line1, const String& line2 = "", const String& line3 = "");
};

#endif // OTAMANAGER_HPP