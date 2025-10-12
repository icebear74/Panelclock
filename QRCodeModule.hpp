#ifndef QRCODEMODULE_HPP
#define QRCODEMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <QRCodeGFX.h>

class QRCodeModule {
private:
    U8G2_FOR_ADAFRUIT_GFX& u8g2;
    GFXcanvas16& canvas;
    QRCodeGFX qrcodegfx;
    String guestSsid;
    String guestPassword;

public:
    // Konstruktor
    QRCodeModule(U8G2_FOR_ADAFRUIT_GFX& u8g2_ref, GFXcanvas16& canvas_ref)
        : u8g2(u8g2_ref), canvas(canvas_ref), qrcodegfx(canvas_ref) {
    }

    // Methode zum Setzen der Gast-WLAN-Daten
    void setWifiCredentials(const String& ssid, const String& password) {
        guestSsid = ssid;
        guestPassword = password;
    }

    // Methode zum Zeichnen des QR-Codes
    void draw() {
        // Den Datenbereich (Canvas) komplett schwarz füllen
        canvas.fillScreen(0);
        u8g2.begin(canvas); // u8g2 auf den richtigen Canvas einstellen

        // Prüfen, ob eine SSID konfiguriert ist
        if (guestSsid.isEmpty()) {
            u8g2.setFont(u8g2_font_profont12_tf);
            u8g2.setForegroundColor(0xF800); // Rot
            const char* msg = "Gast-WLAN nicht konfiguriert";
            int x = (canvas.width() - u8g2.getUTF8Width(msg)) / 2;
            int y = canvas.height() / 2;
            u8g2.setCursor(x, y);
            u8g2.print(msg);
            return;
        }

        // Titel "Gast-WLAN" über dem QR-Code anzeigen
        u8g2.setFont(u8g2_font_profont12_tf);
        u8g2.setForegroundColor(0xFFFF); // Weiß
        const char* title = "Gast-WLAN";
        int title_x = (canvas.width() - u8g2.getUTF8Width(title)) / 2;
        u8g2.setCursor(title_x, 10);
        u8g2.print(title);

        // Standard-WLAN-String für den QR-Code erstellen
        String wifiString = "WIFI:S:" + guestSsid + ";T:WPA;P:" + guestPassword + ";;";

        // --- FINALE KORREKTUR BASIEREND AUF DER HEADER-DATEI ---

        // 1. Skalierung (Größe der "Punkte") festlegen. Entspricht 'moduleSize'.
        int scale = 2;
        qrcodegfx.setScale(scale);

        // 2. Farben festlegen (optional, standard ist schwarz auf weiß)
        qrcodegfx.setColors(0x0000, 0xFFFF); // Schwarzer Hintergrund, weiße Punkte

        // 3. Version über den internen Generator setzen. Wichtig für die Größenberechnung.
        int qrVersion = 3;
        qrcodegfx.getGenerator().setVersion(qrVersion);

        // 4. QR-Code Daten generieren. Dieser Schritt ist nötig, bevor man die Größe abfragen kann.
        qrcodegfx.generateData(wifiString);

        // 5. Größe in Pixeln abfragen, um den Code zu zentrieren.
        int qrCodeWidth = qrcodegfx.getSideLength();
        int x_offset = (canvas.width() - qrCodeWidth) / 2;
        int y_offset = (canvas.height() - qrCodeWidth) / 2 + 5; // Zentriert mit kleinem Offset

        // 6. Den vorberechneten QR-Code an der Zielposition zeichnen.
        qrcodegfx.draw(x_offset, y_offset);
        
    }
};

#endif // QRCODEMODULE_HPP