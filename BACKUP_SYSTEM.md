# Panelclock Backup & Wiederherstellungssystem

## Übersicht

Das Panelclock-Backup-System ermöglicht die Erstellung vollständiger System-Backups, die alle Konfigurationen, persistierten Daten und Zertifikate in einer einzigen JSON-Datei speichern. Das System unterstützt:

- ✅ Automatische tägliche Backups
- ✅ Manuelle Backup-Erstellung über Webinterface
- ✅ Download von Backups
- ✅ Upload und Wiederherstellung von Backups
- ✅ Automatische Rotation (behält die letzten 10 Backups)
- ✅ Funktioniert auch im Access Point Modus (für Notfall-Recovery)

## Architektur

### BackupManager-Klasse

Die zentrale Komponente ist die `BackupManager`-Klasse (`BackupManager.hpp/cpp`), die folgende Hauptfunktionen bereitstellt:

- `createBackup(bool manualBackup)` - Erstellt ein neues Backup
- `restoreFromBackup(const PsramString& filename)` - Stellt ein Backup wieder her
- `listBackups()` - Listet alle verfügbaren Backups auf
- `rotateBackups(int keepCount)` - Löscht alte Backups, behält die neuesten
- `periodicCheck()` - Prüft, ob ein automatisches Backup erstellt werden sollte

### Backup-Dateiformat

Backups werden als JSON-Dateien im Verzeichnis `/backups/` gespeichert mit folgendem Format:

```
backup_YYYY-MM-DD_HH-MM-SS.json
```

oder für manuelle Backups:

```
manual_backup_YYYY-MM-DD_HH-MM-SS.json
```

### Backup-Inhalt

Jedes Backup enthält folgende Daten:

#### 1. Metadaten
```json
{
  "version": "1.0",
  "timestamp": 1699913160,
  "manual": false
}
```

#### 2. Konfiguration (`configuration`)
- **Device Config**: Alle Einstellungen aus `config.json`
  - Hostname, SSID, Passwörter
  - API-Keys (Tankerkönig, Weather, etc.)
  - Modul-Einstellungen (Kalender, Darts, etc.)
  - Sensor-Konfiguration
  - Standort (Latitude/Longitude)
  
- **Hardware Config**: Pin-Belegung aus `hardware.json`
  - RGB-Pins (R1, G1, B1, R2, G2, B2)
  - Address-Pins (A, B, C, D, E)
  - Control-Pins (CLK, LAT, OE)

#### 3. Zertifikate (`certificates`)
Alle PEM/CRT/CER-Dateien aus dem Root-Verzeichnis:
```json
{
  "tankerkoenig.pem": "-----BEGIN CERTIFICATE-----\n...",
  "google.pem": "-----BEGIN CERTIFICATE-----\n..."
}
```

#### 4. JSON-Dateien (`json_files`)
Andere persistierte JSON-Daten:
- `station_cache.json` - Cache der Tankstellen
- `station_price_stats.json` - Preisstatistiken
- `price_cache.json` - Preis-Cache
- `calendar_cache.json` - Kalender-Daten
- `weather_cache.json` - Wetter-Daten

## Webinterface

Das Backup-System ist über das Webinterface unter `/backup` erreichbar.

### Hauptmenü-Integration

Der Backup-Link ist im Hauptmenü als orangefarbener Button verfügbar:
```
Backup & Wiederherstellung
```

### Backup-Seite Funktionen

#### 1. Neues Backup erstellen
- Button: "Backup jetzt erstellen"
- Erstellt ein manuelles Backup mit Präfix `manual_`
- Zeigt Erfolgsmeldung mit Dateinamen an

#### 2. Backup hochladen & wiederherstellen
- Dateiauswahl für JSON-Backup-Dateien
- Button: "Backup hochladen & wiederherstellen"
- Warnung vor automatischem Neustart
- Upload → Restore → Automatischer Neustart

#### 3. Verfügbare Backups
- Tabelle mit allen vorhandenen Backups
- Spalten: Dateiname, Zeitstempel, Größe
- Aktionen pro Backup:
  - **Download**: Backup herunterladen
  - **Wiederherstellen**: Backup einspielen (mit Neustart)

## API-Endpunkte

### Web-Routen

```cpp
GET  /backup                  → Backup-Seite anzeigen
POST /api/backup/create       → Neues Backup erstellen
GET  /api/backup/download     → Backup herunterladen (?file=...)
POST /api/backup/upload       → Backup hochladen (multipart/form-data)
POST /api/backup/restore      → Backup wiederherstellen (JSON body)
GET  /api/backup/list         → Liste aller Backups
```

### Beispiel-Requests

#### Backup erstellen
```http
POST /api/backup/create
```
Response:
```json
{
  "success": true,
  "filename": "manual_backup_2023-11-13_22-16-00.json"
}
```

#### Backup-Liste abrufen
```http
GET /api/backup/list
```
Response:
```json
{
  "backups": [
    {
      "filename": "manual_backup_2023-11-13_22-16-00.json",
      "timestamp": "2023-11-13 22:16:00",
      "size": 45678
    },
    {
      "filename": "backup_2023-11-13_00-00-00.json",
      "timestamp": "2023-11-13 00:00:00",
      "size": 44321
    }
  ]
}
```

#### Backup herunterladen
```http
GET /api/backup/download?file=backup_2023-11-13_22-16-00.json
```
Response: JSON-Datei als Download

#### Backup wiederherstellen
```http
POST /api/backup/restore
Content-Type: application/json

{
  "filename": "backup_2023-11-13_22-16-00.json"
}
```
Response:
```json
{
  "success": true,
  "message": "Restore successful, rebooting..."
}
```
Gerät startet automatisch neu.

## Automatische Backups

### Zeitplan

Der `BackupManager` prüft stündlich (alle 3600000ms), ob ein automatisches Backup erstellt werden sollte. Ein Backup wird erstellt, wenn:

1. Noch kein Backup existiert (`_lastBackupTime == 0`), oder
2. Das letzte Backup vor mehr als 24 Stunden erstellt wurde

### Implementierung

Im `Application::update()` Loop:

```cpp
if (backupManager) {
    static unsigned long lastBackupCheck = 0;
    unsigned long now = millis();
    // Check every hour if automatic backup is needed
    if (now - lastBackupCheck >= 3600000) { // 1 hour
        lastBackupCheck = now;
        backupManager->periodicCheck();
    }
}
```

Die Methode `periodicCheck()` ruft intern `shouldCreateAutomaticBackup()` auf und erstellt bei Bedarf ein Backup mit `createBackup(false)` (nicht-manuelles Backup).

### Persistierung

Der Zeitstempel des letzten Backups wird in `/last_backup_time.txt` gespeichert, um auch nach einem Neustart die 24-Stunden-Regel einzuhalten.

## Backup-Rotation

### Automatische Rotation

Nach jedem erfolgreichen Backup wird automatisch `rotateBackups()` aufgerufen, die:

1. Alle Backups im `/backups/`-Verzeichnis listet
2. Nach Dateiname (enthält Zeitstempel) sortiert (neueste zuerst)
3. Alle Backups ab Position 10 löscht

### Beispiel

Vor Rotation:
```
backup_2023-11-13_22-00-00.json (neueste)
backup_2023-11-13_21-00-00.json
...
backup_2023-11-13_12-00-00.json
backup_2023-11-13_11-00-00.json (älteste, wird gelöscht)
```

Nach Rotation (10 Backups behalten):
```
backup_2023-11-13_22-00-00.json
backup_2023-11-13_21-00-00.json
...
backup_2023-11-13_13-00-00.json (älteste nach Rotation)
```

## Access Point Modus

### Notfall-Recovery

Ein besonderes Feature des Backup-Systems ist die Verfügbarkeit im Access Point Modus. Dies ermöglicht:

1. **Neuinstallation**: Bei einer Neuinstallation (keine WLAN-Konfiguration) startet das Gerät im AP-Modus
2. **Backup einspielen**: Sofortiges Einspielen eines Backups über die Web-UI
3. **Vollständige Wiederherstellung**: Alle Einstellungen, Daten und Zertifikate werden wiederhergestellt
4. **Automatischer Neustart**: Nach dem Restore startet das Gerät neu und verbindet sich mit dem WLAN aus dem Backup

### Verwendung

```
1. Gerät hat keine WLAN-Konfiguration
2. Gerät startet Access Point "Panelclock-Setup"
3. Mit AP verbinden (z.B. über Smartphone)
4. Browser öffnen → http://192.168.4.1
5. Menü → "Backup & Wiederherstellung"
6. Backup-Datei hochladen
7. "Backup hochladen & wiederherstellen" klicken
8. Gerät startet neu und ist vollständig wiederhergestellt
```

## Erweiterung für neue Module

Um Backup-Support für ein neues Modul hinzuzufügen, speichere die Modul-Daten einfach in einer JSON-Datei und füge den Dateipfad zur Liste in `BackupManager::collectJsonFiles()` hinzu.

### Beispiel: Neues Modul mit Backup

1. **Modul speichert Daten in JSON-Datei:**

In `YourModule.cpp`:

```cpp
void YourModule::saveData() {
    DynamicJsonDocument doc(4096);
    
    // Füge deine Daten hinzu
    doc["someData"] = _someData;
    doc["moreData"] = _moreData.c_str();
    
    JsonArray array = doc.createNestedArray("items");
    for (const auto& item : _items) {
        array.add(item);
    }
    
    // Speichere in LittleFS
    File file = LittleFS.open("/your_module_data.json", "w");
    if (file) {
        serializeJson(doc, file);
        file.close();
    }
}

void YourModule::loadData() {
    if (!LittleFS.exists("/your_module_data.json")) return;
    
    File file = LittleFS.open("/your_module_data.json", "r");
    if (file) {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        
        if (!error) {
            _someData = doc["someData"] | 0;
            _moreData = doc["moreData"] | "";
            // ... restore other data
        }
    }
}
```

2. **Füge Datei zur Backup-Liste hinzu:**

In `BackupManager.cpp`, erweitere die `jsonFileList` in `collectJsonFiles()`:

```cpp
const char* jsonFileList[] = {
    "/station_cache.json",
    "/station_price_stats.json",
    "/price_cache.json",
    "/calendar_cache.json",
    "/weather_cache.json",
    "/your_module_data.json",  // <-- Deine neue Datei
    nullptr
};
```

Das war's! Dein Modul wird jetzt automatisch ins Backup eingeschlossen und bei der Wiederherstellung korrekt restauriert.

## Sicherheit & Best Practices

### Passwörter

⚠️ **Wichtig**: Backups enthalten **alle Passwörter** im Klartext:
- WLAN-Passwort
- OTA-Update-Passwort
- Fritz!Box-Passwort
- API-Keys

→ Backups sollten **sicher** aufbewahrt werden!

### Speicherplatz

- Backups werden im LittleFS gespeichert
- Typische Backup-Größe: 40-60 KB
- 10 Backups ≈ 400-600 KB
- Achte auf ausreichend freien Speicher im LittleFS

### Wiederherstellung

- **Immer** Backup vor größeren Änderungen erstellen
- Bei Problemen: Backup im AP-Modus einspielen
- Nach Restore: Automatischer Neustart
- Bei Fehlschlag: Logs prüfen (siehe MultiLogger)

## Fehlerbehebung

### Backup wird nicht erstellt

1. Prüfe LittleFS-Status:
   - Über `/fs` (Dateimanager)
   - Ist `/backups/`-Verzeichnis vorhanden?
   - Ausreichend Speicher frei?

2. Prüfe Logs:
   - Web-UI → `/debug`
   - Suche nach `[BackupManager]`-Einträgen

### Backup-Restore schlägt fehl

1. Validiere Backup-Datei:
   - Ist es eine gültige JSON-Datei?
   - Wurde sie korrekt hochgeladen?
   
2. Prüfe Gerätezustand:
   - Ausreichend PSRAM verfügbar?
   - LittleFS nicht voll?

3. Logs prüfen:
   - Fehlermeldungen bei Restore
   - JSON-Parsing-Fehler?

### Access Point Modus funktioniert nicht

1. Gerät neu starten
2. Nach WLAN "Panelclock-Setup" suchen
3. Verbinden (kein Passwort erforderlich)
4. Browser: http://192.168.4.1

## Zusammenfassung

Das Backup-System bietet eine vollständige, benutzerfreundliche Lösung für:

- ✅ Datensicherung aller Systemkonfigurationen
- ✅ Automatische tägliche Backups
- ✅ Einfache Wiederherstellung über Web-UI
- ✅ Notfall-Recovery im Access Point Modus
- ✅ Automatische Backup-Rotation
- ✅ Erweiterbar für neue Module

Das System entspricht allen Anforderungen der ursprünglichen Spezifikation und bietet eine solide Grundlage für zukünftige Erweiterungen.
