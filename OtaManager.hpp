#ifndef OTAMANAGER_HPP
#define OTAMANAGER_HPP

#include <Arduino.h>

// Vollständige Headers für Display-Objekte
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <U8g2_for_Adafruit_GFX.h>

#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN

class OtaManager {
public:
    OtaManager(GFXcanvas16* fullCanvas, MatrixPanel_I2S_DMA* dma_display, VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp, U8G2_FOR_ADAFRUIT_GFX* u8g2);

    // Richte ArduinoOTA callbacks ein
    void begin();

private:
    // Display-Objekte
    GFXcanvas16* _fullCanvas;
    MatrixPanel_I2S_DMA* _dma_display;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp;
    U8G2_FOR_ADAFRUIT_GFX* _u8g2;

    // Alte Helper (Kompatibilität)
    void drawProgressBar(int x, int y, int width, int height, float percentage, uint16_t borderColor, uint16_t fillColor);
    void displayOtaStatus(const String& line1, const String& line2 = "", const String& line3 = "");

    // Neue Helper für Pac-Man Anzeige / Victory
    // textColor hat Default-Wert Weiß, damit bestehende Aufrufe weiterhin funktionieren
    void displayOtaTextCentered(const String& line1, const String& line2 = "", const String& line3 = "", uint16_t textColor = 0xFFFF);
    void drawPacmanProgressSmooth(float percentage);
    void victoryFireworksLoop();
};

#endif // OTAMANAGER_HPP