#ifndef CONNECTION_MANAGER_HPP
#define CONNECTION_MANAGER_HPP

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <algorithm>
#include <vector>
#include "webconfig.hpp"
#include "PsramUtils.hpp"

// Fallback-Kette für NTP-Server
#define DEFAULT_NTP_SERVER_PRIMARY "ptbtime1.ptb.de"
#define DEFAULT_NTP_SERVER_SECONDARY "de.pool.ntp.org"
#define DEFAULT_NTP_SERVER_TERTIARY_IP "216.239.35.0" // Google Public NTP
#define DEFAULT_NTP_UPDATE_INTERVAL_MIN 60

// Globale Funktion aus der .ino-Datei bekannt machen
void displayStatus(const char* msg);

class ConnectionManager {
public:
    enum class ConnectionStatus {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        RECONNECTING_FAST,
        RECONNECTING_FULL_SCAN,
        FAILED_PERMANENTLY
    };

    ConnectionManager(DeviceConfig& config);

    bool begin();
    void update();
    bool isConnected() const;
    ConnectionStatus getStatus() const;

private:
    DeviceConfig& deviceConfig;
    WiFiUDP ntpUdp;
    NTPClient ntpClient;
    ConnectionStatus status = ConnectionStatus::DISCONNECTED;
    uint8_t lastBssid[6] = {0};
    int lastChannel = 0;
    unsigned long ntpUpdateIntervalMs;
    unsigned long lastNtpSyncTime = 0;
};

#endif // CONNECTION_MANAGER_HPP