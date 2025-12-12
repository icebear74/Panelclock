# SofaScore Darts API - Analyse und Möglichkeiten

## Übersicht
SofaScore bietet umfangreiche APIs für Live-Darts-Daten. Diese Dokumentation zeigt, was alles abgefragt und angezeigt werden kann.

## API Basis-URL
```
https://api.sofascore.com/api/v1
```

## Verfügbare Endpoints

### 1. Turniere und Events

#### Turnierliste abrufen
```
GET /config/unique-tournaments/{sprache}/{sport}
Beispiel: /config/unique-tournaments/de/darts
```
**Liefert:**
- Liste aller Darts-Turniere
- Turnier-IDs, Namen, Kategorien
- Verfügbare Saisons

**Anzeige-Möglichkeiten:**
- ✅ Dropdown/Auswahl verschiedener Turniere (PDC, WDF, etc.)
- ✅ Turniername und aktuelle Saison anzeigen

#### Geplante Events/Spiele
```
GET /sport/darts/scheduled-events/{datum}
Beispiel: /sport/darts/scheduled-events/2024-12-15
```
**Liefert:**
- Alle geplanten Darts-Spiele für ein bestimmtes Datum
- Spieler-Namen
- Turnier-Information
- Start-Zeitstempel
- Venue (Austragungsort)

**Anzeige-Möglichkeiten:**
- ✅ "Heute" / "Morgen" Spielplan
- ✅ Uhrzeit und Spieler-Namen
- ✅ Turniername pro Spiel

#### Event-Details
```
GET /event/{eventId}
```
**Liefert:**
- Vollständige Match-Informationen
- Aktueller Score (Sets und Legs)
- Status (geplant, live, beendet)
- Spieler-Details
- Turnier-Kontext

**Anzeige-Möglichkeiten:**
- ✅ Live-Score: "van Gerwen 3:2 Humphries"
- ✅ Set-Stand: "Set 4, Leg 3"
- ✅ Match-Status: "LIVE" / "Beendet" / "Startet in 30min"

---

### 2. Live-Daten und Statistiken

#### Match-Incidents (Play-by-Play)
```
GET /event/{eventId}/incidents
```
**Liefert detaillierte Ereignisse:**
- **180er-Würfe** (Maximum Score)
- **Checkouts** (Finish-Würfe mit Punktzahl)
- **Leg-Gewinner** pro Leg
- **Set-Stand** nach jedem Set
- **Bust** (Überwerfen)
- **Zeitstempel** pro Ereignis

**JSON Beispiel:**
```json
{
  "incidents": [
    {
      "type": "180",
      "player": {"id": 123, "name": "Michael van Gerwen"},
      "set": 2,
      "leg": 4,
      "score": 180,
      "description": "180 thrown"
    },
    {
      "type": "checkout",
      "player": {"id": 456, "name": "Luke Humphries"},
      "set": 3,
      "leg": 5,
      "score": 120,
      "description": "120 checkout"
    }
  ]
}
```

**Anzeige-Möglichkeiten:**
- ✅ **Letzte Highlights anzeigen:**
  - "🎯 van Gerwen: 180!"
  - "✓ Humphries: 120 Checkout"
- ✅ **180er-Zähler:** "van Gerwen: 5x 180"
- ✅ **Checkout-Liste:** Letzte 3 Checkouts anzeigen
- ✅ **Leg-für-Leg Fortschritt**

#### Match-Statistiken
```
GET /event/{eventId}/statistics
```
**Liefert aggregierte Statistiken:**
- Gesamtzahl der 180er pro Spieler
- Anzahl der Checkouts
- Durchschnittswurf (Average)
- Checkout-Quote
- Legs gewonnen / verloren
- Sets gewonnen / verloren

**Anzeige-Möglichkeiten:**
- ✅ **Statistik-Vergleich:**
  ```
  van Gerwen  |  Humphries
  180er: 8    |  180er: 5
  Avg: 103.5  |  Avg: 99.2
  Checkout: 45% | Checkout: 38%
  ```

#### Graph-Daten (Momentum)
```
GET /event/{eventId}/graph
```
**Liefert:**
- Verlauf der Punktestände
- Momentum-Shifts
- Performance-Trends über Zeit

**Anzeige-Möglichkeiten:**
- ✅ Einfaches Balkendiagramm für Momentum
- ✅ "🔥 van Gerwen dominiert aktuell"

---

### 3. Turnier-Informationen

#### Turnier-Tabellen/Standings
```
GET /unique-tournament/{tournamentId}/season/{seasonId}/standings/total
```
**Liefert:**
- Rangliste innerhalb eines Turniers
- Punkte, Siege, Niederlagen
- Platzierung

**Anzeige-Möglichkeiten:**
- ✅ Top 8 der aktuellen Turnier-Tabelle
- ✅ Position deines Lieblingsspielers

#### Turnier-Runden
```
GET /unique-tournament/{tournamentId}/season/{seasonId}/events/round/{roundNumber}
```
**Liefert:**
- Alle Spiele einer bestimmten Runde (z.B. "Achtelfinale")
- Ergebnisse oder kommende Spiele

**Anzeige-Möglichkeiten:**
- ✅ "Viertelfinale - Alle Spiele"
- ✅ Turnierbaum-Übersicht

---

### 4. Spieler-Informationen

#### Spieler-Statistiken
```
GET /player/{playerId}/unique-tournament/{tournamentId}/season/{seasonId}/statistics/overall
```
**Liefert:**
- Spieler-Performance in einem spezifischen Turnier
- Durchschnittswerte
- Siege/Niederlagen
- Form-Kurve

**Anzeige-Möglichkeiten:**
- ✅ "Michael van Gerwen - Turnier-Stats"
- ✅ Wins/Losses, Average, 180er-Rate

---

## Was kann auf dem Panel angezeigt werden?

### Priorität 1: Live-Scoring (Basis)
```
╔════════════════════════════════╗
║ LIVE 🔴 PDC World Championship ║
║                                ║
║   van Gerwen    3 : 2   Humphries
║      Sets                       ║
║                                ║
║   Current: Set 4, Leg 2        ║
╚════════════════════════════════╝
```

### Priorität 2: Live-Scoring mit letztem Highlight
```
╔════════════════════════════════╗
║ LIVE 🔴 World Championship     ║
║   van Gerwen    3 : 2   Humphries
║                                ║
║ 🎯 Last: van Gerwen 180!       ║
║   Set 4, Leg 2                 ║
╚════════════════════════════════╝
```

### Priorität 3: Live-Scoring mit Statistiken
```
╔════════════════════════════════╗
║ LIVE 🔴 PDC World Champ        ║
║   van Gerwen    3 : 2   Humphries
║                                ║
║ 180er: 8  |  Avg: 103.5        ║
║ 180er: 5  |  Avg: 99.2         ║
╚════════════════════════════════╝
```

### Priorität 4: Spielplan (Scheduled)
```
╔════════════════════════════════╗
║ Heute - Darts Spielplan        ║
║                                ║
║ 20:00  van Gerwen vs Price     ║
║        PDC World Championship  ║
║                                ║
║ 21:30  Wright vs Anderson      ║
║        PDC World Championship  ║
╚════════════════════════════════╝
```

### Priorität 5: Turnier-Auswahl-Menü
```
╔════════════════════════════════╗
║ Darts Live - Turniere          ║
║                                ║
║ ▶ PDC World Championship       ║
║   PDC Premier League           ║
║   World Matchplay              ║
║   Grand Slam of Darts          ║
╚════════════════════════════════╝
```

---

## Konfigurationsmöglichkeiten

### Minimal-Konfiguration
```cpp
bool dartsSofascoreEnabled = false;
int dartsSofascoreFetchIntervalMin = 2;  // Alle 2 Minuten
int dartsSofascoreDisplaySec = 20;       // 20 Sekunden anzeigen
```

### Erweiterte Konfiguration
```cpp
// Turnier-Filter
String sofascoreTournamentIds = "17,34,23";  // PDC WC, Premier League, etc.

// Anzeige-Optionen
bool sofascoreShowScheduled = true;    // Kommende Spiele
bool sofascoreShowLive = true;         // Live-Spiele
bool sofascoreShowStatistics = true;   // Statistiken einblenden
bool sofascoreShow180s = true;         // 180er anzeigen

// Spieler-Filter (optional)
String sofascoreTrackedPlayers = "van Gerwen,Humphries,Wright";
```

---

## Implementierungs-Vorschlag

### Stufe 1: Basis Live-Score
- ✅ Live-Spiele erkennen
- ✅ Spieler-Namen und Score anzeigen
- ✅ Match-Status (LIVE/Beendet)

### Stufe 2: Highlights
- ✅ Letzte 180er anzeigen
- ✅ Letzte Checkouts
- ✅ Set/Leg Stand

### Stufe 3: Statistiken
- ✅ 180er-Zähler
- ✅ Average anzeigen
- ✅ Checkout-Quote

### Stufe 4: Spielplan
- ✅ Kommende Spiele heute/morgen
- ✅ Uhrzeit und Turnier

### Stufe 5: Turnier-Auswahl
- ✅ Benutzer wählt Turniere aus
- ✅ Nur ausgewählte Turniere anzeigen

---

## Detaillierte Match-Seite (z.B. Event ID: 15142767)

Eine spezifische Match-Seite auf SofaScore bietet mehrere Tabs mit detaillierten Informationen:

### Tab 1: Übersicht / Live-Score
**Endpoint:** `GET /event/{eventId}`

**Verfügbare Daten:**
```json
{
  "event": {
    "id": 15142767,
    "homeTeam": {"name": "Merk", "id": 123},
    "awayTeam": {"name": "Huybrechts", "id": 456},
    "homeScore": {"current": 3, "period1": 1, "period2": 2},
    "awayScore": {"current": 2, "period1": 0, "period2": 2},
    "status": {"type": "inprogress", "description": "Live"},
    "tournament": {"name": "PDC World Championship"},
    "startTimestamp": 1734123600
  }
}
```

**Anzeige-Möglichkeiten:**
- ✅ Match-Header: "Merk vs Huybrechts"
- ✅ Live-Score mit Sets: "3:2"
- ✅ Status: "🔴 LIVE" / "Beendet" / "Startet um 20:00"
- ✅ Turniername: "PDC World Championship"

---

### Tab 2: Statistiken
**Endpoint:** `GET /event/{eventId}/statistics`

**Verfügbare detaillierte Statistiken:**
```json
{
  "statistics": [
    {
      "period": "ALL",
      "groups": [
        {
          "groupName": "Scores",
          "statisticsItems": [
            {"name": "Average", "home": "103.5", "away": "99.2"},
            {"name": "180s", "home": "8", "away": "5"},
            {"name": "140+", "home": "12", "away": "9"},
            {"name": "100+", "home": "24", "away": "21"},
            {"name": "Checkout %", "home": "45.5", "away": "38.2"},
            {"name": "Highest Checkout", "home": "170", "away": "141"},
            {"name": "Legs Won", "home": "18", "away": "15"}
          ]
        }
      ]
    }
  ]
}
```

**Panel-Anzeige Beispiel (kompakt):**
```
╔════════════════════════════════╗
║ Merk vs Huybrechts - Stats    ║
║                                ║
║        Merk    |  Huybrechts   ║
║ Avg:   103.5  |  99.2          ║
║ 180s:  8      |  5             ║
║ CO%:   45%    |  38%           ║
║ Legs:  18     |  15            ║
╚════════════════════════════════╝
```

**Anzeige-Möglichkeiten:**
- ✅ **Average-Vergleich** (wichtigste Statistik)
- ✅ **180er-Zähler** beider Spieler
- ✅ **Checkout-Prozentsatz**
- ✅ **Höchster Checkout** (z.B. "170 von Merk")
- ✅ **Legs gewonnen** (detaillierter als nur Sets)
- ✅ **140+ und 100+ Würfe** (High-Scores)

---

### Tab 3: Incidents (Point-by-Point / Echtzeit-Events)
**Endpoint:** `GET /event/{eventId}/incidents`

**Verfügbare Ereignisse in Echtzeit:**
```json
{
  "incidents": [
    {
      "id": 1,
      "type": "period",
      "text": "Set 1",
      "homeScore": 1,
      "awayScore": 0,
      "time": 1
    },
    {
      "id": 2,
      "type": "score180",
      "player": {"name": "Merk", "id": 123},
      "text": "180 scored",
      "time": 5
    },
    {
      "id": 3,
      "type": "checkout",
      "player": {"name": "Huybrechts", "id": 456},
      "text": "120 checkout",
      "value": 120,
      "time": 12
    },
    {
      "id": 4,
      "type": "legWon",
      "player": {"name": "Merk", "id": 123},
      "text": "Leg won",
      "homeScore": 1,
      "awayScore": 0,
      "time": 15
    }
  ]
}
```

**Event-Typen:**
- `score180` - Maximum Score (180 Punkte)
- `checkout` - Erfolgreicher Finish
- `legWon` - Leg gewonnen
- `period` - Set-Start/Ende
- `bust` - Überworfen (optional)
- `missedDouble` - Doppel verfehlt

**Panel-Anzeige Beispiel (Highlight-Ticker):**
```
╔════════════════════════════════╗
║ LIVE - Recent Highlights       ║
║                                ║
║ 🎯 Merk: 180!                  ║
║ ✓ Huybrechts: 120 Checkout    ║
║ 🏆 Merk wins Leg (Set 3-2)     ║
║                                ║
║ Current: Set 4, Leg 2          ║
╚════════════════════════════════╝
```

**Anzeige-Möglichkeiten:**
- ✅ **Live-Ticker** der letzten 3-5 Events
- ✅ **180er-Alerts** hervorheben
- ✅ **High-Checkouts** anzeigen (>100)
- ✅ **Leg-Gewinner** mit aktuellem Stand
- ✅ **Scrollender Event-Feed** (älteste raus, neueste rein)

---

### Tab 4: H2H (Head-to-Head)
**Endpoint:** `GET /event/{eventId}/h2h`

**Verfügbare Vergleichsdaten:**
```json
{
  "h2h": {
    "summary": {
      "homeWins": 5,
      "awayWins": 3,
      "draws": 0
    },
    "previousMatches": [
      {
        "tournament": "PDC Premier League",
        "date": "2024-11-15",
        "homeScore": 6,
        "awayScore": 4,
        "winner": "home"
      }
    ]
  }
}
```

**Anzeige-Möglichkeiten:**
- ✅ "Bisherige Duelle: Merk 5-3 Huybrechts"
- ✅ "Letztes Match: 6:4 für Merk (Nov 2024)"
- ✅ Kurze History (nur wenn Platz)

---

### Tab 5: Lineups / Player Info
**Endpoint:** `GET /event/{eventId}` (Teil der Antwort)

**Verfügbare Spieler-Infos:**
```json
{
  "homeTeam": {
    "id": 123,
    "name": "Maik Kuivenhoven",
    "country": {"name": "Netherlands", "alpha2": "NL"},
    "ranking": 32
  },
  "awayTeam": {
    "id": 456,
    "name": "Kim Huybrechts",
    "country": {"name": "Belgium", "alpha2": "BE"},
    "ranking": 28
  }
}
```

**Anzeige-Möglichkeiten:**
- ✅ Spieler-Namen mit Flaggen (🇳🇱 🇧🇪)
- ✅ Weltranglisten-Position
- ✅ Formkurve (wenn verfügbar)

---

### Tab 6: Point-by-Point (Detaillierter Wurf-für-Wurf)
**Endpoint:** `GET /event/{eventId}/point-by-point`

**Sehr detaillierte Wurf-Daten (wenn verfügbar):**
```json
{
  "pointByPoint": [
    {
      "leg": 1,
      "set": 1,
      "throws": [
        {"player": "home", "score": 60, "remaining": 441},
        {"player": "away", "score": 85, "remaining": 416},
        {"player": "home", "score": 100, "remaining": 341}
      ]
    }
  ]
}
```

**Anzeige-Möglichkeiten:**
- ✅ "Restpunkte: Merk 341 / Huybrechts 416"
- ✅ Wurf-für-Wurf Ticker (sehr detailliert)
- ⚠️ **Hinweis:** Nicht für alle Spiele verfügbar!

---

## Zusammenfassung: Was ist auf der Match-Seite verfügbar?

### ✅ Definitiv verfügbar (für alle Matches):
1. **Live-Score** (Sets/Legs)
2. **Match-Status** (Live, Beendet, Geplant)
3. **Turnier-Name** und Kontext
4. **Basis-Statistiken** (Average, 180er, Checkout%)

### ✅ Meistens verfügbar (Live-Matches):
5. **Incidents/Events** (180er, Checkouts, Leg-Gewinner)
6. **Detaillierte Statistiken** (140+, 100+, Highest Checkout)
7. **Head-to-Head** Historie

### ⚠️ Manchmal verfügbar:
8. **Point-by-Point** Wurf-Details (nur Top-Events)
9. **Restpunkte** pro Wurf

---

## Empfehlung für Panel-Display

### Anzeigeformat 1: Live-Score Kompakt
```
╔════════════════════════════════╗
║ 🔴 LIVE - PDC World Champ      ║
║                                ║
║    Merk  3 : 2  Huybrechts     ║
║           Sets                 ║
║    Set 4, Leg 2                ║
╚════════════════════════════════╝
```

### Anzeigeformat 2: Mit Highlights (rotierend)
**Seite 1 - Score:**
```
╔════════════════════════════════╗
║ 🔴 LIVE - World Championship   ║
║    Merk  3 : 2  Huybrechts     ║
║    Set 4, Leg 2 läuft...       ║
╚════════════════════════════════╝
```

**Seite 2 - Statistiken:**
```
╔════════════════════════════════╗
║ Merk vs Huybrechts - Stats    ║
║ Avg:  103.5  |  99.2           ║
║ 180s:   8    |   5             ║
║ CO%:   45%   |  38%            ║
╚════════════════════════════════╝
```

**Seite 3 - Letzte Events:**
```
╔════════════════════════════════╗
║ Recent Highlights:             ║
║ 🎯 Merk: 180!                  ║
║ ✓ Huybrechts: 120 Checkout    ║
║ 🏆 Merk gewinnt Leg            ║
╚════════════════════════════════╝
```

### Rotation:
- Jede Seite für 10-15 Sekunden
- Bei neuem Event (180, Checkout) → sofort zu Seite 3 springen
- Dann zurück zur Rotation

---

## API-Zugriff

### Beispiel: Match-Daten für Event 15142767 (Merk vs Huybrechts)

**Basis-Daten abrufen:**
```cpp
// Im WebClientModule registrieren:
String url = "https://api.sofascore.com/api/v1/event/15142767";
webClient->registerResource(url, 2, nullptr);  // Alle 2 Minuten

// Daten verarbeiten:
webClient->accessResource(url, [](const char* json, size_t size, time_t last_update, bool is_stale) {
    // JSON parsen mit ArduinoJson
    // Spieler-Namen, Score, Status extrahieren
});
```

**Statistiken abrufen:**
```cpp
String statsUrl = "https://api.sofascore.com/api/v1/event/15142767/statistics";
// Liefert: Average, 180s, Checkout%, Legs Won, etc.
```

**Live-Events abrufen:**
```cpp
String incidentsUrl = "https://api.sofascore.com/api/v1/event/15142767/incidents";
// Liefert: Alle 180er, Checkouts, Leg-Gewinner in Echtzeit
```

---

## API-Zugriff (Allgemein)

### Keine Authentifizierung erforderlich
Die SofaScore API ist öffentlich zugänglich (unofficial), keine API-Keys nötig.

### Rate Limiting
- Empfohlen: Alle 1-2 Minuten abfragen für Live-Daten
- Scheduled Events: 1x täglich ausreichend

### HTTPS/Zertifikat
- Verwendet HTTPS
- Standard CA-Zertifikat sollte funktionieren

---

## Fazit

### ✅ Was definitiv machbar ist:
1. **Live-Scores** mit Spieler-Namen und Set-Stand
2. **180er und Checkouts** in Echtzeit
3. **Match-Statistiken** (Average, 180er-Anzahl)
4. **Spielplan** für kommende Spiele
5. **Turnier-Filter** (Benutzer wählt aus)
6. **Tracked Players** (Lieblingsspieler hervorheben)

### ⚠️ Einschränkungen:
- API ist unofficial, kann sich ändern
- Keine Push-Notifications (nur Polling alle 1-2 Min)
- Keine Leg-für-Leg detaillierte Wurf-Historie

### 💡 Empfehlung:
**Starte mit Stufe 1+2** - Live-Score mit Highlights (180er/Checkouts)
Das bietet den größten Mehrwert und ist gut auf dem Panel darstellbar!
