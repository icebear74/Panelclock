#include "BackupManager.hpp"
#include "Application.hpp"
#include "DrawableModule.hpp"
#include "MultiLogger.hpp"
#include "webconfig.hpp"
#include "HardwareConfig.hpp"
#include <base64.h>
#include <time.h>
#include <sys/stat.h>

// External references
extern DeviceConfig* deviceConfig;
extern HardwareConfig* hardwareConfig;
extern GeneralTimeConverter* timeConverter;
extern TankerkoenigModule* tankerkoenigModule;

#define BACKUP_DIR "/backups"
#define LAST_BACKUP_TIME_FILE "/last_backup_time.txt"

BackupManager::BackupManager(Application* app) 
    : _app(app), _lastBackupTime(0) {
}

BackupManager::~BackupManager() {
}

void BackupManager::begin() {
    Log.println("[BackupManager] Initializing...");
    ensureBackupDirectory();
    loadLastBackupTime();
    Log.println("[BackupManager] Ready");
}

void BackupManager::ensureBackupDirectory() {
    if (!LittleFS.exists(BACKUP_DIR)) {
        Log.printf("[BackupManager] Creating backup directory: %s\n", BACKUP_DIR);
        LittleFS.mkdir(BACKUP_DIR);
    }
}

PsramString BackupManager::generateBackupFilename(bool manualBackup) {
    char filename[64];
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    
    const char* prefix = manualBackup ? "manual_" : "";
    snprintf(filename, sizeof(filename), "%sbackup_%04d-%02d-%02d_%02d-%02d-%02d.json",
             prefix,
             timeinfo->tm_year + 1900,
             timeinfo->tm_mon + 1,
             timeinfo->tm_mday,
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);
    
    return PsramString(filename);
}

PsramString BackupManager::getBackupPath(const PsramString& filename) {
    PsramString path = BACKUP_DIR;
    path += "/";
    path += filename;
    return path;
}

bool BackupManager::createBackup(bool manualBackup) {
    Log.println("[BackupManager] Creating backup...");
    
    // Generate filename
    PsramString filename = generateBackupFilename(manualBackup);
    PsramString fullPath = getBackupPath(filename);
    
    Log.printf("[BackupManager] Backup file: %s\n", fullPath.c_str());
    
    // Create JSON document (allocate in PSRAM)
    DynamicJsonDocument doc(256 * 1024); // 256KB should be enough
    
    // Add metadata
    doc["version"] = "1.0";
    doc["timestamp"] = time(nullptr);
    doc["manual"] = manualBackup;
    
    // Collect all data
    collectConfiguration(doc);
    collectModuleData(doc);
    collectCertificates(doc);
    collectJsonFiles(doc);
    
    // Write to file
    File file = LittleFS.open(fullPath.c_str(), "w");
    if (!file) {
        Log.printf("[BackupManager] ERROR: Could not create backup file: %s\n", fullPath.c_str());
        return false;
    }
    
    size_t bytesWritten = serializeJson(doc, file);
    file.close();
    
    Log.printf("[BackupManager] Backup created successfully: %s (%u bytes)\n", 
               filename.c_str(), bytesWritten);
    
    // Update last backup time
    _lastBackupTime = time(nullptr);
    saveLastBackupTime();
    
    // Rotate old backups
    rotateBackups();
    
    return true;
}

void BackupManager::collectConfiguration(JsonDocument& doc) {
    Log.println("[BackupManager] Collecting configuration...");
    
    JsonObject config = doc["configuration"].to<JsonObject>();
    
    // Device config
    if (deviceConfig) {
        JsonObject deviceConf = config["device"].to<JsonObject>();
        deviceConf["hostname"] = deviceConfig->hostname.c_str();
        deviceConf["ssid"] = deviceConfig->ssid.c_str();
        deviceConf["password"] = deviceConfig->password.c_str();
        deviceConf["otaPassword"] = deviceConfig->otaPassword.c_str();
        deviceConf["timezone"] = deviceConfig->timezone.c_str();
        deviceConf["tankerApiKey"] = deviceConfig->tankerApiKey.c_str();
        deviceConf["stationId"] = deviceConfig->stationId.c_str();
        deviceConf["tankerkoenigStationIds"] = deviceConfig->tankerkoenigStationIds.c_str();
        deviceConf["stationFetchIntervalMin"] = deviceConfig->stationFetchIntervalMin;
        deviceConf["icsUrl"] = deviceConfig->icsUrl.c_str();
        deviceConf["calendarFetchIntervalMin"] = deviceConfig->calendarFetchIntervalMin;
        deviceConf["calendarScrollMs"] = deviceConfig->calendarScrollMs;
        deviceConf["calendarDateColor"] = deviceConfig->calendarDateColor.c_str();
        deviceConf["calendarTextColor"] = deviceConfig->calendarTextColor.c_str();
        deviceConf["calendarDisplaySec"] = deviceConfig->calendarDisplaySec;
        deviceConf["stationDisplaySec"] = deviceConfig->stationDisplaySec;
        deviceConf["calendarFastBlinkHours"] = deviceConfig->calendarFastBlinkHours;
        deviceConf["calendarUrgentThresholdHours"] = deviceConfig->calendarUrgentThresholdHours;
        deviceConf["calendarUrgentDurationSec"] = deviceConfig->calendarUrgentDurationSec;
        deviceConf["calendarUrgentRepeatMin"] = deviceConfig->calendarUrgentRepeatMin;
        deviceConf["dartsOomEnabled"] = deviceConfig->dartsOomEnabled;
        deviceConf["dartsProTourEnabled"] = deviceConfig->dartsProTourEnabled;
        deviceConf["dartsDisplaySec"] = deviceConfig->dartsDisplaySec;
        deviceConf["trackedDartsPlayers"] = deviceConfig->trackedDartsPlayers.c_str();
        deviceConf["fritzboxEnabled"] = deviceConfig->fritzboxEnabled;
        deviceConf["fritzboxIp"] = deviceConfig->fritzboxIp.c_str();
        deviceConf["fritzboxUser"] = deviceConfig->fritzboxUser.c_str();
        deviceConf["fritzboxPassword"] = deviceConfig->fritzboxPassword.c_str();
        deviceConf["weatherEnabled"] = deviceConfig->weatherEnabled;
        deviceConf["weatherApiKey"] = deviceConfig->weatherApiKey.c_str();
        deviceConf["weatherFetchIntervalMin"] = deviceConfig->weatherFetchIntervalMin;
        deviceConf["weatherDisplaySec"] = deviceConfig->weatherDisplaySec;
        deviceConf["weatherShowCurrent"] = deviceConfig->weatherShowCurrent;
        deviceConf["weatherShowHourly"] = deviceConfig->weatherShowHourly;
        deviceConf["weatherShowDaily"] = deviceConfig->weatherShowDaily;
        deviceConf["weatherDailyForecastDays"] = deviceConfig->weatherDailyForecastDays;
        deviceConf["weatherHourlyMode"] = deviceConfig->weatherHourlyMode;
        deviceConf["weatherHourlySlotMorning"] = deviceConfig->weatherHourlySlotMorning;
        deviceConf["weatherHourlySlotNoon"] = deviceConfig->weatherHourlySlotNoon;
        deviceConf["weatherHourlySlotEvening"] = deviceConfig->weatherHourlySlotEvening;
        deviceConf["weatherHourlyInterval"] = deviceConfig->weatherHourlyInterval;
        deviceConf["weatherAlertsEnabled"] = deviceConfig->weatherAlertsEnabled;
        deviceConf["weatherAlertsDisplaySec"] = deviceConfig->weatherAlertsDisplaySec;
        deviceConf["weatherAlertsRepeatMin"] = deviceConfig->weatherAlertsRepeatMin;
        deviceConf["tankerkoenigCertFile"] = deviceConfig->tankerkoenigCertFile.c_str();
        deviceConf["dartsCertFile"] = deviceConfig->dartsCertFile.c_str();
        deviceConf["googleCertFile"] = deviceConfig->googleCertFile.c_str();
        deviceConf["webClientBufferSize"] = deviceConfig->webClientBufferSize;
        deviceConf["mwaveSensorEnabled"] = deviceConfig->mwaveSensorEnabled;
        deviceConf["mwaveOffCheckDuration"] = deviceConfig->mwaveOffCheckDuration;
        deviceConf["mwaveOffCheckOnPercent"] = deviceConfig->mwaveOffCheckOnPercent;
        deviceConf["mwaveOnCheckDuration"] = deviceConfig->mwaveOnCheckDuration;
        deviceConf["mwaveOnCheckPercentage"] = deviceConfig->mwaveOnCheckPercentage;
        deviceConf["userLatitude"] = deviceConfig->userLatitude;
        deviceConf["userLongitude"] = deviceConfig->userLongitude;
        deviceConf["movingAverageDays"] = deviceConfig->movingAverageDays;
        deviceConf["trendAnalysisDays"] = deviceConfig->trendAnalysisDays;
    }
    
    // Hardware config
    if (hardwareConfig) {
        JsonObject hwConf = config["hardware"].to<JsonObject>();
        hwConf["R1"] = hardwareConfig->R1;
        hwConf["G1"] = hardwareConfig->G1;
        hwConf["B1"] = hardwareConfig->B1;
        hwConf["R2"] = hardwareConfig->R2;
        hwConf["G2"] = hardwareConfig->G2;
        hwConf["B2"] = hardwareConfig->B2;
        hwConf["A"] = hardwareConfig->A;
        hwConf["B"] = hardwareConfig->B;
        hwConf["C"] = hardwareConfig->C;
        hwConf["D"] = hardwareConfig->D;
        hwConf["E"] = hardwareConfig->E;
        hwConf["CLK"] = hardwareConfig->CLK;
        hwConf["LAT"] = hardwareConfig->LAT;
        hwConf["OE"] = hardwareConfig->OE;
    }
}

void BackupManager::collectModuleData(JsonDocument& doc) {
    Log.println("[BackupManager] Collecting module data...");
    
    JsonObject modules = doc["modules"].to<JsonObject>();
    
    // Collect from TankerkoenigModule (has existing backup method)
    if (tankerkoenigModule) {
        DynamicJsonDocument moduleDoc(64 * 1024);
        JsonObject moduleData = tankerkoenigModule->backup(moduleDoc);
        if (!moduleData.isNull()) {
            modules["tankerkoenig"] = moduleData;
        }
    }
    
    // TODO: Add other modules that implement backup() method when they exist
    // For now, we'll also collect their JSON files via collectJsonFiles()
}

void BackupManager::collectCertificates(JsonDocument& doc) {
    Log.println("[BackupManager] Collecting certificates...");
    
    JsonObject certs = doc["certificates"].to<JsonObject>();
    
    // Collect all .pem files from root directory
    File root = LittleFS.open("/", "r");
    if (!root || !root.isDirectory()) {
        Log.println("[BackupManager] Could not open root directory");
        return;
    }
    
    File file = root.openNextFile();
    while (file) {
        PsramString filename = file.name();
        if (filename.endsWith(".pem") || filename.endsWith(".crt") || filename.endsWith(".cer")) {
            Log.printf("[BackupManager] Backing up certificate: %s\n", filename.c_str());
            
            // Read certificate content
            size_t fileSize = file.size();
            char* buffer = (char*)ps_malloc(fileSize + 1);
            if (buffer) {
                file.seek(0);
                size_t bytesRead = file.readBytes(buffer, fileSize);
                buffer[bytesRead] = '\0';
                
                certs[filename.c_str()] = buffer;
                free(buffer);
            }
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
}

void BackupManager::collectJsonFiles(JsonDocument& doc) {
    Log.println("[BackupManager] Collecting JSON files...");
    
    JsonObject jsonFiles = doc["json_files"].to<JsonObject>();
    
    // List of JSON files to back up (excluding config.json and hardware.json as they're handled separately)
    const char* jsonFileList[] = {
        "/station_cache.json",
        "/station_price_stats.json",
        "/price_cache.json",
        "/calendar_cache.json",
        "/weather_cache.json",
        nullptr
    };
    
    for (int i = 0; jsonFileList[i] != nullptr; i++) {
        const char* filepath = jsonFileList[i];
        if (LittleFS.exists(filepath)) {
            Log.printf("[BackupManager] Backing up JSON file: %s\n", filepath);
            
            File file = LittleFS.open(filepath, "r");
            if (file) {
                DynamicJsonDocument fileDoc(32 * 1024);
                DeserializationError error = deserializeJson(fileDoc, file);
                file.close();
                
                if (!error) {
                    // Extract just the filename (without path) for the key
                    PsramString filename = filepath;
                    int lastSlash = filename.lastIndexOf('/');
                    if (lastSlash >= 0) {
                        filename = filename.substring(lastSlash + 1);
                    }
                    jsonFiles[filename.c_str()] = fileDoc.as<JsonObject>();
                } else {
                    Log.printf("[BackupManager] Error parsing JSON file %s: %s\n", 
                               filepath, error.c_str());
                }
            }
        }
    }
}

bool BackupManager::restoreFromBackup(const PsramString& filename) {
    Log.printf("[BackupManager] Restoring from backup: %s\n", filename.c_str());
    
    PsramString fullPath = getBackupPath(filename);
    
    if (!LittleFS.exists(fullPath.c_str())) {
        Log.printf("[BackupManager] ERROR: Backup file not found: %s\n", fullPath.c_str());
        return false;
    }
    
    File file = LittleFS.open(fullPath.c_str(), "r");
    if (!file) {
        Log.printf("[BackupManager] ERROR: Could not open backup file: %s\n", fullPath.c_str());
        return false;
    }
    
    // Parse backup file
    DynamicJsonDocument doc(256 * 1024);
    DeserializationError error = deserializeJson(doc, file);
    file.close();
    
    if (error) {
        Log.printf("[BackupManager] ERROR: Could not parse backup file: %s\n", error.c_str());
        return false;
    }
    
    Log.println("[BackupManager] Backup file parsed successfully");
    
    // Restore configuration
    if (doc["configuration"].is<JsonObject>()) {
        JsonObject config = doc["configuration"];
        
        // Restore device config
        if (config["device"].is<JsonObject>() && deviceConfig) {
            JsonObject deviceConf = config["device"];
            // We'll write this to config.json
            File configFile = LittleFS.open("/config.json", "w");
            if (configFile) {
                serializeJson(deviceConf, configFile);
                configFile.close();
                Log.println("[BackupManager] Device config restored");
            }
        }
        
        // Restore hardware config
        if (config["hardware"].is<JsonObject>() && hardwareConfig) {
            JsonObject hwConf = config["hardware"];
            File hwFile = LittleFS.open("/hardware.json", "w");
            if (hwFile) {
                serializeJson(hwConf, hwFile);
                hwFile.close();
                Log.println("[BackupManager] Hardware config restored");
            }
        }
    }
    
    // Restore certificates
    if (doc["certificates"].is<JsonObject>()) {
        JsonObject certs = doc["certificates"];
        for (JsonPair kv : certs) {
            PsramString certPath = "/";
            certPath += kv.key().c_str();
            
            File certFile = LittleFS.open(certPath.c_str(), "w");
            if (certFile) {
                certFile.print(kv.value().as<const char*>());
                certFile.close();
                Log.printf("[BackupManager] Certificate restored: %s\n", certPath.c_str());
            }
        }
    }
    
    // Restore JSON files
    if (doc["json_files"].is<JsonObject>()) {
        JsonObject jsonFiles = doc["json_files"];
        for (JsonPair kv : jsonFiles) {
            PsramString filepath = "/";
            filepath += kv.key().c_str();
            
            File jsonFile = LittleFS.open(filepath.c_str(), "w");
            if (jsonFile) {
                serializeJson(kv.value(), jsonFile);
                jsonFile.close();
                Log.printf("[BackupManager] JSON file restored: %s\n", filepath.c_str());
            }
        }
    }
    
    // Restore module data
    if (doc["modules"].is<JsonObject>()) {
        JsonObject modules = doc["modules"];
        
        // Restore TankerkoenigModule data
        if (modules["tankerkoenig"].is<JsonObject>() && tankerkoenigModule) {
            JsonObject tankerData = modules["tankerkoenig"];
            tankerkoenigModule->restore(tankerData);
            Log.println("[BackupManager] TankerkoenigModule data restored");
        }
    }
    
    Log.println("[BackupManager] Restore completed successfully. Device needs restart.");
    return true;
}

PsramVector<BackupInfo> BackupManager::listBackups() {
    PsramVector<BackupInfo> backups;
    
    File dir = LittleFS.open(BACKUP_DIR, "r");
    if (!dir || !dir.isDirectory()) {
        Log.println("[BackupManager] Backup directory not found");
        return backups;
    }
    
    File file = dir.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            PsramString filename = file.name();
            // Extract just filename without path
            int lastSlash = filename.lastIndexOf('/');
            if (lastSlash >= 0) {
                filename = filename.substring(lastSlash + 1);
            }
            
            if (filename.endsWith(".json")) {
                size_t fileSize = file.size();
                
                // Try to extract timestamp from file content
                time_t timestamp = file.getLastWrite();
                char timeStr[32];
                struct tm* timeinfo = localtime(&timestamp);
                snprintf(timeStr, sizeof(timeStr), "%04d-%02d-%02d %02d:%02d:%02d",
                        timeinfo->tm_year + 1900,
                        timeinfo->tm_mon + 1,
                        timeinfo->tm_mday,
                        timeinfo->tm_hour,
                        timeinfo->tm_min,
                        timeinfo->tm_sec);
                
                backups.push_back(BackupInfo(filename, PsramString(timeStr), fileSize));
            }
        }
        file.close();
        file = dir.openNextFile();
    }
    dir.close();
    
    // Sort backups by filename (which includes timestamp)
    std::sort(backups.begin(), backups.end(), 
              [](const BackupInfo& a, const BackupInfo& b) {
                  return a.filename > b.filename; // Descending order (newest first)
              });
    
    return backups;
}

void BackupManager::rotateBackups(int keepCount) {
    Log.printf("[BackupManager] Rotating backups (keeping %d most recent)...\n", keepCount);
    
    PsramVector<BackupInfo> backups = listBackups();
    
    // Delete old backups beyond keepCount
    for (size_t i = keepCount; i < backups.size(); i++) {
        PsramString fullPath = getBackupPath(backups[i].filename);
        Log.printf("[BackupManager] Deleting old backup: %s\n", backups[i].filename.c_str());
        LittleFS.remove(fullPath.c_str());
    }
}

bool BackupManager::shouldCreateAutomaticBackup() {
    // Check if 24 hours have passed since last backup
    time_t now = time(nullptr);
    const time_t ONE_DAY = 24 * 60 * 60;
    
    if (_lastBackupTime == 0 || (now - _lastBackupTime) >= ONE_DAY) {
        return true;
    }
    
    return false;
}

void BackupManager::periodicCheck() {
    if (shouldCreateAutomaticBackup()) {
        Log.println("[BackupManager] Time for automatic backup");
        createBackup(false); // automatic backup
    }
}

void BackupManager::loadLastBackupTime() {
    if (LittleFS.exists(LAST_BACKUP_TIME_FILE)) {
        File file = LittleFS.open(LAST_BACKUP_TIME_FILE, "r");
        if (file) {
            String timeStr = file.readString();
            _lastBackupTime = timeStr.toInt();
            file.close();
            Log.printf("[BackupManager] Last backup time loaded: %ld\n", _lastBackupTime);
        }
    }
}

void BackupManager::saveLastBackupTime() {
    File file = LittleFS.open(LAST_BACKUP_TIME_FILE, "w");
    if (file) {
        file.print(_lastBackupTime);
        file.close();
    }
}

PsramString BackupManager::readFileAsBase64(const PsramString& path) {
    // Not currently used but kept for potential binary file support
    return PsramString("");
}

bool BackupManager::writeBase64ToFile(const PsramString& path, const PsramString& base64Content) {
    // Not currently used but kept for potential binary file support
    return false;
}
