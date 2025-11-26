#ifndef PIXELSCROLLER_HPP
#define PIXELSCROLLER_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"

/**
 * @brief Scroll-Modus für den PixelScroller
 */
enum class ScrollMode {
    NONE,       ///< Kein Scrolling (Text passt oder soll nicht scrollen)
    CONTINUOUS, ///< Kontinuierliches Scrollen (Text läuft endlos durch)
    PINGPONG    ///< Ping-Pong Scrolling (hin und her)
};

/**
 * @brief Status des Scrollers
 */
enum class ScrollerStatus {
    IDLE,       ///< Noch nicht gestartet oder pausiert
    SCROLLING,  ///< Aktives Scrolling
    PAUSING,    ///< In der Pause zwischen Scroll-Zyklen
    FINISHED    ///< Einmaliges Scrolling beendet
};

/**
 * @brief Konfiguration für einen PixelScroller
 */
struct PixelScrollerConfig {
    ScrollMode mode = ScrollMode::CONTINUOUS;   ///< Scroll-Modus
    uint32_t pauseBetweenCyclesMs = 0;          ///< Pause zwischen Scroll-Zyklen in ms (0 = keine Pause)
    uint32_t scrollSpeedDivider = 5;            ///< Teiler für die Scroll-Geschwindigkeit (configuredMs / divider)
    uint16_t textColor = 0xFFFF;                ///< Textfarbe (RGB565)
    bool enablePulsing = false;                 ///< Pulsing/Blinken aktivieren
    float pulsingMinBrightness = 0.25f;         ///< Minimale Helligkeit beim Pulsing (0.0 - 1.0)
    float pulsingPeriodMs = 2000.0f;            ///< Periode des Pulsings in ms
    int paddingPixels = 20;                     ///< Pixelabstand zwischen Ende und erneutem Anfang des Textes
};

/**
 * @brief Zustand eines einzelnen Scroll-Slots
 * 
 * Wird in PSRAM alloziert für speichereffiziente Nutzung.
 */
struct PixelScrollState {
    int32_t pixelOffset = 0;            ///< Aktueller Pixel-Offset
    int32_t maxPixelOffset = 0;         ///< Maximaler Pixel-Offset für den Text
    int32_t textWidthPixels = 0;        ///< Breite des Textes in Pixeln
    int32_t visibleWidthPixels = 0;     ///< Sichtbare Breite in Pixeln
    bool pingPongDirection = true;      ///< true = vorwärts, false = rückwärts (für PingPong)
    uint32_t lastScrollTime = 0;        ///< Letzte Scroll-Aktualisierung (millis)
    uint32_t pauseStartTime = 0;        ///< Startzeit der aktuellen Pause
    ScrollerStatus status = ScrollerStatus::IDLE;
    
    // PSRAM-Allokation
    void* operator new(size_t size) {
        void* p = ps_malloc(size);
        if (!p) {
            Serial.println("FATAL: PixelScrollState PSRAM-Allokation fehlgeschlagen!");
        }
        return p;
    }
    
    void operator delete(void* p) {
        free(p);
    }
    
    // Für Vektor-Allokation
    void* operator new[](size_t size) {
        void* p = ps_malloc(size);
        if (!p) {
            Serial.println("FATAL: PixelScrollState[] PSRAM-Allokation fehlgeschlagen!");
        }
        return p;
    }
    
    void operator delete[](void* p) {
        free(p);
    }
};

/**
 * @brief Wiederverwendbare Bibliothek für pixelweises Text-Scrolling
 * 
 * Diese Bibliothek bietet pixelweises Scrolling mit folgenden Features:
 * - PSRAM-basierte Speicherung aller Zustände
 * - PingPong-Scrolling (Text scrollt hin und her)
 * - Scroll-Pausen (scrolle einmal, pausiere X Sekunden, wiederhole)
 * - Pulsing/Blinken-Unterstützung mit konfigurierbarer Farbe
 * - Konfigurierbarer Geschwindigkeits-Teiler
 * 
 * Verwendung:
 * 1. Erstelle eine PixelScroller-Instanz (einmal pro Modul)
 * 2. Rufe tick() in der tick()-Methode des Moduls auf
 * 3. Rufe drawScrollingText() in der draw()-Methode auf
 */
class PixelScroller {
public:
    /**
     * @brief Konstruktor
     * @param u8g2 Referenz auf die U8G2-Instanz für Font-Messungen
     * @param configuredScrollSpeedMs Konfigurierte Scroll-Geschwindigkeit aus Webinterface
     */
    PixelScroller(U8G2_FOR_ADAFRUIT_GFX& u8g2, uint32_t configuredScrollSpeedMs = 250);
    
    ~PixelScroller();
    
    /**
     * @brief Setzt die globale Konfiguration
     * @param config Die neue Konfiguration
     */
    void setConfig(const PixelScrollerConfig& config);
    
    /**
     * @brief Setzt die konfigurierte Scroll-Geschwindigkeit (aus Webinterface)
     * @param ms Scroll-Geschwindigkeit in Millisekunden
     */
    void setConfiguredScrollSpeed(uint32_t ms);
    
    /**
     * @brief Tick-Methode - muss regelmäßig aufgerufen werden (z.B. in tick())
     * 
     * Aktualisiert alle aktiven Scroll-Zustände basierend auf der verstrichenen Zeit.
     * 
     * @return true wenn mindestens ein Scroll-Zustand aktualisiert wurde
     */
    bool tick();
    
    /**
     * @brief Setzt alle Scroll-Zustände zurück
     */
    void reset();
    
    /**
     * @brief Setzt einen spezifischen Slot zurück
     * @param slotIndex Index des Slots
     */
    void resetSlot(size_t slotIndex);
    
    /**
     * @brief Passt die Anzahl der Scroll-Slots an
     * @param count Benötigte Anzahl an Slots
     */
    void ensureSlots(size_t count);
    
    /**
     * @brief Zeichnet scrollenden Text
     * 
     * Diese Methode berechnet, ob der Text gescrollt werden muss, und zeichnet
     * ihn entsprechend. Wenn der Text passt, wird er einfach gezeichnet.
     * 
     * @param canvas Die Canvas zum Zeichnen
     * @param text Der zu zeichnende Text
     * @param x X-Position der linken Kante
     * @param y Y-Position der Baseline
     * @param maxWidth Maximale Breite in Pixeln
     * @param slotIndex Index des Scroll-Slots (für mehrere parallele Scrolls)
     * @param overrideColor Optionale Farbüberschreibung (0 = Config-Farbe verwenden)
     * @return true wenn der Text gescrollt wird, false wenn er passt
     */
    bool drawScrollingText(GFXcanvas16& canvas, const char* text, int x, int y, 
                          int maxWidth, size_t slotIndex, uint16_t overrideColor = 0);
    
    /**
     * @brief Zeichnet scrollenden Text mit Pulsing
     * 
     * Wie drawScrollingText(), aber mit aktiviertem Pulsing/Blinken.
     * 
     * @param canvas Die Canvas zum Zeichnen
     * @param text Der zu zeichnende Text
     * @param x X-Position der linken Kante
     * @param y Y-Position der Baseline
     * @param maxWidth Maximale Breite in Pixeln
     * @param slotIndex Index des Scroll-Slots
     * @param baseColor Basis-Farbe (wird gepulsed)
     * @param fastPulse true für schnelles Pulsing (1000ms), false für langsames (2000ms)
     * @return true wenn der Text gescrollt wird
     */
    bool drawScrollingTextWithPulse(GFXcanvas16& canvas, const char* text, int x, int y,
                                    int maxWidth, size_t slotIndex, uint16_t baseColor,
                                    bool fastPulse = false);
    
    /**
     * @brief Berechnet die gepulste Farbe basierend auf der aktuellen Zeit
     * @param baseColor Basis-Farbe
     * @param minBrightness Minimale Helligkeit (0.0 - 1.0)
     * @param periodMs Periode des Pulsings in ms
     * @return Gedimmte Farbe
     */
    static uint16_t calculatePulsedColor(uint16_t baseColor, float minBrightness, float periodMs);
    
    /**
     * @brief Dimmt eine RGB565-Farbe
     * @param color Die zu dimmende Farbe
     * @param brightness Helligkeit (0.0 - 1.0)
     * @return Gedimmte Farbe
     */
    static uint16_t dimColor(uint16_t color, float brightness);
    
    /**
     * @brief Getter für die aktuelle Konfiguration
     */
    const PixelScrollerConfig& getConfig() const { return _config; }
    
    /**
     * @brief Getter für die effektive Scroll-Geschwindigkeit in ms
     */
    uint32_t getEffectiveScrollSpeed() const;
    
    /**
     * @brief Prüft ob ein Slot gerade scrollt
     * @param slotIndex Index des Slots
     * @return true wenn der Slot aktiv scrollt
     */
    bool isScrolling(size_t slotIndex) const;
    
    /**
     * @brief Getter für die Anzahl der Slots
     */
    size_t getSlotCount() const { return _scrollStates.size(); }

private:
    U8G2_FOR_ADAFRUIT_GFX& _u8g2;
    PixelScrollerConfig _config;
    uint32_t _configuredScrollSpeedMs;
    
    // Scroll-Zustände in PSRAM-Vector
    PsramVector<PixelScrollState> _scrollStates;
    
    /**
     * @brief Berechnet die Textbreite in Pixeln
     * @param text Der Text
     * @return Breite in Pixeln
     */
    int calculateTextWidth(const char* text) const;
    
    /**
     * @brief Initialisiert einen Scroll-Zustand für einen Text
     * @param state Der Zustand
     * @param textWidth Textbreite in Pixeln
     * @param visibleWidth Sichtbare Breite in Pixeln
     */
    void initScrollState(PixelScrollState& state, int textWidth, int visibleWidth);
    
    /**
     * @brief Aktualisiert einen einzelnen Scroll-Zustand
     * @param state Der Zustand
     * @return true wenn aktualisiert wurde
     */
    bool updateScrollState(PixelScrollState& state);
    
    /**
     * @brief Zeichnet den Text mit Clipping
     * @param canvas Die Canvas
     * @param text Der Text
     * @param x X-Position (wird um Offset verschoben)
     * @param y Y-Position
     * @param maxWidth Maximale Breite
     * @param pixelOffset Aktueller Pixel-Offset
     * @param color Textfarbe
     */
    void drawClippedText(GFXcanvas16& canvas, const char* text, int x, int y,
                         int maxWidth, int pixelOffset, uint16_t color);
    
    /**
     * @brief Zeichnet Text mit Clipping an der linken Kante
     * @param text Der zu zeichnende Text
     * @param clipX Linke Clipping-Grenze
     * @param y Y-Position der Baseline
     * @param clipWidth Breite des sichtbaren Bereichs
     * @param pixelOffset Pixel-Offset (wie weit nach links verschoben)
     */
    void drawTextWithLeftClip(const char* text, int clipX, int y, 
                              int clipWidth, int pixelOffset);
};

#endif // PIXELSCROLLER_HPP
