# ThemeParkModule - Benutzeranleitung

## Übersicht

Das ThemeParkModule zeigt Wartezeiten von Attraktionen aus ausgewählten Freizeitparks auf der Panelclock an. Die Daten werden über die kostenlose API von [Wartezeiten.APP](https://www.wartezeiten.app/) abgerufen.

## Funktionen

### Anzeige
- **Park-Name:** Als Überschrift auf jeder Seite
- **Crowd Level:** Auslastungsanzeige von 0-10 (farbkodiert)
  - Grün (0-3): Niedrige Auslastung
  - Hellgrün (4-5): Mittlere Auslastung
  - Gelb (6-7): Hohe Auslastung
  - Orange (8): Sehr hohe Auslastung
  - Rot (9-10): Maximale Auslastung
- **Wartezeiten:** Die 8 Attraktionen mit den längsten Wartezeiten
  - Farbkodierung der Wartezeiten:
    - Grün: < 15 Minuten
    - Gelb: 15-29 Minuten
    - Orange: 30-59 Minuten
    - Rot: ≥ 60 Minuten
- **Seitenindikator:** Zeigt aktuelle Seite und Gesamtzahl bei mehreren Parks

## Konfiguration

### Schritt 1: Modul aktivieren
1. Öffne die Web-Oberfläche der Panelclock
2. Navigiere zu "Anzeige-Module" → Tab "Freizeitparks"
3. Aktiviere das Kontrollkästchen "Freizeitpark-Anzeige aktivieren"

### Schritt 2: Abrufintervall einstellen
- **Empfohlen:** 10-15 Minuten
- Wartezeiten ändern sich nicht sekündlich
- Zu kurze Intervalle belasten die API unnötig

### Schritt 3: Anzeigedauer pro Park
- **Empfohlen:** 15-20 Sekunden
- Genug Zeit, um die Wartezeiten zu lesen
- Bei mehreren Parks entsprechend anpassen

### Schritt 4: Parks auswählen
1. Klicke auf "Verfügbare Parks laden"
2. Warte, bis die Liste der Parks erscheint
3. Wähle die gewünschten Parks durch Anklicken der Checkboxen
4. Die IDs werden automatisch gespeichert

### Schritt 5: Speichern
- Klicke auf "Alle Module speichern (Live-Update)"
- Die Konfiguration wird sofort angewendet
- Kein Neustart erforderlich!

## Verfügbare Parks

Die API von Wartezeiten.APP bietet Daten von Parks weltweit, darunter:
- Europa-Park (Rust, Deutschland)
- Phantasialand (Brühl, Deutschland)
- Heide Park (Soltau, Deutschland)
- Disneyland Paris (Frankreich)
- Efteling (Niederlande)
- Und viele mehr...

Die vollständige Liste wird dynamisch über die API geladen.

## Technische Details

### API-Endpunkte
- **Parks-Liste:** `https://api.wartezeiten.app/v1/parks`
- **Wartezeiten:** `https://api.wartezeiten.app/v1/parks/{parkId}/queue`

### Datenstruktur
Die API liefert für jeden Park:
- Park-ID und Name
- Crowd Level (Auslastung)
- Bereiche (Lands) mit Attraktionen
- Für jede Attraktion:
  - Name
  - Wartezeit in Minuten
  - Status (geöffnet/geschlossen)

### Speicherverwaltung
- Verwendet PSRAM für große Datenstrukturen
- Thread-sichere Implementierung mit Mutexen
- Asynchrones Laden im Hintergrund

## Fehlerbehebung

### "Keine Daten" wird angezeigt
**Mögliche Ursachen:**
1. Modul ist aktiviert, aber keine Parks ausgewählt
   - Lösung: Parks über die Web-Oberfläche auswählen
2. Noch keine Daten von der API empfangen
   - Lösung: Warten bis zum nächsten Abruf-Intervall
3. API ist nicht erreichbar
   - Lösung: Internet-Verbindung prüfen

### Parks erscheinen nicht in der Liste
**Mögliche Ursachen:**
1. API ist temporär nicht verfügbar
   - Lösung: Später erneut versuchen
2. Netzwerkproblem
   - Lösung: WLAN-Verbindung prüfen

### Wartezeiten sind veraltet
**Mögliche Ursachen:**
1. Abrufintervall zu lang
   - Lösung: Intervall in der Konfiguration verkürzen
2. Park ist geschlossen
   - Lösung: Normal, wenn außerhalb der Öffnungszeiten

## Best Practices

### Empfohlene Einstellungen
- **Abrufintervall:** 10 Minuten
- **Anzeigedauer:** 15 Sekunden pro Park
- **Anzahl Parks:** 2-4 Parks für gute Rotation

### Kombinationen mit anderen Modulen
Das ThemeParkModule funktioniert gut in Kombination mit:
- ClockModule (Uhrzeit)
- WeatherModule (Wetter vor Ort)
- CalendarModule (Besuchstermine)

## Attribution

Das Modul nutzt die kostenlose API von [Wartezeiten.APP](https://www.wartezeiten.app/). Bitte beachte die Nutzungsbedingungen der API und würdige den Service entsprechend.

## Support

Bei Problemen oder Fragen:
1. Prüfe die Logs in der seriellen Konsole
2. Überprüfe die Web-Oberfläche auf Fehlermeldungen
3. Öffne ein Issue auf GitHub mit detaillierter Beschreibung

---

**Version:** 1.0  
**Datum:** November 2025  
**Kompatibilität:** Panelclock mit WeatherModule und neueren Versionen
