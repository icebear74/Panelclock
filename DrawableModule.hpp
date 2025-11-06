#ifndef DRAWABLE_MODULE_HPP
#define DRAWABLE_MODULE_HPP

#include <functional>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"
#include "MemoryLogger.hpp"

/**
 * Priority-System für Module:
 * - Normal (0): Nur für interne Playlist-Verwaltung, NICHT für Requests verwenden!
 * - PlayNext (1): OneShot - Modul wird als nächstes in Playlist eingefügt und danach gelöscht
 * - Low/Medium/High/Realtime (2-5): Interrupts - unterbrechen normale Playlist
 * 
 * UID-Regeln:
 * - UID=0: Reserviert für normale Playlist-Rotation, NICHT in Requests verwenden!
 *          Bei Release: UID=0 = Alle Interrupts des Moduls beenden (Notfall-Release)
 * - UID>0: Frei verwendbar für Module (empfohlen: fortlaufend ab 1)
 * - Jede UID darf pro Modul nur einmal gleichzeitig aktiv sein
 * - Module sind selbst für eindeutige UIDs verantwortlich
 * 
 * Duration-Regeln:
 * - Wird bei jedem Request mitgegeben (in Millisekunden)
 * - Modul kann früher beenden mit releasePriorityEx(uid)
 * - Oder Timeout abwarten (automatisches Ende nach Duration)
 */
enum class Priority {
    Normal = 0,      // Normale Playlist-Rotation (NICHT für Requests!)
    PlayNext = 1,    // OneShot - als nächstes in Playlist einreihen
    Low = 2,         // Niedrigster Interrupt
    Medium = 3,      // Mittlerer Interrupt  
    High = 4,        // Hoher Interrupt
    Realtime = 5     // Höchster Interrupt (z.B. Alarm)
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

    // --- Erweiterte Aktivierung mit UID ---
    /**
     * Wird vom PanelManager aufgerufen wenn das Modul aktiviert wird.
     * @param uid Die UID des Requests (0 = normale Playlist, >0 = spezifischer Request)
     */
    virtual void activateModule(uint32_t uid) {
        LOG_MEMORY_STRATEGIC(getModuleName());
        _activeUID = uid;
        _isFinished = false;
        
        if (uid == 0) {
            // Normale Playlist-Rotation
            resetPaging();
        }
        // Module können hier spezifischen Content für UID laden
        onActivate();
    }

    // --- Timing-Methoden ---
    virtual void tick() {}           // Animation, so schnell wie möglich (nur wenn aktiv & !pausiert)
    virtual void logicTick() {}       // Seitensteuerung, alle 100ms (nur wenn aktiv & !pausiert)
    virtual void periodicTick() {}    // Background-Tasks, läuft IMMER für ALLE Module

    // --- Status & Konfiguration ---
    virtual bool isFinished() const { return _isFinished; }
    virtual bool isEnabled() = 0;  // Modul kann sich selbst deaktivieren
    virtual void configure(const ModuleConfig& config) {}
    virtual void pause() {}
    virtual void resume() {}
    
    // --- Display-Eigenschaften ---
    virtual void draw() = 0;
    virtual unsigned long getDisplayDuration() = 0;
    virtual unsigned long getSafetyBuffer() { return 10000; } // 2s Standard-Puffer
    virtual void resetPaging() = 0;
    
    // --- Playlist-Zugehörigkeit ---
    /**
     * Gibt an, ob dieses Modul in die normale Playlist-Rotation aufgenommen werden soll.
     * @return true = Modul wird zur Playlist hinzugefügt (Standard)
     *         false = Modul ist nur für Interrupts/OneShots (z.B. FritzboxModule)
     */
    virtual bool canBeInPlaylist() const { return true; }
    
    // --- Erweiterte Callbacks mit UID ---
    void setRequestCallbackEx(std::function<bool(DrawableModule*, Priority, uint32_t, unsigned long)> cb) { 
        requestCallbackEx = cb; 
    }
    void setReleaseCallbackEx(std::function<void(DrawableModule*, uint32_t)> cb) { 
        releaseCallbackEx = cb; 
    }

    // --- Legacy Callbacks (DEPRECATED - nur für Übergangsphase) ---
    void setRequestCallback(std::function<void(DrawableModule*)> cb) {
        // Leer - ignoriert während Umbau
    }
    void setReleaseCallback(std::function<void(DrawableModule*)> cb) {
        // Leer - ignoriert während Umbau
    }

    // --- Backup/Restore für Zustandssicherung ---
    virtual JsonObject backup(JsonDocument& doc) { return doc.to<JsonObject>(); }
    virtual void restore(JsonObject& obj) {}
    
    /**
     * Wird aufgerufen wenn das Modul wegen Timeout hart beendet wird.
     * Module können hier Aufräumarbeiten durchführen.
     */
    virtual void timeIsUp() {
        Serial.printf("[%s] Timeout erreicht, wurde beendet.\n", getModuleName());
    }

    // --- Getter ---
    uint32_t getMaxRuntime() const { return maxRuntimeMs; }
    uint32_t getActiveUID() const { return _activeUID; }

protected:
    // Helper-Funktionen für Module
    
    /**
     * DEPRECATED: Nutze requestPriorityEx()
     */
    void requestPriority() { 
        // Leer während Umbau
    }
    
    /**
     * Fordert Priorität mit spezifischer UID und Laufzeit an.
     * @param prio Priority-Level (PlayNext, Low, Medium, High, Realtime)
     * @param uid Eindeutige ID für diesen Request (>0)
     * @param durationMs Maximale Laufzeit in Millisekunden
     * @return true wenn Request akzeptiert, false wenn abgelehnt
     */
    bool requestPriorityEx(Priority prio, uint32_t uid, unsigned long durationMs) {
        if (requestCallbackEx) return requestCallbackEx(this, prio, uid, durationMs);
        return false;
    }
    
    /**
     * DEPRECATED: Nutze releasePriorityEx()
     */
    void releasePriority() { 
        // Leer während Umbau
    }
    
    /**
     * Gibt Priorität für spezifische UID frei.
     * @param uid Die UID des zu beendenden Requests (0 = ALLE Requests dieses Moduls)
     */
    void releasePriorityEx(uint32_t uid) {
        if (releaseCallbackEx) releaseCallbackEx(this, uid);
    }
    
    virtual void onActivate() {}
    
    // Member-Variablen
    bool _isFinished = false;
    uint32_t maxRuntimeMs = 30000;
    uint32_t _activeUID = 0;

private:
    // Callbacks
    std::function<bool(DrawableModule*, Priority, uint32_t, unsigned long)> requestCallbackEx;
    std::function<void(DrawableModule*, uint32_t)> releaseCallbackEx;
};

#endif // DRAWABLE_MODULE_HPP