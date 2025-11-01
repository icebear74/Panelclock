/**
 * @file PanelManager.cpp
 * @author icebear74, copilot
 * @brief Finale, zustandsbasierte Implementierung des PanelManagers.
 * @version 6.1
 */
#include "PanelManager.hpp"
#include "ClockModule.hpp"
#include "MwaveSensorModule.hpp"
#include "GeneralTimeConverter.hpp"
#include <time.h>
#include <algorithm>

// Helper für Debug-Logging
#define PM_DEBUG_LOG(format, ...) Serial.printf("[PM_DEBUG] " format "\n", ##__VA_ARGS__)

PanelManager::PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter) 
    : _hwConfig(hwConfig), _timeConverter(timeConverter) {}

PanelManager::~PanelManager() {
    delete _dma_display;
    delete _virtualDisp;
    delete _canvasTime;
    delete _canvasData;
    delete _u8g2;
}

bool PanelManager::begin() {
    _u8g2 = new U8G2_FOR_ADAFRUIT_GFX();
    _canvasTime = new GFXcanvas16(FULL_WIDTH, TIME_AREA_H);
    _canvasData = new GFXcanvas16(FULL_WIDTH, DATA_AREA_H);
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
    _dma_display = new MatrixPanel_I2S_DMA(mxconfig);
    _dma_display->begin();
    _dma_display->setBrightness8(128);
    _dma_display->clearScreen();
    _virtualDisp = new VirtualMatrixPanel_T<1, 1>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
    _virtualDisp->setDisplay(*_dma_display);
    _lastTickTimestamp = millis();
    PM_DEBUG_LOG("PanelManager initialisiert. Drei-Listen-Architektur aktiv.");
    return true;
}

void PanelManager::registerClockModule(ClockModule* mod) { _clockMod = mod; }
void PanelManager::registerSensorModule(MwaveSensorModule* mod) { _sensorMod = mod; }

void PanelManager::registerModule(DrawableModule* mod, bool isHidden, bool isDisabled) {
    if (!mod) return;
    mod->setRequestCallback([this](DrawableModule* m){ this->handlePriorityRequest(m); });
    mod->setReleaseCallback([this](DrawableModule* m){ this->handlePriorityRelease(m); });
    
    _moduleCatalog.push_back({
        mod,
        mod->getModuleName(),      // moduleNameShort
        mod->getModuleDisplayName(), // moduleNameDisplay
        false, false, isHidden, isDisabled, 0, 0,
        mod->getPriority()
    });
    PM_DEBUG_LOG("Modul '%s' zum Katalog hinzugefügt (Hidden: %d, Disabled: %d)", mod->getModuleName(), isHidden, isDisabled);

    if (!isHidden && !isDisabled) {
        PlaylistEntry newEntry = _moduleCatalog.back();
        newEntry.totalRuntimeMs = newEntry.module->getMaxRuntime();
        _playlist.push_back(newEntry);
        PM_DEBUG_LOG("...und zur Default-Playlist hinzugefügt mit Laufzeit %lu ms.", newEntry.totalRuntimeMs);
    }
}

void PanelManager::handlePriorityRequest(DrawableModule* mod) {
    const PlaylistEntry* catalogEntry = nullptr;
    for (const auto& entry : _moduleCatalog) {
        if (entry.module == mod) {
            catalogEntry = &entry;
            break;
        }
    }
    if (!catalogEntry || catalogEntry->isDisabled) return;

    if (catalogEntry->priority == Priority::Normal) {
        if (!_interruptQueue.empty()) _pendingNormalPriorityModule = mod;
        else _nextNormalPriorityModule = mod;
        PM_DEBUG_LOG("Play-Next Anfrage für '%s' erhalten.", catalogEntry->moduleNameShort.c_str());
        return;
    }

    if (_interruptQueue.empty()) {
        pausePlaylist();
    }

    for (const auto& entry : _interruptQueue) { if (entry.module == mod) return; }

    PlaylistEntry interruptEntry = *catalogEntry;
    _interruptQueue.push_back(interruptEntry);
    
    std::sort(_interruptQueue.begin(), _interruptQueue.end(), [](const PlaylistEntry& a, const PlaylistEntry& b) {
        return a.priority > b.priority;
    });

    PM_DEBUG_LOG("Interrupt-Anfrage für '%s' (Prio: %d) verarbeitet. Queue-Größe: %d", catalogEntry->moduleNameShort.c_str(), (int)interruptEntry.priority, _interruptQueue.size());
}

void PanelManager::handlePriorityRelease(DrawableModule* mod) {
    auto it = std::remove_if(_interruptQueue.begin(), _interruptQueue.end(), 
        [mod](const PlaylistEntry& entry) { return entry.module == mod; });

    if (it != _interruptQueue.end()) {
        PM_DEBUG_LOG("Interrupt für '%s' freigegeben. Verbleibende Interrupts: %d", it->moduleNameShort.c_str(), _interruptQueue.size() - 1);
        _interruptQueue.erase(it, _interruptQueue.end());
        
        if (_interruptQueue.empty()) {
            resumePlaylist();
        }
    }
}

void PanelManager::tick() {
    unsigned long now = millis();
    unsigned long deltaTime = now - _lastTickTimestamp;
    _lastTickTimestamp = now;

    for (const auto& entry : _moduleCatalog) {
        if (entry.module && !entry.isDisabled) {
            entry.module->periodicTick();
        }
    }

    if (!_interruptQueue.empty()) {
        // --- INTERRUPT-MODUS ---
        PlaylistEntry* winner = &_interruptQueue.front();

        for (auto& entry : _interruptQueue) {
            if (&entry != winner && entry.isRunning && !entry.isPaused) {
                PM_DEBUG_LOG("Pausiere niederprioren Interrupt '%s'.", entry.moduleNameShort.c_str());
                entry.isPaused = true;
            }
        }
        
        if (!winner->isRunning) {
            activateEntry(*winner);
        }
        if (winner->isPaused) {
             winner->isPaused = false;
             PM_DEBUG_LOG("Setze hochprioren Interrupt '%s' fort.", winner->moduleNameShort.c_str());
        }
        
        if (winner->remainingTimeMs > deltaTime) winner->remainingTimeMs -= deltaTime;
        else winner->remainingTimeMs = 0;

        winner->module->tick();
        
        if (winner->remainingTimeMs == 0 || winner->module->isFinished()) {
            PM_DEBUG_LOG("Interrupt '%s' ist beendet.", winner->moduleNameShort.c_str());
            handlePriorityRelease(winner->module);
        }

    } else {
        // --- NORMALER PLAYLIST-MODUS ---
        int runningIndex = -1;
        for (int i = 0; i < _playlist.size(); ++i) {
            if (_playlist[i].isRunning) {
                runningIndex = i;
                break;
            }
        }

        if (runningIndex == -1) {
            PM_DEBUG_LOG("Kein Modul aktiv, starte nächstes.");
            switchNextModule();
            return;
        }

        PlaylistEntry& currentEntry = _playlist[runningIndex];
        if (currentEntry.isPaused) return;

        if (currentEntry.remainingTimeMs > deltaTime) currentEntry.remainingTimeMs -= deltaTime;
        else currentEntry.remainingTimeMs = 0;

        currentEntry.module->tick();

        if (currentEntry.remainingTimeMs == 0 || currentEntry.module->isFinished()) {
            PM_DEBUG_LOG("Modul '%s' beendet. Wechsle zum nächsten.", currentEntry.moduleNameShort.c_str());
            currentEntry.isRunning = false;
            if (_pendingNormalPriorityModule) {
                _nextNormalPriorityModule = _pendingNormalPriorityModule;
                _pendingNormalPriorityModule = nullptr;
            }
            switchNextModule();
        }
    }
}

void PanelManager::switchNextModule() {
    if (_playlist.empty()) return;

    int startIndex = (_currentPlaylistIndex == -1) ? 0 : (_currentPlaylistIndex + 1);

    if (_nextNormalPriorityModule) {
        for (int i = 0; i < _playlist.size(); ++i) {
            if (_playlist[i].module == _nextNormalPriorityModule) {
                startIndex = i;
                PM_DEBUG_LOG("Play-Next: Springe zu Modul '%s'.", _playlist[i].moduleNameShort.c_str());
                break;
            }
        }
        _nextNormalPriorityModule = nullptr;
    }

    for (int i = 0; i < _playlist.size(); ++i) {
        int indexToCheck = (startIndex + i) % _playlist.size();
        PlaylistEntry& entry = _playlist[indexToCheck];

        if (entry.module && !entry.isDisabled && !entry.isHidden) {
            _currentPlaylistIndex = indexToCheck;
            activateEntry(entry);
            return;
        }
    }
    _currentPlaylistIndex = -1;
    PM_DEBUG_LOG("Kein aktivierbares Modul in der Playlist gefunden.");
}

void PanelManager::pausePlaylist() {
    int runningIndex = -1;
    for (int i = 0; i < _playlist.size(); ++i) {
        if (_playlist[i].isRunning) {
            runningIndex = i;
            break;
        }
    }
    
    if (runningIndex != -1) {
        PlaylistEntry& entry = _playlist[runningIndex];
        if (!entry.isPaused) {
            entry.isPaused = true;
            PM_DEBUG_LOG("Playlist pausiert. Modul '%s' (isRunning: %d, isPaused: %d).", entry.moduleNameShort.c_str(), entry.isRunning, entry.isPaused);
        }
    }
}

void PanelManager::resumePlaylist() {
    int pausedIndex = -1;
    for (int i = 0; i < _playlist.size(); ++i) {
        if (_playlist[i].isRunning && _playlist[i].isPaused) {
            pausedIndex = i;
            break;
        }
    }

    if (pausedIndex != -1) {
        PlaylistEntry& entry = _playlist[pausedIndex];
        entry.isPaused = false;
        _currentPlaylistIndex = pausedIndex;
        PM_DEBUG_LOG("Playlist fortgesetzt mit Modul '%s'. Restzeit: %lu ms.", entry.moduleNameShort.c_str(), entry.remainingTimeMs);
    } else {
        PM_DEBUG_LOG("Resume aufgerufen, aber kein pausiertes Modul gefunden. Starte Playlist neu.");
        switchNextModule();
    }
}

void PanelManager::activateEntry(PlaylistEntry& entry) {
    PM_DEBUG_LOG("Aktiviere Modul '%s'.", entry.moduleNameShort.c_str());
    entry.totalRuntimeMs = entry.module->getMaxRuntime();
    entry.remainingTimeMs = entry.totalRuntimeMs;
    entry.isRunning = true;
    entry.isPaused = false; 
    entry.module->activateModule();
    PM_DEBUG_LOG("...Laufzeit auf %lu ms gesetzt.", entry.totalRuntimeMs);
}

void PanelManager::render() {
    if (!_sensorMod || !_sensorMod->isDisplayOn()) {
        if (_dma_display) _dma_display->clearScreen();
        return;
    }
    
    drawClockArea();
    
    if (!_interruptQueue.empty()) {
        if (_interruptQueue.front().isRunning) {
            _interruptQueue.front().module->draw();
        }
    } else {
        int runningIndex = -1;
        for(int i = 0; i < _playlist.size(); ++i) {
            if (_playlist[i].isRunning && !_playlist[i].isPaused) {
                runningIndex = i;
                break;
            }
        }
        if (runningIndex != -1) {
            _playlist[runningIndex].module->draw();
        } else {
            if (_canvasData) _canvasData->fillScreen(0);
        }
    }

    _virtualDisp->drawRGBBitmap(0, 0, _canvasTime->getBuffer(), FULL_WIDTH, TIME_AREA_H);
    _virtualDisp->drawRGBBitmap(0, TIME_AREA_H, _canvasData->getBuffer(), FULL_WIDTH, DATA_AREA_H);
}

void PanelManager::drawClockArea() {
    if (!_clockMod) return;
    time_t now_utc;
    time(&now_utc);
    struct tm timeinfo;
    time_t local_epoch = _timeConverter.toLocal(now_utc);
    localtime_r(&local_epoch, &timeinfo);
    _clockMod->setTime(timeinfo);
    if (_sensorMod) _clockMod->setSensorState(_sensorMod->isDisplayOn(), _sensorMod->getLastOnTime(), _sensorMod->getLastOffTime(), _sensorMod->getOnPercentage());
    _clockMod->tick();
    _clockMod->draw();
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
}