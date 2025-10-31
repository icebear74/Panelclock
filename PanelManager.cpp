#include "PanelManager.hpp"
#include "ClockModule.hpp"
#include "MwaveSensorModule.hpp"
#include "GeneralTimeConverter.hpp"
#include <time.h>
#include <algorithm>

const PsramVector<DrawableModule*>& PanelManager::getAllModules() const {
    return _dataAreaModules;
}

PanelManager::PanelManager(HardwareConfig& hwConfig, GeneralTimeConverter& timeConverter) 
    : _hwConfig(hwConfig), _timeConverter(timeConverter) {}

PanelManager::~PanelManager() {
    delete _dma_display;
    delete _virtualDisp;
    delete _canvasTime;
    delete _canvasData;
    delete _fullCanvas;
    delete _u8g2;
}

bool PanelManager::begin() {
    _u8g2 = new U8G2_FOR_ADAFRUIT_GFX();

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

    _virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(VDISP_NUM_ROWS, VDISP_NUM_COLS, PANEL_RES_X, PANEL_RES_Y);
    _virtualDisp->setDisplay(*_dma_display);

    Serial.println("PanelManager: Display-Initialisierung abgeschlossen.");
    return true;
}

void PanelManager::registerClockModule(ClockModule* mod) { _clockMod = mod; }
void PanelManager::registerSensorModule(MwaveSensorModule* mod) { _sensorMod = mod; }

void PanelManager::registerModule(DrawableModule* mod) {
    if (!mod) return;

    mod->setRequestCallback([this](DrawableModule* m){ this->handlePriorityRequest(m); });
    mod->setReleaseCallback([this](DrawableModule* m){ this->handlePriorityRelease(m); });

    _dataAreaModules.push_back(mod);
}

void PanelManager::handlePriorityRequest(DrawableModule* mod) {
    Priority prio = mod->getPriority();

    if (prio == Priority::Normal) {
        if (!_interruptQueue.empty()) {
            _pendingNormalPriorityModule = mod;
            Serial.println("[PanelManager] 'Play-Next'-Anfrage (Prio: Normal) erhalten und geparkt, da Interrupt aktiv ist.");
        } else {
            _nextNormalPriorityModule = mod;
            Serial.println("[PanelManager] 'Play-Next'-Anfrage (Prio: Normal) erhalten.");
        }
        return;
    }

    if (_interruptQueue.empty()) {
        pausePlaylist();
    }

    if (std::find(_interruptQueue.begin(), _interruptQueue.end(), mod) == _interruptQueue.end()) {
        _interruptQueue.push_back(mod);
        std::sort(_interruptQueue.begin(), _interruptQueue.end(), [](const DrawableModule* a, const DrawableModule* b) {
            return a->getPriority() > b->getPriority();
        });
        Serial.printf("[PanelManager] Interrupt-Anfrage erhalten (Prio: %d). Aktive Interrupts: %d\n", (int)prio, _interruptQueue.size());
    }
}

void PanelManager::handlePriorityRelease(DrawableModule* mod) {
    auto it = std::find(_interruptQueue.begin(), _interruptQueue.end(), mod);
    if (it != _interruptQueue.end()) {
        _interruptQueue.erase(it);
        Serial.printf("[PanelManager] Interrupt-Priorität freigegeben (Prio: %d). Verbleibende Interrupts: %d\n", (int)mod->getPriority(), _interruptQueue.size());

        if (_interruptQueue.empty()) {
            resumePlaylist();
        }
    }
}

void PanelManager::tick() {
    if (!_interruptQueue.empty()) {
        DrawableModule* currentInterrupt = _interruptQueue.front();
        currentInterrupt->tick();

        if (currentInterrupt != _lastActiveInterrupt) {
            _lastActiveInterrupt = currentInterrupt;
            _interruptStartTime = millis();
            Serial.printf("[PanelManager] Neuer aktiver Interrupt (Prio: %d). Watchdog gestartet.\n", (int)currentInterrupt->getPriority());
        }
        unsigned long maxDuration = currentInterrupt->getMaxInterruptDuration();
        if (maxDuration > 0 && millis() - _interruptStartTime > maxDuration) {
            Serial.printf("[PanelManager] WARNUNG: Interrupt-Timeout für Modul (Prio: %d) überschritten! Erzwinge Freigabe.\n", (int)currentInterrupt->getPriority());
            handlePriorityRelease(currentInterrupt);
        }
        return;
    } 
    
    if (_lastActiveInterrupt != nullptr) {
        _lastActiveInterrupt = nullptr;
        _interruptStartTime = 0;
    }
    
    if (_currentModuleIndex < 0 && !_dataAreaModules.empty()) {
        switchNextModule();
    }
    
    if (_currentModuleIndex < 0) return;

    DrawableModule* currentMod = _dataAreaModules[_currentModuleIndex];
    currentMod->tick();

    unsigned long displayDuration = (_pausedModuleRemainingTime > 0) ? _pausedModuleRemainingTime : currentMod->getDisplayDuration();

    if (millis() - _lastSwitchTime > displayDuration) {
        _pausedModuleRemainingTime = 0;

        if (_pendingNormalPriorityModule) {
            _nextNormalPriorityModule = _pendingNormalPriorityModule;
            _pendingNormalPriorityModule = nullptr;
            Serial.println("[PanelManager] Geparkte 'Play-Next'-Anfrage wird beim nächsten Wechsel aktiv.");
        }
        
        switchNextModule();
    }
}

void PanelManager::switchNextModule(bool resume) {
    if (_dataAreaModules.empty()) {
        _currentModuleIndex = -1;
        return;
    }

    if (resume && _pausedModuleIndex != -1) {
        _currentModuleIndex = _pausedModuleIndex;
        _pausedModuleIndex = -1;
        
        // KORREKTUR: Setze den Timer zurück, damit das wiederaufgenommene Modul
        // seine volle Restzeit bekommt, bevor weitergeschaltet wird.
        _lastSwitchTime = millis();

    } else if (_nextNormalPriorityModule) {
        bool found = false;
        for (int i = 0; i < _dataAreaModules.size(); ++i) {
            if (_dataAreaModules[i] == _nextNormalPriorityModule) {
                _currentModuleIndex = i;
                found = true;
                break;
            }
        }
        if (found) {
            Serial.println("[PanelManager] 'Play-Next'-Modul wird jetzt angezeigt.");
        } else {
            _currentModuleIndex = (_currentModuleIndex + 1) % _dataAreaModules.size();
        }
        _nextNormalPriorityModule = nullptr;
        _lastSwitchTime = millis(); // Auch hier den Timer zurücksetzen
    } else {
        _currentModuleIndex = (_currentModuleIndex + 1) % _dataAreaModules.size();
        _lastSwitchTime = millis(); // Und auch hier
    }

    int initialIndex = _currentModuleIndex;
    do {
        if (_dataAreaModules[_currentModuleIndex]->isEnabled()) {
            _dataAreaModules[_currentModuleIndex]->resetPaging();
            // Der _lastSwitchTime wird bereits oben gesetzt, hier nicht noch einmal.
            return;
        }
        _currentModuleIndex = (_currentModuleIndex + 1) % _dataAreaModules.size();
    } while (_currentModuleIndex != initialIndex);

    _currentModuleIndex = -1;
}

void PanelManager::pausePlaylist() {
    if (_currentModuleIndex != -1) {
        unsigned long elapsedTime = millis() - _lastSwitchTime;
        unsigned long totalDuration = _dataAreaModules[_currentModuleIndex]->getDisplayDuration();
        _pausedModuleRemainingTime = (elapsedTime < totalDuration) ? (totalDuration - elapsedTime) : 0;
        _pausedModuleIndex = _currentModuleIndex;
        _currentModuleIndex = -1;
        Serial.printf("[PanelManager] Playlist pausiert. Modul %d hat %lu ms Restzeit.\n", _pausedModuleIndex, _pausedModuleRemainingTime);
    }
}

void PanelManager::resumePlaylist() {
    if (_pausedModuleIndex != -1) {
        Serial.printf("[PanelManager] Playlist wird fortgesetzt mit Modul %d.\n", _pausedModuleIndex);
        switchNextModule(true);
    } else {
        if (_pendingNormalPriorityModule) {
            _nextNormalPriorityModule = _pendingNormalPriorityModule;
            _pendingNormalPriorityModule = nullptr;
        }
        switchNextModule(false);
    }
}

void PanelManager::render() {
    if (!_sensorMod || !_sensorMod->isDisplayOn()) {
        if (_dma_display) {
            _dma_display->clearScreen();
            _dma_display->flipDMABuffer();
        }
        return;
    }

    if (!_virtualDisp || !_dma_display || !_canvasTime || !_canvasData) return;

    drawClockArea();
    drawDataArea();

    _virtualDisp->drawRGBBitmap(0, 0, _canvasTime->getBuffer(), _canvasTime->width(), _canvasTime->height());
    _virtualDisp->drawRGBBitmap(0, TIME_AREA_H, _canvasData->getBuffer(), _canvasData->width(), _canvasData->height());
    _dma_display->flipDMABuffer();
}

void PanelManager::drawClockArea() {
    if (!_clockMod || !_sensorMod) return;

    time_t now_utc;
    time(&now_utc);
    struct tm timeinfo;
    time_t local_epoch = _timeConverter.toLocal(now_utc);
    localtime_r(&local_epoch, &timeinfo);

    _clockMod->setTime(timeinfo);
    _clockMod->setSensorState(_sensorMod->isDisplayOn(), _sensorMod->getLastOnTime(), _sensorMod->getLastOffTime(), _sensorMod->getOnPercentage());
    _clockMod->tick();
    _clockMod->draw();
}

void PanelManager::drawDataArea() {
    if (!_interruptQueue.empty()) {
        _interruptQueue.front()->draw();
        return;
    }

    if (_currentModuleIndex >= 0 && _currentModuleIndex < _dataAreaModules.size()) {
        _dataAreaModules[_currentModuleIndex]->draw();
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