#include "PixelScroller.hpp"
#include <math.h>

PixelScroller::PixelScroller(U8G2_FOR_ADAFRUIT_GFX& u8g2, uint32_t configuredScrollSpeedMs)
    : _u8g2(u8g2), _configuredScrollSpeedMs(configuredScrollSpeedMs) {
    // Standardkonfiguration
    _config = PixelScrollerConfig();
}

PixelScroller::~PixelScroller() {
    // PsramVector räumt automatisch auf
}

void PixelScroller::setConfig(const PixelScrollerConfig& config) {
    _config = config;
}

void PixelScroller::setConfiguredScrollSpeed(uint32_t ms) {
    _configuredScrollSpeedMs = ms;
}

uint32_t PixelScroller::getEffectiveScrollSpeed() const {
    // Effektive Geschwindigkeit = konfigurierte Geschwindigkeit / Teiler
    if (_config.scrollSpeedDivider == 0) return _configuredScrollSpeedMs;
    return _configuredScrollSpeedMs / _config.scrollSpeedDivider;
}

bool PixelScroller::tick() {
    bool anyUpdated = false;
    
    for (auto& state : _scrollStates) {
        if (updateScrollState(state)) {
            anyUpdated = true;
        }
    }
    
    return anyUpdated;
}

void PixelScroller::reset() {
    _scrollStates.clear();
}

void PixelScroller::resetSlot(size_t slotIndex) {
    if (slotIndex < _scrollStates.size()) {
        _scrollStates[slotIndex] = PixelScrollState();
    }
}

void PixelScroller::ensureSlots(size_t count) {
    if (_scrollStates.size() != count) {
        _scrollStates.resize(count);
    }
}

bool PixelScroller::updateScrollState(PixelScrollState& state) {
    if (state.status == ScrollerStatus::IDLE || state.status == ScrollerStatus::FINISHED) {
        return false;
    }
    
    uint32_t now = millis();
    uint32_t effectiveSpeed = getEffectiveScrollSpeed();
    
    // Minimale Geschwindigkeit von 1ms
    if (effectiveSpeed < 1) effectiveSpeed = 1;
    
    // Pause-Behandlung
    if (state.status == ScrollerStatus::PAUSING) {
        if (_config.pauseBetweenCyclesMs > 0 && 
            (now - state.pauseStartTime) >= _config.pauseBetweenCyclesMs) {
            // Pause beendet, weiter scrollen
            state.status = ScrollerStatus::SCROLLING;
            state.lastScrollTime = now;
        }
        return false;
    }
    
    // Prüfen ob genug Zeit vergangen ist
    if ((now - state.lastScrollTime) < effectiveSpeed) {
        return false;
    }
    
    state.lastScrollTime = now;
    
    // Scrolling-Logik je nach Modus
    switch (_config.mode) {
        case ScrollMode::CONTINUOUS:
            // Kontinuierliches Scrollen - Text läuft von rechts nach links durch
            state.pixelOffset++;
            if (state.pixelOffset >= state.maxPixelOffset) {
                // Zyklus beendet
                if (_config.pauseBetweenCyclesMs > 0) {
                    // Pause starten
                    state.status = ScrollerStatus::PAUSING;
                    state.pauseStartTime = now;
                    state.pixelOffset = 0;
                } else {
                    // Sofort von vorne
                    state.pixelOffset = 0;
                }
            }
            break;
            
        case ScrollMode::PINGPONG:
            // PingPong - Text scrollt hin und her
            if (state.pingPongDirection) {
                // Vorwärts
                state.pixelOffset++;
                if (state.pixelOffset >= state.maxPixelOffset) {
                    state.pingPongDirection = false;
                    if (_config.pauseBetweenCyclesMs > 0) {
                        state.status = ScrollerStatus::PAUSING;
                        state.pauseStartTime = now;
                    }
                }
            } else {
                // Rückwärts
                state.pixelOffset--;
                if (state.pixelOffset <= 0) {
                    state.pingPongDirection = true;
                    if (_config.pauseBetweenCyclesMs > 0) {
                        state.status = ScrollerStatus::PAUSING;
                        state.pauseStartTime = now;
                    }
                }
            }
            break;
            
        case ScrollMode::NONE:
        default:
            return false;
    }
    
    return true;
}

int PixelScroller::calculateTextWidth(const char* text) const {
    if (!text || text[0] == '\0') return 0;
    return _u8g2.getUTF8Width(text);
}

void PixelScroller::initScrollState(PixelScrollState& state, int textWidth, int visibleWidth) {
    state.textWidthPixels = textWidth;
    state.visibleWidthPixels = visibleWidth;
    state.pixelOffset = 0;
    state.pingPongDirection = true;
    state.lastScrollTime = millis();
    state.pauseStartTime = 0;
    
    if (_config.mode == ScrollMode::CONTINUOUS) {
        // Bei kontinuierlichem Scrolling: Text + Padding + nochmal Text
        // Der Offset muss so weit gehen, bis der Text komplett durchgescrollt ist
        state.maxPixelOffset = textWidth + _config.paddingPixels;
    } else if (_config.mode == ScrollMode::PINGPONG) {
        // Bei PingPong: Nur so weit scrollen, bis der Text-Anfang sichtbar wird
        state.maxPixelOffset = textWidth - visibleWidth;
        if (state.maxPixelOffset < 0) state.maxPixelOffset = 0;
    } else {
        state.maxPixelOffset = 0;
    }
    
    state.status = (state.maxPixelOffset > 0) ? ScrollerStatus::SCROLLING : ScrollerStatus::IDLE;
}

bool PixelScroller::drawScrollingText(GFXcanvas16& canvas, const char* text, int x, int y,
                                      int maxWidth, size_t slotIndex, uint16_t overrideColor) {
    if (!text || text[0] == '\0') return false;
    
    // Slot sicherstellen
    if (slotIndex >= _scrollStates.size()) {
        _scrollStates.resize(slotIndex + 1);
    }
    
    PixelScrollState& state = _scrollStates[slotIndex];
    int textWidth = calculateTextWidth(text);
    
    // Farbe bestimmen
    uint16_t color = (overrideColor != 0) ? overrideColor : _config.textColor;
    
    // Prüfen ob Text passt
    if (textWidth <= maxWidth) {
        // Text passt - einfach zeichnen, kein Scrolling nötig
        state.status = ScrollerStatus::IDLE;
        state.maxPixelOffset = 0;
        _u8g2.setForegroundColor(color);
        _u8g2.setCursor(x, y);
        _u8g2.print(text);
        return false;
    }
    
    // Text muss gescrollt werden
    // Zustand initialisieren wenn nötig
    if (state.status == ScrollerStatus::IDLE || 
        state.textWidthPixels != textWidth ||
        state.visibleWidthPixels != maxWidth) {
        initScrollState(state, textWidth, maxWidth);
    }
    
    // Text mit Clipping zeichnen
    drawClippedText(canvas, text, x, y, maxWidth, state.pixelOffset, color);
    
    return true;
}

bool PixelScroller::drawScrollingTextWithPulse(GFXcanvas16& canvas, const char* text, int x, int y,
                                               int maxWidth, size_t slotIndex, uint16_t baseColor,
                                               bool fastPulse) {
    // Pulsing-Farbe berechnen
    float periodMs = fastPulse ? 1000.0f : 2000.0f;
    uint16_t pulsedColor = calculatePulsedColor(baseColor, _config.pulsingMinBrightness, periodMs);
    
    return drawScrollingText(canvas, text, x, y, maxWidth, slotIndex, pulsedColor);
}

uint16_t PixelScroller::calculatePulsedColor(uint16_t baseColor, float minBrightness, float periodMs) {
    // Berechne Pulse-Faktor mit Cosinus für sanfte Übergänge
    float cosInput = (millis() % (int)periodMs) / periodMs * 2.0f * PI;
    float pulseFactor = minBrightness + (1.0f - minBrightness) * (cos(cosInput) + 1.0f) / 2.0f;
    
    return dimColor(baseColor, pulseFactor);
}

uint16_t PixelScroller::dimColor(uint16_t color, float brightness) {
    if (brightness >= 1.0f) return color;
    if (brightness <= 0.0f) return 0;
    
    // RGB565 extrahieren
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5) & 0x3F;
    uint8_t b = color & 0x1F;
    
    // Dimmen
    r = (uint8_t)(r * brightness);
    g = (uint8_t)(g * brightness);
    b = (uint8_t)(b * brightness);
    
    // Zurück zu RGB565
    return (r << 11) | (g << 5) | b;
}

void PixelScroller::drawClippedText(GFXcanvas16& canvas, const char* text, int x, int y,
                                    int maxWidth, int pixelOffset, uint16_t color) {
    // Bei kontinuierlichem Scrolling zeichnen wir den Text zweimal:
    // 1. Normaler Text mit Offset
    // 2. Text am Ende für nahtlosen Übergang
    
    _u8g2.setForegroundColor(color);
    
    int textWidth = calculateTextWidth(text);
    
    if (_config.mode == ScrollMode::CONTINUOUS) {
        // Kontinuierliches Scrolling - Text + Padding + Text (nahtlos)
        int padding = _config.paddingPixels;
        int totalWidth = textWidth + padding;
        
        // Erster Text
        int textX = x - pixelOffset;
        if (textX + textWidth > x) {
            // Clipping durch Canvas-Grenzen - wir zeichnen den Text und lassen die Canvas clippen
            // Für pixelgenaues Clipping müssten wir zeichenweise arbeiten, 
            // aber das wäre sehr langsam. Stattdessen zeichnen wir einfach und 
            // überschreiben den überlaufenden Bereich später mit dem Hintergrund
            // ODER wir nutzen einen temporären Bereich
            
            // Einfache Methode: Text zeichnen, ist leicht außerhalb sichtbar
            // Das ist OK für LED-Panels
            _u8g2.setCursor(textX, y);
            _u8g2.print(text);
        }
        
        // Zweiter Text für nahtlosen Übergang
        int secondTextX = textX + totalWidth;
        if (secondTextX < x + maxWidth && secondTextX + textWidth > x) {
            _u8g2.setCursor(secondTextX, y);
            _u8g2.print(text);
        }
        
    } else if (_config.mode == ScrollMode::PINGPONG) {
        // PingPong - einfach verschieben
        _u8g2.setCursor(x - pixelOffset, y);
        _u8g2.print(text);
        
    } else {
        // Kein Scrolling
        _u8g2.setCursor(x, y);
        _u8g2.print(text);
    }
}

bool PixelScroller::isScrolling(size_t slotIndex) const {
    if (slotIndex >= _scrollStates.size()) return false;
    const auto& state = _scrollStates[slotIndex];
    return state.status == ScrollerStatus::SCROLLING || state.status == ScrollerStatus::PAUSING;
}
