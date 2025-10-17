#ifndef FRITZBOXMODULE_HPP
#define FRITZBOXMODULE_HPP

#include <Arduino.h>
#include <WiFiClient.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "PsramUtils.hpp"
#include "WebClientModule.hpp" 
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <ArduinoJson.h>

// Der Port des Call Monitors
#define FRITZ_PORT 1012

class FritzboxModule {
public:
    FritzboxModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, WebClientModule* webClient)
     : u8g2(u8g2), canvas(canvas), webClient(webClient), enabled(false), callActive(false), needsRedraw(false), lastConnectionAttempt(0) {
        dataMutex = xSemaphoreCreateMutex();
        fritzIp.clear();
    }

    ~FritzboxModule() {
        if (dataMutex) vSemaphoreDelete(dataMutex);
    }

    void begin(BaseType_t core) {
        xTaskCreatePinnedToCore(
            FritzboxModule::taskRunner, "FritzboxTask", 4096, this, 1, NULL, core
        );
    }

    void setConfig(bool isEnabled, const PsramString& ip) {
        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            this->enabled = isEnabled;
            this->fritzIp = ip;
            if (!isEnabled && client.connected()) {
                client.stop();
                Serial.println("[Fritzbox] Modul deaktiviert, Verbindung getrennt.");
            }
            xSemaphoreGive(dataMutex);
        }
    }
    
    bool isEnabled() { return this->enabled; }
    bool isCallActive() { return callActive; }
    bool redrawNeeded() {
        if (needsRedraw) {
            needsRedraw = false;
            return true;
        }
        return false;
    }

    void draw() {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

        canvas.fillScreen(0);
        u8g2.begin(canvas);

        if (callActive) {
            u8g2.setFont(u8g2_font_7x14B_tf);
            u8g2.setForegroundColor(0xF800);
            int callIndicatorWidth = u8g2.getUTF8Width("ANRUF");
            u8g2.setCursor((canvas.width() - callIndicatorWidth) / 2, 12);
            u8g2.print("ANRUF");

            PsramString mainDisplayLine = callerName.empty() ? callerNumber : callerName;
            u8g2.setFont(u8g2_font_logisoso18_tn);
            u8g2.setForegroundColor(0xFFFF);
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
                u8g2.setForegroundColor(0x07E0);
                int sublineWidth = u8g2.getUTF8Width(subline.c_str());
                u8g2.setCursor((canvas.width() - sublineWidth) / 2, 55);
                u8g2.print(subline.c_str());
            }
        } else {
            u8g2.setFont(u8g2_font_6x13_tf);
            u8g2.setForegroundColor(0x07E0);
            u8g2.setCursor(10, 30);
            u8g2.print("Warte auf Anrufe...");
        }

        xSemaphoreGive(dataMutex);
    }

private:
    U8G2_FOR_ADAFRUIT_GFX &u8g2;
    GFXcanvas16 &canvas;
    WebClientModule* webClient;
    SemaphoreHandle_t dataMutex;

    volatile bool enabled;
    volatile bool callActive;
    volatile bool needsRedraw;
    unsigned long lastConnectionAttempt;

    PsramString fritzIp;
    PsramString callerNumber;
    PsramString callerName;
    PsramString callerLocation;
    
    WiFiClient client;

    static void taskRunner(void *pvParameters) {
        static_cast<FritzboxModule*>(pvParameters)->taskLoop();
    }

    void taskLoop() {
        unsigned long lastKeepAlive = 0;

        for (;;) {
            bool isModuleEnabled = false;
            PsramString currentFritzIp;
            if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                isModuleEnabled = this->enabled;
                currentFritzIp = this->fritzIp;
                xSemaphoreGive(dataMutex);
            }

            if (!isModuleEnabled || WiFi.status() != WL_CONNECTED || currentFritzIp.empty()) {
                if (client.connected()) {
                    client.stop();
                    Serial.println("[Fritzbox] Modul deaktiviert, Verbindung getrennt.");
                }
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }

            if (!client.connected()) {
                if (millis() - lastConnectionAttempt > 10000) { 
                    Serial.printf("[Fritzbox] Verbinde mit Call Monitor auf %s...\n", currentFritzIp.c_str());
                    lastConnectionAttempt = millis();
                    if (client.connect(currentFritzIp.c_str(), FRITZ_PORT)) {
                        Serial.println("[Fritzbox] Verbunden!");
                        lastKeepAlive = millis(); // Keep-Alive Timer zurücksetzen
                    } else {
                        Serial.println("[Fritzbox] Verbindung fehlgeschlagen.");
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(1000));
            } else {
                // ====================================================================================
                // FINALE, KORREKTE KEEP-ALIVE LOGIK
                // ====================================================================================
                while (client.connected()) {
                    if (client.available()) {
                        String line = client.readStringUntil('\n');
                        parseCallMonitorLine(line);
                        lastKeepAlive = millis(); // Daten empfangen gilt als Keep-Alive
                    } else {
                        // Wenn keine Daten da sind, prüfe, ob wir ein Keep-Alive senden müssen
                        if (millis() - lastKeepAlive > 5000) {
                            client.print(" "); // Sende ein harmloses Zeichen, um die Verbindung offen zu halten
                            lastKeepAlive = millis();
                        }
                    }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                Serial.println("[Fritzbox] Verbindung verloren. Starte Neuverbindungsprozess.");
                client.stop();
            }
        }
    }

    void parseCallMonitorLine(const String& line) {
        int firstSemi = line.indexOf(';');
        int secondSemi = line.indexOf(';', firstSemi + 1);
        int thirdSemi = line.indexOf(';', secondSemi + 1);
        int fourthSemi = line.indexOf(';', thirdSemi + 1);
        if (firstSemi < 0 || secondSemi < 0 || thirdSemi < 0 || fourthSemi < 0) return;
        String type = line.substring(firstSemi + 1, secondSemi);

        if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
            if (type == "RING") {
                callerNumber = line.substring(thirdSemi + 1, fourthSemi).c_str();
                if (callerNumber.length() == 0) callerNumber = "Unbekannt";
                callerName.clear();
                callerLocation.clear();
                queryCallerInfo(callerNumber);
                callActive = true;
                needsRedraw = true;
            } else if (type == "DISCONNECT") {
                if (callActive) {
                    callActive = false;
                    needsRedraw = true;
                }
            }
            xSemaphoreGive(dataMutex);
        }
    }

    void queryCallerInfo(const PsramString& number) {
        if (!webClient || fritzIp.empty() || number == "Unbekannt") return;

        PsramString url = "http://";
        url += fritzIp;
        url += "/cgi-bin/carddav_lookup";

        PsramString postBody = "tel=";
        postBody += number;
        
        Serial.printf("[Fritzbox] Frage %s mit Body '%s' an...\n", url.c_str(), postBody.c_str());

        webClient->postRequest(url, postBody, "application/x-www-form-urlencoded", 
            [this](const char* buffer, size_t size) {
                if (buffer && size > 0) {
                    this->parseCallerInfo(buffer, size);
                } else {
                    Serial.println("[Fritzbox] Keine oder leere Antwort bei der Namensabfrage.");
                }
            }
        );
    }

    void parseCallerInfo(const char* buffer, size_t size) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, buffer, size);

        if (error) {
            Serial.printf("[Fritzbox] Fehler beim Parsen der JSON-Antwort: %s\n", error.c_str());
            return;
        }
        
        if (doc["result"].is<JsonObject>()) {
            JsonObject result = doc["result"];
            if (result["name"].is<const char*>()) {
                if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
                    callerName = result["name"].as<const char*>();
                    Serial.printf("[Fritzbox] Name gefunden: %s\n", callerName.c_str());
                    if (result["location"].is<const char*>()) {
                        callerLocation = result["location"].as<const char*>();
                        Serial.printf("[Fritzbox] Ort gefunden: %s\n", callerLocation.c_str());
                    }
                    needsRedraw = true;
                    xSemaphoreGive(dataMutex);
                }
            }
        }
    }
};

#endif // FRITZBOXMODULE_HPP