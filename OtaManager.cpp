#include "OtaManager.hpp"
#include <ArduinoOTA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp> // Für die Implementierung
#include <U8g2_for_Adafruit_GFX.h>             // Für die Implementierung

// Konstanten aus der .ino-Datei
const int FULL_WIDTH = 64 * 3;
const int FULL_HEIGHT = 32 * 3;

OtaManager::OtaManager(GFXcanvas16* fullCanvas, MatrixPanel_I2S_DMA* dma_display, VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp, U8G2_FOR_ADAFRUIT_GFX* u8g2)
    : _fullCanvas(fullCanvas), _dma_display(dma_display), _virtualDisp(virtualDisp), _u8g2(u8g2) {}

void OtaManager::drawProgressBar(int x, int y, int width, int height, float percentage, uint16_t borderColor, uint16_t fillColor) {
    if (!_fullCanvas) return;

    percentage = constrain(percentage, 0.0, 100.0);
    
    // Leeren Rahmen zeichnen
    _fullCanvas->drawRect(x, y, width, height, borderColor);

    // Füllung berechnen und zeichnen
    int innerWidth = width - 4; // 2 Pixel Rand auf jeder Seite
    int innerHeight = height - 4;
    int fillWidth = (int)(innerWidth * (percentage / 100.0));

    if (fillWidth > 0) {
        _fullCanvas->fillRect(x + 2, y + 2, fillWidth, innerHeight, fillColor);
    }
    // Den Rest des inneren Bereichs löschen (um Artefakte von vorherigen Zeichnungen zu entfernen)
    if (fillWidth < innerWidth) {
        _fullCanvas->fillRect(x + 2 + fillWidth, y + 2, innerWidth - fillWidth, innerHeight, 0);
    }
}

void OtaManager::displayOtaStatus(const String& line1, const String& line2, const String& line3) {
  if (!_fullCanvas || !_u8g2) return;
  
  _u8g2->begin(*_fullCanvas);
  _u8g2->setFont(u8g2_font_6x13_tf);
  _u8g2->setForegroundColor(0xFFFF); // Weiß für Text
  _u8g2->setBackgroundColor(0);
  
  int y = 12;
  if (!line1.isEmpty()) { 
      _u8g2->setCursor((_fullCanvas->width() - _u8g2->getUTF8Width(line1.c_str())) / 2, y); 
      _u8g2->print(line1); 
      y += 14; 
  }
  if (!line2.isEmpty()) { 
      _u8g2->setCursor((_fullCanvas->width() - _u8g2->getUTF8Width(line2.c_str())) / 2, y); 
      _u8g2->print(line2); 
      y += 14; 
  }
  if (!line3.isEmpty()) { 
      _u8g2->setCursor((_fullCanvas->width() - _u8g2->getUTF8Width(line3.c_str())) / 2, y); 
      _u8g2->print(line3); 
  }
}

void OtaManager::begin() {
  // Dimensionen und Farben für den Fortschrittsbalken
  const int barWidth = FULL_WIDTH - 40;
  const int barHeight = 15;
  const int barX = (FULL_WIDTH - barWidth) / 2;
  const int barY = 60;
  const uint16_t barBorderColor = 0xFFFF; // Weiß
  const uint16_t progressColor = 0x05E0; // Dunkles Grün

  ArduinoOTA.onStart([this, barWidth, barHeight, barX, barY, barBorderColor, progressColor]() { 
    if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
    String type = ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Filesystem"; 
    
    _fullCanvas->fillScreen(0);
    displayOtaStatus("OTA Update", type + " wird geladen...");
    drawProgressBar(barX, barY, barWidth, barHeight, 0, barBorderColor, progressColor);
    
    _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
    _dma_display->flipDMABuffer();
  });

  ArduinoOTA.onEnd([this, barWidth, barHeight, barX, barY, barBorderColor, progressColor]() { 
    if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
    
    drawProgressBar(barX, barY, barWidth, barHeight, 100, barBorderColor, progressColor);
    displayOtaStatus("OTA Update", "Fertig.", "Neustart..."); 
    
    _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
    _dma_display->flipDMABuffer();
    delay(1500); 
  });

  ArduinoOTA.onProgress([this, barWidth, barHeight, barX, barY, barBorderColor, progressColor](unsigned int progress, unsigned int total) { 
    if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
    float percentage = (total > 0) ? (float)progress / (float)total * 100.0 : 0;
    
    drawProgressBar(barX, barY, barWidth, barHeight, percentage, barBorderColor, progressColor);

    _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
    _dma_display->flipDMABuffer();
  });

  ArduinoOTA.onError([this](ota_error_t error) { 
    if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
    String msg; 
    switch (error) { 
      case OTA_AUTH_ERROR: msg = "Auth Fehler"; break; 
      case OTA_BEGIN_ERROR: msg = "Begin Fehler"; break; 
      case OTA_CONNECT_ERROR: msg = "Verbindungsfehler"; break; 
      case OTA_RECEIVE_ERROR: msg = "Empfangsfehler"; break; 
      case OTA_END_ERROR: msg = "End Fehler"; break; 
      default: msg = "Unbekannter Fehler"; break;
    }
    _fullCanvas->fillScreen(0);
    displayOtaStatus("OTA FEHLER:", msg);
    _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
    _dma_display->flipDMABuffer();
    delay(3000);
  });
}