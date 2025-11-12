#ifndef MWAVE_SENSOR_MODULE_HPP
#define MWAVE_SENSOR_MODULE_HPP

#include <Arduino.h>
#include <vector>
#include "PsramUtils.hpp"
#include "webconfig.hpp"
#include "HardwareConfig.hpp"

// Enum für den Sensortyp
enum SensorEventType { EVENT_ON, EVENT_OFF };

// Struct für ein Sensor-Event
struct SensorEvent {
    time_t timestamp;
    SensorEventType type;
};

// Struct für den Log-Eintrag
struct DisplayStateLogEntry {
    time_t timestamp;
    bool state; // true = AN, false = AUS
};


class MwaveSensorModule {
public:
    MwaveSensorModule(DeviceConfig& deviceConf, HardwareConfig& hardwareConf, HardwareSerial& serial);
    void begin();
    void update(time_t now_utc);
    bool isDisplayOn() const;
    time_t getLastOnTime() const;
    time_t getLastOffTime() const;
    float getOnPercentage() const;
    const PsramVector<DisplayStateLogEntry>& getDisplayStateLog() const;

private:
    DeviceConfig& config;
    HardwareConfig& hwConfig;
    HardwareSerial& sensorSerial;

    bool isDisplayOnState = true;
    time_t lastStateChangeToOnTime = 0;
    time_t lastStateChangeToOffTime = 0;
    bool initialStateSet = false;
    float currentOnPercentage = 100.0f;

    PsramVector<SensorEvent> slidingEventHistory;
    PsramVector<DisplayStateLogEntry> displayStateLog;
    
    time_t offCheckWindowStartTime = 0;
    unsigned int onCountInOffWindow = 0;
    unsigned int offCountInOffWindow = 0;
    time_t firstOffTimeInOffWindow = 0;

    void sendHexData(const char* hexString);
    void processSensorData(const char* data, time_t now);
    void handleSensorSerial(time_t now);
    void resetOffCheckWindowCounters();
    void logStateChange(bool state, time_t timestamp);

    // Statische Member nur deklarieren
    static char sensorDataBuffer[256];
    static int sensorDataIndex;
};

#endif // MWAVE_SENSOR_MODULE_HPP