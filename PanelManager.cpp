#include "PanelManager.hpp"
#include "ClockModule.hpp"
#include "MwaveSensorModule.hpp"
#include "GeneralTimeConverter.hpp"
#include <time.h>
#include <algorithm>

// ============================================================================
// Konstruktor & Destruktor
// ============================================================================

PanelManager::PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter) 
    : _hwConfig(hwConfig), _timeConverter(timeConverter) {
    Serial.println("[PanelManager] Konstruktor - PlaylistEntry-basierte Version mit UID-System");
    _logicTickMutex = xSemaphoreCreateMutex();
    _canvasMutex = xSemaphoreCreateMutex();
}

PanelManager::~PanelManager() {
    // Cleanup PlaylistEntries
    for (auto* entry : _playlist) {
        delete entry;
    }
    for (auto* entry : _interruptQueue) {
        delete entry;
    }
    
    // Cleanup Display-Objekte
    delete _dma_display;
    delete _virtualDisp;
    delete _canvasTime;
    delete _canvasData;
    delete _fullCanvas;
    delete _u8g2;
    
    // NEU: Cleanup für Logic-Tick-Task
    if (_logicTickTaskHandle) vTaskDelete(_logicTickTaskHandle);
    if (_logicTickMutex) vSemaphoreDelete(_logicTickMutex);
    if (_canvasMutex) vSemaphoreDelete(_canvasMutex);
}

// ============================================================================
// Initialisierung
// ============================================================================

bool PanelManager::begin() {
    Serial.println("[PanelManager] Initialisiere Display...");
    
    _u8g2 = new U8G2_FOR_ADAFRUIT_GFX();
    
    // PSRAM-Allokation für Canvases
    uint16_t* timeBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * TIME_AREA_H * sizeof(uint16_t));
    uint16_t* dataBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * DATA_AREA_H * sizeof(uint16_t));
    uint16_t* fullBuffer = (uint16_t*)ps_malloc(FULL_WIDTH * FULL_HEIGHT * sizeof(uint16_t));
    
    if (!timeBuffer || !dataBuffer || !fullBuffer) {
        Serial.println("FATAL: PSRAM-Allokation für Canvases fehlgeschlagen!");
        return false;
    }
    
    _canvasTime = new GFXcanvas16(FULL_WIDTH, TIME_AREA_H, timeBuffer);
    _canvasData = new GFXcanvas16(FULL_WIDTH, DATA_AREA_H, dataBuffer);
    _fullCanvas = new GFXcanvas16(FULL_WIDTH, FULL_HEIGHT, fullBuffer);
    
    // HUB75 Konfiguration
    HUB75_I2S_CFG::i2s_pins _pins = {
        (int8_t)_hwConfig.R1, (int8_t)_hwConfig.G1, (int8_t)_hwConfig.B1,
        (int8_t)_hwConfig.R2, (int8_t)_hwConfig.G2, (int8_t)_hwConfig.B2,
        (int8_t)_hwConfig.A,  (int8_t)_hwConfig.B,  (int8_t)_hwConfig.C,
        (int8_t)_hwConfig.D,  (int8_t)_hwConfig.E,
        (int8_t)_hwConfig.LAT, (int8_t)_hwConfig.OE, (int8_t)_hwConfig.CLK
    };
    
    HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, VDISP_NUM_ROWS * VDISP_NUM_COLS, _pins);
    mxconfig.double_buff = false;
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;
    mxconfig.clkphase = false;
    
    _dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    if (!_dma_display->begin()) {
        Serial.println("FATAL: MatrixPanel_I2S_DMA begin() fehlgeschlagen!");
        return false;
    }
    
    _dma_display->setBrightness8(128);
    _dma_display->clearScreen();
    
    _virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(
        VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y
    );
    _virtualDisp->setDisplay(*_dma_display);
    
    // NEU: Logic-Tick-Task starten
    xTaskCreate(logicTickTaskWrapper, "LogicTickTask", 4096, this, 1, &_logicTickTaskHandle);
    
    Serial.println("[PanelManager] Display-Initialisierung erfolgreich abgeschlossen.");
    return true;
}

void PanelManager::registerClockModule(ClockModule* mod) { 
    _clockMod = mod;
    Serial.println("[PanelManager] ClockModule registriert");
}

void PanelManager::registerSensorModule(MwaveSensorModule* mod) { 
    _sensorMod = mod;
    Serial.println("[PanelManager] SensorModule registriert");
}

void PanelManager::registerModule(DrawableModule* mod) {
    if (!mod) return;
    
    // Erweiterte Callbacks setzen mit Lambda-Funktionen
    mod->setRequestCallbackEx([this](DrawableModule* m, Priority prio, uint32_t uid, unsigned long durationMs) {
        return this->handlePriorityRequest(m, prio, uid, durationMs);
    });
    
    mod->setReleaseCallbackEx([this](DrawableModule* m, uint32_t uid) {
        this->handlePriorityRelease(m, uid);
    });
    
    // Fullscreen-Canvas setzen wenn das Modul Fullscreen unterstützt
    if (mod->supportsFullscreen() && _fullCanvas) {
        mod->setFullscreenCanvas(_fullCanvas);
        Serial.printf("[PanelManager] Fullscreen-Canvas für '%s' gesetzt\n", mod->getModuleName());
    }
    
    // Zu Catalog hinzufügen (IMMER, für periodicTick)
    _moduleCatalog.push_back(mod);
    
    // Nur zur Playlist hinzufügen wenn erlaubt
    if (mod->canBeInPlaylist()) {
        PlaylistEntry* entry = new PlaylistEntry(mod, 0, Priority::Normal);
        _playlist.push_back(entry);
        Serial.printf("[PanelManager] Modul '%s' registriert und zur Playlist hinzugefügt\n", 
                     mod->getModuleName());
    } else {
        Serial.printf("[PanelManager] Modul '%s' registriert (Interrupt-Only, nicht in Playlist)\n", 
                     mod->getModuleName());
    }
}

// ============================================================================
// A) handlePriorityRequest - Die Eingangstür
// ============================================================================

bool PanelManager::handlePriorityRequest(DrawableModule* mod, Priority prio, uint32_t uid, unsigned long durationMs) {
    if (!mod) {
        Serial.println("[PanelManager] FEHLER: handlePriorityRequest mit nullptr aufgerufen!");
        return false;
    }
    
    // Validierung: Priority::Normal ist ungültig für Requests
    if (prio == Priority::Normal) {
        Serial.println("[PanelManager] FEHLER: Priority::Normal ist ungültig für Requests!");
        return false;
    }
    
    // Validierung: UID=0 ist reserviert
    if (uid == 0) {
        Serial.println("[PanelManager] FEHLER: UID=0 ist für normale Playlist reserviert!");
        return false;
    }
    
    // Validierung: Duration muss > 0 sein
    if (durationMs == 0) {
        Serial.println("[PanelManager] FEHLER: Duration muss > 0 sein!");
        return false;
    }
    
    Serial.printf("[PanelManager] Priority Request: Modul='%s', Prio=%d, UID=%lu, Duration=%lums\n", 
                  mod->getModuleName(), (int)prio, uid, durationMs);
    
    // Duplikat-Prüfung
    if (prio == Priority::PlayNext) {
        // Prüfe in Playlist
        for (auto* entry : _playlist) {
            if (entry->module == mod && entry->uid == uid && entry->isOneShot()) {
                Serial.printf("[PanelManager] FEHLER: OneShot mit UID=%lu bereits in Playlist!\n", uid);
                return false;
            }
        }
    } else {
        // Prüfe in InterruptQueue
        for (auto* entry : _interruptQueue) {
            if (entry->module == mod && entry->uid == uid) {
                Serial.printf("[PanelManager] FEHLER: Interrupt mit UID=%lu bereits in Queue!\n", uid);
                return false;
            }
        }
    }
    
    // Fall 1: Priority::PlayNext (OneShot in Playlist)
    if (prio == Priority::PlayNext) {
        PlaylistEntry* newEntry = new PlaylistEntry(mod, uid, prio, durationMs);
        
        // Finde das aktuell laufende oder pausierte Modul in der Playlist
        PlaylistEntry* currentRunning = findRunningInPlaylist();
        if (!currentRunning) {
            currentRunning = findPausedInPlaylist();
        }
        
        if (currentRunning) {
            // Finde Index und füge danach ein
            int currentIndex = findPlaylistIndex(currentRunning);
            if (currentIndex >= 0 && currentIndex < (int)_playlist.size() - 1) {
                _playlist.insert(_playlist.begin() + currentIndex + 1, newEntry);
                Serial.printf("[PanelManager] OneShot eingefügt nach Index %d\n", currentIndex);
            } else {
                // Am Ende einfügen
                _playlist.push_back(newEntry);
                Serial.println("[PanelManager] OneShot am Ende der Playlist eingefügt");
            }
        } else {
            // Kein laufendes Modul, füge am Anfang ein
            _playlist.insert(_playlist.begin(), newEntry);
            Serial.println("[PanelManager] OneShot am Anfang der Playlist eingefügt");
        }
        return true;
    }
    
    // Fall 2: Interrupt (Low, Medium, High, Realtime)
    else {
        // Prüfe ob es der erste Interrupt ist
        bool wasEmpty = _interruptQueue.empty();
        
        // Neuen Interrupt erstellen
        PlaylistEntry* newEntry = new PlaylistEntry(mod, uid, prio, durationMs);
        
        // Finde aktiven Interrupt (falls vorhanden)
        PlaylistEntry* currentInterrupt = nullptr;
        for (auto* entry : _interruptQueue) {
            if (entry->isRunning && !entry->isPaused) {
                currentInterrupt = entry;
                break;
            }
        }
        
        // Entscheide wo der neue Interrupt eingefügt wird
        if (!currentInterrupt) {
            // Kein aktiver Interrupt, sortiere nach Priorität ein
            auto it = _interruptQueue.begin();
            while (it != _interruptQueue.end() && (*it)->priority >= prio) {
                ++it;
            }
            _interruptQueue.insert(it, newEntry);
            
            // ✅ AKTIVIERE DEN INTERRUPT SOFORT!
            newEntry->activate();
            Serial.printf("[PanelManager] Neuer Interrupt '%s' sofort aktiviert\n",
                         newEntry->module->getModuleName());
        }
        else if (prio > currentInterrupt->priority) {
            // Höhere Priorität -> pausiere aktuellen und füge vorne ein
            currentInterrupt->pause();
            _interruptQueue.insert(_interruptQueue.begin(), newEntry);
            
            // ✅ AKTIVIERE DEN NEUEN INTERRUPT!
            newEntry->activate();
            
            Serial.printf("[PanelManager] Höherer Interrupt (Prio %d) pausiert aktuellen (Prio %d) und wurde aktiviert\n",
                         (int)prio, (int)currentInterrupt->priority);
        }
        else if (prio == currentInterrupt->priority) {
            // Gleiche Priorität -> wird "PlayNext" im Interrupt-Kontext
            // Füge nach dem aktuellen ein
            auto it = std::find(_interruptQueue.begin(), _interruptQueue.end(), currentInterrupt);
            if (it != _interruptQueue.end()) {
                _interruptQueue.insert(it + 1, newEntry);
                Serial.println("[PanelManager] Gleiche Priorität -> nach aktuellem Interrupt eingefügt (nicht aktiviert)");
            }
        }
        else {
            // Niedrigere Priorität -> ans Ende
            _interruptQueue.push_back(newEntry);
            Serial.println("[PanelManager] Niedrigere Priorität -> ans Ende der Queue (nicht aktiviert)");
        }
        
        // Wenn dies der erste Interrupt war, pausiere Playlist
        if (wasEmpty) {
            PlaylistEntry* runningPlaylist = findRunningInPlaylist();
            if (runningPlaylist) {
                runningPlaylist->pause();
                Serial.printf("[PanelManager] Playlist-Modul '%s' pausiert für ersten Interrupt\n",
                            runningPlaylist->module->getModuleName());
            }
        }
        
        Serial.printf("[PanelManager] Interrupt hinzugefügt. Queue-Größe: %d\n", 
                     _interruptQueue.size());
        return true;
    }
}

void PanelManager::handlePriorityRelease(DrawableModule* mod, uint32_t uid) {
    if (!mod) return;
    
    // Spezialfall: UID=0 = Release ALLE Interrupts dieses Moduls
    if (uid == 0) {
        Serial.printf("[PanelManager] Priority Release ALL: Modul='%s'\n", mod->getModuleName());
        
        bool wasActive = false;
        auto it = _interruptQueue.begin();
        while (it != _interruptQueue.end()) {
            if ((*it)->module == mod) {
                if ((*it)->isRunning && !(*it)->isPaused) {
                    wasActive = true;
                }
                Serial.printf("[PanelManager] Entferne Interrupt UID=%lu\n", (*it)->uid);
                delete *it;
                it = _interruptQueue.erase(it);
            } else {
                ++it;
            }
        }
        
        Serial.printf("[PanelManager] Alle Interrupts von '%s' entfernt. Verbleibende: %d\n", 
                     mod->getModuleName(), _interruptQueue.size());
        
        // Wenn ein aktiver entfernt wurde, reaktiviere nächsten
        if (wasActive) {
            // Suche nach pausiertem Interrupt zum Fortsetzen
            for (auto* entry : _interruptQueue) {
                if (entry->isPaused) {
                    entry->resume();
                    Serial.printf("[PanelManager] Pausierter Interrupt '%s' fortgesetzt\n",
                                entry->module->getModuleName());
                    return;
                }
            }
        }
        
        // Wenn Queue nun leer ist, resume pausiertes Playlist-Modul
        if (_interruptQueue.empty()) {
            PlaylistEntry* pausedEntry = findPausedInPlaylist();
            if (pausedEntry) {
                pausedEntry->resume();
                Serial.printf("[PanelManager] Playlist-Modul '%s' fortgesetzt\n",
                            pausedEntry->module->getModuleName());
            }
        }
        return;
    }
    
    // Normaler Fall: Release spezifische UID
    Serial.printf("[PanelManager] Priority Release: Modul='%s', UID=%lu\n", 
                  mod->getModuleName(), uid);
    
    // Suche und entferne aus InterruptQueue
    auto it = std::find_if(_interruptQueue.begin(), _interruptQueue.end(),
        [mod, uid](const PlaylistEntry* entry) {
            return entry->module == mod && entry->uid == uid;
        });
    
    if (it != _interruptQueue.end()) {
        // Prüfe ob es das aktive Entry war
        bool wasActive = (*it)->isRunning && !(*it)->isPaused;
        
        delete *it;
        _interruptQueue.erase(it);
        Serial.printf("[PanelManager] Interrupt entfernt. Verbleibende: %d\n", 
                     _interruptQueue.size());
        
        // Wenn es aktiv war, müssen wir das nächste aktivieren
        if (wasActive) {
            // Suche nach pausiertem Interrupt zum Fortsetzen
            for (auto* entry : _interruptQueue) {
                if (entry->isPaused) {
                    entry->resume();
                    Serial.printf("[PanelManager] Pausierter Interrupt '%s' fortgesetzt\n",
                                entry->module->getModuleName());
                    return;
                }
            }
        }
        
        // Wenn Queue nun leer ist, resume pausiertes Playlist-Modul
        if (_interruptQueue.empty()) {
            PlaylistEntry* pausedEntry = findPausedInPlaylist();
            if (pausedEntry) {
                pausedEntry->resume();
                Serial.printf("[PanelManager] Playlist-Modul '%s' fortgesetzt\n",
                            pausedEntry->module->getModuleName());
            }
        }
    }
}

// ============================================================================
// B) tick - Der Watchdog
// ============================================================================

void PanelManager::tick() {
    // Schritt 1: periodicTick für alle Module (läuft IMMER)
    for (DrawableModule* mod : _moduleCatalog) {
        if (mod) {
            mod->periodicTick();
        }
    }
    
    // Schritt 2: Finde aktives Modul
    PlaylistEntry* activeEntry = findActiveEntry();
    
    // Schritt 3: Wenn kein Modul aktiv, starte eines
    if (!activeEntry) {
        _fullscreenActive = false;  // Kein Modul = kein Fullscreen
        switchNextModule();
        return;
    }
    
    // Schritt 3.5: Fullscreen-Modus aktualisieren basierend auf aktivem Modul
    _fullscreenActive = activeEntry->module && activeEntry->module->wantsFullscreen();
    
    // Schritt 4: Prüfe ob Modul noch enabled ist
    if (!activeEntry->module->isEnabled()) {
        Serial.printf("[PanelManager::tick] Modul '%s' wurde deaktiviert, wechsle...\n",
                     activeEntry->module->getModuleName());
        
        // Wenn es ein OneShot war, muss er entfernt werden
        if (activeEntry->isOneShot()) {
            auto it = std::find(_playlist.begin(), _playlist.end(), activeEntry);
            if (it != _playlist.end()) {
                _playlist.erase(it);
                delete activeEntry;
                Serial.println("[PanelManager::tick] Deaktivierter OneShot entfernt");
            }
        }
        
        _fullscreenActive = false;
        switchNextModule();
        return;
    }
    
    // Schritt 5: Tick für aktives Modul (wenn nicht pausiert)
    if (!activeEntry->isPaused) {
        tickActiveModule();
        
        // Schritt 6: Prüfe ob Modul fertig ist
        if (activeEntry->isFinished()) {
            // Berechne alle Zeitwerte für Debug-Ausgabe
            unsigned long elapsed = millis() - activeEntry->startTime - activeEntry->pausedDuration;
            unsigned long maxDuration = activeEntry->module->getDisplayDuration();
            unsigned long safetyBuffer = max(1000UL, maxDuration / 10);
            unsigned long totalMaxDuration = maxDuration + safetyBuffer;
            
            // DEBUG: Detaillierte Ausgabe WARUM das Modul beendet wurde
            Serial.println("=================================================================");
            
            if (activeEntry->module->isFinished()) {
                // Modul hat sich SELBST beendet
                long remainingTime = (long)totalMaxDuration - (long)elapsed;
                
                Serial.printf("✓ MODUL SELBST-BEENDET: '%s'\n", activeEntry->module->getModuleName());
                Serial.printf("  Laufzeit:      %6lu ms\n", elapsed);
                Serial.printf("  Max erlaubt:   %6lu ms (Basis: %lu ms + Puffer: %lu ms)\n", 
                             totalMaxDuration, maxDuration, safetyBuffer);
                
                if (remainingTime > 0) {
                    Serial.printf("  Restzeit:      %6ld ms (Puffer war ausreichend)\n", remainingTime);
                } else {
                    Serial.printf("  WARNUNG: Puffer zu knapp! Überzogen um %ld ms\n", -remainingTime);
                }
            } else {
                // Timeout durch PanelManager
                Serial.printf("⏱ TIMEOUT: '%s'\n", activeEntry->module->getModuleName());
                Serial.printf("  Laufzeit:      %6lu ms\n", elapsed);
                Serial.printf("  Max erlaubt:   %6lu ms (Basis: %lu ms + Puffer: %lu ms)\n", 
                             totalMaxDuration, maxDuration, safetyBuffer);
                Serial.printf("  Überzogen um:  %6lu ms\n", elapsed - totalMaxDuration);
                
                activeEntry->module->timeIsUp();
            }
            
            Serial.println("=================================================================");
            
            switchNextModule();
        }
    }
}

void PanelManager::tickActiveModule() {
    PlaylistEntry* activeEntry = findActiveEntry();
    if (!activeEntry || !activeEntry->module || activeEntry->isPaused) return;
    
    // Normal tick (für Animationen)
    activeEntry->module->tick();
    
    // Logic tick entfernt - läuft jetzt im separaten Task
}

// ============================================================================
// NEU: Separater Logic-Tick-Task
// ============================================================================

void PanelManager::logicTickTaskWrapper(void* param) {
    PanelManager* self = static_cast<PanelManager*>(param);
    self->logicTickTask();
}

void PanelManager::logicTickTask() {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(100));  // Genau 100ms
        
        if (xSemaphoreTake(_logicTickMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            PlaylistEntry* activeEntry = findActiveEntry();
            if (activeEntry && activeEntry->module && !activeEntry->isPaused) {
                activeEntry->module->logicTick();
                activeEntry->logicTickCounter++;
            }
            xSemaphoreGive(_logicTickMutex);
        }
    }
}

// ============================================================================
// C) switchNextModule - Der Master of Switching
// ============================================================================

void PanelManager::switchNextModule() {
    Serial.println("[switchNextModule] Start");
    
    // Phase 1: Deaktivierung des aktuellen Moduls
    PlaylistEntry* currentRunning = findActiveEntry();
    int lastActivePlaylistIndex = -1;
    
    if (currentRunning) {
        Serial.printf("[switchNextModule] Deaktiviere Modul '%s' (UID=%lu)\n", 
                     currentRunning->module->getModuleName(), currentRunning->uid);
        
        // Deaktiviere das Modul
        currentRunning->deactivate();
        
        // War es ein Interrupt?
        auto interruptIt = std::find(_interruptQueue.begin(), _interruptQueue.end(), currentRunning);
        if (interruptIt != _interruptQueue.end()) {
            // Interrupt entfernen
            _interruptQueue.erase(interruptIt);
            delete currentRunning;
            Serial.println("[switchNextModule] Interrupt entfernt");
            
            // Prüfe ob es einen pausierten Interrupt gibt
            for (auto* entry : _interruptQueue) {
                if (entry->isPaused) {
                    entry->resume();
                    Serial.printf("[switchNextModule] Reaktiviere pausierten Interrupt '%s'\n",
                                entry->module->getModuleName());
                    return; // Funktion endet hier!
                }
            }
            
            // Wenn Queue nun leer, reaktiviere pausiertes Playlist-Modul
            if (_interruptQueue.empty()) {
                PlaylistEntry* pausedEntry = findPausedInPlaylist();
                if (pausedEntry) {
                    pausedEntry->resume();
                    Serial.printf("[switchNextModule] Reaktiviere pausiertes Playlist-Modul '%s'\n",
                                pausedEntry->module->getModuleName());
                    return; // Funktion endet hier!
                }
            }
        }
        // War es ein Playlist-Modul?
        else {
            lastActivePlaylistIndex = findPlaylistIndex(currentRunning);
            Serial.printf("[switchNextModule] War Playlist-Modul bei Index %d\n", lastActivePlaylistIndex);
            
            // Wenn OneShot, entfernen
            if (currentRunning->isOneShot()) {
                auto playlistIt = std::find(_playlist.begin(), _playlist.end(), currentRunning);
                if (playlistIt != _playlist.end()) {
                    _playlist.erase(playlistIt);
                    delete currentRunning;
                    Serial.println("[switchNextModule] OneShot-Modul entfernt");
                    // Index anpassen, da wir ein Element entfernt haben
                    if (lastActivePlaylistIndex >= (int)_playlist.size()) {
                        lastActivePlaylistIndex = -1;
                    }
                }
            }
        }
    }
    
    // Phase 2: Aktivierung des nächsten Moduls
    
    // Priorität 1: Interrupts
    if (!_interruptQueue.empty()) {
        // Finde ersten nicht-pausierten Interrupt oder aktiviere den ersten
        for (auto* entry : _interruptQueue) {
            if (!entry->isRunning || !entry->isPaused) {
                entry->activate();
                Serial.printf("[switchNextModule] Aktiviere Interrupt '%s' (UID=%lu, Prio=%d)\n",
                             entry->module->getModuleName(), entry->uid, (int)entry->priority);
                return;
            }
        }
    }
    
    // Priorität 2: Playlist
    if (_playlist.empty()) {
        Serial.println("[switchNextModule] WARNUNG: Playlist ist leer!");
        return;
    }
    
    // Startpunkt für die Suche
    int searchStart = (lastActivePlaylistIndex >= 0) ? 
                     (lastActivePlaylistIndex + 1) % _playlist.size() : 0;
    

    // Suche nächstes aktivierbares Modul
    int attempts = 0;
    while (attempts < (int)_playlist.size()) {
        int idx = (searchStart + attempts) % _playlist.size();
        PlaylistEntry* candidate = _playlist[idx];
        
        // Prüfe ob Modul aktivierbar ist
        if (candidate->canActivate()) {
            candidate->activate();
            Serial.printf("[switchNextModule] Aktiviere Playlist-Modul '%s' bei Index %d\n",
                         candidate->module->getModuleName(), idx);
            return;
        }
        
        // Wenn es ein deaktivierter OneShot ist, entfernen
        if (candidate->isOneShot() && !candidate->module->isEnabled()) {
            Serial.printf("[switchNextModule] Entferne deaktivierten OneShot '%s'\n",
                         candidate->module->getModuleName());
            _playlist.erase(_playlist.begin() + idx);
            delete candidate;
            // Nicht attempts erhöhen, da wir ein Element entfernt haben
            if (searchStart > idx) searchStart--;
            continue;
        }
        
        attempts++;
    }
    
    Serial.println("[switchNextModule] WARNUNG: Kein aktivierbares Modul in Playlist gefunden!");
}

// ============================================================================
// D) findActiveEntry - Der Spürhund
// ============================================================================

PlaylistEntry* PanelManager::findActiveEntry() {
    // Debug auskommentiert - bei Bedarf wieder aktivieren
    // Serial.println("[findActiveEntry] START - Suche in InterruptQueue...");
    
    // Erst InterruptQueue durchsuchen (höchste Priorität)
    for (auto* entry : _interruptQueue) {
        if (entry && entry->isRunning && !entry->isPaused) {
            // Serial.printf("[findActiveEntry] Gefunden in InterruptQueue: '%s' (UID=%lu)\n",
            //              entry->module ? entry->module->getModuleName() : "NULL", entry->uid);
            return entry;
        }
    }
    
    // Serial.println("[findActiveEntry] Nichts in InterruptQueue, suche in Playlist...");
    
    // Dann Playlist durchsuchen (normale und pausierte)
    for (auto* entry : _playlist) {
        if (entry && entry->isRunning) {
            // Serial.printf("[findActiveEntry] Gefunden in Playlist: '%s' (UID=%lu, isPaused=%d)\n",
            //              entry->module ? entry->module->getModuleName() : "NULL",
            //              entry->uid, entry->isPaused);
            return entry;
        }
    }
    
    // Serial.println("[findActiveEntry] FEHLER: Nichts gefunden!");
    return nullptr;
}

// ============================================================================
// Helper-Funktionen
// ============================================================================

PlaylistEntry* PanelManager::findRunningInPlaylist() {
    for (auto* entry : _playlist) {
        if (entry && entry->isRunning && !entry->isPaused) {
            return entry;
        }
    }
    return nullptr;
}

PlaylistEntry* PanelManager::findPausedInPlaylist() {
    for (auto* entry : _playlist) {
        if (entry && entry->isRunning && entry->isPaused) {
            return entry;
        }
    }
    return nullptr;
}

int PanelManager::findPlaylistIndex(PlaylistEntry* entry) {
    if (!entry) return -1;
    
    for (size_t i = 0; i < _playlist.size(); i++) {
        if (_playlist[i] == entry) {
            return (int)i;
        }
    }
    return -1;
}

PlaylistEntry* PanelManager::findEntryByModuleAndUID(DrawableModule* mod, uint32_t uid) {
    // Suche in InterruptQueue
    for (auto* entry : _interruptQueue) {
        if (entry->module == mod && entry->uid == uid) {
            return entry;
        }
    }
    
    // Suche in Playlist
    for (auto* entry : _playlist) {
        if (entry->module == mod && entry->uid == uid) {
            return entry;
        }
    }
    
    return nullptr;
}

// ============================================================================
// Rendering
// ============================================================================

void PanelManager::render() {
    if (!_virtualDisp || !_dma_display || !_canvasTime || !_canvasData || !_fullCanvas) return;
    
    // Prüfe ob das physische Display eingeschaltet sein soll
    bool displayOn = _sensorMod && _sensorMod->isDisplayOn();
    
    // Lock canvas access for thread-safe rendering
    if (_canvasMutex && xSemaphoreTake(_canvasMutex, portMAX_DELAY) == pdTRUE) {
        // Canvas IMMER zeichnen (für Streaming auch wenn Display aus)
        if (_fullscreenActive) {
            // Fullscreen-Modus: Modul zeichnet auf gesamten Bildschirm
            drawFullscreenArea();
        } else {
            // Normaler Modus: Uhr oben, Daten unten
            drawClockArea();
            drawDataArea();
        }
        
        // Physisches Display nur aktualisieren wenn eingeschaltet
        if (displayOn) {
            if (_fullscreenActive) {
                _virtualDisp->drawRGBBitmap(0, 0, _fullCanvas->getBuffer(), 
                                            _fullCanvas->width(), _fullCanvas->height());
            } else {
                _virtualDisp->drawRGBBitmap(0, 0, _canvasTime->getBuffer(), 
                                            _canvasTime->width(), _canvasTime->height());
                _virtualDisp->drawRGBBitmap(0, TIME_AREA_H, _canvasData->getBuffer(), 
                                            _canvasData->width(), _canvasData->height());
            }
            _dma_display->flipDMABuffer();
        } else {
            // Display aus: Nur Bildschirm löschen
            _dma_display->clearScreen();
            _dma_display->flipDMABuffer();
        }
        
        xSemaphoreGive(_canvasMutex);
    }
}

void PanelManager::drawFullscreenArea() {
    PlaylistEntry* activeEntry = findActiveEntry();
    
    if (activeEntry && activeEntry->module && !activeEntry->isPaused) {
        activeEntry->module->draw();
    } else {
        if (_fullCanvas) _fullCanvas->fillScreen(0);
    }
}

void PanelManager::drawClockArea() {
    if (!_clockMod || !_sensorMod) return;
    
    time_t now_utc;
    time(&now_utc);
    struct tm timeinfo;
    time_t local_epoch = _timeConverter.toLocal(now_utc);
    localtime_r(&local_epoch, &timeinfo);
    
    _clockMod->setTime(timeinfo);
    _clockMod->setSensorState(_sensorMod->isDisplayOn(), 
                             _sensorMod->getLastOnTime(), 
                             _sensorMod->getLastOffTime(), 
                             _sensorMod->getOnPercentage());
    _clockMod->tick();
    _clockMod->draw();
}

void PanelManager::drawDataArea() {
    PlaylistEntry* activeEntry = findActiveEntry();
    
    if (activeEntry && activeEntry->module && !activeEntry->isPaused) {
        activeEntry->module->draw();
    } else {
        if (_canvasData) _canvasData->fillScreen(0);
    }
}

void PanelManager::displayStatus(const char* msg) {
    if (!_dma_display || !_virtualDisp || !_u8g2) return;
    
    _dma_display->clearScreen();
    _u8g2->begin(*_virtualDisp);
    _u8g2->setFont(u8g2_font_6x13_tf);
    _u8g2->setForegroundColor(0xFFFF);
    _u8g2->setBackgroundColor(0x0000);
    
    int y = 12;
    char* str = strdup(msg);
    if (!str) return;
    
    char* line = strtok(str, "\n");
    while(line != NULL) {
        int x = (FULL_WIDTH - _u8g2->getUTF8Width(line)) / 2;
        if (x < 0) x = 0;
        _u8g2->setCursor(x, y);
        _u8g2->print(line);
        y += 14;
        line = strtok(NULL, "\n");
    }
    
    free(str);
    _dma_display->flipDMABuffer();
}

// ============================================================================
// Thread-safe Panel Buffer Copy for Streaming
// ============================================================================

bool PanelManager::copyFullPanelBuffer(uint16_t* destinationBuffer, size_t bufferSize) {
    if (!destinationBuffer) {
        return false;
    }
    
    // Calculate required buffer size (full panel)
    size_t totalRequired = FULL_WIDTH * FULL_HEIGHT;
    
    if (bufferSize < totalRequired) {
        return false;
    }
    
    // Lock canvas access
    if (_canvasMutex && xSemaphoreTake(_canvasMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (_fullscreenActive && _fullCanvas) {
            // Fullscreen-Modus: Kopiere den kompletten Fullscreen-Canvas
            uint16_t* fullBuffer = _fullCanvas->getBuffer();
            if (fullBuffer) {
                memcpy(destinationBuffer, fullBuffer, totalRequired * sizeof(uint16_t));
            } else {
                xSemaphoreGive(_canvasMutex);
                return false;
            }
        } else if (_canvasTime && _canvasData) {
            // Normaler Modus: Kopiere Time + Data Canvas
            size_t timeBufferSize = FULL_WIDTH * TIME_AREA_H;
            size_t dataBufferSize = FULL_WIDTH * DATA_AREA_H;
            
            // Copy time area
            uint16_t* timeBuffer = _canvasTime->getBuffer();
            if (timeBuffer) {
                memcpy(destinationBuffer, timeBuffer, timeBufferSize * sizeof(uint16_t));
            }
            
            // Copy data area
            uint16_t* dataBuffer = _canvasData->getBuffer();
            if (dataBuffer) {
                memcpy(destinationBuffer + timeBufferSize, dataBuffer, dataBufferSize * sizeof(uint16_t));
            }
        } else {
            xSemaphoreGive(_canvasMutex);
            return false;
        }
        
        xSemaphoreGive(_canvasMutex);
        return true;
    }
    
    return false;
}
