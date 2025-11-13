#ifndef BACKUP_MANAGER_HPP
#define BACKUP_MANAGER_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "PsramUtils.hpp"

/**
 * @brief BackupManager - Handles creating and restoring system backups
 * 
 * This manager creates unified backup files containing:
 * - Device configuration (config.json, hardware.json)
 * - Module persistent data (using DrawableModule::backup())
 * - PEM certificates
 * - Other JSON data files
 * 
 * Features:
 * - Creates timestamped backup files in /backups/ directory
 * - Keeps only the 10 most recent backups (automatic rotation)
 * - Supports backup download and restore upload
 * - Works in both normal and Access Point mode
 */

class Application;
class DrawableModule;

struct BackupInfo {
    PsramString filename;
    PsramString timestamp;
    size_t size;
    
    BackupInfo() : size(0) {}
    BackupInfo(const PsramString& fn, const PsramString& ts, size_t sz)
        : filename(fn), timestamp(ts), size(sz) {}
};

class BackupManager {
public:
    BackupManager(Application* app);
    ~BackupManager();
    
    /**
     * @brief Initialize the backup manager and ensure backup directory exists
     */
    void begin();
    
    /**
     * @brief Create a full system backup
     * @param manualBackup If true, marks as manual backup
     * @return true if backup was created successfully
     */
    bool createBackup(bool manualBackup = false);
    
    /**
     * @brief Restore from a backup file
     * @param filename Name of the backup file in /backups/ directory
     * @return true if restore was successful
     */
    bool restoreFromBackup(const PsramString& filename);
    
    /**
     * @brief List all available backups
     * @return Vector of backup information structures
     */
    PsramVector<BackupInfo> listBackups();
    
    /**
     * @brief Delete old backups, keeping only the N most recent
     * @param keepCount Number of backups to keep (default: 10)
     */
    void rotateBackups(int keepCount = 10);
    
    /**
     * @brief Get the full path to a backup file
     * @param filename Backup filename
     * @return Full path in LittleFS
     */
    PsramString getBackupPath(const PsramString& filename);
    
    /**
     * @brief Check if it's time for an automatic backup (once per day)
     * @return true if automatic backup should be created
     */
    bool shouldCreateAutomaticBackup();
    
    /**
     * @brief Perform periodic check for automatic backup
     */
    void periodicCheck();

private:
    Application* _app;
    time_t _lastBackupTime;
    
    /**
     * @brief Ensure /backups directory exists
     */
    void ensureBackupDirectory();
    
    /**
     * @brief Generate a timestamp-based filename
     * @param manualBackup If true, adds "manual" prefix
     * @return Filename like "backup_2023-11-13_22-16-00.json"
     */
    PsramString generateBackupFilename(bool manualBackup);
    
    /**
     * @brief Collect all module data using their backup() methods
     * @param doc JsonDocument to store module data
     */
    void collectModuleData(JsonDocument& doc);
    
    /**
     * @brief Collect all PEM certificates from LittleFS
     * @param doc JsonDocument to store certificate data
     */
    void collectCertificates(JsonDocument& doc);
    
    /**
     * @brief Collect device and hardware configuration
     * @param doc JsonDocument to store configuration
     */
    void collectConfiguration(JsonDocument& doc);
    
    /**
     * @brief Collect other JSON files from LittleFS
     * @param doc JsonDocument to store file data
     */
    void collectJsonFiles(JsonDocument& doc);
    
    /**
     * @brief Read file content as base64 (for binary files)
     * @param path File path
     * @return Base64 encoded content
     */
    PsramString readFileAsBase64(const PsramString& path);
    
    /**
     * @brief Write base64 content to file
     * @param path File path
     * @param base64Content Base64 encoded content
     * @return true if successful
     */
    bool writeBase64ToFile(const PsramString& path, const PsramString& base64Content);
    
    /**
     * @brief Load timestamp of last backup from persistent storage
     */
    void loadLastBackupTime();
    
    /**
     * @brief Save timestamp of last backup to persistent storage
     */
    void saveLastBackupTime();
};

#endif // BACKUP_MANAGER_HPP
