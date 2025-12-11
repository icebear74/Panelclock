#ifndef WEB_HANDLERS_HPP
#define WEB_HANDLERS_HPP

// Timeout for waiting on buffer flushing before ESP32 restart (milliseconds)
#define BUFFER_FLUSH_TIMEOUT_MS 3000

// Prototypes for all web handlers used by setupWebServer()
// Implementations are in WebHandlers.cpp

void handleRoot();
void handleConfigBase();
void handleConfigModules();
void handleSaveBase();
void handleSaveModules();
void handleConfigLocation();
void handleSaveLocation();
void handleConfigHardware();
void handleSaveHardware();
void handleNotFound();
void handleDebugData();
void handleDebugStationHistory();
void handleTankerkoenigSearchLive();
void handleThemeParksList();
void handleStreamPage();

// Backup handlers
void handleBackupPage();
void handleBackupCreate();
void handleBackupDownload();
void handleBackupUpload();
void handleBackupList();
void handleBackupRestore();

// Firmware update handlers
void handleFirmwarePage();
void handleFirmwareUpload();

#endif // WEB_HANDLERS_HPP
