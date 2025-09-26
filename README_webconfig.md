````markdown name=README_webconfig.md
# Web-Konfigurations-UI für Panelclock

Diese Dateien fügen ein deutsches Web‑Interface zur Konfiguration der Geräte‑Secrets hinzu (WLAN, Tankerkoenig API, Tankstellen‑ID, OTA‑Passwort, Hostname). Zusätzlich gibt es eine API zum Scannen von verfügbaren WLANs und zum Verbindungsaufbau.

Wichtig:
- `config.json` liegt im LittleFS und enthält sensible Daten im Klartext. Füge `secrets.h` zu `.gitignore` hinzu, wenn du Build‑Defaults verwendest.
- Die Web‑UI verwendet UTF‑8/Deutsch; Umlaute sollten korrekt dargestellt werden.

Integration:
1. Kopiere `webconfig.h`, `webconfig.cpp` und `secrets.h.example` ins Projekt.
2. Initialisiere LittleFS wie bisher (`LittleFS.begin(...)`) bevor du `startConfigPortalIfNeeded()` aufrufst.
3. Rufe `startConfigPortalIfNeeded()` in `setup()` auf.
4. Wenn das Portal aktiv ist, rufe `handleConfigPortal()` regelmäßig in `loop()` auf.
5. Wenn `deviceConfig.ssid` gesetzt ist, verbinde per `WiFi.begin(deviceConfig.ssid, deviceConfig.password)` und rufe `ArduinoOTA.begin()` wie gewohnt.

AP & Scan:
- Wenn keine WLAN‑Einstellungen vorhanden sind, startet das Gerät einen AccessPoint `Panelclock-Setup` mit einer einfachen WebUI.
- Per Knopf oder API kannst du verfügbare WLANs scannen und diese Liste im Browser anzeigen lassen.
- Durch Klick auf ein Netzwerk wird dessen SSID ins Formular übernommen; anschließend kannst du Passwort eingeben und auf "Mit ausgewähltem Netzwerk verbinden" klicken.

Sicherheit:
- Die Konfigurationsseite ist unverschlüsselt (HTTP). Für produktiven Einsatz solltest du eine Authentifizierung oder Verschlüsselung hinzufügen.
````