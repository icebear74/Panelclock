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
    
    /**
     * @brief Set countdown duration (non-persistent, for current session only)
     * @param durationMinutes Duration in minutes
     */
    void setDuration(uint32_t durationMinutes);
    
    /**
     * @brief Start the countdown timer with current or specified duration
     * @param durationMinutes Optional duration override (0 = use current setting)
     * @return true if countdown was started successfully
     */
    bool startCountdown(uint32_t durationMinutes = 0);
    
    /**
     * @brief Stop the countdown timer
     */
    void stopCountdown();
    
    /**
     * @brief Pause the countdown timer
     * @return true if paused successfully
     */
    bool pauseCountdown();
    
    /**
     * @brief Resume the countdown timer from pause
     * @return true if resumed successfully
     */
    bool resumeCountdown();
    
    /**
     * @brief Reset the countdown timer to original duration
     */
    void resetCountdown();
    
    /**
     * @brief Check if countdown is currently running
     * @return true if countdown is active
     */
    bool isRunning() const { return _isRunning; }
    
    /**
     * @brief Check if countdown is currently paused
     * @return true if countdown is paused
     */
    bool isPaused() const { return _isPaused; }

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
    
    // Countdown is utility-only, not in normal playlist
    bool canBeInPlaylist() const override { return false; }
    
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
    bool _wantsFullscreen = false;  // Normal size display (not fullscreen)
    uint32_t _durationMinutes = 15;  // Default: 15 minutes (non-persistent, session only)
    
    // Countdown state
    bool _isRunning = false;
    bool _isPaused = false;
    unsigned long _startTimeMillis = 0;  // millis() when countdown started
    unsigned long _pausedTimeMillis = 0;  // millis() when paused
    unsigned long _totalPausedMs = 0;  // Total time spent paused
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
