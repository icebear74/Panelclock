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

#endif // WEB_HANDLERS_HPP