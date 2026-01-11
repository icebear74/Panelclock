#ifndef COUNTDOWN_MODULE_HPP
#define COUNTDOWN_MODULE_HPP

#include <U8g2_for_Adafruit_GFX.h>
#include <functional>
#include "DrawableModule.hpp"
#include "GeneralTimeConverter.hpp"

#if __has_include("gfxcanvas.h")
  #include "gfxcanvas.h"
#elif __has_include(<gfxcanvas.h>)
  #include <gfxcanvas.h>
#elif __has_include("GFXcanvas.h")
  #include "GFXcanvas.h"
#elif __has_include(<GFXcanvas.h>)
  #include <GFXcanvas.h>
#elif __has_include("Adafruit_GFX.h")
  #include <Adafruit_GFX.h>
#else
  #error "gfxcanvas header not found. Please install the Adafruit GFX library."
#endif

struct DeviceConfig;

// UID for countdown interrupts
#define COUNTDOWN_INTERRUPT_UID_BASE 6000

/**
 * @brief Countdown module with millisecond precision, percentage display, and calorie tracking
 * 
 * Features:
 * - Configurable duration (default: 15 minutes)
 * - Start via web interface
 * - Display with milliseconds (MM:SS.mmm)
 * - Progress percentage bar
 * - Calorie burn calculation (6 cal/min base rate = 90 cal / 15 min)
 * - Priority interrupt when started
 */
class CountdownModule : public DrawableModule {
public:
    CountdownModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref,
                    const GeneralTimeConverter& timeConverter_ref, DeviceConfig* config);
    ~CountdownModule();

    void onUpdate(std::function<void()> callback);
    void setConfig(bool enabled, uint32_t durationMinutes, unsigned long displaySec);
    
    /**
     * @brief Start the countdown timer
     * @return true if countdown was started successfully
     */
    bool startCountdown();
    
    /**
     * @brief Stop the countdown timer
     */
    void stopCountdown();
    
    /**
     * @brief Check if countdown is currently running
     * @return true if countdown is active
     */
    bool isRunning() const { return _isRunning; }

    // DrawableModule Interface
    const char* getModuleName() const override { return "CountdownModule"; }
    const char* getModuleDisplayName() const override { return "Countdown"; }
    void draw() override;
    void tick() override;
    void logicTick() override;
    void periodicTick() override;
    unsigned long getDisplayDuration() override;
    bool isEnabled() override;
    void resetPaging() override;
    int getCurrentPage() const override { return 0; }
    int getTotalPages() const override { return 1; }
    
    // Fullscreen support
    bool supportsFullscreen() const override { return true; }
    bool wantsFullscreen() const override { return _wantsFullscreen && _fullscreenCanvas != nullptr; }

private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    GFXcanvas16* _currentCanvas = nullptr;  // Points to either &canvas or _fullscreenCanvas
    const GeneralTimeConverter& timeConverter;
    DeviceConfig* config;
    std::function<void()> updateCallback;

    // Configuration
    bool _enabled = false;
    bool _wantsFullscreen = true;  // Default to fullscreen for better visibility
    uint32_t _durationMinutes = 15;  // Default: 15 minutes
    unsigned long _displayDuration = 20000;  // 20 seconds per page
    
    // Countdown state
    bool _isRunning = false;
    unsigned long _startTimeMillis = 0;  // millis() when countdown started
    unsigned long _targetDurationMs = 0;  // Total duration in milliseconds
    
    // Interrupt management
    bool _hasActiveInterrupt = false;
    uint32_t _interruptUID = 0;
    
    // Animation
    uint8_t _blinkPhase = 0;  // For blinking effects
    
    // Constants
    static constexpr float CALORIES_PER_MINUTE = 6.0f;  // 90 cal / 15 min = 6 cal/min
    
    // Helper methods
    void calculateRemainingTime(unsigned long& minutes, unsigned long& seconds, unsigned long& millis) const;
    float calculatePercentComplete() const;
    float calculateCaloriesBurned() const;
    void drawCountdown();
    void drawPercentageBar(float percent);
};

#endif // COUNTDOWN_MODULE_HPP
