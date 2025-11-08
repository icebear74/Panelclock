# Panelclock - Multifunktionale ESP32 LED-Matrix-Uhr

Panelclock ist ein flexibles und modular aufgebautes Projekt für eine ESP32-basierte Uhr, die auf einem HUB75 RGB-LED-Matrix-Panel läuft. Es ist weit mehr als nur eine Uhr; es ist ein vielseitiges Informationsdisplay, das durch eine Vielzahl von Modulen erweitert werden kann.

Das System ist für den 24/7-Betrieb ausgelegt, mit einem ausgeklügelten Webinterface zur Konfiguration, Over-the-Air (OTA) Update-Fähigkeit und einem robusten Speichermanagement, das auf PSRAM setzt.

## Kernarchitektur

Das Herzstück des Projekts ist eine modulare Architektur, die von der zentralen `Application`-Klasse gesteuert wird. Jede Informationseinheit, die auf dem Panel angezeigt werden kann (wie die Uhrzeit, Kalender, Sportergebnisse), ist als eigenständiges `DrawableModule` gekapselt.

- **Application (`Application.cpp/.hpp`):** Der Dirigent des gesamten Projekts. Initialisiert alle Manager und Module und steuert die Hauptschleife.
- **PanelManager (`PanelManager.cpp/.hpp`):** Der Display-Controller. Er verwaltet die Liste der aktiven Module, wechselt zwischen ihnen in einer vordefinierten Rotation und ist für das eigentliche Rendern des Bildes auf dem LED-Panel zuständig.
- **DrawableModule (`DrawableModule.hpp`):** Eine virtuelle Basisklasse (Interface), die von allen Anzeigemodulen implementiert wird. Sie definiert die grundlegenden Funktionen, die ein Modul haben muss, wie `draw()`, `logicTick()` und `getDisplayDuration()`.
- **WebClientModule (`WebClientModule.cpp/.hpp`):** Ein zentraler, asynchroner Manager für alle HTTP-Anfragen. Module registrieren hier die benötigten Web-Ressourcen, und der WebClient kümmert sich effizient um das Abrufen und Cachen der Daten im Hintergrund.
- **WebServer & Konfiguration (`webconfig.cpp/.hpp`, `WebServerManager.cpp/.hpp`):** Das Projekt hostet ein umfangreiches Web-Interface, das eine vollständige Konfiguration aller Module ohne Neu-Kompilierung ermöglicht. Einstellungen werden auf dem Gerät im LittleFS gespeichert.

## Hauptfunktionen

- **Modulare Anzeige:** Einfache Erweiterbarkeit durch neue Module. Die angezeigten Informationen rotieren automatisch.
- **Web-Konfiguration:** Ein integrierter Webserver ermöglicht die bequeme Konfiguration von WLAN, Zeitzone, Modul-Einstellungen und mehr über einen Browser.
- **OTA-Updates:** Die Firmware kann "Over-the-Air" über das Web-Interface aktualisiert werden.
- **Effizientes Ressourcen-Management:** Intensive Nutzung von PSRAM für Web-Daten, JSON-Parsing und Spielerlisten, um den knappen internen Speicher des ESP32 zu schonen.
- **Asynchrone Datenabrufe:** Alle externen Daten werden im Hintergrund geladen, ohne die Anzeige oder andere Operationen zu blockieren.
- **Präsenz-Erkennung:** Durch das `MwaveSensorModule` kann das Display bei Abwesenheit automatisch abgeschaltet und bei Anwesenheit reaktiviert werden, um Strom zu sparen.
- **Robuste Zeitumrechnung:** Ein maßgeschneiderter `GeneralTimeConverter` sorgt für eine zuverlässige Umrechnung von UTC in die lokale Zeit, inklusive korrekter Sommer-/Winterzeit-Regeln (DST).

## Module

Das Panelclock-System besteht aus einer Reihe von spezialisierten Modulen:

### Standard-Module
- **ClockModule:** Eine einfache, aber elegante Digitaluhr. Das Kernmodul.
- **CalendarModule:** Zeigt Termine aus einer oder mehreren iCal-Quellen (.ics) an. Unterstützt dank des `RRuleParser` auch komplexe Wiederholungsregeln.

### Informations-Module
- **DartsRankingModule:** Zeigt die "Order of Merit" und "Pro Tour" Ranglisten der Darts-Welt an. Tracked Players können farblich hervorgehoben werden, und bei Live-Turnieren wird die aktuelle Runde des Spielers angezeigt.
- **CuriousHolidaysModule:** Holt und zeigt kuriose Feiertage für den aktuellen und den nächsten Tag von `kuriose-feiertage.de`.
- **TankerkoenigModule:** Ruft über die Tankerkönig-API die aktuellen Spritpreise für konfigurierte Tankstellen ab und zeigt diese an.
- **FritzboxModule:** Stellt eine Verbindung zu einer AVM FRITZ!Box her, um eine Anrufliste der letzten verpassten, angenommenen oder getätigten Anrufe anzuzeigen.

### System-Module
- **MwaveSensorModule:** Steuert einen RCWL-0516 Mikrowellen-Bewegungssensor, um die Anwesenheit von Personen im Raum zu erkennen und das Display zu schalten.
- **ConnectionManager:** Verwaltet die WLAN-Verbindung und sorgt für einen automatischen Reconnect.
- **OtaManager:** Stellt die Funktionalität für Over-the-Air-Updates bereit.

## Konfiguration

Die gesamte Konfiguration erfolgt über das Web-Interface, das nach dem ersten Start im eigenen WLAN des Geräts (Access Point Modus) oder später über die IP-Adresse des Geräts im Heimnetzwerk erreichbar ist.

Die Konfigurationsdateien werden im LittleFS-Dateisystem auf dem ESP32 gespeichert.

Detaillierte Informationen zur Konfiguration findest du in der [README_webconfig.md](README_webconfig.md).

## Hardware & Abhängigkeiten

- **Controller:** ESP32 mit PSRAM (z.B. WROVER-Modul).
- **Display:** HUB75-kompatibles RGB-LED-Matrix-Panel.
- **Bibliotheken:**
  - `ESP32-HUB75-LED-MATRIX-PANEL-DMA-Display` für die Ansteuerung des Panels.
  - `Adafruit_GFX_Library` und `U8g2_for_Adafruit_GFX` für die Text- und Grafikdarstellung.
  - `ArduinoJson` für die Verarbeitung von API-Antworten.
  - `NTPClient` für die Zeitsynchronisation.
  - Sowie die Standard-ESP32-Bibliotheken (`WiFi`, `HTTPClient`, etc.).

## Build & Flash

Das Projekt ist als Arduino-Sketch strukturiert.
- **Partitionsschema:** Es wird eine spezielle `partitions.csv` verwendet, um genügend Platz für die große Applikation, OTA-Updates und das LittleFS-Dateisystem für die Web-Dateien bereitzustellen.
- **PSRAM:** PSRAM muss in den Board-Einstellungen der Arduino IDE aktiviert sein.

---
*Dieses README wurde automatisch basierend auf einer Analyse des Quellcodes generiert.*
