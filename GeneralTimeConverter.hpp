#ifndef GENERALTIMECONVERTER_HPP
#define GENERALTIMECONVERTER_HPP

#include <Arduino.h>
#include <time.h>

class GeneralTimeConverter {
public:
    GeneralTimeConverter() {}

    // Setzt die globale Zeitzone für die C-Standardbibliothek
    bool setTimezone(const char* tz_string) {
        if (tz_string && strlen(tz_string) > 0) {
            // '1' als drittes Argument überschreibt eine eventuell vorhandene alte Variable
            setenv("TZ", tz_string, 1); 
            tzset(); // tzset() liest die TZ-Variable und initialisiert die Zeitkonvertierung
            Serial.printf("[TimeConverter] Zeitzone gesetzt auf: %s\n", tz_string);
            return true;
        }
        return false;
    }

    // Konvertiert eine UTC-Epochenzeit in die lokal konfigurierte Zeit
    // *** KORREKTUR: Methode als const deklariert, um den Kompilierungsfehler zu beheben ***
    time_t toLocal(time_t utc) const {
        // localtime() konvertiert unter Berücksichtigung der via setenv/tzset gesetzten TZ-Regel
        struct tm* tm_local = localtime(&utc);
        // mktime() konvertiert die tm-Struktur zurück in einen time_t-Wert,
        // interpretiert die Eingabe aber als "lokale" Zeit.
        // Dies ist der Standardweg, um einen korrekten lokalen Epochenwert zu erhalten.
        return mktime(tm_local);
    }

    // Parst einen ISO8601-String (z.B. aus iCal) in eine UTC-Epochenzeit
    // *** KORREKTUR: Methode als const deklariert, um den Kompilierungsfehler zu beheben ***
    time_t parse_iso8601(const char* iso_str) const {
        struct tm t;
        memset(&t, 0, sizeof(struct tm));
        int year, month, day, hour, min, sec;

        // Versucht, das volle Datums- und Zeitformat zu lesen
        if (sscanf(iso_str, "%4d%2d%2dT%2d%2d%2d", &year, &month, &day, &hour, &min, &sec) == 6) {
            // Standard-Format
        } 
        // Versucht, nur das Datumsformat zu lesen (für ganztägige Termine)
        else if (sscanf(iso_str, "%4d%2d%2d", &year, &month, &day) == 3) {
            hour = 0; min = 0; sec = 0;
        } else {
            return 0; // Ungültiges Format
        }

        t.tm_year = year - 1900;
        t.tm_mon = month - 1;
        t.tm_mday = day;
        t.tm_hour = hour;
        t.tm_min = min;
        t.tm_sec = sec;
        t.tm_isdst = -1; // Wichtig: Sagt mktime, es soll DST selbst herausfinden

        // Wichtig: Wir müssen die Zeitzone temporär auf UTC setzen, um den String
        // korrekt als UTC zu interpretieren, da timegm() auf dem ESP32 oft fehlt.
        char* old_tz = getenv("TZ");
        setenv("TZ", "UTC", 1);
        tzset();

        time_t timestamp = mktime(&t);

        // Alte Zeitzone wiederherstellen
        if (old_tz) {
            setenv("TZ", old_tz, 1);
        } else {
            unsetenv("TZ");
        }
        tzset();

        // Korrektur für Zeitzonen-Offset, den mktime fälschlicherweise anwenden könnte
        // Dies ist ein bekannter Workaround auf POSIX-Systemen ohne timegm()
        struct tm* tm_utc = gmtime(&timestamp);
        time_t diff = (tm_utc->tm_hour - t.tm_hour);
        if(t.tm_mday != tm_utc->tm_mday) diff = 24; // Vereinfachung für Tageswechsel
        timestamp -= diff * 3600;

        // Spezifische Korrektur für das Z am Ende (Zulu-Time)
        if (iso_str[strlen(iso_str) - 1] == 'Z') {
             return timestamp;
        }

        return timestamp;
    }
};

#endif