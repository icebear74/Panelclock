# Fonts und Icons im Panelclock-Projekt

Diese Dokumentation beschreibt alle verwendeten Schriftarten (Fonts) in allen Modulen sowie alle Icons im WeatherModule mit deren genauen Verwendungszwecken.

---

## Verwendete Fonts in allen Modulen

Alle Module verwenden Fonts aus der U8g2-Bibliothek. Die Fonts werden über die `u8g2.setFont()` Methode gesetzt.

### 1. ClockModule (Hauptuhr-Modul)

**Zweck:** Zeigt die aktuelle Uhrzeit, Datum, Wochentag und Systemstatusinformationen an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_5x8_tf` | Speicherinformationen | Kleine Schrift für Systemspeicher-Anzeige (Heap-Status) in der obersten Zeile |
| `u8g2_font_fub20_tf` | Hauptuhrzeit | Große, fette Schrift für die Hauptuhrzeitanzeige (HH:MM:SS) - gut lesbar |
| `u8g2_font_6x10_tf` | Datum, Wochentag und KW | Mittelgroße Schrift für Datumsanzeige (DD.MM.YYYY), Wochentag (z.B. "Montag") und Kalenderwoche/Tag-des-Jahres |

---

### 2. CalendarModule (Kalender-Modul)

**Zweck:** Zeigt Termine und Events aus iCal-Quellen an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_5x8_tf` | Standard-Eventliste | Kleine, kompakte Schrift für die Auflistung mehrerer Termine mit Scrollen |
| `u8g2_font_6x13_tf` | Standard-Ansicht | Mittelgroße Schrift für normale Terminanzeige |
| `u8g2_font_7x14_tf` | Hervorgehobene Events | Größere Schrift für wichtige oder zeitnahe Termine |
| `u8g2_font_logisoso16_tf` | Dringende Termine (Urgent) | Große, auffällige Schrift für sehr dringende Termine (innerhalb der Urgent-Schwelle) |
| `u8g2_font_helvB12_tf` | Zeitanzeige bei Urgent | Fettgedruckte Schrift für Zeitangaben bei dringenden Terminen |

---

### 3. WeatherModule (Wetter-Modul)

**Zweck:** Zeigt aktuelle Wetterdaten, Vorhersagen und Wetterwarnungen an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_helvR08_tr` | Hauptschrift für alle Wetterdaten | Standard Helvetica-Schrift (Regular, 8pt) - wird konsistent für die meisten Texte verwendet: Titel, Temperaturen, Luftfeuchtigkeit, Wolkendecke, Wind |
| `u8g2_font_helvB08_tr` | Diagramm-Titel | Fettgedruckte Helvetica (Bold, 8pt) für Überschriften von Charts (z.B. "NIEDERSCHLAG HEUTE") |
| `u8g2_font_4x6_tf` | Achsenbeschriftungen in Diagrammen | Sehr kleine Schrift für Y-Achsen-Beschriftungen in Niederschlags-Charts |
| `u8g2_font_helvB10_tr` | Wetterwarnungs-Titel | Größere, fettgedruckte Schrift für den "WETTERWARNUNG"-Titel bei Alerts |

---

### 4. CuriousHolidaysModule (Kuriose Feiertage-Modul)

**Zweck:** Zeigt kuriose Feiertage des aktuellen und nächsten Tages an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_6x10_tf` | Standardtext | Mittelgroße Schrift für die Anzeige von Feiertagsnamen und Beschreibungen |
| `u8g2_font_7x14_tf` | Hervorgehobener Text | Größere Schrift für wichtige oder lange Feiertags-Bezeichnungen |
| `u8g2_font_helvB14_tf` | Überschriften | Fettgedruckte, große Schrift für Tagesüberschriften ("HEUTE", "MORGEN") |

---

### 5. DartsRankingModule (Darts-Ranglisten-Modul)

**Zweck:** Zeigt Darts-Ranglisten (Order of Merit, Pro Tour) und Live-Turnier-Informationen an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_profont12_tf` | Hauptrangliste | ProFont 12pt für die Darstellung der Spielernamen und Rangplätze |
| `u8g2_font_profont10_tf` | Detailinformationen | Kleinere ProFont 10pt für zusätzliche Details (Punkte, Runden-Info bei Live-Turnieren) |
| `u8g2_font_5x8_tf` | Zusatzinformationen | Sehr kleine Schrift für kompakte Zusatzinformationen |

---

### 6. FritzboxModule (FRITZ!Box-Anrufliste-Modul)

**Zweck:** Zeigt Anrufliste der FRITZ!Box (verpasste, eingehende, ausgehende Anrufe).

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_7x14B_tf` | Überschrift | Fettgedruckte Schrift für den Modultitel ("Anrufe") |
| `u8g2_font_logisoso18_tn` | Anzahl der Anrufe | Große Zahlenschrift für die Anzeige der Anruf-Anzahl |
| `u8g2_font_7x14_tf` | Anrufdetails | Normale Schrift für Anrufernamen und Nummern |
| `u8g2_font_6x13_tf` | Zeitstempel | Mittelgroße Schrift für Datum/Uhrzeit der Anrufe |

---

### 7. TankerkoenigModule (Tankstellen-Preise-Modul)

**Zweck:** Zeigt aktuelle Spritpreise von konfigurierten Tankstellen an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_7x14_tf` | Tankstellennamen | Normale Schrift für den Namen der Tankstelle |
| `u8g2_font_helvB14_tf` | Spritpreise | Fettgedruckte, große Schrift für die Preisanzeige (gut lesbar) |
| `u8g2_font_5x8_tf` | Spritsorten und Details | Kleine Schrift für Kraftstofftypen (E5, E10, Diesel) und Zusatzinformationen |
| `u8g2_font_6x13_me` | Sonderzeichen | Schrift mit erweiterten Zeichen für Euro-Symbol (€) und andere Symbole |

---

### 8. ThemeParkModule (Freizeitpark-Wartezeiten-Modul)

**Zweck:** Zeigt Wartezeiten für Attraktionen in ausgewählten Freizeitparks an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_6x13_tf` | Parkname und Attraktionsnamen | Standard-Schrift für den Namen des Parks und der Attraktionen (mit Scrollen bei langen Namen) |
| `u8g2_font_9x15_tf` | Öffnungszeiten und Auslastung | Größere Schrift für Parkstatus ("Geschlossen", Öffnungszeiten, Crowd-Level) |
| `u8g2_font_5x8_tf` | Wartezeiten-Details | Kleine, kompakte Schrift für die Liste der Attraktionen mit Wartezeiten |

---

### 9. OtaManager (Over-The-Air Update Manager)

**Zweck:** Zeigt Update-Status während OTA-Firmware-Updates an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_6x13_tf` | Update-Statusmeldungen | Mittelgroße Schrift für alle Statusmeldungen während des Update-Vorgangs |

---

### 10. PanelManager (Display-Manager)

**Zweck:** Zeigt Status-Informationen beim Modul-Wechsel an.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `u8g2_font_6x13_tf` | Modul-Wechsel-Anzeige | Mittelgroße Schrift für kurze Statusmeldungen beim Übergang zwischen Modulen |

---

### 11. WebPages.hpp (Web-Interface)

**Zweck:** HTML/CSS Fonts für die Web-Konfigurationsoberfläche.

| Font | Verwendung | Beschreibung |
|------|-----------|--------------|
| `Arial, Helvetica, sans-serif` | Standard-Webseiten-Schrift | Systemschrift für den gesamten Web-Interface-Text |
| `monospace` | Log-Ausgabe | Monospace-Schrift für die Log-Anzeige (Terminal-Stil), Schriftgröße 12px |

---

## Icons im WeatherModule

Das WeatherModule verwendet zwei Arten von Icons: **Haupt-Wetter-Icons** (Main Icons) und **Spezial-Icons** (Special Icons). Alle Icons werden in drei Größen verwendet: 48x48px, 24x24px und 16x16px, je nach Anzeigekontext.

### Icon-System Übersicht

- **Haupt-Icons (WeatherIcons_Main.cpp):** Wettersymbole basierend auf WMO-Wettercodes
- **Spezial-Icons (WeatherIcons_Special.cpp):** Zusätzliche Icons für Wind, Temperatur, UV-Index, Luftfeuchtigkeit, etc.
- **Tag/Nacht-Varianten:** Viele Haupt-Icons haben separate Tag- und Nacht-Versionen (z.B. `clear` und `clear_night`)

---

## Haupt-Wetter-Icons (Main Weather Icons)

Diese Icons repräsentieren die aktuellen Wetterbedingungen basierend auf WMO-Codes.

### Klarer Himmel und Bewölkung

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `clear` | Tag | Aktuelle Wetter-Ansicht (JETZT), Stunden- und Tagesvorhersage | 0 | Klarer Himmel - Sonne sichtbar |
| `clear_night` | Nacht | Aktuelle Wetter-Ansicht (JETZT), Stunden- und Tagesvorhersage | 0 | Klarer Nachthimmel - Mond sichtbar |
| `mainly_clear` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 1 | Überwiegend klar - einzelne Wolken |
| `mainly_clear_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 1 | Überwiegend klare Nacht - Mond teilweise sichtbar |
| `partly_cloudy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 2 | Teilweise bewölkt - Sonne und Wolken |
| `partly_cloudy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 2 | Teilweise bewölkte Nacht - Mond und Wolken |
| `overcast` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 3 | Bedeckt - vollständig bewölkter Himmel |
| `overcast_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 3 | Bedeckte Nacht - keine Sterne sichtbar |

---

### Nebel

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `fog` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 45 | Nebel - eingeschränkte Sicht |
| `fog_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 45 | Nebel bei Nacht |
| `rime_fog` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 48 | Eisnebel (Raureif-Nebel) |
| `rime_fog_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 48 | Eisnebel bei Nacht |

---

### Nieselregen (Drizzle)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `drizzle_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 51 | Leichter Nieselregen |
| `drizzle_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 51 | Leichter Nieselregen bei Nacht |
| `drizzle_moderate` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 53 | Mäßiger Nieselregen |
| `drizzle_moderate_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 53 | Mäßiger Nieselregen bei Nacht |
| `drizzle_dense` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 55 | Starker Nieselregen |
| `drizzle_dense_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 55 | Starker Nieselregen bei Nacht |

---

### Gefrierender Nieselregen (Freezing Drizzle)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `freezing_drizzle_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 56 | Leichter gefrierender Nieselregen |
| `freezing_drizzle_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 56 | Leichter gefrierender Nieselregen bei Nacht |
| `freezing_drizzle_dense` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 57 | Starker gefrierender Nieselregen |
| `freezing_drizzle_dense_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 57 | Starker gefrierender Nieselregen bei Nacht |

---

### Regen (Rain)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `rain_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 61 | Leichter Regen |
| `rain_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 61 | Leichter Regen bei Nacht |
| `rain_moderate` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 63 | Mäßiger Regen |
| `rain_moderate_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 63 | Mäßiger Regen bei Nacht |
| `rain_heavy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 65 | Starker Regen |
| `rain_heavy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 65 | Starker Regen bei Nacht |

---

### Gefrierender Regen (Freezing Rain)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `freezing_rain_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 66 | Leichter gefrierender Regen |
| `freezing_rain_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 66 | Leichter gefrierender Regen bei Nacht |
| `freezing_rain_heavy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 67 | Starker gefrierender Regen |
| `freezing_rain_heavy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 67 | Starker gefrierender Regen bei Nacht |

---

### Schnee (Snow)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `snow_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 71 | Leichter Schneefall |
| `snow_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 71 | Leichter Schneefall bei Nacht |
| `snow_moderate` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 73 | Mäßiger Schneefall |
| `snow_moderate_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 73 | Mäßiger Schneefall bei Nacht |
| `snow_heavy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 75 | Starker Schneefall |
| `snow_heavy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 75 | Starker Schneefall bei Nacht |
| `snow_grains` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 77 | Schneegriesel |
| `snow_grains_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 77 | Schneegriesel bei Nacht |

---

### Regenschauer (Showers)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `showers_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 80 | Leichte Regenschauer |
| `showers_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 80 | Leichte Regenschauer bei Nacht |
| `showers_moderate` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 81 | Mäßige Regenschauer |
| `showers_moderate_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 81 | Mäßige Regenschauer bei Nacht |
| `showers_heavy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 82 | Starke Regenschauer |
| `showers_heavy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 82 | Starke Regenschauer bei Nacht |

---

### Schneeschauer (Snow Showers)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `snow_showers_light` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 85 | Leichte Schneeschauer |
| `snow_showers_light_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 85 | Leichte Schneeschauer bei Nacht |
| `snow_showers_heavy` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 86 | Starke Schneeschauer |
| `snow_showers_heavy_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 86 | Starke Schneeschauer bei Nacht |

---

### Gewitter (Thunderstorms)

| Icon-Name | Tag/Nacht | Verwendung | WMO-Code | Beschreibung |
|-----------|-----------|-----------|----------|--------------|
| `thunderstorm` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 95 | Gewitter |
| `thunderstorm_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 95 | Gewitter bei Nacht |
| `thunderstorm_light_hail` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 96 | Gewitter mit leichtem Hagel |
| `thunderstorm_light_hail_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 96 | Gewitter mit leichtem Hagel bei Nacht |
| `thunderstorm_heavy_hail` | Tag | Aktuelle Wetter-Ansicht, Vorhersagen | 99 | Gewitter mit starkem Hagel |
| `thunderstorm_heavy_hail_night` | Nacht | Aktuelle Wetter-Ansicht, Vorhersagen | 99 | Gewitter mit starkem Hagel bei Nacht |

---

## Spezial-Icons (Special Icons)

Diese Icons werden für zusätzliche Wetterinformationen und Detailansichten verwendet.

### Wind-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `wind_calm` | 16x16 | HEUTE - Details (Seite 3) | Windstill (0-5 km/h) |
| `wind_light` | 16x16 | HEUTE - Details (Seite 3) | Leichter Wind (5-15 km/h) |
| `wind_moderate` | 16x16 | HEUTE - Details (Seite 3) | Mäßiger Wind (15-30 km/h) |
| `wind_strong` | 16x16 | HEUTE - Details (Seite 3) | Starker Wind (30-50 km/h) |
| `wind_storm` | 16x16 | HEUTE - Details (Seite 3) | Sturm (>50 km/h) |
| `wind_north` | 16x16 | Nicht aktuell verwendet | Windrichtung Nord (für zukünftige Erweiterung) |
| `wind_northeast` | 16x16 | Nicht aktuell verwendet | Windrichtung Nordost |
| `wind_east` | 16x16 | Nicht aktuell verwendet | Windrichtung Ost |
| `wind_southeast` | 16x16 | Nicht aktuell verwendet | Windrichtung Südost |
| `wind_south` | 16x16 | Nicht aktuell verwendet | Windrichtung Süd |
| `wind_southwest` | 16x16 | Nicht aktuell verwendet | Windrichtung Südwest |
| `wind_west` | 16x16 | Nicht aktuell verwendet | Windrichtung West |
| `wind_northwest` | 16x16 | Nicht aktuell verwendet | Windrichtung Nordwest |

**Verwendungskontext:** In der "HEUTE - Details"-Ansicht (Seite 3) wird die Windgeschwindigkeit mit einem passenden Icon visualisiert. Das Icon wechselt dynamisch basierend auf der aktuellen Windgeschwindigkeit.

---

### Temperatur-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `temp_hot` | 16x16 | HEUTE (Seite 2) | Heiße Temperatur - zeigt Maximaltemperatur des Tages an |
| `temp_warm` | 16x16 | Nicht aktuell verwendet | Warme Temperatur (für zukünftige Erweiterung) |
| `temp_moderate` | 16x16 | HEUTE (Seite 2) | Moderate Temperatur - zeigt Durchschnittstemperatur des Tages an |
| `temp_cool` | 16x16 | Nicht aktuell verwendet | Kühle Temperatur (für zukünftige Erweiterung) |
| `temp_cold` | 16x16 | HEUTE (Seite 2) | Kalte Temperatur - zeigt Minimaltemperatur des Tages an |
| `temp_freezing` | 16x16 | Nicht aktuell verwendet | Gefrierende Temperatur (für zukünftige Erweiterung) |

**Verwendungskontext:** In der "HEUTE"-Ansicht (Seite 2) werden die Tages-Temperaturwerte (Maximum, Minimum, Durchschnitt) mit entsprechenden Icons visualisiert, um die Informationen intuitiver zu machen.

---

### UV-Index-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `uv_low` | 16x16 | Nicht aktuell verwendet | Niedriger UV-Index (0-2) |
| `uv_moderate` | 16x16 | HEUTE (Seite 2) | Mäßiger UV-Index (3-5) - wird angezeigt wenn UV-Daten verfügbar |
| `uv_high` | 16x16 | Nicht aktuell verwendet | Hoher UV-Index (6-7) |
| `uv_very_high` | 16x16 | Nicht aktuell verwendet | Sehr hoher UV-Index (8-10) |
| `uv_extreme` | 16x16 | Nicht aktuell verwendet | Extremer UV-Index (11+) |

**Verwendungskontext:** In der "HEUTE"-Ansicht (Seite 2) wird der UV-Index mit Icon angezeigt, wenn entsprechende Daten von der API verfügbar sind. Zeigt die Sonnenbrandgefahr an.

---

### Luftfeuchtigkeits-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `humidity_low` | 16x16 | HEUTE - Details (Seite 3) | Niedrige Luftfeuchtigkeit (<40%) |
| `humidity_moderate` | 16x16 | HEUTE - Details (Seite 3) | Mäßige Luftfeuchtigkeit (40-70%) |
| `humidity_high` | 16x16 | HEUTE - Details (Seite 3) | Hohe Luftfeuchtigkeit (>70%) |

**Verwendungskontext:** In der "HEUTE - Details"-Ansicht (Seite 3) wird die aktuelle Luftfeuchtigkeit mit einem Icon visualisiert. Das Icon wechselt dynamisch basierend auf dem Feuchtigkeitswert:
- Unter 40%: `humidity_low` (trockene Luft)
- 40-70%: `humidity_moderate` (angenehme Luftfeuchtigkeit)
- Über 70%: `humidity_high` (schwüle Luft)

---

### Luftdruck-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `pressure_rising` | 16x16 | Nicht aktuell verwendet | Steigender Luftdruck (Wetterbesserung) |
| `pressure_steady` | 16x16 | Nicht aktuell verwendet | Stabiler Luftdruck |
| `pressure_falling` | 16x16 | Nicht aktuell verwendet | Fallender Luftdruck (Wetterverschlechterung) |

**Status:** Diese Icons sind vorbereitet für zukünftige Erweiterungen zur Anzeige von Luftdrucktrends.

---

### Sichtweiten-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `visibility_clear` | 16x16 | Nicht aktuell verwendet | Klare Sicht (>10km) |
| `visibility_good` | 16x16 | Nicht aktuell verwendet | Gute Sicht (5-10km) |
| `visibility_moderate` | 16x16 | Nicht aktuell verwendet | Mäßige Sicht (1-5km) |
| `visibility_poor` | 16x16 | Nicht aktuell verwendet | Schlechte Sicht (<1km) |

**Status:** Diese Icons sind vorbereitet für zukünftige Erweiterungen zur Anzeige der Sichtweite.

---

### Warnungs-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `warning_generic` | 48x48 | Wetterwarnung-Seite | Allgemeine Wetterwarnung - wird bei allen Wetteralerts angezeigt |
| `warning_wind` | 16x16 | Nicht aktuell verwendet | Warnung vor starkem Wind |
| `warning_rain` | 16x16 | Nicht aktuell verwendet | Warnung vor starkem Regen |
| `warning_snow` | 16x16 | Nicht aktuell verwendet | Warnung vor starkem Schneefall |
| `warning_ice` | 16x16 | Nicht aktuell verwendet | Warnung vor Glätte/Eis |
| `warning_heat` | 16x16 | Nicht aktuell verwendet | Warnung vor Hitze |
| `warning_cold` | 16x16 | Nicht aktuell verwendet | Warnung vor Kälte |
| `warning_fog` | 16x16 | Nicht aktuell verwendet | Warnung vor Nebel |
| `warning_thunderstorm` | 16x16 | Nicht aktuell verwendet | Warnung vor Gewitter |

**Verwendungskontext:** Wenn Wetterwarnungen von der API empfangen werden, wird eine spezielle Alert-Seite angezeigt mit einem großen `warning_generic` Icon (48x48px), pulsierendem rotem Hintergrund und den Warnung-Details (Event-Name, Start-/Endzeit).

---

### Astronomie- und Sonder-Icons

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `sunrise` | 16x16 | HEUTE (Seite 2), HEUTE - Details (Seite 3) | Sonnenaufgang - zeigt Sonnenaufgangszeit oder Sonnenscheindauer |
| `sunset` | 16x16 | HEUTE (Seite 2) | Sonnenuntergang - zeigt Sonnenuntergangszeit |
| `rainbow` | 16x16 | Nicht aktuell verwendet | Regenbogen (für spezielle Wetterbedingungen) |
| `arrow_up` | 16x16 | Nicht aktuell verwendet | Pfeil nach oben (für Trends) |
| `arrow_down` | 16x16 | Nicht aktuell verwendet | Pfeil nach unten (für Trends) |
| `arrow_right` | 16x16 | Nicht aktuell verwendet | Pfeil nach rechts (für Navigation) |
| `arrow_left` | 16x16 | Nicht aktuell verwendet | Pfeil nach links (für Navigation) |
| `arrow_up_right` | 16x16 | Nicht aktuell verwendet | Pfeil nach oben rechts (für Trends) |
| `arrow_down_right` | 16x16 | Nicht aktuell verwendet | Pfeil nach unten rechts (für Trends) |

**Verwendungskontext Sunrise/Sunset:**
- **Seite 2 (HEUTE):** `sunrise` zeigt die Sonnenaufgangszeit, `sunset` die Sonnenuntergangszeit
- **Seite 3 (HEUTE - Details):** `sunrise` wird auch verwendet um die Sonnenscheindauer in Stunden anzuzeigen

---

### Niederschlags-Icon

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `rain` | 16x16 | HEUTE - Details (Seite 3) | Niederschlag (Regen/Schnee) - wird angezeigt wenn Niederschlag >0mm |

**Verwendungskontext:** In der "HEUTE - Details"-Ansicht (Seite 3) wird dieses Icon neben der Niederschlagsmenge angezeigt, wenn die Gesamtniederschlagsmenge (Regen + Schnee) größer als 0mm ist.

---

### Unbekannt/Platzhalter-Icon

| Icon-Name | Größe | Verwendung in Ansicht | Beschreibung |
|-----------|-------|----------------------|--------------|
| `unknown` | 16/24/48 | Alle Ansichten (Fallback) | Platzhalter-Icon - wird angezeigt wenn ein angefordertes Icon nicht gefunden wird oder bei Wolkendecke-Anzeige |

**Verwendungskontext:** Dient als Fallback für fehlende Icons und wird auch bei der Wolkendecke-Anzeige in "HEUTE - Details" (Seite 3) verwendet.

---

## WeatherModule Ansichten und Icon-Verwendung nach Seiten

### Seite 1: JETZT (Aktuelle Wetter-Ansicht)

**Zweck:** Zeigt das aktuelle Wetter mit allen wichtigen Echtzeit-Informationen.

**Verwendete Icons:**
- **1x Haupt-Wetter-Icon (48x48px):** Großes Wetter-Icon links, basierend auf aktuellem WMO-Code mit Tag/Nacht-Variante
  - Position: Links (x=10, y=9)
  - Beispiele: `clear`, `rain_moderate_night`, `partly_cloudy`

**Angezeigte Daten:**
- Aktueller Wetter-Icon (48x48)
- Temperatur (farbcodiert nach Klimaabweichung)
- Gefühlte Temperatur (farbcodiert)
- Luftfeuchtigkeit und Wolkendecke
- Windgeschwindigkeit
- Zeitstempel der Datenaktualisierung

---

### Seite 2: HEUTE (Teil 1 - Tagesüberblick)

**Zweck:** Zeigt wichtige Tageswerte und astronomische Zeiten.

**Verwendete Icons:**
- **1x `temp_hot` (16x16px):** Maximaltemperatur des Tages
  - Position: Links oben (x=10, y=14)
- **1x `temp_cold` (16x16px):** Minimaltemperatur des Tages
  - Position: Links mitte (x=10, y=30)
- **1x `sunrise` (16x16px):** Sonnenaufgangszeit
  - Position: Links unten (x=10, y=46)
- **1x `temp_moderate` (16x16px):** Durchschnittstemperatur des Tages
  - Position: Rechts oben (x=100, y=14)
- **1x `sunset` (16x16px):** Sonnenuntergangszeit
  - Position: Rechts mitte (x=100, y=30)
- **1x `uv_moderate` (16x16px):** UV-Index (nur wenn Daten verfügbar)
  - Position: Rechts unten (x=100, y=46)

---

### Seite 3: HEUTE - Details (Teil 2 - Detailansicht)

**Zweck:** Zeigt zusätzliche Wetterdetails des aktuellen Tages.

**Verwendete Icons:**
- **1x `unknown` (16x16px):** Wolkendecke-Prozentsatz
  - Position: Links oben (x=10, y=14)
- **1x `rain` (16x16px):** Niederschlagsmenge (nur wenn >0mm)
  - Position: Links mitte (x=10, y=30)
- **1x Wind-Icon (16x16px):** Dynamisch basierend auf Windgeschwindigkeit
  - Position: Links unten (x=10, y=46)
  - Varianten: `wind_calm`, `wind_light`, `wind_moderate`, `wind_strong`, `wind_storm`
- **1x Luftfeuchtigkeits-Icon (16x16px):** Dynamisch basierend auf Luftfeuchtigkeit
  - Position: Rechts oben (x=100, y=14)
  - Varianten: `humidity_low` (<40%), `humidity_moderate` (40-70%), `humidity_high` (>70%)
- **1x `sunrise` (16x16px):** Sonnenscheindauer in Stunden (nur wenn >0)
  - Position: Rechts mitte (x=100, y=30)

---

### Seite 4: NIEDERSCHLAG HEUTE (Niederschlags-Diagramm)

**Zweck:** Grafische Darstellung des Niederschlags über den Tag verteilt.

**Verwendete Icons:** Keine Icons, nur Diagramm-Visualisierung mit Achsenbeschriftungen

---

### Seite 5+: Stündliche Vorhersage (HOURLY FORECAST)

**Zweck:** Zeigt stündliche Wettervorhersagen (2 Stunden pro Seite).

**Verwendete Icons:**
- **2x Haupt-Wetter-Icons (24x24px):** Pro dargestellter Stunde ein Wetter-Icon
  - Position: Zentriert über jedem Stunden-Block
  - Basierend auf WMO-Code mit Tag/Nacht-Variante je nach Tageszeit

**Angezeigte Daten pro Stunde:**
- Wetter-Icon (24x24)
- Uhrzeit
- Temperatur (farbcodiert)
- Gefühlte Temperatur (farbcodiert)
- Regenwahrscheinlichkeit und Menge

---

### Seite 6+: Tägliche Vorhersage (DAILY FORECAST)

**Zweck:** Zeigt Tagesvorhersagen (3 Tage pro Seite, ab morgen).

**Verwendete Icons:**
- **3x Haupt-Wetter-Icons (24x24px):** Pro dargestelltem Tag ein Wetter-Icon
  - Position: Zentriert über jedem Tages-Block
  - Basierend auf WMO-Code, mit Tag/Nacht-Variante (Mittags-Zeit wird verwendet)

**Angezeigte Daten pro Tag:**
- Wetter-Icon (24x24)
- Tagesname (z.B. "Morgen", "Montag")
- Maximaltemperatur (farbcodiert)
- Minimaltemperatur (farbcodiert)
- Regenwahrscheinlichkeit

---

### Wetterwarnungs-Seite (Alert-Ansicht)

**Zweck:** Zeigt aktive Wetterwarnungen prominent an.

**Verwendete Icons:**
- **1x `warning_generic` (48x48px):** Großes Warnungs-Icon mit pulsierender Animation
  - Position: Links (x=10, y=9)
  - Hintergrund: Roter, gedimmter Hintergrund (0xF800 @ 50%)
  - Animation: Icon pulsiert in der Helligkeit

**Angezeigte Daten:**
- Warnungs-Icon (48x48, pulsierend)
- "WETTERWARNUNG" Titel (groß, fett)
- Event-Name (z.B. "Sturmwarnung")
- Gültigkeitszeitraum (Von/Bis Uhrzeit)

---

## Zusammenfassung Icon-Größen

Das WeatherModule verwendet drei verschiedene Icon-Größen für unterschiedliche Kontexte:

- **48x48 Pixel:** Hauptansicht (JETZT-Seite) und Wetterwarnungen - maximale Sichtbarkeit
- **24x24 Pixel:** Stündliche und tägliche Vorhersagen - kompakte Darstellung mehrerer Elemente
- **16x16 Pixel:** Detail-Informationen und Zusatz-Icons - platzsparend für Sekundär-Informationen

Alle Icons werden beim ersten Laden automatisch in die benötigten Größen skaliert und im PSRAM-Cache gespeichert, um die Render-Performance zu optimieren.

---

## Anmerkungen zur Icon-Verwaltung

1. **Tag/Nacht-Transformation:** Haupt-Wetter-Icons, die keine explizite Nacht-Variante haben, werden automatisch abgedunkelt für die Nachtdarstellung.

2. **Icon-Cache:** Alle skalierten Icons werden im PSRAM zwischengespeichert, um wiederholtes Skalieren zu vermeiden.

3. **Fallback-Mechanismus:** 
   - Wenn ein angefordertes Icon nicht gefunden wird, wird zunächst die Tag-Version versucht
   - Wenn auch diese fehlt, wird das `unknown` Icon verwendet
   - Fehlende Icons werden nur einmal pro Session geloggt

4. **Icon-Registry:** Die Icons sind in einem zentralen `WeatherIconSet` registriert, das sowohl Main- als auch Special-Icons verwaltet.

5. **Bilineare Skalierung:** Alle Icons werden mit bilinearer Interpolation skaliert, um eine hohe Bildqualität bei verschiedenen Größen zu gewährleisten.

---

## Technische Details

- **Icon-Format:** RGB888 (3 Bytes pro Pixel)
- **Speicherort:** PROGMEM (Flash-Speicher) für Original-Icons, PSRAM für Cache
- **Basis-Größe:** 48x48 Pixel (Originalgröße in PROGMEM)
- **Skalierungs-Algorithmus:** Bilineare Interpolation für glatte Verkleinerung
- **Farbraum-Konvertierung:** RGB888 → RGB565 für Display-Ausgabe

---

*Stand: 2025-11-17*
*Dokumentation für Panelclock ESP32 LED Matrix Projekt*
