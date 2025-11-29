#ifndef WEB_HANDLERS_HPP
#define WEB_HANDLERS_HPP

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

// Birthday module debug handler
void handleBirthdayDebug();

// Backup handlers
void handleBackupPage();
void handleBackupCreate();
void handleBackupDownload();
void handleBackupUpload();
void handleBackupList();
void handleBackupRestore();

#endif // WEB_HANDLERS_HPP
