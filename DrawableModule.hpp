#ifndef DRAWABLE_MODULE_HPP
#define DRAWABLE_MODULE_HPP

#include <functional>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"
#include "MemoryLogger.hpp" // Benötigt für das automatische Speicher-Logging.

enum class Priority {
    Normal = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Highest = 4
};

struct ModuleConfig {
    bool resetOnActivate = true;
    uint32_t maxRuntimeMs = 30000;
};

class DrawableModule {
public:
    virtual ~DrawableModule() = default;

    // --- Metadaten für UI und Konfiguration ---
    virtual const char* getModuleName() const { return "UnknownModule"; }
    virtual const char* getModuleDisplayName() const { return "Unknown"; }
    virtual int getCurrentPage() const { return 0; }
    virtual int getTotalPages() const { return 1; }

    // =================================================================
    // =========== DIE MINIMALE KORREKTUR FÜR DEN COMPILER =============
    // =================================================================
    /**
     * @brief Gibt den Failsafe-Timeout des Moduls zurück.
     * Ermöglicht dem PanelManager, den korrekten Wert zu lesen.
     */
    uint32_t getMaxRuntime() const { return maxRuntimeMs; }
    // =================================================================


    // --- Lebenszyklus- und Zustands-Methoden ---
    void activateModule() {
        LOG_MEMORY_STRATEGIC(getModuleName());
        _isFinished = false; 
        onActivate();
    }
    virtual bool isFinished() const { return _isFinished; }
    virtual void configure(const ModuleConfig& config) {}
    virtual void timeIsUp() {}
    virtual void pause() {}
    virtual void resume() {}
    virtual JsonObject backup(JsonDocument& doc) { return doc.to<JsonObject>(); }
    virtual void restore(JsonObject& obj) {}

    // --- Alte Schnittstelle ---
    virtual void draw() = 0;
    virtual unsigned long getDisplayDuration() = 0;
    virtual bool isEnabled() = 0;
    virtual void resetPaging() = 0;
    
    virtual void tick() {}
    virtual void periodicTick() {}
    virtual Priority getPriority() const { return Priority::Normal; }
    virtual unsigned long getMaxInterruptDuration() const { return 0; }

    // --- Callbacks ---
    void setRequestCallback(std::function<void(DrawableModule*)> cb) { requestCallback = cb; }
    void setReleaseCallback(std::function<void(DrawableModule*)> cb) { releaseCallback = cb; }

protected:
    void requestPriority() { if (requestCallback) requestCallback(this); }
    void releasePriority() { if (releaseCallback) releaseCallback(this); }
    virtual void onActivate() {}
    bool _isFinished = false;
    uint32_t maxRuntimeMs = 30000;

private:
    std::function<void(DrawableModule*)> requestCallback;
    std::function<void(DrawableModule*)> releaseCallback;
};

#endif // DRAWABLEMODULE_HPP