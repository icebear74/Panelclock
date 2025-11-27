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
    // Mindestens 1ms um Division durch Null zu vermeiden
    _configuredScrollSpeedMs = (ms > 0) ? ms : 1;
}

uint32_t PixelScroller::getEffectiveScrollSpeed() const {
    // Konfigurierte Geschwindigkeit direkt verwenden (kein Teiler mehr)
    return (_configuredScrollSpeedMs > 0) ? _configuredScrollSpeedMs : 1;
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
            if (_config.scrollReverse) {
                // Rückwärts-Scrolling: Text erscheint von links, scrollt nach rechts
                state.pixelOffset--;
                if (state.pixelOffset <= -state.maxPixelOffset) {
                    // Zyklus beendet - Text ist wieder am Anfang
                    if (_config.pauseBetweenCyclesMs > 0) {
                        state.status = ScrollerStatus::PAUSING;
                        state.pauseStartTime = now;
                    }
                    state.pixelOffset = 0;
                }
            } else {
                // Vorwärts-Scrolling: Text läuft von rechts nach links durch
                state.pixelOffset++;
                if (state.pixelOffset >= state.maxPixelOffset) {
                    // Zyklus beendet - Text ist wieder am Anfang
                    if (_config.pauseBetweenCyclesMs > 0) {
                        state.status = ScrollerStatus::PAUSING;
                        state.pauseStartTime = now;
                    }
                    state.pixelOffset = 0;
                }
            }
            break;
            
        case ScrollMode::PINGPONG:
            // PingPong - Text scrollt hin und her
            // Bei PingPong ist die Pause am Ende eines Hin-und-Her-Zyklus
            // (also wenn der Text wieder am Anfang steht)
            if (state.pingPongDirection) {
                // Vorwärts (oder Rückwärts bei scrollReverse)
                if (_config.scrollReverse) {
                    state.pixelOffset--;
                    if (state.pixelOffset <= -state.maxPixelOffset) {
                        state.pingPongDirection = false;
                    }
                } else {
                    state.pixelOffset++;
                    if (state.pixelOffset >= state.maxPixelOffset) {
                        state.pingPongDirection = false;
                    }
                }
            } else {
                // Zurück zum Anfang
                if (_config.scrollReverse) {
                    state.pixelOffset++;
                    if (state.pixelOffset >= 0) {
                        state.pingPongDirection = true;
                        state.pixelOffset = 0;
                        // Pause nach komplettem Hin-und-Her-Zyklus
                        if (_config.pauseBetweenCyclesMs > 0) {
                            state.status = ScrollerStatus::PAUSING;
                            state.pauseStartTime = now;
                        }
                    }
                } else {
                    state.pixelOffset--;
                    if (state.pixelOffset <= 0) {
                        state.pingPongDirection = true;
                        state.pixelOffset = 0;
                        // Pause nach komplettem Hin-und-Her-Zyklus
                        if (_config.pauseBetweenCyclesMs > 0) {
                            state.status = ScrollerStatus::PAUSING;
                            state.pauseStartTime = now;
                        }
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
    // Pixelweises Clipping: Wir zeichnen zeichenweise und überspringen Zeichen/Pixel
    // die außerhalb des sichtbaren Bereichs (x bis x+maxWidth) liegen
    
    if (!text || text[0] == '\0' || maxWidth <= 0) return;
    
    _u8g2.setForegroundColor(color);
    
    int textWidth = calculateTextWidth(text);
    int padding = _config.paddingPixels;
    int totalWidth = textWidth + padding;  // Für kontinuierliches Scrolling
    
    // Für PingPong: Text scrollt hin und her innerhalb des Bereichs
    if (_config.mode == ScrollMode::PINGPONG) {
        // pixelOffset gibt an, wie weit der Text nach links/rechts verschoben wird
        drawTextWithClipping(text, x, y, maxWidth, pixelOffset);
        return;
    }
    
    // Kontinuierliches Scrolling: Text läuft durch, erscheint sofort wieder
    if (_config.mode == ScrollMode::CONTINUOUS) {
        if (_config.scrollReverse) {
            // Rückwärts-Scrolling: Text kommt von links
            // pixelOffset ist negativ oder 0
            // Bei pixelOffset = 0: Text ist am Anfang (links ausgerichtet)
            // Bei pixelOffset = -totalWidth: Text ist einmal durchgescrollt
            
            // Erster Text
            drawTextWithClipping(text, x, y, maxWidth, pixelOffset);
            
            // Zweiter Text für nahtlosen Übergang (kommt von links)
            int secondOffset = pixelOffset + totalWidth;
            if (secondOffset > 0 && secondOffset < maxWidth + textWidth) {
                drawTextWithClipping(text, x, y, maxWidth, secondOffset);
            }
        } else {
            // Vorwärts-Scrolling: Text geht nach links
            // pixelOffset ist positiv oder 0
            
            // Erster Text (wird nach links gescrollt)
            drawTextWithClipping(text, x, y, maxWidth, pixelOffset);
            
            // Zweiter Text für nahtlosen Übergang (kommt von rechts)
            int secondOffset = pixelOffset - totalWidth;
            if (secondOffset < 0 && secondOffset > -(maxWidth + textWidth)) {
                drawTextWithClipping(text, x, y, maxWidth, secondOffset);
            }
        }
        return;
    }
    
    // Kein Scrolling - einfach zeichnen (mit Clipping)
    drawTextWithClipping(text, x, y, maxWidth, 0);
}

void PixelScroller::drawTextWithClipping(const char* text, int clipX, int y, 
                                         int clipWidth, int pixelOffset) {
    // Zeichnet Text mit Clipping an beiden Kanten (links UND rechts)
    // pixelOffset: wie weit der Text nach links verschoben ist (positive Werte = nach links)
    // Der Text wird geclippt zwischen clipX (links) und clipX + clipWidth (rechts)
    
    if (!text || text[0] == '\0' || clipWidth <= 0) return;
    
    // Berechne wo der Text im virtuellen Raum startet
    int virtualTextX = clipX - pixelOffset;  // Ohne Clipping wäre Text hier
    int rightClipX = clipX + clipWidth;  // Rechte Clipping-Grenze
    
    // Wenn der gesamte Text rechts außerhalb des sichtbaren Bereichs ist, nichts zeichnen
    int textWidth = calculateTextWidth(text);
    if (virtualTextX >= rightClipX) return;
    
    // Wenn der gesamte Text links außerhalb ist, nichts zeichnen
    if (virtualTextX + textWidth <= clipX) return;
    
    // Zeichenweises Rendering mit Clipping an BEIDEN Kanten
    int currentX = virtualTextX;
    const char* ptr = text;
    
    while (*ptr != '\0') {
        // UTF-8: Ermittle die Länge des aktuellen Zeichens
        int charLen = 1;
        unsigned char c = (unsigned char)*ptr;
        if (c >= 0xC0 && c < 0xE0) charLen = 2;
        else if (c >= 0xE0 && c < 0xF0) charLen = 3;
        else if (c >= 0xF0) charLen = 4;
        
        // Temporären String für dieses Zeichen erstellen
        char charBuf[5] = {0};
        for (int i = 0; i < charLen && ptr[i] != '\0'; i++) {
            charBuf[i] = ptr[i];
        }
        
        int charWidth = _u8g2.getUTF8Width(charBuf);
        int charEndX = currentX + charWidth;
        
        // Zeichen ist komplett links vom sichtbaren Bereich - überspringen
        if (charEndX <= clipX) {
            currentX = charEndX;
            ptr += charLen;
            continue;
        }
        
        // Zeichen beginnt komplett rechts vom sichtbaren Bereich - fertig
        // Da wir von links nach rechts durch den Text gehen, sind alle weiteren
        // Zeichen ebenfalls rechts außerhalb
        if (currentX >= rightClipX) {
            break;
        }
        
        // Zeichen ist im sichtbaren Bereich - aber NUR zeichnen wenn es KOMPLETT 
        // im Bereich passt (sowohl links als auch rechts)
        // Das verhindert, dass Zeichen den Bereich rechts davon überschreiben
        if (currentX >= clipX && charEndX <= rightClipX) {
            _u8g2.setCursor(currentX, y);
            _u8g2.print(charBuf);
        }
        
        currentX = charEndX;
        ptr += charLen;
    }
}

bool PixelScroller::isScrolling(size_t slotIndex) const {
    if (slotIndex >= _scrollStates.size()) return false;
    const auto& state = _scrollStates[slotIndex];
    return state.status == ScrollerStatus::SCROLLING || state.status == ScrollerStatus::PAUSING;
}
