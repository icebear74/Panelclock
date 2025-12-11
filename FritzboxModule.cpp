#include "FritzboxModule.hpp"
#include "MultiLogger.hpp"
#include "WebClientModule.hpp"
#include <ArduinoJson.h>
#include <WiFi.h>

FritzboxModule::FritzboxModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, WebClientModule* webClient)
     : u8g2(u8g2), canvas(canvas), webClient(webClient), enabled(false), callActive(false), lastConnectionAttempt(0) {
    dataMutex = xSemaphoreCreateMutex();
    fritzIp.clear();
}

FritzboxModule::~FritzboxModule() {
    if (dataMutex) vSemaphoreDelete(dataMutex);
}

void FritzboxModule::begin(BaseType_t core) {
    xTaskCreatePinnedToCore(
        FritzboxModule::taskRunner, "FritzboxTask", 4096, this, 1, NULL, core
    );
}

void FritzboxModule::setConfig(bool isEnabled, const PsramString& ip) {
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        this->enabled = isEnabled;
        this->fritzIp = ip;
        if (!isEnabled && client.connected()) {
            client.stop();
            Log.println("[Fritzbox] Modul deaktiviert, Verbindung getrennt.");
        }
        xSemaphoreGive(dataMutex);
    }
}

void FritzboxModule::closeConnection() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (client.connected()) {
            client.stop();
            Log.println("[Fritzbox] Callmonitor-Verbindung vor Neustart sauber geschlossen.");
        }
        xSemaphoreGive(dataMutex);
    }
}

void FritzboxModule::shutdown() {
    closeConnection();
}

void FritzboxModule::draw() {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    canvas.fillScreen(0);
    u8g2.begin(canvas);

    if (callActive) {
        u8g2.setFont(u8g2_font_7x14B_tf);
        u8g2.setForegroundColor(0xF800); // Rot
        int callIndicatorWidth = u8g2.getUTF8Width("ANRUF");
        u8g2.setCursor((canvas.width() - callIndicatorWidth) / 2, 12);
        u8g2.print("ANRUF");

        PsramString mainDisplayLine = callerName.empty() ? callerNumber : callerName;
        u8g2.setFont(u8g2_font_logisoso18_tn);
        u8g2.setForegroundColor(0xFFFF); // Weiß
        int nameWidth = u8g2.getUTF8Width(mainDisplayLine.c_str());
        u8g2.setCursor((canvas.width() - nameWidth) / 2, 35);
        u8g2.print(mainDisplayLine.c_str());

        if (!callerName.empty()) {
            PsramString subline = callerNumber;
            if (!callerLocation.empty()) {
                subline += " (";
                subline += callerLocation;
                subline += ")";
            }
            u8g2.setFont(u8g2_font_7x14_tf);
            u8g2.setForegroundColor(0x07E0); // Grün
            int sublineWidth = u8g2.getUTF8Width(subline.c_str());
            u8g2.setCursor((canvas.width() - sublineWidth) / 2, 55);
            u8g2.print(subline.c_str());
        }
    } else {
        u8g2.setFont(u8g2_font_6x13_tf);
        u8g2.setForegroundColor(0x07E0);
        u8g2.setCursor(10, 30);
        u8g2.print("Fritz!Box Modul aktiv");
    }

    xSemaphoreGive(dataMutex);
}

bool FritzboxModule::isEnabled() {
    bool isEnabled = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        isEnabled = this->enabled;
        xSemaphoreGive(dataMutex);
    }
    return isEnabled;
}

void FritzboxModule::taskRunner(void *pvParameters) {
    static_cast<FritzboxModule*>(pvParameters)->taskLoop();
}

void FritzboxModule::taskLoop() {
    for (;;) {
        bool isModuleEnabled = false;
        PsramString currentFritzIp;
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            isModuleEnabled = this->enabled;
            currentFritzIp = this->fritzIp;
            xSemaphoreGive(dataMutex);
        }

        if (!isModuleEnabled || WiFi.status() != WL_CONNECTED || currentFritzIp.empty()) {
            if (client.connected()) client.stop();
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        if (!client.connected()) {
            if (millis() - lastConnectionAttempt > 10000) { 
                //Log.printf("[Fritzbox] Verbinde mit Call Monitor auf %s...\n", currentFritzIp.c_str());
                lastConnectionAttempt = millis();
                if (client.connect(currentFritzIp.c_str(), FRITZ_PORT)) {
                   // Log.println("[Fritzbox] Verbunden!");
                } else {
                    Log.println("[Fritzbox] Verbindung fehlgeschlagen.");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            while (client.available()) {
                // Read line into a PSRAM buffer to reduce heap usage
                PsramString line;
                while (client.available()) {
                    char c = client.read();
                    if (c == '\n') break;
                    line += c;
                }
                if (!line.empty()) {
                    parseCallMonitorLine(line);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void FritzboxModule::parseCallMonitorLine(const PsramString& line) {
    int firstSemi = indexOf(line, ";");
    int secondSemi = indexOf(line, ";", firstSemi + 1);
    int thirdSemi = indexOf(line, ";", secondSemi + 1);
    int fourthSemi = indexOf(line, ";", thirdSemi + 1);
    if (firstSemi < 0 || secondSemi < 0 || thirdSemi < 0 || fourthSemi < 0) return;
    PsramString type = line.substr(firstSemi + 1, secondSemi - firstSemi - 1);

    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
        if (type == "RING") {
            callerNumber = line.substr(thirdSemi + 1, fourthSemi - thirdSemi - 1);
            if (callerNumber.length() == 0) callerNumber = "Unbekannt";
            callerName.clear();
            callerLocation.clear();
            queryCallerInfo(callerNumber);
            callActive = true;
            
            // NEU: Nutze das neue Priority-System mit fester UID und Duration
            bool success = requestPriorityEx(Priority::High, FRITZBOX_CALL_UID, FRITZBOX_MAX_DURATION_MS);
            if (success) {
                Log.println("[Fritzbox] Anruf-Interrupt erfolgreich angefordert (max 15 Min)");
            } else {
                Log.println("[Fritzbox] WARNUNG: Anruf-Interrupt wurde abgelehnt!");
            }
            
        } else if (type == "DISCONNECT") {
            if (callActive) {
                callActive = false;
                
                // NEU: Release mit UID
                releasePriorityEx(FRITZBOX_CALL_UID);
                Log.println("[Fritzbox] Anruf beendet, Interrupt freigegeben");
            }
        }
        xSemaphoreGive(dataMutex);
    }
}

void FritzboxModule::queryCallerInfo(const PsramString& number) {
    if (!webClient || fritzIp.empty() || number == "Unbekannt") return;

    PsramString url = "http://";
    url += fritzIp;
    url += "/cgi-bin/carddav_lookup";

    PsramString postBody = "tel=";
    postBody += number;
    
    webClient->postRequest(url, postBody, "application/x-www-form-urlencoded", 
        [this](const char* buffer, size_t size) {
            if (buffer && size > 0) {
                this->parseCallerInfo(buffer, size);
            } else {
                Log.println("[Fritzbox] Keine oder leere Antwort bei der Namensabfrage.");
            }
        }
    );
}

void FritzboxModule::parseCallerInfo(const char* buffer, size_t size) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buffer, size);

    if (error) {
        Log.printf("[Fritzbox] Fehler beim Parsen der JSON-Antwort: %s\n", error.c_str());
        return;
    }
    
    if (doc["result"].is<JsonObject>()) {
        JsonObject result = doc["result"];
        if (result["name"].is<const char*>()) {
            if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                callerName = result["name"].as<const char*>();
                Log.printf("[Fritzbox] Name gefunden: %s\n", callerName.c_str());
                if (result["location"].is<const char*>()) {
                    callerLocation = result["location"].as<const char*>();
                    Log.printf("[Fritzbox] Ort gefunden: %s\n", callerLocation.c_str());
                }
                
                // Kein erneuter Request nötig - der Request wurde bereits bei RING gestellt
                // Wir müssen hier nur neu zeichnen lassen (passiert automatisch im Render-Loop)
                
                xSemaphoreGive(dataMutex);
            }
        }
    }
}