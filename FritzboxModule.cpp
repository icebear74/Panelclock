#include "FritzboxModule.hpp"
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
            Serial.println("[Fritzbox] Modul deaktiviert, Verbindung getrennt.");
        }
        xSemaphoreGive(dataMutex);
    }
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

// NEU: Implementierung der fehlenden Methoden
unsigned long FritzboxModule::getDisplayDuration() {
    // Dieses Modul ist nicht Teil der normalen Rotation, daher ist die Dauer irrelevant.
    return 0;
}

bool FritzboxModule::isEnabled() {
    bool isEnabled = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        isEnabled = this->enabled;
        xSemaphoreGive(dataMutex);
    }
    return isEnabled;
}

void FritzboxModule::resetPaging() {
    // Dieses Modul hat keine Seiten, daher ist hier nichts zu tun.
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
                Serial.printf("[Fritzbox] Verbinde mit Call Monitor auf %s...\n", currentFritzIp.c_str());
                lastConnectionAttempt = millis();
                if (client.connect(currentFritzIp.c_str(), FRITZ_PORT)) {
                    Serial.println("[Fritzbox] Verbunden!");
                } else {
                    Serial.println("[Fritzbox] Verbindung fehlgeschlagen.");
                }
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            while (client.available()) {
                String line = client.readStringUntil('\n');
                parseCallMonitorLine(line);
            }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void FritzboxModule::parseCallMonitorLine(const String& line) {
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
            requestPriority();
        } else if (type == "DISCONNECT") {
            if (callActive) {
                callActive = false;
                releasePriority();
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
                Serial.println("[Fritzbox] Keine oder leere Antwort bei der Namensabfrage.");
            }
        }
    );
}

void FritzboxModule::parseCallerInfo(const char* buffer, size_t size) {
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
                requestPriority(); 
                xSemaphoreGive(dataMutex);
            }
        }
    }
}