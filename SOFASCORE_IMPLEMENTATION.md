# SofaScore Live Darts - Implementierung Komplett ✅

## Übersicht

Das neue **SofaScoreLiveModule** ist vollständig implementiert und in die Panelclock-Application integriert. Es bietet drei Hauptfunktionen:

1. **Turnierauswahl** - Filtern nach bestimmten Turnieren
2. **Tagesergebnisse** - Alle Darts-Spiele des Tages anzeigen
3. **Live-Averages** - Echtzeit-Statistiken für laufende Spiele

---

## Implementierte Features

### 1. Turnierauswahl ✅

**API Endpoint:**
```
GET https://api.sofascore.com/api/v1/config/unique-tournaments/de/darts
```

**Funktionalität:**
- Lädt Liste aller verfügbaren Darts-Turniere
- Beispiele: PDC World Championship, Premier League, World Matchplay
- Filtert Matches basierend auf konfigurierten Turnier-IDs

**Konfiguration:**
```cpp
PsramString dartsSofascoreTournamentIds = "17,23,34";
// 17 = PDC World Championship
// 23 = Premier League
// 34 = World Matchplay
```

---

### 2. Tagesergebnisse ✅

**API Endpoint:**
```
GET https://api.sofascore.com/api/v1/sport/darts/scheduled-events/2024-12-15
```

**Was wird angezeigt:**
- Alle Darts-Spiele des aktuellen Tages
- Pro Match:
  - Turniername
  - Spielernamen (mit Scrolling bei langen Namen)
  - Aktueller Score (Sets gewonnen)
  - Status:
    - 🔴 **LIVE** (rote Schrift)
    - **FIN** (grüne Schrift)
    - **Uhrzeit** für geplante Spiele

**Display-Beispiel:**
```
╔════════════════════════════════╗
║ Today's Darts           2/5    ║
║   PDC World Championship       ║
║                                ║
║ 🔴 LIVE    3 : 2               ║
║            Sets                ║
║                                ║
║ Michael van Gerwen             ║
║ Luke Humphries                 ║
╚════════════════════════════════╝
```

**Features:**
- Ein Match pro Seite
- Automatisches Durchrotieren aller Matches
- Anzeige von geplanten Matches mit Startzeit
- Filtern nach ausgewählten Turnieren

---

### 3. Live-Averages ✅

**API Endpoint:**
```
GET https://api.sofascore.com/api/v1/event/{eventId}/statistics
```

**Was wird angezeigt:**
- Nur für **Live-Spiele** (Status = inprogress)
- Detaillierte Statistiken:
  - **Average** (Durchschnittswurf z.B. 103.5)
  - **180s** (Anzahl der Maximum-Scores)
  - **Checkout %** (Erfolgsquote beim Finish)

**Display-Beispiel:**
```
╔════════════════════════════════╗
║ 🔴 LIVE  PDC World Champ  1/2  ║
║                                ║
║           3 : 2                ║
║           Sets                 ║
║                                ║
║ van Gerwen (103.5)             ║
║ Humphries (99.2)               ║
║                                ║
║      180s: 8 | 5               ║
║      CO%: 45 | 38              ║
╚════════════════════════════════╝
```

**Features:**
- Average direkt beim Spielernamen: "van Gerwen (103.5)"
- 180er-Zähler beider Spieler
- Checkout-Prozentsatz
- Aktualisierung alle 2 Minuten

---

## Anzeigemodi und Rotation

Das Modul wechselt automatisch zwischen Modi:

1. **Daily Results Mode**
   - Zeigt alle Matches des Tages
   - Rotiert durch alle Seiten (1 Match = 1 Seite)
   - Dauer pro Seite: konfigurierbar (Standard: 20 Sek)

2. **Live Match Mode**
   - Zeigt nur Live-Spiele mit Statistiken
   - Rotiert durch alle Live-Matches
   - Dauer pro Seite: konfigurierbar (Standard: 20 Sek)

3. **Finish**
   - Nach Anzeige aller Seiten: Modul beendet
   - Nächstes Modul in der Rotation wird angezeigt

---

## Konfiguration

### In webconfig.hpp:

```cpp
/// Aktiviert das SofaScore Live-Modul
bool dartsSofascoreEnabled = false;

/// Abrufintervall in Minuten (empfohlen: 2-5 Min)
int dartsSofascoreFetchIntervalMin = 2;

/// Anzeigedauer pro Seite in Sekunden
int dartsSofascoreDisplaySec = 20;

/// Kommaseparierte Turnier-IDs (leer = alle Turniere)
PsramString dartsSofascoreTournamentIds;
```

### Beispiel-Konfiguration:

```cpp
dartsSofascoreEnabled = true;
dartsSofascoreFetchIntervalMin = 2;
dartsSofascoreDisplaySec = 15;
dartsSofascoreTournamentIds = "17,23,34,51";
// Nur PDC World Championship, Premier League, World Matchplay, Grand Slam
```

---

## Integration in Application

Das Modul ist vollständig in `Application.cpp` integriert:

### 1. Initialisierung:
```cpp
_sofascoreMod = new SofaScoreLiveModule(*_panelManager->getU8g2(), 
                                       *_panelManager->getCanvasData(), 
                                       webClient, deviceConfig);
_panelManager->registerModule(_sofascoreMod);
```

### 2. Konfiguration:
```cpp
_sofascoreMod->setConfig(deviceConfig->dartsSofascoreEnabled, 
                        deviceConfig->dartsSofascoreFetchIntervalMin, 
                        deviceConfig->dartsSofascoreDisplaySec, 
                        deviceConfig->dartsSofascoreTournamentIds);
```

### 3. Daten-Lifecycle:
```cpp
// In der main loop:
if(_sofascoreMod) _sofascoreMod->queueData();    // Daten anfordern
if(_sofascoreMod) _sofascoreMod->processData();  // Daten verarbeiten
```

### 4. Update-Callback:
```cpp
_sofascoreMod->onUpdate(redrawCb);  // Neu zeichnen bei Änderungen
```

---

## API-Zugriff und Datenfluss

### 1. Tournament List (1x täglich):
```
WebClient -> /config/unique-tournaments/de/darts
          -> JSON parse
          -> availableTournaments[]
```

### 2. Daily Events (alle 60 Min):
```
WebClient -> /sport/darts/scheduled-events/2024-12-15
          -> JSON parse
          -> dailyMatches[] + liveMatches[]
          -> Filter by tournament IDs
```

### 3. Live Statistics (alle 2 Min):
```
For each live match:
  WebClient -> /event/{eventId}/statistics
            -> JSON parse
            -> Update match.homeAverage, match.away180s, etc.
```

---

## Technische Details

### Speicherverwaltung:
- **PSRAM Allocation** für alle dynamischen Daten
- **ArduinoJson** mit SpiRamAllocator
- Mutex-geschützte Datenstrukturen
- Proper cleanup in Destruktoren

### Datenstrukturen:

```cpp
struct SofaScoreTournament {
    int id;
    char* name;
    char* slug;
    bool isEnabled;
};

struct SofaScoreMatch {
    int eventId;
    char* homePlayerName;
    char* awayPlayerName;
    int homeScore;  // Sets
    int awayScore;  // Sets
    char* tournamentName;
    MatchStatus status;  // SCHEDULED, LIVE, FINISHED
    long startTimestamp;
    
    // Live statistics:
    float homeAverage;
    float awayAverage;
    int home180s;
    int away180s;
    float homeCheckoutPercent;
    float awayCheckoutPercent;
};
```

### Display Features:
- **PixelScroller** für lange Spielernamen
- **Multi-Page Rotation** mit Page-Counter
- **Color Coding**:
  - Rot (0xF800) = LIVE
  - Grün (0x07E0) = Finished
  - Gelb (0xFFE0) = Stats/180s
  - Cyan (0x07FF) = Checkout%
  - Grau (0xAAAA) = Tournament name

---

## Testing und Validierung

### Manuelles Testing:
1. **Turnierliste abrufen:**
   ```cpp
   curl "https://api.sofascore.com/api/v1/config/unique-tournaments/de/darts"
   ```

2. **Heutige Events:**
   ```cpp
   curl "https://api.sofascore.com/api/v1/sport/darts/scheduled-events/$(date +%Y-%m-%d)"
   ```

3. **Live-Statistiken (Beispiel Event ID 15142767):**
   ```cpp
   curl "https://api.sofascore.com/api/v1/event/15142767/statistics"
   ```

### Erwartetes Verhalten:
- ✅ Modul lädt Tournaments beim Start
- ✅ Aktualisiert Daily Events jede Stunde
- ✅ Zeigt nur Matches aus konfigurierten Turnieren
- ✅ Wechselt zu Live-Modus wenn Spiele laufen
- ✅ Updated Live-Stats alle 2 Minuten
- ✅ Rotiert durch alle Seiten

---

## Nächste Schritte (Optional)

### 1. Web-Konfiguration UI
- [ ] Form-Felder in WebPages.hpp hinzufügen
- [ ] Save/Load Handlers in WebHandlers.cpp
- [ ] Tournament-Auswahl mit Checkboxen
- [ ] Live-Preview der aktivierten Turniere

### 2. Erweiterte Features (Nice-to-have)
- [ ] **Incidents anzeigen** (180er-Alerts, Checkouts)
- [ ] **Push-ähnliche Updates** (schnellere Polling bei Live-Matches)
- [ ] **Head-to-Head Historie** anzeigen
- [ ] **Turnier-Standings** (Tabellen)
- [ ] **Spielplan für morgen**

### 3. Optimierungen
- [ ] Caching von Turnier-Liste im Flash
- [ ] Reduziertes Polling für Scheduled Events
- [ ] Besseres Error Handling bei API-Fehlern
- [ ] Fallback-Display bei fehlender Internetverbindung

---

## Bekannte Einschränkungen

1. **Unofficial API** - SofaScore API ist nicht offiziell dokumentiert, kann sich ändern
2. **Kein Push** - Nur Polling möglich, keine Echtzeit-Push-Notifications
3. **Rate Limiting** - Empfohlen max. alle 2 Minuten für Live-Daten
4. **Verfügbarkeit** - Nicht alle Turniere haben detaillierte Statistiken

---

## Zusammenfassung

✅ **Vollständig implementiert:**
- Turnierauswahl mit Filterung
- Tagesergebnisse mit Status-Anzeige
- Live-Averages mit 180s und Checkout%

✅ **Integriert in Application:**
- Kompletter Lifecycle (init, config, queue, process, draw)
- Callbacks und Memory Management
- Multi-Page Rotation

✅ **Bereit für:**
- Web-UI Integration
- Testing mit echten API-Daten
- Deployment

**Status: Implementation Complete** 🎯🎉
