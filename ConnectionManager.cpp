#include "ConnectionManager.hpp"
#include <WiFi.h>
#include "MultiLogger.hpp"

ConnectionManager::ConnectionManager(DeviceConfig& config) 
    : deviceConfig(config), 
      ntpClient(ntpUdp, DEFAULT_NTP_SERVER_PRIMARY, 0, 60 * 60 * 24 * 1000UL) 
{
    ntpUpdateIntervalMs = DEFAULT_NTP_UPDATE_INTERVAL_MIN * 60 * 1000UL;
}

bool ConnectionManager::begin() {
    if (deviceConfig.ssid.empty()) { return false; }
    status = ConnectionStatus::CONNECTING;
    displayStatus("Suche WLANs...");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks();
    if (n == 0) { displayStatus("Keine WLANs gefunden!"); delay(2000); status = ConnectionStatus::DISCONNECTED; return false; }
    struct WifiAP { PsramString ssid; PsramString bssid; int32_t rssi; int32_t channel; };
    std::vector<WifiAP, PsramAllocator<WifiAP>> matchingAPs;
    for (int i = 0; i < n; ++i) { if (WiFi.SSID(i) == deviceConfig.ssid.c_str()) { matchingAPs.push_back({WiFi.SSID(i).c_str(), WiFi.BSSIDstr(i).c_str(), WiFi.RSSI(i), WiFi.channel(i)}); } }
    if (matchingAPs.empty()) { displayStatus("WLAN nicht gefunden!"); delay(2000); status = ConnectionStatus::DISCONNECTED; return false; }
    std::sort(matchingAPs.begin(), matchingAPs.end(), [](const WifiAP& a, const WifiAP& b) { return a.rssi > b.rssi; });
    PsramString foundApsMsg = "APs fuer '"; foundApsMsg += deviceConfig.ssid.c_str(); foundApsMsg += "':\n";
    for (const auto& ap : matchingAPs) { foundApsMsg += ap.bssid.c_str(); foundApsMsg += " ("; foundApsMsg += String(ap.rssi).c_str(); foundApsMsg += " dBm)\n"; }
    displayStatus(foundApsMsg.c_str());
    delay(2000);
    const auto& bestAP = matchingAPs[0];
    displayStatus("Verbinde mit dem staerksten Signal...");
    uint8_t bssid_arr[6];
    sscanf(bestAP.bssid.c_str(), "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &bssid_arr[0], &bssid_arr[1], &bssid_arr[2], &bssid_arr[3], &bssid_arr[4], &bssid_arr[5]);
    WiFi.begin(deviceConfig.ssid.c_str(), deviceConfig.password.c_str(), bestAP.channel, bssid_arr);
    int retries = 40;
    while (WiFi.status() != WL_CONNECTED && retries > 0) { delay(500); retries--; }

    if (WiFi.status() == WL_CONNECTED) {
        PsramString msg = "Verbunden!\nIP: ";
        msg += WiFi.localIP().toString().c_str();
        displayStatus(msg.c_str());
        delay(1000);
        memcpy(lastBssid, WiFi.BSSID(), 6);
        lastChannel = WiFi.channel();

        Log.println("\n--- WLAN-Verbindungsinformationen ---");
        Log.printf("  IP-Adresse:  %s\n", WiFi.localIP().toString().c_str());
        Log.printf("  Gateway:     %s\n", WiFi.gatewayIP().toString().c_str());
        Log.printf("  Subnetzmaske:%s\n", WiFi.subnetMask().toString().c_str());
        Log.printf("  DNS-Server 1:%s\n", WiFi.dnsIP(0).toString().c_str());
        if (WiFi.dnsIP(1).toString() != "0.0.0.0") { Log.printf("  DNS-Server 2:%s\n", WiFi.dnsIP(1).toString().c_str()); }
        Log.println("-------------------------------------\n");

        displayStatus("Synchronisiere Zeit...");
        ntpClient.begin();
        
        Log.println("--- NTP-Vorbereitung ---");
        IPAddress ntpServerIP;
        Log.printf("  Prüfe DNS-Auflösung für '%s'...\n", DEFAULT_NTP_SERVER_PRIMARY);
        if (WiFi.hostByName(DEFAULT_NTP_SERVER_PRIMARY, ntpServerIP)) { Log.printf("  > DNS-Auflösung ERFOLGREICH. IP: %s\n", ntpServerIP.toString().c_str()); } 
        else { Log.println("  > DNS-Auflösung FEHLGESCHLAGEN!"); }
        Log.println("------------------------\n");

        Log.printf("NTP Versuch 1: %s\n", DEFAULT_NTP_SERVER_PRIMARY);
        ntpClient.setPoolServerName(DEFAULT_NTP_SERVER_PRIMARY);
        if (!ntpClient.forceUpdate()) {
            Log.println("  > Fehler bei Versuch 1.");
            
            Log.printf("NTP Versuch 2: %s\n", DEFAULT_NTP_SERVER_SECONDARY);
            ntpClient.setPoolServerName(DEFAULT_NTP_SERVER_SECONDARY);
            if (!ntpClient.forceUpdate()) {
                Log.println("  > Fehler bei Versuch 2.");
                
                PsramString gatewayIpStr = WiFi.gatewayIP().toString().c_str();
                Log.printf("NTP Versuch 3 (Dynamisches Gateway): %s\n", gatewayIpStr.c_str());
                displayStatus("Externe NTPs fehlgeschl.\nVersuche lokales Gateway...");
                ntpClient.setPoolServerName(gatewayIpStr.c_str());
                if (!ntpClient.forceUpdate()) {
                    Log.println("  > Fehler bei Versuch 3.");

                    Log.printf("NTP Versuch 4 (Fallback IP): %s\n", DEFAULT_NTP_SERVER_TERTIARY_IP);
                    displayStatus("Lokales Gateway fehlgeschl.\nVersuche Fallback IP...");
                    ntpClient.setPoolServerName(DEFAULT_NTP_SERVER_TERTIARY_IP);
                    
                    while (!ntpClient.forceUpdate()) {
                        Log.println("  > Fehler bei Versuch 4. Beginne von vorn in 2s...");
                        displayStatus("Zeit-Sync fehlgeschl.\nVersuche erneut...");
                        delay(2000);
                    }
                }
            }
        }

        time_t utc_time = ntpClient.getEpochTime();
        timeval tv = { utc_time, 0 };
        settimeofday(&tv, nullptr);
        
        Log.println("\nZeit erfolgreich synchronisiert! Systemzeit ist UTC.");
        displayStatus("Zeit OK");
        delay(1000);

        lastNtpSyncTime = millis();
        status = ConnectionStatus::CONNECTED;
        return true;

    } else {
        displayStatus("WLAN fehlgeschlagen!");
        WiFi.disconnect();
        delay(2000);
        status = ConnectionStatus::DISCONNECTED;
        return false;
    }
}

void ConnectionManager::update() {
    if (status == ConnectionStatus::CONNECTED) {
        if (millis() - lastNtpSyncTime > ntpUpdateIntervalMs) {
            Log.println("[ConnectionManager] Führe periodisches NTP-Update aus...");
            ntpClient.setPoolServerName(DEFAULT_NTP_SERVER_PRIMARY);
            if (ntpClient.forceUpdate()) {
                time_t utc_time = ntpClient.getEpochTime();
                timeval tv = { utc_time, 0 };
                settimeofday(&tv, nullptr);
                Log.println("[ConnectionManager] Periodisches NTP-Update erfolgreich.");
                lastNtpSyncTime = millis();
            } else {
                Log.println("[ConnectionManager] Periodisches NTP-Update fehlgeschlagen.");
            }
        }
    }
}

bool ConnectionManager::isConnected() const {
    return status == ConnectionStatus::CONNECTED;
}

ConnectionManager::ConnectionStatus ConnectionManager::getStatus() const {
    return status;
}