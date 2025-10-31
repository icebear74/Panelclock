#ifndef DRAWABLEMODULE_HPP
#define DRAWABLEMODULE_HPP

#include <functional>
#include <ArduinoJson.h>
#include "PsramUtils.hpp"
#include "MemoryLogger.hpp" // Benötigt für das automatische Speicher-Logging.

/**
 * @brief Enum für die Prioritätsstufen von Interrupt-Anfragen.
 * Bleibt global, da es von beiden Modul-Typen (LEGACY und MODERN)
 * für die Interrupt-Logik im PanelManager verwendet wird.
 */
enum class Priority {
    Normal = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Highest = 4
};

/**
 * @brief Eine Struktur zur Aufnahme der benutzerspezifischen Konfiguration
 * für ein MODERN-Modul. Diese Werte werden zur Laufzeit aus der
 * globalen Konfiguration (JSON) geladen und an das Modul übergeben.
 */
struct ModuleConfig {
    /**
     * @brief Wenn true, wird das Modul bei jeder Aktivierung zurückgesetzt (z.B. Seite 0).
     * Wenn false, setzt das Modul seine Arbeit dort fort, wo es aufgehört hat.
     */
    bool resetOnActivate = true;
    
    /**
     * @brief Die maximale Zeit in Millisekunden, die ein Modul laufen darf,
     * bevor es vom PanelManager unterbrochen wird (Plan B).
     */
    uint32_t maxRuntimeMs = 30000; // 30 Sekunden als sicherer Standardwert
};

/**
 * @brief Die abstrakte Basisklasse für alle anzeigbaren Module.
 * Sie definiert eine hybride Schnittstelle, die während des Umbaus sowohl
 * die alte (LEGACY) als auch die neue (MODERN) Modul-Architektur unterstützt.
 */
class DrawableModule {
public:
    virtual ~DrawableModule() = default;

    // =================================================================
    // =========== NEUE SCHNITTSTELLE (für MODERN-Module) ==============
    // =================================================================

    // --- Metadaten für UI und Konfiguration ---

    /**
     * @brief Gibt den eindeutigen internen Namen des Moduls zurück.
     * Wird für die Playlist-Konfiguration und das Backup/Restore verwendet.
     * @return Ein konstanter C-String, z.B. "DartsRankingModule".
     */
    virtual const char* getModuleName() const { return "UnknownModule"; }
    
    /**
     * @brief Gibt einen benutzerfreundlichen Anzeigenamen für das Modul zurück.
     * Wird im Web-Interface zur Konfiguration der Playlist angezeigt.
     * @return Ein konstanter C-String, z.B. "Darts-Rangliste".
     */
    virtual const char* getModuleDisplayName() const { return "Unknown"; }

    /**
     * @brief Gibt die aktuell angezeigte Seite des Moduls zurück.
     * Nützlich für Debugging oder erweiterte UI-Anzeigen.
     */
    virtual int getCurrentPage() const { return 0; }

    /**
     * @brief Gibt die Gesamtanzahl der Seiten des Moduls zurück.
     */
    virtual int getTotalPages() const { return 1; }

    // --- Lebenszyklus- und Zustands-Methoden ---
    
    /**
     * @brief [FINALE API] Wird vom PanelManager aufgerufen, um ein Modul zu aktivieren.
     * Diese Methode ist NICHT virtuell und kann nicht überschrieben werden.
     * Sie stellt sicher, dass kritisches Setup (Logging, Flag-Reset) immer ausgeführt wird.
     * Anschließend ruft sie die virtuelle `onActivate()` Methode auf, die von den
     * Modulen für ihre spezifische Startlogik implementiert werden kann.
     */
    void activateModule() {
        // 1. GARANTIERTES LOGGING
        LOG_MEMORY_STRATEGIC(getModuleName());
        
        // 2. GARANTIERTES ZURÜCKSETZEN DES FLAGS
        _isFinished = false; 
        
        // 3. Optionaler Aufruf der modulspezifischen Logik
        onActivate();
    }

    /**
     * @brief Prüft, ob das Modul seine Arbeit von sich aus beenden möchte (Plan A).
     * Wird in jedem Tick vom PanelManager abgefragt.
     * @return true, wenn das Modul die Kontrolle freiwillig abgeben möchte.
     */
    virtual bool isFinished() const { return _isFinished; }

    /**
     * @brief Konfiguriert ein MODERN-Modul mit den Einstellungen aus dem Web-Interface.
     */
    virtual void configure(const ModuleConfig& config) {}

    /**
     * @brief Wird vom PanelManager aufgerufen, wenn die `maxRuntimeMs` eines Moduls
     * abgelaufen ist und es "hart" unterbrochen wird (Plan B).
     * Das Modul kann hier seinen Zustand für die nächste Aktivierung sichern.
     */
    virtual void timeIsUp() {}

    /**
     * @brief Pausiert das Modul (z.B. bei einem Interrupt).
     */
    virtual void pause() {}

    /**
     * @brief Setzt ein pausiertes Modul fort.
     */
    virtual void resume() {}

    // --- Backup/Restore-Methoden ---

    /**
     * @brief Erstellt ein JSON-Objekt mit den zu sichernden Daten des Moduls.
     * @param doc Das globale JsonDocument, in dem das Objekt erstellt wird.
     * @return Ein JsonObject, das die Daten dieses Moduls enthält.
     */
    virtual JsonObject backup(JsonDocument& doc) { return doc.to<JsonObject>(); }

    /**
     * @brief Stellt den Zustand des Moduls aus einem JSON-Objekt wieder her.
     * @param obj Das JsonObject, das die wiederherzustellenden Daten enthält.
     */
    virtual void restore(JsonObject& obj) {}

    // =================================================================
    // ============ ALTE SCHNITTSTELLE (für LEGACY-Module) ==============
    // =================================================================

    virtual void draw() = 0;
    virtual unsigned long getDisplayDuration() = 0;
    virtual bool isEnabled() = 0;
    
    /**
     * @brief [VERALTET] Wird für LEGACY-Module anstelle von activateModule() aufgerufen.
     * Wird beim Umbau eines Moduls auf MODERN entfernt und durch onActivate() ersetzt.
     */
    virtual void resetPaging() = 0;
    
    virtual void tick() {}
    virtual void periodicTick() {}
    virtual Priority getPriority() const { return Priority::Normal; }
    virtual unsigned long getMaxInterruptDuration() const { return 0; }

    // =================================================================
    // ================== GEMEINSAME CALLBACKS =========================
    // =================================================================

    void setRequestCallback(std::function<void(DrawableModule*)> cb) { requestCallback = cb; }
    void setReleaseCallback(std::function<void(DrawableModule*)> cb) { releaseCallback = cb; }

protected:
    void requestPriority() { if (requestCallback) requestCallback(this); }
    void releasePriority() { if (releaseCallback) releaseCallback(this); }

    /**
     * @brief [NEU & ZUM ÜBERSCHREIBEN] Wird von activateModule() aufgerufen.
     * Ein MODERN-Modul implementiert HIER seine spezifische Logik, die beim
     * Start ausgeführt werden soll (z.B. das Zurücksetzen eines Seiten-Counters).
     */
    virtual void onActivate() {}

    /**
     * @brief Das Flag, das ein MODERN-Modul auf `true` setzt, um seine Beendigung
     * zu signalisieren (Plan A). Es ist `protected`, damit erbende Klassen
     * darauf zugreifen können.
     */
    bool _isFinished = false;

private:
    std::function<void(DrawableModule*)> requestCallback;
    std::function<void(DrawableModule*)> releaseCallback;
};

#endif // DRAWABLEMODULE_HPP