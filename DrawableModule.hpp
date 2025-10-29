#ifndef DRAWABLEMODULE_HPP
#define DRAWABLEMODULE_HPP

#include <functional>

// Enum für die Prioritätsstufen
enum class Priority {
    Normal = 0,   // Spielt als nächstes in der normalen Rotation
    Low = 1,
    Medium = 2,
    High = 3,
    Highest = 4   // Unterbricht sofort alles andere
};

class DrawableModule {
public:
    virtual ~DrawableModule() = default;

    // Methoden, die von jedem Modul implementiert werden müssen
    virtual void draw() = 0;
    virtual unsigned long getDisplayDuration() = 0;
    virtual bool isEnabled() = 0;
    virtual void resetPaging() = 0;

    // Standard-Implementierungen, die überschrieben werden können
    virtual void tick() {}

    // NEU: Diese Methode wird vom Application-Loop für JEDES Modul IMMER aufgerufen.
    virtual void periodicTick() {}

    virtual Priority getPriority() const { return Priority::Normal; }
    virtual unsigned long getMaxInterruptDuration() const { return 0; }

    // Callback-Setter, die vom PanelManager verwendet werden
    void setRequestCallback(std::function<void(DrawableModule*)> cb) { requestCallback = cb; }
    void setReleaseCallback(std::function<void(DrawableModule*)> cb) { releaseCallback = cb; }

protected:
    // Methoden, die von den Modulen aufgerufen werden, um mit dem PanelManager zu kommunizieren
    void requestPriority() { if (requestCallback) requestCallback(this); }
    void releasePriority() { if (releaseCallback) releaseCallback(this); }

private:
    std::function<void(DrawableModule*)> requestCallback;
    std::function<void(DrawableModule*)> releaseCallback;
};

#endif // DRAWABLEMODULE_HPP