# Finale Spezifikation für PanelManager (Neustart von Grund auf)

## 1. Grundphilosophie & Datenstrukturen

   Zustandslosigkeit Der `PanelManager` verwendet keine Member-Variable, um sich den Index des zuletzt gelaufenen Moduls zu merken (kein `_currentPlaylistIndex`). Der Zustand wird zur Laufzeit aus den Listen abgeleitet.
   Drei Kernlisten
    1.  `_moduleCatalog` Eine unveränderliche Master-Liste aller registrierten Module. Dient als Quelle für `periodicTick` und zum Erstellen neuer `PlaylistEntry`-Objekte.
    2.  `_playlist` Die Liste für die normale Rotation. Sie enthält auch temporär eingefügte One-Shot-Module.
    3.  `_interruptQueue` Eine priorisierte Liste für unterbrechende Module (Anrufe, Alarme). Diese Liste hat immer Vorrang.

## 2. Die vier zentralen Funktionen und ihre Aufgaben

### A) `handlePriorityRequest(mod, prio, uid)` Die Eingangstür

   Zweck Dies ist die einzige Funktion, die von Modulen aufgerufen wird, um eine Anzeige außerhalb der normalen Rotation anzufordern.
   Ablauf
    1.  Duplikat-Prüfung Verhindert, dass dieselbe Anfrage (gleiches Modul, gleiche UID) mehrfach hinzugefügt wird.
    2.  Fall 1 `priority == PriorityPlayNext` (One-Shot-Modul)
           Erstellt einen neuen `PlaylistEntry` mit dem Flag `isOneShot = true`.
           Findet das aktuell laufende Modul in der `_playlist`.
           Fügt den neuen One-Shot-Eintrag direkt nach dem laufenden Modul in die `_playlist` ein.
    3.  Fall 2 `priority  PriorityPlayNext` (Interrupt)
           Erstellt einen neuen `PlaylistEntry`.
           WICHTIG Wenn dies der allererste Interrupt ist (die `_interruptQueue` war zuvor leer), muss das aktuell in der `_playlist` laufende Modul pausiert werden. Dazu wird dessen `isPaused`-Flag auf `true` gesetzt.
           Fügt den neuen Eintrag zur `_interruptQueue` hinzu.
           Sortiert die `_interruptQueue` absteigend nach Priorität, damit das wichtigste Modul immer an erster Stelle (`_interruptQueue.front()`) steht.

### B) `tick()` Der Watchdog

   Zweck Reine Überwachung. Stößt nur noch Aktionen an.
   Ablauf
    1.  Ruft `periodicTick()` für alle Module in `_moduleCatalog` auf.
    2.  Findet das aktive Modul (`activeEntry`).
    3.  Wenn kein Modul aktiv ist, ruft es einmal `switchNextModule()` auf, um die Kette zu starten.
    4.  Wenn ein Modul aktiv und nicht pausiert ist
           Ruft `tick()` und (alle ~100ms) `logicTick()` auf.
           Zieht die `delta`-Zeit von der `remainingTimeMs` ab.
    5.  Wenn das aktive Modul fertig ist (`isFinished()` oder Timeout), ruft es nur noch `switchNextModule()` auf.

### C) `switchNextModule()` Der Master of Switching

   Zweck Führt den gesamten Umschaltvorgang atomar durch. Hat keine Parameter.
   Ablauf
    1.  Phase 1 Deaktivierung (falls nötig)
           Findet das aktuell laufende Modul (`currentRunning`).
           Wenn ein Modul lief
               Setzt `currentRunning-isRunning = false`.
               Ermittelt, ob es ein Interrupt oder Playlist-Modul war.
               Wenn Interrupt Entfernt es aus der `_interruptQueue`. Wenn die Queue nun leer ist, wird das pausierte Playlist-Modul gesucht, `isPaused` auf `false` gesetzt, und die Funktion endet sofort (das reaktivierte Modul läuft weiter).
               Wenn Playlist-Modul Speichert seinen Index in einer lokalen Variable (`lastActivePlaylistIndex`). Wenn `isOneShot`, wird es aus der `_playlist` entfernt.

    2.  Phase 2 Aktivierung
           Priorität 1 Interrupts. Ist die `_interruptQueue` nicht leer Wenn ja, aktiviere `_interruptQueue.front()`. Funktion endet.
           Priorität 2 Playlist. Wenn keine Interrupts anstehen
               Startet die Suche nach dem nächsten Modul bei `(lastActivePlaylistIndex + 1)`.
               Das erste Modul, das die Kriterien (`!isDisabled`, `isEnabled()`, etc.) erfüllt, wird aktiviert. Funktion endet.

### D) `findActiveEntry()` Der Spürhund

   Zweck Einfache, dumme Suche nach dem `isRunning`-Flag.
   Ablauf
    1.  Durchsucht `_interruptQueue`. Findet es ein Modul mit `isRunning == true`, gibt es einen Zeiger darauf zurück.
    2.  Wenn nicht, durchsucht es `_playlist`. Findet es ein Modul mit `isRunning == true`, gibt es einen Zeiger darauf zurück. (Ein `isPaused`-Modul gilt weiterhin als `isRunning`, da es nur unterbrochen ist).
    3.  Wenn nichts gefunden wird, gibt es `nullptr` zurück.