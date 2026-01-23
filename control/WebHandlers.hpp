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
void handleToggleDebugFile();
void handleTankerkoenigSearchLive();
void handleThemeParksList();
void handleSofascoreTournamentsList();
void handleSofascoreDebugSnapshot();
void handleStreamPage();

// Countdown handlers
void handleCountdownPage();
void handleCountdownStart();
void handleCountdownStop();
void handleCountdownPause();
void handleCountdownReset();
void handleCountdownStatus();

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

// Helper function to ensure buffers are flushed before ESP32 restart
void flushBuffersBeforeRestart();

#endif // WEB_HANDLERS_HPP
