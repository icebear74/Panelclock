#include "CountdownModule.hpp"
#include "webconfig.hpp"
#include "MultiLogger.hpp"
#include <Arduino.h>

CountdownModule::CountdownModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref,
                                 const GeneralTimeConverter& timeConverter_ref, DeviceConfig* config)
    : u8g2(u8g2_ref), canvas(canvas_ref), _currentCanvas(&canvas_ref),
      timeConverter(timeConverter_ref), config(config) {
}

CountdownModule::~CountdownModule() {
    stopCountdown();
}

void CountdownModule::onUpdate(std::function<void()> callback) {
    updateCallback = callback;
}

void CountdownModule::setConfig(bool enabled, uint32_t durationMinutes, unsigned long displaySec) {
    this->_enabled = enabled;
    this->_durationMinutes = durationMinutes > 0 ? durationMinutes : 15;  // Default to 15 minutes
    this->_displayDuration = displaySec > 0 ? displaySec * 1000UL : 20000;
    
    Log.printf("[Countdown] Config updated: enabled=%d, duration=%d min, display=%d sec\n",
               enabled, _durationMinutes, displaySec);
}

bool CountdownModule::isEnabled() {
    return _enabled;
}

unsigned long CountdownModule::getDisplayDuration() {
    // If countdown is running, return 0 for continuous display
    if (_isRunning) {
        return 0;  // Continuous display - module controls its own lifetime
    }
    return _displayDuration;
}

void CountdownModule::resetPaging() {
    _isFinished = false;
    _blinkPhase = 0;
    
    // Release interrupt if active
    if (_hasActiveInterrupt && _interruptUID > 0) {
        releasePriorityEx(_interruptUID);
        _hasActiveInterrupt = false;
        Log.println("[Countdown] Released interrupt on reset");
    }
}

bool CountdownModule::startCountdown() {
    if (_isRunning) {
        Log.println("[Countdown] Already running, ignoring start request");
        return false;
    }
    
    _isRunning = true;
    _startTimeMillis = millis();
    _targetDurationMs = (unsigned long)_durationMinutes * 60000UL;
    _isFinished = false;
    _blinkPhase = 0;
    
    Log.printf("[Countdown] Started: duration=%d minutes (%lu ms)\n", _durationMinutes, _targetDurationMs);
    
    // Request high priority interrupt to show countdown immediately
    _interruptUID = COUNTDOWN_INTERRUPT_UID_BASE + 1;
    _hasActiveInterrupt = true;
    
    // Use total duration + 1 second buffer for interrupt timeout
    unsigned long interruptDuration = _targetDurationMs + 1000;
    if (requestPriorityEx(Priority::High, _interruptUID, interruptDuration)) {
        Log.printf("[Countdown] Interrupt requested with UID %u for %lu ms\n", _interruptUID, interruptDuration);
    } else {
        Log.println("[Countdown] Interrupt request failed");
        _hasActiveInterrupt = false;
    }
    
    if (updateCallback) {
        updateCallback();
    }
    
    return true;
}

void CountdownModule::stopCountdown() {
    if (!_isRunning) {
        return;
    }
    
    _isRunning = false;
    _isFinished = true;
    
    Log.println("[Countdown] Stopped");
    
    // Release interrupt if active
    if (_hasActiveInterrupt && _interruptUID > 0) {
        releasePriorityEx(_interruptUID);
        _hasActiveInterrupt = false;
        Log.println("[Countdown] Released interrupt on stop");
    }
    
    if (updateCallback) {
        updateCallback();
    }
}

void CountdownModule::tick() {
    // Animation tick for blinking effects
    _blinkPhase = (_blinkPhase + 1) % 20;  // Blink cycle every 20 ticks
    
    if (_isRunning && updateCallback) {
        updateCallback();  // Update display for millisecond precision
    }
}

void CountdownModule::logicTick() {
    // Check if countdown has finished
    if (_isRunning) {
        unsigned long elapsed = millis() - _startTimeMillis;
        if (elapsed >= _targetDurationMs) {
            Log.println("[Countdown] Finished!");
            stopCountdown();
        }
    }
}

void CountdownModule::periodicTick() {
    // Nothing to do in periodic tick
}

void CountdownModule::calculateRemainingTime(unsigned long& minutes, unsigned long& seconds, unsigned long& millis) const {
    if (!_isRunning) {
        minutes = 0;
        seconds = 0;
        millis = 0;
        return;
    }
    
    unsigned long elapsed = ::millis() - _startTimeMillis;
    
    if (elapsed >= _targetDurationMs) {
        minutes = 0;
        seconds = 0;
        millis = 0;
        return;
    }
    
    unsigned long remaining = _targetDurationMs - elapsed;
    
    minutes = remaining / 60000;
    seconds = (remaining % 60000) / 1000;
    millis = remaining % 1000;
}

float CountdownModule::calculatePercentComplete() const {
    if (!_isRunning || _targetDurationMs == 0) {
        return 0.0f;
    }
    
    unsigned long elapsed = millis() - _startTimeMillis;
    
    if (elapsed >= _targetDurationMs) {
        return 100.0f;
    }
    
    return (float)elapsed / (float)_targetDurationMs * 100.0f;
}

float CountdownModule::calculateCaloriesBurned() const {
    if (!_isRunning) {
        return 0.0f;
    }
    
    unsigned long elapsed = millis() - _startTimeMillis;
    
    if (elapsed >= _targetDurationMs) {
        elapsed = _targetDurationMs;
    }
    
    // Convert to minutes and multiply by calories per minute
    float minutes = (float)elapsed / 60000.0f;
    return minutes * CALORIES_PER_MINUTE;
}

void CountdownModule::draw() {
    // Choose the correct canvas based on fullscreen mode
    _currentCanvas = wantsFullscreen() ? _fullscreenCanvas : &canvas;
    if (!_currentCanvas) {
        Log.println("[Countdown] draw() - No valid canvas!");
        return;
    }
    
    _currentCanvas->fillScreen(0);
    u8g2.begin(*_currentCanvas);
    
    if (!_isRunning) {
        // Show "Countdown stopped" message
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF);
        const char* msg = "Countdown stopped";
        int msgWidth = u8g2.getUTF8Width(msg);
        u8g2.setCursor((_currentCanvas->width() - msgWidth) / 2, _currentCanvas->height() / 2);
        u8g2.print(msg);
        return;
    }
    
    drawCountdown();
}

void CountdownModule::drawCountdown() {
    // Calculate values
    unsigned long mins, secs, ms;
    calculateRemainingTime(mins, secs, ms);
    float percent = calculatePercentComplete();
    float calories = calculateCaloriesBurned();
    
    int y = 20;  // Starting Y position
    
    // Line 1: Title "COUNTDOWN"
    u8g2.setFont(u8g2_font_profont12_tf);
    
    // Blink red when less than 10 seconds remaining
    if (mins == 0 && secs < 10) {
        // Blink red/white
        if (_blinkPhase < 10) {
            u8g2.setForegroundColor(0xF800);  // Red
        } else {
            u8g2.setForegroundColor(0xFFFF);  // White
        }
    } else {
        u8g2.setForegroundColor(0x07FF);  // Cyan
    }
    
    const char* title = "COUNTDOWN";
    int titleWidth = u8g2.getUTF8Width(title);
    u8g2.setCursor((_currentCanvas->width() - titleWidth) / 2, y);
    u8g2.print(title);
    
    y += 18;
    
    // Line 2: Time remaining (MM:SS.mmm)
    u8g2.setFont(u8g2_font_profont15_tf);
    u8g2.setForegroundColor(0xFFFF);  // White
    
    char timeStr[16];
    snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu.%03lu", mins, secs, ms);
    int timeWidth = u8g2.getUTF8Width(timeStr);
    u8g2.setCursor((_currentCanvas->width() - timeWidth) / 2, y);
    u8g2.print(timeStr);
    
    y += 14;
    
    // Line 3: Percentage
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.setForegroundColor(0xFFE0);  // Yellow
    
    char percentStr[16];
    snprintf(percentStr, sizeof(percentStr), "%.1f%%", percent);
    int percentWidth = u8g2.getUTF8Width(percentStr);
    u8g2.setCursor((_currentCanvas->width() - percentWidth) / 2, y);
    u8g2.print(percentStr);
    
    y += 12;
    
    // Line 4: Percentage bar
    drawPercentageBar(percent);
    
    y += 12;
    
    // Line 5: Calories burned
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.setForegroundColor(0x07E0);  // Green
    
    char caloriesStr[32];
    snprintf(caloriesStr, sizeof(caloriesStr), "%.1f kcal", calories);
    int caloriesWidth = u8g2.getUTF8Width(caloriesStr);
    u8g2.setCursor((_currentCanvas->width() - caloriesWidth) / 2, y);
    u8g2.print(caloriesStr);
}

void CountdownModule::drawPercentageBar(float percent) {
    if (!_currentCanvas) return;
    
    const int BAR_WIDTH = _currentCanvas->width() - 20;  // 10px margin on each side
    const int BAR_HEIGHT = 8;
    const int BAR_X = 10;
    const int BAR_Y = _currentCanvas->height() - 20;
    
    // Draw border
    _currentCanvas->drawRect(BAR_X, BAR_Y, BAR_WIDTH, BAR_HEIGHT, 0xFFFF);  // White border
    
    // Draw filled portion
    int fillWidth = (int)((float)BAR_WIDTH * percent / 100.0f);
    if (fillWidth > BAR_WIDTH - 2) fillWidth = BAR_WIDTH - 2;  // Don't overflow border
    if (fillWidth > 0) {
        _currentCanvas->fillRect(BAR_X + 1, BAR_Y + 1, fillWidth, BAR_HEIGHT - 2, 0x07E0);  // Green fill
    }
}
