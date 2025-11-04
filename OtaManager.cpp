#include "OtaManager.hpp"
#include <ArduinoOTA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include <U8g2_for_Adafruit_GFX.h>
#include <math.h>
#include <esp_system.h> // für esp_random()
#include <algorithm>
#include <vector>

using std::min;
using std::max;

// Konstanten (wie im Projekt)
const int FULL_WIDTH = 64 * 3;
const int FULL_HEIGHT = 32 * 3;

OtaManager::OtaManager(GFXcanvas16* fullCanvas, MatrixPanel_I2S_DMA* dma_display, VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp, U8G2_FOR_ADAFRUIT_GFX* u8g2)
    : _fullCanvas(fullCanvas), _dma_display(dma_display), _virtualDisp(virtualDisp), _u8g2(u8g2) {}

// Helper: RGB -> 16bit 565
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Zentrischer Text-Renderer, nutzt u8g2
void OtaManager::displayOtaTextCentered(const String& line1, const String& line2, const String& line3, uint16_t textColor) {
    if (!_fullCanvas || !_u8g2) return;
    _u8g2->begin(*_fullCanvas);
    _u8g2->setFont(u8g2_font_6x13_tf);
    _u8g2->setForegroundColor(textColor);
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

// Zufallszahl im Bereich [low, highExclusive)
static inline int randRange(int low, int highExclusive) {
    if (highExclusive <= low) return low;
    uint32_t r = esp_random();
    return low + (int)(r % (uint32_t)(highExclusive - low));
}

// --- Emoji drawing (inlined) ---
enum EmojiKind { Emoji_Angry = 0, Emoji_Warning = 1, Emoji_Happy = 2 };

// draw a simple filled arc-like smile by drawing multiple short lines
static void drawSmileArc(GFXcanvas16* c, int cx, int cy, int radius, int thickness, float startDeg, float endDeg, uint16_t color) {
    const int steps = max(6, (int)radius * 2);
    for (int s = 0; s < steps; ++s) {
        float t = (float)s / (float)(steps - 1);
        float deg = startDeg + t * (endDeg - startDeg);
        float rad = deg * M_PI / 180.0f;
        int x = cx + (int)round(cosf(rad) * (radius));
        int y = cy + (int)round(sinf(rad) * (radius));
        for (int w = -thickness/2; w <= thickness/2; ++w) {
            c->drawPixel(x, y + w, color);
        }
    }
}

// Draw a compact emoji into the canvas.
// cx,cy = center; size = radius in pixels (approx 8..24)
static void drawEmoji(GFXcanvas16* canvas, int cx, int cy, int size, EmojiKind kind) {
    if (!canvas || size < 6) return;

    const uint16_t white = rgb565(255,255,255);
    const uint16_t black = rgb565(0,0,0);

    uint16_t faceCol = rgb565(200,200,200);
    uint16_t accent = black;

    switch (kind) {
        case Emoji_Angry:
            faceCol = rgb565(200,20,20); // deep red
            accent = rgb565(40,10,10);
            break;
        case Emoji_Warning:
            faceCol = rgb565(240,150,20); // orange
            accent = rgb565(100,60,0);
            break;
        case Emoji_Happy:
            faceCol = rgb565(40,180,40); // green
            accent = rgb565(0,80,0);
            break;
    }

    // Face
    canvas->fillCircle(cx, cy, size, faceCol);
    canvas->drawCircle(cx, cy, size, black);

    int ex = max(1, size/3);
    int ey = -max(1, size/6);
    int eyeR = max(1, size/6);

    // Eyes (white + pupil)
    canvas->fillCircle(cx - ex, cy + ey, eyeR, white);
    canvas->fillCircle(cx - ex, cy + ey, max(1, eyeR-1), black);
    canvas->fillCircle(cx + ex, cy + ey, eyeR, white);
    canvas->fillCircle(cx + ex, cy + ey, max(1, eyeR-1), black);

    if (kind == Emoji_Angry) {
        int browY = cy + ey - eyeR - 2;
        // left eyebrow (slanted inwards)
        canvas->fillTriangle(cx - ex - eyeR - 1, browY+2,
                             cx - ex + eyeR, browY - 4,
                             cx - ex + eyeR + 2, browY + 2, accent);
        // right eyebrow (mirrored)
        canvas->fillTriangle(cx + ex + eyeR + 1, browY+2,
                             cx + ex - eyeR, browY - 4,
                             cx + ex - eyeR - 2, browY + 2, accent);
        // mouth: black rectangle with white "teeth"
        int mw = size; int mh = max(2, size/3);
        int mx = cx - mw/2; int my = cy + size/3;
        canvas->fillRect(mx, my, mw, mh, black);
        int teeth = max(2, mw / 6);
        for (int t = 0; t < teeth; ++t) {
            int tx = mx + (t * (mw / teeth));
            int tw = max(1, mw / (teeth*2));
            canvas->fillRect(tx + 2, my + 1, tw, mh - 2, white);
        }
    } else if (kind == Emoji_Warning) {
        int browY = cy + ey - eyeR - 2;
        canvas->drawLine(cx - ex - 6, browY - 2, cx - ex + 2, browY + 2, accent);
        canvas->drawLine(cx + ex + 6, browY - 2, cx + ex - 2, browY + 2, accent);
        drawSmileArc(canvas, cx, cy + size/4, size/3, 1, 200.0f, 340.0f, accent);
        uint16_t blue = rgb565(0,120,200);
        canvas->fillCircle(cx + size/2 - 4, cy - size/2 + 6, max(1,size/6), blue);
    } else {
        drawSmileArc(canvas, cx, cy + size/4, size/3, 2, 200.0f, 340.0f, black);
    }
}

// --- particle/explosion helpers ---
struct Particle {
    float x, y;
    float vx, vy;
    uint16_t color;
    float prevx, prevy;
};

static void drawParticle(GFXcanvas16* canvas, const Particle& p) {
    int px = (int)round(p.x);
    int py = (int)round(p.y);
    canvas->fillCircle(px, py, 1, p.color);
}

static void spawnParticlesFromSource(std::vector<Particle>& parts, float sx, float sy, uint16_t baseColor, int baseCount) {
    for (int i = 0; i < baseCount; ++i) {
        Particle P;
        float angle = ((esp_random() & 0xFFFF) / 65535.0f) * 2.0f * M_PI;
        float speed = 0.6f + (esp_random() % 120) / 100.0f;
        P.x = sx; P.y = sy;
        P.vx = cosf(angle) * speed * (1.0f + ((esp_random() & 0x3F) / 255.0f));
        P.vy = sinf(angle) * speed * (1.0f + ((esp_random() & 0x3F) / 255.0f)) - 0.04f;
        uint8_t r = (((baseColor >> 11) & 0x1F) << 3) + (esp_random() & 31);
        uint8_t g = (((baseColor >> 5) & 0x3F) << 2) + (esp_random() & 31);
        uint8_t b = ((baseColor & 0x1F) << 3) + (esp_random() & 31);
        P.color = rgb565(min((int)r,255), min((int)g,255), min((int)b,255));
        P.prevx = P.x; P.prevy = P.y;
        parts.push_back(P);
    }
}

// --- ghost state (3 ghosts) with stable diagonal movement ---
struct GhostState { float x,y,vx,vy; int vy_sign; uint16_t color; };
static GhostState s_ghosts[3];
static bool s_ghostsInit = false;
static unsigned long s_lastGhostMillis = 0;

// Draw a ghost shape (reuse)
static void drawGhostShapeLocal(GFXcanvas16* canvas, int gx, int gy, uint16_t color) {
    int r = 9;
    canvas->fillCircle(gx, gy - 6, r, color);
    canvas->fillRect(gx - r, gy - 6, r * 2, r + 8, color);
    canvas->fillCircle(gx - 6, gy + 6, 3, color);
    canvas->fillCircle(gx, gy + 6, 3, color);
    canvas->fillCircle(gx + 6, gy + 6, 3, color);
    canvas->fillCircle(gx - 4, gy - 4, 2, 0xFFFF);
    canvas->fillCircle(gx + 3, gy - 4, 2, 0xFFFF);
    canvas->fillCircle(gx - 4, gy - 4, 1, 0x0000);
    canvas->fillCircle(gx + 3, gy - 4, 1, 0x0000);
}

// Explosion without clearing full screen: erase only shapes and animate particles
static void playExplosionFromSources_NoClear(GFXcanvas16* canvas, VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* virtualDisp, MatrixPanel_I2S_DMA* dma_display,
                                             int pacX, int pacY, int pacR, uint16_t pacColor,
                                             GhostState* ghosts, int ghostCount,
                                             const std::vector<std::pair<int,int>>& eatenDotPos,
                                             uint16_t dotColor) {
    if (!canvas || !virtualDisp || !dma_display) return;

    std::vector<Particle> parts;
    parts.reserve(300);

    spawnParticlesFromSource(parts, (float)pacX, (float)pacY, pacColor, 80);
    for (int g = 0; g < ghostCount; ++g) spawnParticlesFromSource(parts, ghosts[g].x, ghosts[g].y, ghosts[g].color, 50);

    int take = min((int)eatenDotPos.size(), 40);
    for (int i = 0; i < take; ++i) {
        int sx = eatenDotPos[eatenDotPos.size() - 1 - i].first;
        int sy = eatenDotPos[eatenDotPos.size() - 1 - i].second;
        spawnParticlesFromSource(parts, (float)sx, (float)sy, dotColor, 3);
    }

    while ((int)parts.size() < 200) spawnParticlesFromSource(parts, pacX + randRange(-8,9), pacY + randRange(-6,7), pacColor, 1);

    // Erase only local regions (pacman, ghosts, dots) so the rest of the display remains visible
    canvas->fillCircle(pacX, pacY, pacR + 4, 0);
    for (int g = 0; g < ghostCount; ++g) {
        int gx = (int)round(ghosts[g].x);
        int gy = (int)round(ghosts[g].y);
        canvas->fillRect(gx - 12, gy - 16, 24, 28, 0);
    }
    for (size_t i = 0; i < eatenDotPos.size(); ++i) {
        int dx = eatenDotPos[i].first;
        int dy = eatenDotPos[i].second;
        canvas->fillCircle(dx, dy, 3, 0);
    }

    const int frames = 50;
    const int delayMs = 100;
    for (int f = 0; f < frames; ++f) {
        float lifePct = (float)f / (float)max(1, frames - 1);
        for (size_t i = 0; i < parts.size(); ++i) {
            int erx = (int)round(parts[i].prevx);
            int ery = (int)round(parts[i].prevy);
            if (erx >= 0 && erx < FULL_WIDTH && ery >= 0 && ery < FULL_HEIGHT) canvas->fillCircle(erx, ery, 1, 0);

            parts[i].x += parts[i].vx * (1.0f + lifePct * 0.6f);
            parts[i].y += parts[i].vy * (1.0f + lifePct * 0.6f);
            parts[i].vy += 0.03f * lifePct;
            parts[i].prevx = parts[i].x; parts[i].prevy = parts[i].y;

            drawParticle(canvas, parts[i]);
        }
        virtualDisp->drawRGBBitmap(0, 0, canvas->getBuffer(), canvas->width(), canvas->height());
        dma_display->flipDMABuffer();
        delay(delayMs);
    }

    for (int p = 0; p < 30; ++p) {
        int sx = randRange(8, FULL_WIDTH - 8);
        int sy = randRange(FULL_HEIGHT/2 - 8, FULL_HEIGHT/2 + 28);
        canvas->fillCircle(sx, sy, 1 + (esp_random() & 1), rgb565(randRange(120,255), randRange(120,255), randRange(120,255)));
    }
    virtualDisp->drawRGBBitmap(0, 0, canvas->getBuffer(), canvas->width(), canvas->height());
    dma_display->flipDMABuffer();
    delay(300);
}

// --- Pac-Man + Ghosts animation ----------------------------------
// Draw Pacman, dots and moving ghosts. Ghosts roam full-screen but move diagonally (vy from vx sign stable).
void OtaManager::drawPacmanProgressSmooth(float percentage) {
    if (!_fullCanvas || !_virtualDisp || !_dma_display || !_u8g2) return;

    const uint16_t bg = 0x0000;
    const uint16_t pacmanColor = rgb565(255, 204, 0);
    const uint16_t dotColor = 0xFFFF;
    const uint16_t eatenDotColor = rgb565(120, 80, 40);
    const uint16_t textColor = 0xFFFF;

    const int marginX = 10;
    const int baselineY = FULL_HEIGHT / 2 + 6;
    const int pacmanRadius = 11;
    const int baseTotalDots = 80;
    const int usableWidth = FULL_WIDTH - 2 * marginX;

    float nominalSpacing = (float)usableWidth / (float)(baseTotalDots - 1);
    int subsample = 1;
    if (nominalSpacing < 5.0f) subsample = 2;
    if (nominalSpacing < 3.5f) subsample = 3;

    int totalDots = baseTotalDots;
    float spacing = (float)usableWidth / (float)(totalDots - 1);
    int startX = marginX;
    int cy = baselineY;

    // Clear full canvas to avoid artifacts; then redraw title (keeps title visually consistent)
    _fullCanvas->fillScreen(bg);
    displayOtaTextCentered("OTA Update", "");

    float exactPos = (percentage / 100.0f) * (float)(totalDots - 1);
    int eatenCount = (int)floor(exactPos);

    // draw dots
    std::vector<std::pair<int,int>> dotPositions;
    for (int i = 0; i < totalDots; ++i) {
        if ((i % subsample) != 0) continue;
        int x = startX + (int)round(i * spacing);
        dotPositions.emplace_back(x, cy);
        if (i < eatenCount) _fullCanvas->fillCircle(x, cy, 2, eatenDotColor);
        else _fullCanvas->fillCircle(x, cy, 2, dotColor);
    }

    // initialize ghosts with stable diagonal direction (vy_sign) and initial vx/vy
    if (!s_ghostsInit) {
        s_ghosts[0].x = FULL_WIDTH * 0.20f; s_ghosts[0].y = cy - 20; s_ghosts[0].vx = 0.6f; s_ghosts[0].vy_sign = 1; s_ghosts[0].color = rgb565(255,0,100); // pink
        s_ghosts[1].x = FULL_WIDTH * 0.50f; s_ghosts[1].y = cy - 28; s_ghosts[1].vx = -0.5f; s_ghosts[1].vy_sign = -1; s_ghosts[1].color = rgb565(0,180,255); // cyan
        s_ghosts[2].x = FULL_WIDTH * 0.80f; s_ghosts[2].y = cy - 22; s_ghosts[2].vx = 0.45f; s_ghosts[2].vy_sign = 1; s_ghosts[2].color = rgb565(60,200,60); // green
        s_ghostsInit = true;
        s_lastGhostMillis = millis();
    }

    // pacman pos
    float pacXf = startX + exactPos * spacing;
    int pacX = (int)round(pacXf);
    int pacY = cy;

    // update ghost positions; keep diagonal movement by deriving vy from vx magnitude and vy_sign
    unsigned long now = millis();
    float dt = (s_lastGhostMillis == 0) ? 0.03f : (float)(now - s_lastGhostMillis) / 1000.0f;
    s_lastGhostMillis = now;

    for (int g = 0; g < 3; ++g) {
        // small jitter occasionally for liveliness (horizontal only)
        if ((esp_random() & 0xFF) < 12) {
            float jitter = (randRange(-10,11) / 400.0f);
            s_ghosts[g].vx += jitter;
            if (s_ghosts[g].vx > 1.6f) s_ghosts[g].vx = 1.6f;
            if (s_ghosts[g].vx < -1.6f) s_ghosts[g].vx = -1.6f;
        }

        // enforce diagonal: vy magnitude proportional to vx
        float diagFactor = 0.55f;
        float vxmag = fabsf(s_ghosts[g].vx);
        int sign = (s_ghosts[g].vy_sign >= 0) ? 1 : -1;
        float targetVy = sign * vxmag * diagFactor;

        // smooth approach to targetVy to avoid instant flips
        s_ghosts[g].vy = s_ghosts[g].vy * 0.7f + targetVy * 0.3f;

        // apply movement scaled by dt
        s_ghosts[g].x += s_ghosts[g].vx * (100.0f * dt);
        s_ghosts[g].y += s_ghosts[g].vy * (100.0f * dt);

        // horizontal wrap-around
        if (s_ghosts[g].x < -20) s_ghosts[g].x = FULL_WIDTH + 20;
        if (s_ghosts[g].x > FULL_WIDTH + 20) s_ghosts[g].x = -20;

        // vertical bounce but keep vy_sign stable; flip vy_sign when hitting bounds
        int minY = 10;
        int maxY = FULL_HEIGHT - 10;
        if (s_ghosts[g].y < minY) {
            s_ghosts[g].y = minY;
            s_ghosts[g].vy_sign = 1;
            s_ghosts[g].vy = fabsf(s_ghosts[g].vx) * diagFactor;
        }
        if (s_ghosts[g].y > maxY) {
            s_ghosts[g].y = maxY;
            s_ghosts[g].vy_sign = -1;
            s_ghosts[g].vy = -fabsf(s_ghosts[g].vx) * diagFactor;
        }

        // draw ghost
        drawGhostShapeLocal(_fullCanvas, (int)round(s_ghosts[g].x), (int)round(s_ghosts[g].y), s_ghosts[g].color);
    }

    // proximity-based mouth angle for Pac-Man (smooth)
    int nextIndex = min(totalDots - 1, eatenCount + 1);
    float nextDotXf = startX + nextIndex * spacing;
    float distToNextDot = fabsf(pacXf - nextDotXf);
    float chompRadius = spacing * 0.9f;
    static float prevMouthAngle = 8.0f;
    float mouthAngle = 8.0f;

    if (distToNextDot < chompRadius) {
        float proximity = 1.0f - (distToNextDot / chompRadius);
        proximity = max(0.0f, min(1.0f, proximity));
        unsigned long t = millis();
        float cycleMs = 90.0f * (1.0f - proximity * 0.5f);
        if (cycleMs < 36.0f) cycleMs = 36.0f;
        float phase = fmodf((float)t, cycleMs) / cycleMs;
        float env = (phase < 0.35f) ? (phase / 0.35f) : 1.0f - ((phase - 0.35f) / 0.65f);
        env = max(0.0f, min(1.0f, env));
        float biteStrength = powf(proximity, 0.9f) * env;
        mouthAngle = 6.0f + biteStrength * 46.0f;
    } else {
        unsigned long t = millis();
        float slow = sinf((float)t / 520.0f) * 0.5f + 0.5f;
        mouthAngle = 6.0f + slow * 6.0f;
    }
    float smoothing = 0.45f;
    float finalAngle = prevMouthAngle * (1.0f - smoothing) + mouthAngle * smoothing;
    prevMouthAngle = finalAngle;
    float a1 = -finalAngle * M_PI / 180.0f;
    float a2 = finalAngle * M_PI / 180.0f;

    // draw Pac-Man on top
    _fullCanvas->fillCircle(pacX, pacY, pacmanRadius, pacmanColor);
    int mx1 = pacX + (int)(cosf(a1) * pacmanRadius);
    int my1 = pacY + (int)(sinf(a1) * pacmanRadius);
    int mx2 = pacX + (int)(cosf(a2) * pacmanRadius);
    int my2 = pacY + (int)(sinf(a2) * pacmanRadius);
    _fullCanvas->fillTriangle(pacX, pacY, mx1, my1, mx2, my2, bg);
    int eyeX = pacX + (int)(pacmanRadius * 0.25f);
    int eyeY = pacY - (int)(pacmanRadius * 0.35f);
    _fullCanvas->fillCircle(eyeX, eyeY, 2, 0x0000);

    // percent text
    _u8g2->begin(*_fullCanvas);
    _u8g2->setFont(u8g2_font_6x13_tf);
    _u8g2->setForegroundColor(textColor);
    char buf[32];
    snprintf(buf, sizeof(buf), "Fortschritt: %.0f %%", percentage);
    _u8g2->setCursor((FULL_WIDTH - _u8g2->getUTF8Width(buf)) / 2, FULL_HEIGHT - 6);
    _u8g2->print(buf);

    _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
    _dma_display->flipDMABuffer();
}

void OtaManager::begin() {
    ArduinoOTA.onStart([this]() {
        if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
        String type = ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Filesystem";
        drawPacmanProgressSmooth(0.0f);
        displayOtaTextCentered("OTA Update", type + " wird geladen...", "");
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
        float percentage = (total > 0) ? (float)progress / (float)total * 100.0f : 0.0f;
        drawPacmanProgressSmooth(percentage);
    });

    ArduinoOTA.onEnd([this]() {
        if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
        drawPacmanProgressSmooth(100.0f);
        displayOtaTextCentered("OTA Update", "Fertig!", "");
        delay(600);
        const int marginX = 10;
        const int baseTotalDots = 80;
        const int usableWidth = FULL_WIDTH - 2 * marginX;
        float spacing = (float)usableWidth / (float)(baseTotalDots - 1);
        const int baselineY = FULL_HEIGHT / 2 + 6;
        std::vector<std::pair<int,int>> eatenDots;
        int startDotIndex = max(0, baseTotalDots - 24);
        for (int i = startDotIndex; i < baseTotalDots; i += 2) {
            int dx = marginX + (int)round(i * spacing);
            int dy = baselineY;
            eatenDots.emplace_back(dx, dy);
        }
        int pacX = marginX + (int)round((baseTotalDots - 1) * spacing);
        int pacY = baselineY;
        playExplosionFromSources_NoClear(_fullCanvas, _virtualDisp, _dma_display, pacX, pacY, 11, rgb565(255,200,30), s_ghosts, 3, eatenDots, rgb565(120,80,40));
    });

    ArduinoOTA.onError([this](ota_error_t error) {
        if (!_fullCanvas || !_dma_display || !_virtualDisp) return;
        String msg;
        EmojiKind ek = Emoji_Angry;
        switch (error) {
            case OTA_AUTH_ERROR: msg = "Auth Fehler"; ek = Emoji_Angry; break;
            case OTA_BEGIN_ERROR: msg = "Begin Fehler"; ek = Emoji_Angry; break;
            case OTA_CONNECT_ERROR: msg = "Verbindungsfehler"; ek = Emoji_Warning; break;
            case OTA_RECEIVE_ERROR: msg = "Empfangsfehler"; ek = Emoji_Angry; break;
            case OTA_END_ERROR: msg = "End Fehler"; ek = Emoji_Angry; break;
            default: msg = "Unbekannter Fehler"; ek = Emoji_Angry; break;
        }

        if (_fullCanvas && _u8g2) {
            // Clear and redraw header+emoji to ensure clean top area
            _fullCanvas->fillRect(0, 0, FULL_WIDTH, 56, 0);
            drawEmoji(_fullCanvas, FULL_WIDTH/2 - 24, 22, 18, ek);
            displayOtaTextCentered("OTA FEHLER:", msg, "");
        }

        _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), _fullCanvas->width(), _fullCanvas->height());
        _dma_display->flipDMABuffer();
        delay(3000);
    });
}