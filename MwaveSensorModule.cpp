#include "MwaveSensorModule.hpp"

// Statische Member hier definieren und initialisieren
char MwaveSensorModule::sensorDataBuffer[256];
int MwaveSensorModule::sensorDataIndex = 0;

MwaveSensorModule::MwaveSensorModule(DeviceConfig& deviceConf, HardwareConfig& hardwareConf, HardwareSerial& serial)
    : config(deviceConf), hwConfig(hardwareConf), sensorSerial(serial) {}

void MwaveSensorModule::begin() {
    if (config.mwaveSensorEnabled) {
        sensorSerial.begin(115200, SERIAL_8N1, hwConfig.mwaveRxPin, hwConfig.mwaveTxPin);
        Serial.println("[MWave] Sensor-Schnittstelle initialisiert.");
        sendHexData("FDFCFBFA0800120000006400000004030201");

        if (hwConfig.displayRelayPin != 255) {
            pinMode(hwConfig.displayRelayPin, OUTPUT);
            digitalWrite(hwConfig.displayRelayPin, HIGH); // Standard AN
            Serial.printf("[MWave] Relais-Pin %d initialisiert.\n", hwConfig.displayRelayPin);
        }
    } else {
        Serial.println("[MWave] Sensor ist deaktiviert.");
    }
}

void MwaveSensorModule::update(time_t now_utc) {
    if (!config.mwaveSensorEnabled) {
        if (!isDisplayOnState) {
            isDisplayOnState = true; // Falls Sensor deaktiviert wird, Display sicherheitshalber einschalten
            if (hwConfig.displayRelayPin != 255) digitalWrite(hwConfig.displayRelayPin, HIGH);
            Serial.println("[MWave] Sensor deaktiviert, schalte Display dauerhaft AN.");
            logStateChange(true, now_utc);
        }
        return;
    }

    if (!initialStateSet) {
        lastStateChangeToOnTime = now_utc;
        logStateChange(true, now_utc);
        initialStateSet = true;
        Serial.println("[MWave] Initialzustand: AN gesetzt.");
    }

    handleSensorSerial(now_utc);

    slidingEventHistory.erase(
        std::remove_if(slidingEventHistory.begin(), slidingEventHistory.end(),
            [now_utc, this](const SensorEvent& event) {
                return (now_utc - event.timestamp) > this->config.mwaveOnCheckDuration;
            }
        ), 
        slidingEventHistory.end()
    );

    if (slidingEventHistory.empty()) {
        currentOnPercentage = 0.0f;
    } else {
        int on_count = 0;
        for (const auto& event : slidingEventHistory) {
            if (event.type == EVENT_ON) on_count++;
        }
        currentOnPercentage = (float)on_count / slidingEventHistory.size() * 100.0f;
    }

    if (isDisplayOnState) {
        if (offCheckWindowStartTime == 0) offCheckWindowStartTime = now_utc;

        if (now_utc - offCheckWindowStartTime >= config.mwaveOffCheckDuration) {
            float on_ratio = (onCountInOffWindow + offCountInOffWindow > 0) ? ((float)onCountInOffWindow / (float)(onCountInOffWindow + offCountInOffWindow) * 100.0f) : 0.0f;
            
            if (offCountInOffWindow > 0 && on_ratio <= config.mwaveOffCheckOnPercent) {
                isDisplayOnState = false;
                lastStateChangeToOffTime = firstOffTimeInOffWindow;
                logStateChange(false, lastStateChangeToOffTime);
                if (hwConfig.displayRelayPin != 255) digitalWrite(hwConfig.displayRelayPin, LOW);
                Serial.printf("[MWave] >> Zustand: AUS (um %llu)\n", (long long unsigned)lastStateChangeToOffTime);
            }
            resetOffCheckWindowCounters();
        }
    } else {
        if (currentOnPercentage > config.mwaveOnCheckPercentage) {
            isDisplayOnState = true;
            lastStateChangeToOnTime = now_utc;
            logStateChange(true, lastStateChangeToOnTime);
            if (hwConfig.displayRelayPin != 255) digitalWrite(hwConfig.displayRelayPin, HIGH);
            Serial.printf("[MWave] >> Zustand: AN (Anteil: %.1f%%)\n", currentOnPercentage);
            resetOffCheckWindowCounters();
        }
    }
}

bool MwaveSensorModule::isDisplayOn() const { return isDisplayOnState; }
time_t MwaveSensorModule::getLastOnTime() const { return lastStateChangeToOnTime; }
time_t MwaveSensorModule::getLastOffTime() const { return lastStateChangeToOffTime; }
float MwaveSensorModule::getOnPercentage() const { return currentOnPercentage; }
const PsramVector<DisplayStateLogEntry>& MwaveSensorModule::getDisplayStateLog() const { return displayStateLog; }

void MwaveSensorModule::sendHexData(const char* hexString) {
    int len = strlen(hexString);
    byte buf[len / 2];
    for (int i = 0; i < len; i += 2) {
        char byte_str[3] = {hexString[i], hexString[i+1], '\0'};
        buf[i / 2] = (byte)strtoul(byte_str, NULL, 16);
    }
    sensorSerial.write(buf, sizeof(buf));
}

void MwaveSensorModule::processSensorData(const char* data, time_t now) {
    SensorEventType type;
    if (strcmp(data, "ON") == 0) {
        type = EVENT_ON;
        onCountInOffWindow++;
    } else if (strcmp(data, "OFF") == 0) {
        type = EVENT_OFF;
        offCountInOffWindow++;
        if (firstOffTimeInOffWindow == 0) {
            firstOffTimeInOffWindow = now;
        }
    } else {
        return;
    }
    slidingEventHistory.push_back({now, type});
}

void MwaveSensorModule::handleSensorSerial(time_t now) {
    while (sensorSerial.available() > 0) {
        char c = (char)sensorSerial.read();
        if (c == '\n') {
            sensorDataBuffer[sensorDataIndex] = '\0';
            if (sensorDataIndex > 0) processSensorData(sensorDataBuffer, now);
            sensorDataIndex = 0;
        } else if (sensorDataIndex < (sizeof(sensorDataBuffer) - 1)) {
            if (c != '\r') sensorDataBuffer[sensorDataIndex++] = c;
        } else {
            sensorDataIndex = 0;
        }
    }
}

void MwaveSensorModule::resetOffCheckWindowCounters() {
    offCheckWindowStartTime = 0;
    onCountInOffWindow = 0;
    offCountInOffWindow = 0;
    firstOffTimeInOffWindow = 0;
}

void MwaveSensorModule::logStateChange(bool state, time_t timestamp) {
    if (displayStateLog.size() >= 10) {
        displayStateLog.erase(displayStateLog.begin());
    }
    displayStateLog.push_back({timestamp, state});
}