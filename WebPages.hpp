#ifndef WEB_PAGES_HPP
#define WEB_PAGES_HPP

#include <Arduino.h> // Erforderlich für PROGMEM

const char HTML_PAGE_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"><meta charset="UTF-8">
<title>Panelclock Config</title><style>body{font-family:sans-serif;background:#222;color:#eee;}
.container{max-width:800px;margin:0 auto;padding:20px;}h1,h2{color:#4CAF50;border-bottom:1px solid #444;padding-bottom:10px;}
label{display:block;margin-top:15px;color:#bbb;}input[type="text"],input[type="password"],input[type="number"],select{width:100%;padding:8px;margin-top:5px;border-radius:4px;border:1px solid #555;box-sizing:border-box;background-color:#333;color:#eee;}
input[type="checkbox"]{margin-right:10px;transform:scale(1.5);}
input[type="color"]{width:100%;height:40px;padding:5px;margin-top:5px;border-radius:4px;border:1px solid #555;box-sizing:border-box;}
input[type="submit"],.button{background-color:#4CAF50;color:white;padding:14px 20px;margin-top:20px;border:none;border-radius:4px;cursor:pointer;width:100%;font-size:16px;text-align:center;text-decoration:none;display:inline-block;}
input[type="submit"]:hover,.button:hover{background-color:#45a049;}
.button-danger{background-color:#f44336;}.button-danger:hover{background-color:#da190b;}
.group{background:#2a2a2a;border:1px solid #444;border-radius:8px;padding:20px;margin-top:20px;}
.footer-link{margin-top:20px;text-align:center;}.footer-link a{color:#4CAF50;text-decoration:none;}
hr{border:0;border-top:1px solid #444;margin:20px 0;}
table{width:100%;border-collapse:collapse;margin-top:20px;}th,td{border:1px solid #555;padding:8px;text-align:left;}th{background-color:#4CAF50;color:black;}
.tab{overflow:hidden;border-bottom:1px solid #444;margin-bottom:20px;}
.tab button{background-color:inherit;float:left;border:none;outline:none;cursor:pointer;padding:14px 16px;transition:0.3s;color:#bbb;font-size:17px;}
.tab button:hover{background-color:#444;}
.tab button.active{color:#4CAF50;border-bottom:2px solid #4CAF50;}
.tabcontent{display:none;}
.subtablinks{background-color:inherit;float:left;border:none;outline:none;cursor:pointer;padding:10px 14px;transition:0.3s;color:#bbb;font-size:15px;}
.subtablinks:hover{background-color:#333;}
.subtablinks.active{color:#4CAF50;border-bottom:2px solid #4CAF50;}
.subtabcontent{display:none;}
a {color:#4CAF50;}
</style></head><body><div class="container">
)rawliteral";
const char HTML_PAGE_FOOTER[] PROGMEM = R"rawliteral(</div></body></html>)rawliteral";

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<h1>Panelclock Hauptmen&uuml;</h1>
<p style="color:#bbb;font-size:14px;margin-top:-10px;">Version {version}</p>
<a href="/config_base" class="button button-danger">Grundkonfiguration (mit Neustart)</a>
<a href="/config_location" class="button">Mein Standort</a>
<a href="/config_modules" class="button">Anzeige-Module (Live-Update)</a>
<a href="/config_hardware" class="button">Optionale Hardware</a>
<!-- File manager button added to main menu -->
<a href="/fs" class="button">Dateimanager</a>
<a href="/backup" class="button" style="background-color:#FF9800;">Backup & Wiederherstellung</a>
<a href="/firmware" class="button button-danger">Firmware Update</a>
<hr>
<a href="/stream" class="button" style="background-color:#2196F3;">Live-Stream & Debug</a>
<a href="/debug" class="button" style="background-color:#555;">Debug Daten</a>
)rawliteral";

const char HTML_CONFIG_BASE[] PROGMEM = R"rawliteral(
<h2>Grundkonfiguration</h2>
<form action="/save_base" method="POST">
<div class="group">
    <h3>Allgemein &amp; WLAN</h3>
    <label for="hostname">Hostname</label><input type="text" id="hostname" name="hostname" value="{hostname}">
    <label for="ssid">SSID</label><input type="text" id="ssid" name="ssid" value="{ssid}">
    <label for="password">Passwort</label><input type="password" id="password" name="password" value="{password}">
    <label for="otaPassword">OTA Update Passwort</label><input type="password" id="otaPassword" name="otaPassword" value="{otaPassword}">
</div>
<hr>
<div class="group">
    <h3>Hardware Pin-Belegung (Display)</h3>
    <p style="color:#ff8c00;">Warnung: Falsche Werte hier erfordern einen Neustart!</p>
    <label for="R1">Pin R1</label><input type="number" id="R1" name="R1" value="{R1}">
    <label for="G1">Pin G1</label><input type="number" id="G1" name="G1" value="{G1}">
    <label for="B1">Pin B1</label><input type="number" id="B1" name="B1" value="{B1}">
    <label for="R2">Pin R2</label><input type="number" id="R2" name="R2" value="{R2}">
    <label for="G2">Pin G2</label><input type="number" id="G2" name="G2" value="{G2}">
    <label for="B2">Pin B2</label><input type="number" id="B2" name="B2" value="{B2}">
    <label for="A">Pin A</label><input type="number" id="A" name="A" value="{A}">
    <label for="B">Pin B</label><input type="number" id="B" name="B" value="{B}">
    <label for="C">Pin C</label><input type="number" id="C" name="C" value="{C}">
    <label for="D">Pin D</label><input type="number" id="D" name="D" value="{D}">
    <label for="E">Pin E</label><input type="number" id="E" name="E" value="{E}">
    <label for="CLK">Pin CLK</label><input type="number" id="CLK" name="CLK" value="{CLK}">
    <label for="LAT">Pin LAT</label><input type="number" id="LAT" name="LAT" value="{LAT}">
    <label for="OE">Pin OE</label><input type="number" id="OE" name="OE" value="{OE}">
</div>
<input type="submit" value="Speichern &amp; Neustarten" class="button-danger">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_CONFIG_LOCATION[] PROGMEM = R"rawliteral(
<link rel="stylesheet" href="https://unpkg.com/leaflet@1.9.4/dist/leaflet.css" />
<script src="https://unpkg.com/leaflet@1.9.4/dist/leaflet.js"></script>
<style>#map{height:400px;width:100%;border-radius:8px;margin-top:15px;}</style>
<h2>Mein Standort</h2>
<p>Dieser Standort wird f&uuml;r die Umkreissuche von Tankstellen und zuk&uuml;nftig f&uuml;r Wetterdaten verwendet.</p>
<form action="/save_location" method="POST">
<div class="group">
    <h3>Zeitzone</h3>
    <label for="timezone">Zeitzone</label><select id="timezone" name="timezone">{tz_options}</select>
</div>
<div class="group">
    <h3>Standort festlegen</h3>
    <label for="address">Adresse suchen</label>
    <div style="display:flex;">
        <input type="text" id="address" placeholder="z.B. Willy-Brandt-Stra&szlig;e 1, Berlin" style="flex-grow:1;margin-top:0;">
        <button type="button" onclick="searchAddress()" class="button" style="width:auto;margin-top:0;margin-left:10px;">Suchen</button>
    </div>
    <div id="map"></div>
    <label for="latitude">Breitengrad (Latitude)</label>
    <input type="number" step="any" id="latitude" name="latitude" value="{latitude}" required>
    <label for="longitude">L&auml;ngengrad (Longitude)</label>
    <input type="number" step="any" id="longitude" name="longitude" value="{longitude}" required>
</div>
<input type="submit" value="Standort speichern">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>

<script>
    var latInput = document.getElementById('latitude');
    var lonInput = document.getElementById('longitude');
    var map = L.map('map').setView([latInput.value, lonInput.value], 13);
    var marker = L.marker([latInput.value, lonInput.value], {draggable: true}).addTo(map);

    L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
    }).addTo(map);

    marker.on('dragend', function(event){
        var position = marker.getLatLng();
        latInput.value = position.lat.toFixed(6);
        lonInput.value = position.lng.toFixed(6);
    });

    function searchAddress() {
        var address = document.getElementById('address').value;
        if (!address) return;
        fetch('https://nominatim.openstreetmap.org/search?format=json&q=' + encodeURIComponent(address))
            .then(response => response.json())
            .then(data => {
                if (data && data.length > 0) {
                    var lat = data[0].lat;
                    var lon = data[0].lon;
                    latInput.value = parseFloat(lat).toFixed(6);
                    lonInput.value = parseFloat(lon).toFixed(6);
                    var newLatLng = new L.LatLng(lat, lon);
                    marker.setLatLng(newLatLng);
                    map.setView(newLatLng, 15);
                } else {
                    alert('Adresse nicht gefunden.');
                }
            })
            .catch(error => {
                console.error('Error:', error);
                alert('Fehler bei der Adresssuche.');
            });
    }
</script>
)rawliteral";


const char HTML_CONFIG_MODULES[] PROGMEM = R"rawliteral(
<h2>Anzeige-Module</h2>

<div class="tab">
  <button class="tablinks" onclick="openTab(event, 'Darts')" id="defaultOpen">Darts</button>
  <button class="tablinks" onclick="openTab(event, 'Freizeitpark')">Freizeitparks</button>
  <button class="tablinks" onclick="openTab(event, 'Fritzbox')">Fritz!Box</button>
  <button class="tablinks" onclick="openTab(event, 'Kalender')">Kalender</button>
  <button class="tablinks" onclick="openTab(event, 'KurioseFeiertage')">Kuriose Feiertage</button>
  <button class="tablinks" onclick="openTab(event, 'Animationen')">Animationen</button>
  <button class="tablinks" onclick="openTab(event, 'Tankstellen')">Tankstellen</button>
  <button class="tablinks" onclick="openTab(event, 'Wetter')">Wetter</button>
  <button class="tablinks" onclick="openTab(event, 'Diverses')">Diverses</button>
</div>

<form action="/save_modules" method="POST" onsubmit="collectStationIds()">

<div id="Darts" class="tabcontent">
    <div class="group">
        <h3>Darts-Ranglisten</h3>
        <input type="checkbox" id="dartsOomEnabled" name="dartsOomEnabled" {dartsOomEnabled_checked}><label for="dartsOomEnabled" style="display:inline;">Order of Merit anzeigen</label><br>
        <input type="checkbox" id="dartsProTourEnabled" name="dartsProTourEnabled" {dartsProTourEnabled_checked}><label for="dartsProTourEnabled" style="display:inline;">Pro Tour anzeigen</label><br>
        <label for="dartsDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="dartsDisplaySec" name="dartsDisplaySec" value="{dartsDisplaySec}">
        <label for="trackedDartsPlayers">Lieblingsspieler (kommagetrennt)</label><input type="text" id="trackedDartsPlayers" name="trackedDartsPlayers" value="{trackedDartsPlayers}">
    </div>
    <div class="group">
        <h3>SofaScore Live-Spiele</h3>
        <p>Zeigt Live-Darts-Spiele von SofaScore mit Statistiken (Average, 180s, Checkout%). Live-Spiele können unterbrechend angezeigt werden.</p>
        <input type="checkbox" id="dartsSofascoreEnabled" name="dartsSofascoreEnabled" {dartsSofascoreEnabled_checked}><label for="dartsSofascoreEnabled" style="display:inline;">SofaScore Live-Spiele aktivieren</label><br>
        <label for="dartsSofascoreFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="dartsSofascoreFetchIntervalMin" name="dartsSofascoreFetchIntervalMin" value="{dartsSofascoreFetchIntervalMin}" min="1">
        <label for="dartsSofascoreDisplaySec">Anzeigedauer pro Spiel (Sekunden)</label><input type="number" id="dartsSofascoreDisplaySec" name="dartsSofascoreDisplaySec" value="{dartsSofascoreDisplaySec}" min="5">
        
        <label>Turniere auswählen</label>
        <button type="button" onclick="loadSofascoreTournaments()" style="margin-bottom:10px;">Turnier-Liste laden</button>
        <div id="sofascoreTournamentsContainer" style="max-height:300px; overflow-y:auto; border:1px solid #ccc; padding:10px; margin-bottom:10px; display:none;">
            <div id="sofascoreTournamentsList">Lade Turniere...</div>
        </div>
        
        <label for="dartsSofascoreTournamentIds">Turnier-IDs (kommagetrennt, leer = alle)</label>
        <input type="text" id="dartsSofascoreTournamentIds" name="dartsSofascoreTournamentIds" value="{dartsSofascoreTournamentIds}" placeholder="z.B. 17,23,34">
        
        <input type="checkbox" id="dartsSofascoreFullscreen" name="dartsSofascoreFullscreen" {dartsSofascoreFullscreen_checked}><label for="dartsSofascoreFullscreen" style="display:inline;">Vollbild-Modus für Live-Spiele</label><br>
        <input type="checkbox" id="dartsSofascoreInterruptOnLive" name="dartsSofascoreInterruptOnLive" {dartsSofascoreInterruptOnLive_checked}><label for="dartsSofascoreInterruptOnLive" style="display:inline;">Live-Spiele unterbrechend anzeigen</label><br>
        <label for="dartsSofascorePlayNextMinutes">PlayNext-Intervall (Minuten, 0=aus):</label> <input type="number" id="dartsSofascorePlayNextMinutes" name="dartsSofascorePlayNextMinutes" value="{dartsSofascorePlayNextMinutes}" min="0" max="1440" style="width:60px;"><br>
    </div>
</div>

<div id="Wetter" class="tabcontent">
    <div class="group">
        <h3>Allgemein</h3>
        <p>Verwendet OpenWeatherMap. Ein kostenloser API-Key ist erforderlich. Der <a href="/config_location">Standort</a> muss konfiguriert sein.</p>
        <input type="checkbox" id="weatherEnabled" name="weatherEnabled" {weatherEnabled_checked}><label for="weatherEnabled" style="display:inline;">Wetter-Anzeige aktivieren</label><br>
        <label for="weatherApiKey">OpenWeatherMap API Key</label><input type="text" id="weatherApiKey" name="weatherApiKey" value="{weatherApiKey}">
        <label for="weatherFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="weatherFetchIntervalMin" name="weatherFetchIntervalMin" value="{weatherFetchIntervalMin}" min="5">
    </div>
    <div class="group">
        <h3>Seiten-Management</h3>
        <label for="weatherDisplaySec">Anzeigedauer pro Seite (Sekunden)</label><input type="number" id="weatherDisplaySec" name="weatherDisplaySec" value="{weatherDisplaySec}" min="1">
        <hr>
        <input type="checkbox" id="weatherShowCurrent" name="weatherShowCurrent" {weatherShowCurrent_checked}><label for="weatherShowCurrent" style="display:inline;">Aktuelles Wetter anzeigen</label><br>
        <input type="checkbox" id="weatherShowHourly" name="weatherShowHourly" {weatherShowHourly_checked}><label for="weatherShowHourly" style="display:inline;">Stunden-Vorschau anzeigen</label><br>
        <input type="checkbox" id="weatherShowDaily" name="weatherShowDaily" {weatherShowDaily_checked}><label for="weatherShowDaily" style="display:inline;">Tages-Vorschau anzeigen</label><br>
        <label for="weatherDailyForecastDays">Anzahl der Vorschau-Tage (1-16)</label><input type="number" id="weatherDailyForecastDays" name="weatherDailyForecastDays" value="{weatherDailyForecastDays}" min="1" max="16">
    </div>
    <div class="group">
        <h3>Stunden-Vorschau Details</h3>
        <label for="weatherHourlyHours">Anzahl der Vorschau-Stunden (1-48)</label><input type="number" id="weatherHourlyHours" name="weatherHourlyHours" value="{weatherHourlyHours}" min="1" max="48">
    </div>
    <div class="group">
        <h3>Offizielle Wetter-Warnungen</h3>
        <input type="checkbox" id="weatherAlertsEnabled" name="weatherAlertsEnabled" {weatherAlertsEnabled_checked}><label for="weatherAlertsEnabled" style="display:inline;">Warnungen anzeigen (unterbricht normale Anzeige)</label><br>
        <label for="weatherAlertsDisplaySec">Anzeigedauer der Warnung (Sekunden)</label><input type="number" id="weatherAlertsDisplaySec" name="weatherAlertsDisplaySec" value="{weatherAlertsDisplaySec}" min="1">
        <label for="weatherAlertsRepeatMin">Wiederholung der Warnung (Minuten)</label><input type="number" id="weatherAlertsRepeatMin" name="weatherAlertsRepeatMin" value="{weatherAlertsRepeatMin}" min="0">
    </div>
</div>

<div id="Freizeitpark" class="tabcontent">
    <div class="group">
        <h3>Freizeitpark-Wartezeiten</h3>
        <p>Zeigt Wartezeiten von Attraktionen aus ausgew&auml;hlten Freizeitparks. Die Daten werden &uuml;ber die API von <a href="https://www.wartezeiten.app/" target="_blank">Wartezeiten.APP</a> abgerufen.</p>
        <input type="checkbox" id="themeParkEnabled" name="themeParkEnabled" {themeParkEnabled_checked}><label for="themeParkEnabled" style="display:inline;">Freizeitpark-Anzeige aktivieren</label><br>
        <label for="themeParkFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="themeParkFetchIntervalMin" name="themeParkFetchIntervalMin" value="{themeParkFetchIntervalMin}" min="5">
        <label for="themeParkDisplaySec">Anzeigedauer pro Park (Sekunden)</label><input type="number" id="themeParkDisplaySec" name="themeParkDisplaySec" value="{themeParkDisplaySec}" min="5">
    </div>
    <div class="group">
        <h4>Freizeitpark-Auswahl</h4>
        <p>W&auml;hle die Freizeitparks aus, deren Wartezeiten angezeigt werden sollen.</p>
        <button type="button" onclick="loadThemeParks()" class="button" style="background-color:#007bff;">Verf&uuml;gbare Parks laden</button>
        <div id="themepark_list" style="margin-top:20px;"></div>
        <input type="hidden" id="themeParkIds" name="themeParkIds" value="{themeParkIds}">
    </div>
</div>

<div id="Tankstellen" class="tabcontent">
    <div class="group">
        <h3>Tankstellen-Anzeige</h3>
        <h4>API &amp; Allgemein</h4>
        <label for="tankerApiKey">Tankerk&ouml;nig API-Key</label><input type="text" id="tankerApiKey" name="tankerApiKey" value="{tankerApiKey}">
        <label for="stationFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="stationFetchIntervalMin" name="stationFetchIntervalMin" value="{stationFetchIntervalMin}">
        <label for="stationDisplaySec">Anzeigedauer pro Tankstelle (Sekunden)</label><input type="number" id="stationDisplaySec" name="stationDisplaySec" value="{stationDisplaySec}">
    </div>
    <div class="group">
        <h4>Berechnungs-Parameter</h4>
        <label for="movingAverageDays">Tage f&uuml;r Durchschnittspreis (Min/Max)</label><input type="number" id="movingAverageDays" name="movingAverageDays" value="{movingAverageDays}">
        <label for="trendAnalysisDays">Tage f&uuml;r Trend-Analyse</label><input type="number" id="trendAnalysisDays" name="trendAnalysisDays" value="{trendAnalysisDays}">
    </div>
    <div class="group">
        <h4>Tankstellen-Suche &amp; Auswahl</h4>
        <p>Hier kannst du Tankstellen im Umkreis deines <a href="/config_location">Standorts</a> suchen und f&uuml;r die Anzeige ausw&auml;hlen.</p>
        <label for="searchRadius">Suchradius (km)</label><input type="number" id="searchRadius" value="5">
        <label for="searchSort">Sortieren nach</label>
        <select id="searchSort">
            <option value="dist" selected>Entfernung</option>
            <option value="price">Preis (g&uuml;nstigster E5)</option>
        </select>
        <button type="button" onclick="searchStations()" class="button" style="background-color:#007bff;">Tankstellen im Umkreis suchen</button>
        <div id="station_results" style="margin-top:20px;"></div>
        <input type="hidden" id="tankerkoenigStationIds" name="tankerkoenigStationIds" value="{tankerkoenigStationIds}">
    </div>
</div>

<div id="Kalender" class="tabcontent">
    <div class="group">
        <h3>Kalender-Anzeige</h3>
        <label for="icsUrl">iCal URL (.ics)</label><input type="text" id="icsUrl" name="icsUrl" value="{icsUrl}">
        <label for="calendarFetchIntervalMin">Abrufintervall (Minuten)</label><input type="number" id="calendarFetchIntervalMin" name="calendarFetchIntervalMin" value="{calendarFetchIntervalMin}">
        <label for="calendarDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="calendarDisplaySec" name="calendarDisplaySec" value="{calendarDisplaySec}">
        <label for="calendarDateColor">Farbe Datum</label><input type="color" id="calendarDateColor" name="calendarDateColor" value="{calendarDateColor}">
        <label for="calendarTextColor">Farbe Text</label><input type="color" id="calendarTextColor" name="calendarTextColor" value="{calendarTextColor}">
        <hr>
        <h4>Urgent-View Einstellungen</h4>
        <label for="calendarFastBlinkHours">Schneller Blinken (Stunden vor Termin)</label><input type="number" id="calendarFastBlinkHours" name="calendarFastBlinkHours" min="0" value="{calendarFastBlinkHours}">
        <label for="calendarUrgentThresholdHours">Ab wann Urgent-View zeigen (Stunden vor Termin)</label><input type="number" id="calendarUrgentThresholdHours" name="calendarUrgentThresholdHours" min="0" value="{calendarUrgentThresholdHours}">
        <label for="calendarUrgentDurationSec">Dauer Urgent-View (Sekunden)</label><input type="number" id="calendarUrgentDurationSec" name="calendarUrgentDurationSec" min="1" value="{calendarUrgentDurationSec}">
        <label for="calendarUrgentRepeatMin">Wiederholung (Minuten) bis nächste Anzeige</label><input type="number" id="calendarUrgentRepeatMin" name="calendarUrgentRepeatMin" min="0" value="{calendarUrgentRepeatMin}">
    </div>
</div>

<div id="Fritzbox" class="tabcontent">
    <div class="group">
        <h3>Fritz!Box Anrufanzeige</h3>
        <p style="color:#ffc107;">Hinweis: Damit dies funktioniert, muss der "Call Monitor" auf der Fritz!Box einmalig per Telefon aktiviert werden. W&auml;hlen Sie dazu: <strong>#96*5*</strong></p>
        <input type="checkbox" id="fritzboxEnabled" name="fritzboxEnabled" {fritzboxEnabled_checked}><label for="fritzboxEnabled" style="display:inline;">Anrufanzeige aktivieren</label><br><br>
        <label for="fritzboxIp">Fritz!Box IP-Adresse (leer lassen f&uuml;r autom. Erkennung)</label><input type="text" id="fritzboxIp" name="fritzboxIp" value="{fritzboxIp}">
    </div>
</div>

<div id="KurioseFeiertage" class="tabcontent">
    <div class="group">
        <h3>Kuriose Feiertage</h3>
        <input type="checkbox" id="curiousHolidaysEnabled" name="curiousHolidaysEnabled" {curiousHolidaysEnabled_checked}><label for="curiousHolidaysEnabled" style="display:inline;">Kuriose Feiertage aktivieren</label><br>
        <label for="curiousHolidaysDisplaySec">Anzeigedauer pro Feiertag (Sekunden)</label><input type="number" id="curiousHolidaysDisplaySec" name="curiousHolidaysDisplaySec" value="{curiousHolidaysDisplaySec}" min="1">
        <p style="color:#bbb; margin-top:10px;">Zeigt t&auml;glich verschiedene kuriose Feiertage aus Deutschland an.</p>
    </div>
</div>

<div id="Animationen" class="tabcontent">
    <h2>Weihnachtsanimationen</h2>
    <p style="color:#bbb; margin-bottom:20px;">Verschiedene Animationen für die Weihnachtszeit. Wenn mehrere aktiviert sind, wechseln sie ab.</p>
    
    <!-- Sub-Tabs für verschiedene Animationen -->
    <div class="tab" style="border-bottom:1px solid #555; margin-bottom:15px;">
        <button type="button" class="subtablinks" onclick="openSubTab(event, 'SubAdventskranz')" id="defaultAnimOpen">Adventskranz</button>
        <button type="button" class="subtablinks" onclick="openSubTab(event, 'SubWeihnachtsbaum')">Weihnachtsbaum</button>
        <button type="button" class="subtablinks" onclick="openSubTab(event, 'SubKamin')">Kamin</button>
        <button type="button" class="subtablinks" onclick="openSubTab(event, 'SubAllgemein')">Allgemein</button>
    </div>
    
    <!-- Adventskranz Sub-Tab -->
    <div id="SubAdventskranz" class="subtabcontent">
        <div class="group">
            <h3>Adventskranz</h3>
            <input type="checkbox" id="adventWreathEnabled" name="adventWreathEnabled" {adventWreathEnabled_checked}><label for="adventWreathEnabled" style="display:inline;">Adventskranz aktivieren</label><br>
            
            <label for="adventWreathColorMode">Kerzenfarben</label>
            <select id="adventWreathColorMode" name="adventWreathColorMode" onchange="toggleCustomColors()">
                <option value="0" {adventWreathColorMode0_selected}>Traditionell (violett/rosa)</option>
                <option value="1" {adventWreathColorMode1_selected}>Bunt (rot/gold/gr&uuml;n/wei&szlig;)</option>
                <option value="2" {adventWreathColorMode2_selected}>Eigene Farben</option>
            </select>
            
            <div id="customColorsDiv" style="display:none;">
                <label>Eigene Kerzenfarben (4 Hex-Farben)</label>
                <div style="display:flex;gap:10px;margin-top:5px;">
                    <div><label style="font-size:12px;">Kerze 1</label><input type="color" id="candleColor1" value="{candleColor1}"></div>
                    <div><label style="font-size:12px;">Kerze 2</label><input type="color" id="candleColor2" value="{candleColor2}"></div>
                    <div><label style="font-size:12px;">Kerze 3</label><input type="color" id="candleColor3" value="{candleColor3}"></div>
                    <div><label style="font-size:12px;">Kerze 4</label><input type="color" id="candleColor4" value="{candleColor4}"></div>
                </div>
                <input type="hidden" id="adventWreathCustomColors" name="adventWreathCustomColors" value="{adventWreathCustomColors}">
            </div>
            
            <label for="adventWreathFlameSpeedMs">Flammen-Animation (ms)</label><input type="number" id="adventWreathFlameSpeedMs" name="adventWreathFlameSpeedMs" value="{adventWreathFlameSpeedMs}" min="20" max="500">
            
            <label for="adventWreathDaysBefore24">Adventskranz: Tage vor dem 24.12.</label><input type="number" id="adventWreathDaysBefore24" name="adventWreathDaysBefore24" value="{adventWreathDaysBefore24}" min="0" max="30">
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="adventWreathOnlyFromFirstAdvent" id="adventWreathOnlyFromFirstAdvent" {adventWreathOnlyFromFirstAdvent_checked}> Erst ab dem 1. Advent zeigen</label><br>
            <label for="adventWreathBerryCount">Anzahl Kugeln/Beeren (4-20)</label><input type="number" id="adventWreathBerryCount" name="adventWreathBerryCount" value="{adventWreathBerryCount}" min="4" max="20">
            <label for="adventWreathBgColor">Hintergrundfarbe</label><input type="color" id="adventWreathBgColor" name="adventWreathBgColor" value="{adventWreathBgColor}">
            
            <p style="color:#bbb; margin-top:10px;">Der Adventskranz zeigt 1-4 brennende Kerzen je nach aktuellem Advent mit zuf&auml;llig flackernden Flammen. Der Kranz ist mit Tannengrün dekoriert.</p>
        </div>
    </div>
    
    <!-- Weihnachtsbaum Sub-Tab -->
    <div id="SubWeihnachtsbaum" class="subtabcontent">
        <div class="group">
            <h3>Weihnachtsbaum</h3>
            <input type="checkbox" id="christmasTreeEnabled" name="christmasTreeEnabled" {christmasTreeEnabled_checked}><label for="christmasTreeEnabled" style="display:inline;">Weihnachtsbaum aktivieren</label><br>
            <label for="christmasTreeDaysBefore24">Weihnachtsbaum: Tage vor dem 24.12.</label><input type="number" id="christmasTreeDaysBefore24" name="christmasTreeDaysBefore24" value="{christmasTreeDaysBefore24}" min="0" max="30">
            <label for="christmasTreeDaysAfter24">Weihnachtsbaum: Tage nach dem 24.12.</label><input type="number" id="christmasTreeDaysAfter24" name="christmasTreeDaysAfter24" value="{christmasTreeDaysAfter24}" min="0" max="30">
            
            <label for="christmasTreeLightSpeedMs">Lichterketten-Blinkgeschwindigkeit (ms)</label><input type="number" id="christmasTreeLightSpeedMs" name="christmasTreeLightSpeedMs" value="{christmasTreeLightSpeedMs}" min="30" max="500">
            <label for="christmasTreeLightCount">Anzahl der Lichter (5-30)</label><input type="number" id="christmasTreeLightCount" name="christmasTreeLightCount" value="{christmasTreeLightCount}" min="5" max="30">
            <label for="christmasTreeGiftCount">Anzahl der Geschenke (0-10)</label><input type="number" id="christmasTreeGiftCount" name="christmasTreeGiftCount" value="{christmasTreeGiftCount}" min="0" max="10">
            <label for="christmasTreeBgColor">Hintergrundfarbe</label><input type="color" id="christmasTreeBgColor" name="christmasTreeBgColor" value="{christmasTreeBgColor}">
            <label for="christmasTreeLightMode">Lichterketten-Farbe</label>
            <select id="christmasTreeLightMode" name="christmasTreeLightMode" onchange="toggleTreeLightColor()">
                <option value="0" {christmasTreeLightMode0_selected}>Zuf&auml;llige Farben</option>
                <option value="1" {christmasTreeLightMode1_selected}>Feste Farbe</option>
            </select>
            <div id="treeLightColorDiv" style="display:none;">
                <label for="christmasTreeLightColor">Feste Lichterfarbe</label><input type="color" id="christmasTreeLightColor" name="christmasTreeLightColor" value="{christmasTreeLightColor}">
            </div>
            <label for="christmasTreeOrnamentCount">Anzahl Kugeln am Baum (8-20)</label><input type="number" id="christmasTreeOrnamentCount" name="christmasTreeOrnamentCount" value="{christmasTreeOrnamentCount}" min="8" max="20">
            
            <p style="color:#bbb; margin-top:10px;">Der Weihnachtsbaum hat blinkende Lichter und Kugeln, die bei jeder Anzeige neu gemischt werden. Geschenke können unter dem Baum platziert werden.</p>
        </div>
    </div>
    
    <!-- Kamin Sub-Tab -->
    <div id="SubKamin" class="subtabcontent">
        <div class="group">
            <h3>Kamin</h3>
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="fireplaceEnabled" id="fireplaceEnabled" {fireplaceEnabled_checked}> Kamin aktivieren</label>
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="fireplaceNightModeOnly" id="fireplaceNightModeOnly" {fireplaceNightModeOnly_checked}> Nur abends anzeigen (nach Sonnenuntergang)</label>
            <label for="fireplaceFlameSpeedMs">Feuer-Animation Geschwindigkeit (ms)</label><input type="number" id="fireplaceFlameSpeedMs" name="fireplaceFlameSpeedMs" value="{fireplaceFlameSpeedMs}" min="20" max="200">
            <label for="fireplaceFlameColor">Feuerfarbe</label>
            <select id="fireplaceFlameColor" name="fireplaceFlameColor">
                <option value="0" {fireplaceFlameColor0_selected}>Klassisch (Orange/Gelb)</option>
                <option value="1" {fireplaceFlameColor1_selected}>Blau</option>
                <option value="2" {fireplaceFlameColor2_selected}>Gr&uuml;n</option>
                <option value="3" {fireplaceFlameColor3_selected}>Violett</option>
            </select>
            <label for="fireplaceBrickColor">Kaminfarbe</label><input type="color" id="fireplaceBrickColor" name="fireplaceBrickColor" value="{fireplaceBrickColor}">
            <label for="fireplaceStockingCount">Anzahl Str&uuml;mpfe am Kaminsims (0-5)</label><input type="number" id="fireplaceStockingCount" name="fireplaceStockingCount" value="{fireplaceStockingCount}" min="0" max="5">
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="fireplaceClockEnabled" id="fireplaceClockEnabled" {fireplaceClockEnabled_checked}> Analoge Uhr auf dem Kaminsims anzeigen</label>
            <label for="fireplaceCandleCount">Anzahl Dekoration auf dem Kaminsims (0-4 mit Uhr, 0-5 ohne)</label><input type="number" id="fireplaceCandleCount" name="fireplaceCandleCount" value="{fireplaceCandleCount}" min="0" max="5">
            <label for="fireplaceBgColor">Hintergrundfarbe Kamin</label><input type="color" id="fireplaceBgColor" name="fireplaceBgColor" value="{fireplaceBgColor}">
            
            <p style="color:#bbb; margin-top:10px;">Der Kamin zeigt ein gem&uuml;tliches Feuer mit Str&uuml;mpfen und Dekoration (Kerzen mit animierter Flamme, Bücher, Vasen, Teekanne, Bilderrahmen). Im Nachtmodus erscheint er nur nach Sonnenuntergang (basierend auf Wetterdaten).</p>
        </div>
    </div>
    
    <!-- Allgemein Sub-Tab -->
    <div id="SubAllgemein" class="subtabcontent">
        <div class="group">
            <h3>Allgemeine Einstellungen</h3>
            <p style="color:#888; font-size:12px; margin:5px 0;">Hinweis: Animationen verwenden jetzt immer den Vollbild-Modus für optimale Darstellung.</p>
            
            <label for="adventWreathDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="adventWreathDisplaySec" name="adventWreathDisplaySec" value="{adventWreathDisplaySec}" min="5">
            <label for="adventWreathRepeatMin">Wiederholungsintervall (Minuten)</label><input type="number" id="adventWreathRepeatMin" name="adventWreathRepeatMin" value="{adventWreathRepeatMin}" min="1">
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" id="adventWreathInterrupt" name="adventWreathInterrupt" {adventWreathInterrupt_checked}> Unterbrechend anzeigen</label><br>
            
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="showNewYearCountdown" id="showNewYearCountdown" {showNewYearCountdown_checked}> Countdown bis Silvester anzeigen</label>
            
            <h3 style="margin-top:20px;">LED-Lauflicht-Rahmen</h3>
            <label style="display: inline-block; margin: 10px 0;"><input type="checkbox" name="ledBorderEnabled" id="ledBorderEnabled" {ledBorderEnabled_checked}> LED-Rahmen aktivieren (für Adventskranz & Weihnachtsbaum)</label>
            <label for="ledBorderSpeedMs">Lauflicht-Geschwindigkeit (ms)</label><input type="number" id="ledBorderSpeedMs" name="ledBorderSpeedMs" value="{ledBorderSpeedMs}" min="30" max="500">
            
            <label>LED-Rahmen Farben (4 Farben)</label>
            <div style="display:flex;gap:10px;margin-top:5px;">
                <div><label style="font-size:12px;">Farbe 1</label><input type="color" id="ledColor1" value="{ledColor1}"></div>
                <div><label style="font-size:12px;">Farbe 2</label><input type="color" id="ledColor2" value="{ledColor2}"></div>
                <div><label style="font-size:12px;">Farbe 3</label><input type="color" id="ledColor3" value="{ledColor3}"></div>
                <div><label style="font-size:12px;">Farbe 4</label><input type="color" id="ledColor4" value="{ledColor4}"></div>
            </div>
            <input type="hidden" id="ledBorderColors" name="ledBorderColors" value="{ledBorderColors}">
            
            <p style="color:#bbb; margin-top:10px;">Gilt für alle Animationen. Der Countdown zeigt die verbleibende Zeit bis zum Jahreswechsel (Tage, Stunden, Minuten, Sekunden) rechtsbündig auf Adventskranz und Weihnachtsbaum an. Die aktuelle Uhrzeit wird linksbündig angezeigt. Der LED-Rahmen läuft mit 4 Phasen und bunten Lampen um den gesamten Bereich herum.</p>
        </div>
    </div>
</div>
<script>
function openSubTab(evt, subTabName) {
    var i, subtabcontent, subtablinks;
    subtabcontent = document.getElementsByClassName("subtabcontent");
    for (i = 0; i < subtabcontent.length; i++) {
        subtabcontent[i].style.display = "none";
    }
    subtablinks = document.getElementsByClassName("subtablinks");
    for (i = 0; i < subtablinks.length; i++) {
        subtablinks[i].className = subtablinks[i].className.replace(" active", "");
    }
    var targetTab = document.getElementById(subTabName);
    if (targetTab) {
        targetTab.style.display = "block";
    }
    if (evt && evt.currentTarget) {
        evt.currentTarget.className += " active";
    }
}
function toggleCustomColors() {
    var mode = document.getElementById('adventWreathColorMode').value;
    document.getElementById('customColorsDiv').style.display = (mode == '2') ? 'block' : 'none';
}
function toggleTreeLightColor() {
    var mode = document.getElementById('christmasTreeLightMode').value;
    document.getElementById('treeLightColorDiv').style.display = (mode == '1') ? 'block' : 'none';
}
function updateCustomColors() {
    var c1 = document.getElementById('candleColor1').value;
    var c2 = document.getElementById('candleColor2').value;
    var c3 = document.getElementById('candleColor3').value;
    var c4 = document.getElementById('candleColor4').value;
    document.getElementById('adventWreathCustomColors').value = c1 + ',' + c2 + ',' + c3 + ',' + c4;
}
function updateLedBorderColors() {
    var c1 = document.getElementById('ledColor1').value;
    var c2 = document.getElementById('ledColor2').value;
    var c3 = document.getElementById('ledColor3').value;
    var c4 = document.getElementById('ledColor4').value;
    document.getElementById('ledBorderColors').value = c1 + ',' + c2 + ',' + c3 + ',' + c4;
}
document.addEventListener('DOMContentLoaded', function() {
    toggleCustomColors();
    toggleTreeLightColor();
    ['candleColor1','candleColor2','candleColor3','candleColor4'].forEach(function(id) {
        document.getElementById(id).addEventListener('change', updateCustomColors);
    });
    ['ledColor1','ledColor2','ledColor3','ledColor4'].forEach(function(id) {
        var elem = document.getElementById(id);
        if (elem) elem.addEventListener('change', updateLedBorderColors);
    });
    // Open first sub-tab by default when Animationen tab is opened
    var firstSubTab = document.getElementById('SubAdventskranz');
    var firstSubTabBtn = document.getElementById('defaultAnimOpen');
    if (firstSubTab && firstSubTabBtn) {
        firstSubTab.style.display = "block";
        firstSubTabBtn.className += " active";
    }
});
</script>

<div id="Diverses" class="tabcontent">
    <div class="group">
        <h3>Datenschutz-Optionen</h3>
        <input type="checkbox" id="dataMockingEnabled" name="dataMockingEnabled" {dataMockingEnabled_checked}><label for="dataMockingEnabled" style="display:inline;">Datenmocking aktivieren (Sensible Daten in der Anzeige maskieren)</label><br>
        <p style="color:#ffc107; margin-top:10px;">Hinweis: Wenn aktiviert, werden Adressen in Tankstellenanzeige und Terminbeschreibungen im Kalender durch Platzhalter ersetzt. Dies betrifft nur die Anzeige, nicht die gespeicherten Daten.</p>
    </div>
    <div class="group">
        <h3>Globale Scrolling-Einstellungen</h3>
        <label for="globalScrollSpeedMs">Scroll-Geschwindigkeit (Millisekunden pro Pixel)</label><input type="number" id="globalScrollSpeedMs" name="globalScrollSpeedMs" value="{globalScrollSpeedMs}" min="10" max="500">
        <p style="color:#bbb; margin-top:5px;">Niedrigere Werte = schnelleres Scrolling.</p>
        
        <label for="scrollMode">Scroll-Modus</label>
        <select id="scrollMode" name="scrollMode">
            <option value="0" {scrollMode0_selected}>Kontinuierlich (Text l&auml;uft durch)</option>
            <option value="1" {scrollMode1_selected}>PingPong (Text scrollt hin und her)</option>
        </select>
        
        <label for="scrollReverse">Scroll-Richtung</label>
        <select id="scrollReverse" name="scrollReverse">
            <option value="0" {scrollReverse0_selected}>Normal (nach links)</option>
            <option value="1" {scrollReverse1_selected}>R&uuml;ckw&auml;rts (nach rechts)</option>
        </select>
        
        <label for="scrollPauseSec">Pause zwischen Scroll-Zyklen (Sekunden)</label><input type="number" id="scrollPauseSec" name="scrollPauseSec" value="{scrollPauseSec}" min="0" max="30">
        <p style="color:#bbb; margin-top:5px;">Bei 0 wird kontinuierlich gescrollt. Bei einem Wert &gt; 0 pausiert der Text nach einem kompletten Durchlauf f&uuml;r die angegebene Zeit.</p>
    </div>
</div>

<input type="submit" value="Alle Module speichern (Live-Update)">
</form>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>

<script>
function openTab(evt, tabName) {
  var i, tabcontent, tablinks;
  tabcontent = document.getElementsByClassName("tabcontent");
  for (i = 0; i < tabcontent.length; i++) {
    tabcontent[i].style.display = "none";
  }
  tablinks = document.getElementsByClassName("tablinks");
  for (i = 0; i < tablinks.length; i++) {
    tablinks[i].className = tablinks[i].className.replace(" active", "");
  }
  document.getElementById(tabName).style.display = "block";
  if (evt) {
    evt.currentTarget.className += " active";
  } else {
    document.querySelector('.tab button[onclick*="' + tabName + '"]').className += " active";
  }
}

document.addEventListener('DOMContentLoaded', function() {
    openTab(null, 'Darts');
});


function searchStations() {
    var radius = document.getElementById('searchRadius').value;
    var sort = document.getElementById('searchSort').value;
    var resultsDiv = document.getElementById('station_results');
    resultsDiv.innerHTML = '<p>Suche l&auml;uft...</p>';

    fetch('/api/tankerkoenig/search?radius=' + radius + '&sort=' + sort)
        .then(response => {
            if (!response.ok) {
                return response.json().then(err => { 
                    let message = err.details || err.message || 'Serverfehler: ' + response.status;
                    throw new Error(message);
                });
            }
            return response.json();
        })
        .then(data => {
            if (!data.ok || !data.stations || data.stations.length === 0) {
                resultsDiv.innerHTML = '<p style="color:red;">Keine Tankstellen gefunden oder Fehler: ' + (data.message || 'Unbekannter Fehler') + '</p>';
                return;
            }

            var currentIds = document.getElementById('tankerkoenigStationIds').value.split(',');
            var html = '<table><tr><th>Auswahl</th><th>Name</th><th>Marke</th><th>Stra&szlig;e</th><th>Ort</th><th>Entf.</th><th>Status</th></tr>';
            data.stations.forEach(station => {
                var checked = currentIds.includes(station.id) ? 'checked' : '';
                html += '<tr>';
                html += '<td><input type="checkbox" class="station-checkbox" value="' + station.id + '" ' + checked + '></td>';
                html += '<td>' + station.name + '</td>';
                html += '<td>' + station.brand + '</td>';
                html += '<td>' + station.street + ' ' + station.houseNumber + '</td>';
                html += '<td>' + station.postCode + ' ' + station.place + '</td>';
                html += '<td>' + station.dist.toFixed(2) + ' km</td>';
                html += '<td>' + (station.isOpen ? '<span style="color:lightgreen;">Ge&ouml;ffnet</span>' : '<span style="color:red;">Geschlossen</span>') + '</td>';
                html += '</tr>';
            });
            html += '</table>';
            resultsDiv.innerHTML = html;
        })
        .catch(error => {
            console.error('Error:', error);
            resultsDiv.innerHTML = '<p style="color:red;">Fehler bei der Abfrage: ' + error.message + '</p>';
        });
}

function collectStationIds() {
    var checkboxes = document.getElementsByClassName('station-checkbox');
    var ids = [];
    if (checkboxes.length > 0) {
        for (var i = 0; i < checkboxes.length; i++) {
            if (checkboxes[i].checked) {
                ids.push(checkboxes[i].value);
            }
        }
        document.getElementById('tankerkoenigStationIds').value = ids.join(',');
    }
}

function loadThemeParks() {
    var resultsDiv = document.getElementById('themepark_list');
    resultsDiv.innerHTML = '<p>Lade Freizeitparks...</p>';
    
    fetch('/api/themeparks/list')
        .then(response => response.json())
        .then(data => {
            if (!data.ok || !data.parks || data.parks.length === 0) {
                resultsDiv.innerHTML = '<p style="color:red;">Keine Freizeitparks gefunden.</p>';
                return;
            }
            
            var currentIds = document.getElementById('themeParkIds').value.split(',');
            var html = '<table><tr><th>Auswahl</th><th>Name</th><th>Land</th></tr>';
            data.parks.forEach(park => {
                var checked = currentIds.includes(park.id) ? 'checked' : '';
                html += '<tr>';
                html += '<td><input type="checkbox" class="park-checkbox" value="' + park.id + '" ' + checked + ' onchange="collectParkIds()"></td>';
                html += '<td>' + park.name + '</td>';
                html += '<td>' + (park.country || '') + '</td>';
                html += '</tr>';
            });
            html += '</table>';
            resultsDiv.innerHTML = html;
        })
        .catch(error => {
            console.error('Error:', error);
            resultsDiv.innerHTML = '<p style="color:red;">Fehler beim Laden der Parks: ' + error.message + '</p>';
        });
}

function collectParkIds() {
    var checkboxes = document.getElementsByClassName('park-checkbox');
    var ids = [];
    for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].checked) {
            ids.push(checkboxes[i].value);
        }
    }
    document.getElementById('themeParkIds').value = ids.join(',');
}

function loadSofascoreTournaments() {
    var container = document.getElementById('sofascoreTournamentsContainer');
    var listDiv = document.getElementById('sofascoreTournamentsList');
    container.style.display = 'block';
    listDiv.innerHTML = '<p>Lade Turniere...</p>';
    
    fetch('/api/sofascore/tournaments')
        .then(response => response.json())
        .then(data => {
            if (!data.ok || !data.tournaments || data.tournaments.length === 0) {
                listDiv.innerHTML = '<p style="color:red;">Keine Turniere gefunden.</p>';
                return;
            }
            
            var currentIds = document.getElementById('dartsSofascoreTournamentIds').value.split(',').map(s => s.trim());
            var html = '<table style="width:100%;"><tr><th>Auswahl</th><th>Turnier-Name</th><th>ID</th></tr>';
            data.tournaments.forEach(tournament => {
                var checked = currentIds.includes(tournament.id.toString()) ? 'checked' : '';
                html += '<tr>';
                html += '<td><input type="checkbox" class="tournament-checkbox" value="' + tournament.id + '" ' + checked + ' onchange="collectTournamentIds()"></td>';
                html += '<td>' + tournament.name + '</td>';
                html += '<td>' + tournament.id + '</td>';
                html += '</tr>';
            });
            html += '</table>';
            listDiv.innerHTML = html;
        })
        .catch(error => {
            console.error('Error:', error);
            listDiv.innerHTML = '<p style="color:red;">Fehler beim Laden der Turniere: ' + error.message + '</p>';
        });
}

function collectTournamentIds() {
    var checkboxes = document.getElementsByClassName('tournament-checkbox');
    var ids = [];
    for (var i = 0; i < checkboxes.length; i++) {
        if (checkboxes[i].checked) {
            ids.push(checkboxes[i].value);
        }
    }
    document.getElementById('dartsSofascoreTournamentIds').value = ids.join(',');
}

</script>
)rawliteral";

const char HTML_CONFIG_HARDWARE[] PROGMEM = R"rawliteral(
<h2>Optionale Hardware</h2>
<form action="/save_hardware" method="POST">
<div class="group">
    <h3>Mikrowellen-Sensor &amp; Display-Steuerung</h3>
    <input type="checkbox" id="mwaveSensorEnabled" name="mwaveSensorEnabled" {mwaveSensorEnabled_checked}><label for="mwaveSensorEnabled" style="display:inline;">Sensor-gesteuerte Anzeige aktivieren</label><br>
    
    <h4>Pins</h4>
    <p style="color:#ff8c00;">Warnung: &Auml;nderungen an den Pins l&ouml;sen einen Neustart aus!</p>
    <label for="mwaveRxPin">Sensor RX Pin</label><input type="number" id="mwaveRxPin" name="mwaveRxPin" value="{mwaveRxPin}">
    <label for="mwaveTxPin">Sensor TX Pin</label><input type="number" id="mwaveTxPin" name="mwaveTxPin" value="{mwaveTxPin}">
    <label for="displayRelayPin">Display Relais Pin (255 = ungenutzt)</label><input type="number" id="displayRelayPin" name="displayRelayPin" value="{displayRelayPin}">

    <h4>Schwellenwerte</h4>
    <label for="mwaveOnCheckPercentage">Einschalt-Schwelle (ON-Anteil in % im schnellen Fenster)</label><input type="number" step="0.1" id="mwaveOnCheckPercentage" name="mwaveOnCheckPercentage" value="{mwaveOnCheckPercentage}">
    <label for="mwaveOnCheckDuration">Dauer schnelles Fenster (Sekunden)</label><input type="number" id="mwaveOnCheckDuration" name="mwaveOnCheckDuration" value="{mwaveOnCheckDuration}">
    <hr>
    <label for="mwaveOffCheckOnPercent">Ausschalt-Schwelle (max. ON-Anteil in % im langsamen Fenster)</label><input type="number" step="0.1" id="mwaveOffCheckOnPercent" name="mwaveOffCheckOnPercent" value="{mwaveOffCheckOnPercent}">
    <label for="mwaveOffCheckDuration">Dauer langsames Fenster (Sekunden)</label><input type="number" id="mwaveOffCheckDuration" name="mwaveOffCheckDuration" value="{mwaveOffCheckDuration}">
</div>
<input type="submit" value="Speichern">
</form>

<div class="group">
    <h3>Debug: Letzte 10 Zustandswechsel</h3>
    {debug_log_table}
</div>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_DEBUG_DATA[] PROGMEM = R"rawliteral(
<h2>Debug Daten</h2>
<div class="group">
    <h3>Gecachte Tankstellen-Stammdaten</h3>
    <p>Dies sind alle Tankstellen, die jemals durch eine Umkreissuche gefunden und im Ger&auml;t gespeichert wurden. Klicke auf eine Tankstelle, um die Preis-Historie anzuzeigen.</p>
    <table>
        <thead>
            <tr>
                <th>ID</th>
                <th>Marke</th>
                <th>Name</th>
                <th>Adresse</th>
            </tr>
        </thead>
        <tbody>
            {station_cache_table}
        </tbody>
    </table>
</div>
<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>
)rawliteral";

const char HTML_DEBUG_STATION_HISTORY[] PROGMEM = R"rawliteral(
<h2>Preis-Historie</h2>
<div class="group">
    <h3>{station_brand} - {station_name}</h3>
    <p>{station_address}<br>ID: {station_id}</p>
</div>
<div class="group">
    <h3>Gespeicherte Tagespreise</h3>
    <p>Zeigt die aufgezeichneten Minimal- und Maximalpreise pro Tag.</p>
    <table>
        <thead>
            <tr>
                <th>Datum</th>
                <th>E5 Min</th>
                <th>E5 Max</th>
                <th>E10 Min</th>
                <th>E10 Max</th>
                <th>Diesel Min</th>
                <th>Diesel Max</th>
            </tr>
        </thead>
        <tbody>
            {history_table}
        </tbody>
    </table>
</div>
<div class="footer-link"><a href="/debug">&laquo; Zur&uuml;ck zur &Uuml;bersicht</a></div>
)rawliteral";

const char HTML_STREAM_PAGE[] PROGMEM = R"rawliteral(
<h1>Panel Live-Stream & Debug</h1>
<div style="text-align: center; margin-bottom: 30px;">
    <h3>LED Panel Vorschau</h3>
    <div style="display: flex; justify-content: center;">
        <canvas id="panelSimulator" style="background-color: #000; border: 2px solid #444; border-radius: 8px;"></canvas>
    </div>
    <div style="margin-top: 15px;">
        <button id="connectBtn" class="button" onclick="toggleConnection()">Verbinden</button>
        <span id="statusText" style="margin-left: 15px; color: #bbb;">Getrennt</span>
    </div>
</div>

<div style="text-align: center; margin-top: 30px; padding: 0 20px;">
    <h3>Log-Ausgabe</h3>
    <div style="display: flex; justify-content: center;">
        <div style="max-width: 1000px; width: 100%;">
            <pre id="logOutput" style="background: #1a1a1a; color: #0f0; padding: 15px; border-radius: 4px; height: 400px; overflow-y: auto; font-family: monospace; font-size: 12px; border: 1px solid #444;"></pre>
            <button onclick="clearLogs()" class="button" style="margin-top: 10px; width: auto;">Logs löschen</button>
        </div>
    </div>
</div>

<div class="footer-link"><a href="/">&laquo; Zurück zum Hauptmenü</a></div>

<script>
let ws = null;
let canvas = document.getElementById('panelSimulator');
let ctx = canvas.getContext('2d');
let logOutput = document.getElementById('logOutput');
let statusText = document.getElementById('statusText');
let connectBtn = document.getElementById('connectBtn');

const PANEL_WIDTH = 192;  // 64 * 3
const PANEL_HEIGHT = 96;   // 32 * 3
const LED_SIZE = 4;        // Optimized for ~800px width
const LED_SPACING = 6.75;  // Spacing increased by 1.5x (4.5 * 1.5 = 6.75) for more authentic look

function initCanvas() {
    // Set canvas size to accommodate LEDs with spacing
    canvas.width = PANEL_WIDTH * LED_SPACING;
    canvas.height = PANEL_HEIGHT * LED_SPACING;
    
    // Draw background and LED grid
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    
    // Draw dark gray circles for "off" LEDs
    ctx.fillStyle = '#222';
    for (let y = 0; y < PANEL_HEIGHT; y++) {
        for (let x = 0; x < PANEL_WIDTH; x++) {
            let cx = x * LED_SPACING + LED_SPACING / 2;
            let cy = y * LED_SPACING + LED_SPACING / 2;
            ctx.beginPath();
            ctx.arc(cx, cy, LED_SIZE / 2, 0, 2 * Math.PI);
            ctx.fill();
        }
    }
}

function toggleConnection() {
    try {
        console.log('[toggleConnection] Button clicked');
        if (ws && ws.readyState === WebSocket.OPEN) {
            console.log('[toggleConnection] Closing existing connection');
            ws.close();
        } else {
            console.log('[toggleConnection] Starting new connection');
            connect();
        }
    } catch (e) {
        console.error('[toggleConnection] Error:', e);
        addLog('[Error] ' + e.message);
    }
}

function connect() {
    statusText.textContent = 'Verbinde...';
    statusText.style.color = '#ffaa00';
    connectBtn.disabled = true;
    
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = protocol + '//' + window.location.hostname + ':81/';
    
    console.log('[WebSocket] Attempting to connect to:', wsUrl);
    addLog('[System] Verbinde zu ' + wsUrl);
    
    ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';
    
    ws.onopen = function() {
        console.log('[WebSocket] Connection opened');
        statusText.textContent = 'Verbunden';
        statusText.style.color = '#4CAF50';
        connectBtn.textContent = 'Trennen';
        connectBtn.disabled = false;
        addLog('[System] WebSocket verbunden');
    };
    
    ws.onclose = function(event) {
        console.log('[WebSocket] Connection closed:', event.code, event.reason);
        statusText.textContent = 'Getrennt';
        statusText.style.color = '#bbb';
        connectBtn.textContent = 'Verbinden';
        connectBtn.disabled = false;
        addLog('[System] WebSocket getrennt (Code: ' + event.code + ')');
    };
    
    ws.onerror = function(err) {
        console.error('[WebSocket] Error:', err);
        statusText.textContent = 'Fehler';
        statusText.style.color = '#f44336';
        connectBtn.disabled = false;
        addLog('[System] WebSocket Fehler - siehe Browser Console');
    };
    
    ws.onmessage = function(event) {
        if (typeof event.data === 'string') {
            // Text message - log data
            console.log('[WebSocket] Text message received:', event.data.substring(0, 100));
            try {
                const msg = JSON.parse(event.data);
                if (msg.type === 'log') {
                    addLog(msg.data);
                }
            } catch (e) {
                console.error('[WebSocket] JSON parse error:', e);
                addLog('[Error] Failed to parse log message');
            }
        } else {
            // Binary message - panel data (RLE compressed)
            console.log('[WebSocket] Binary message received, size:', event.data.byteLength);
            decodeAndRenderPanel(new Uint8Array(event.data));
        }
    };
}

function addLog(text) {
    logOutput.textContent += text + '\n';
    logOutput.scrollTop = logOutput.scrollHeight;
}

function clearLogs() {
    logOutput.textContent = '';
}

function decodeAndRenderPanel(data) {
    // Reset to dark background
    initCanvas();
    
    let pos = 0;
    let pixelIndex = 0;
    
    while (pos < data.length - 2 && pixelIndex < PANEL_WIDTH * PANEL_HEIGHT) {
        let count = data[pos++];
        
        // Check for skip marker (count == 0x00 means skip black pixels)
        if (count === 0x00) {
            if (pos < data.length - 1) {
                let skipHigh = data[pos++];
                let skipLow = data[pos++];
                let skipCount = (skipHigh << 8) | skipLow;
                pixelIndex += skipCount;  // Skip these positions (leave them black)
            }
            continue;
        }
        
        let highByte = data[pos++];
        let lowByte = data[pos++];
        
        // Reconstruct RGB565 pixel
        let rgb565 = (highByte << 8) | lowByte;
        
        // Convert RGB565 to RGB888
        let r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
        let g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
        let b = (rgb565 & 0x1F) * 255 / 31;
        
        // Draw the LEDs
        ctx.fillStyle = 'rgb(' + Math.round(r) + ',' + Math.round(g) + ',' + Math.round(b) + ')';
        
        for (let i = 0; i < count && pixelIndex < PANEL_WIDTH * PANEL_HEIGHT; i++) {
            let x = pixelIndex % PANEL_WIDTH;
            let y = Math.floor(pixelIndex / PANEL_WIDTH);
            
            let cx = x * LED_SPACING + LED_SPACING / 2;
            let cy = y * LED_SPACING + LED_SPACING / 2;
            
            ctx.beginPath();
            ctx.arc(cx, cy, LED_SIZE / 2, 0, 2 * Math.PI);
            ctx.fill();
            
            pixelIndex++;
        }
    }
}

// Initialize canvas on load
try {
    console.log('[Init] Initializing canvas and elements');
    if (!canvas) console.error('[Init] Canvas element not found!');
    if (!logOutput) console.error('[Init] logOutput element not found!');
    if (!statusText) console.error('[Init] statusText element not found!');
    if (!connectBtn) console.error('[Init] connectBtn element not found!');
    
    if (canvas && ctx) {
        initCanvas();
        console.log('[Init] Canvas initialized successfully');
    }
    
    console.log('[Init] Page ready. Click "Verbinden" to start streaming.');
} catch (e) {
    console.error('[Init] Initialization error:', e);
}

</script>
)rawliteral";

// Backup & Restore Page
const char HTML_BACKUP_PAGE[] PROGMEM = R"rawliteral(
<h2>Backup & Wiederherstellung</h2>
<p>Hier k&ouml;nnen Sie ein vollst&auml;ndiges System-Backup erstellen oder ein vorhandenes Backup wiederherstellen.</p>

<div class="group">
    <h3>Neues Backup erstellen</h3>
    <p>Erstellt ein Backup aller Konfigurationen, Modul-Daten und Zertifikate.</p>
    <button onclick="createBackup()" class="button">Backup jetzt erstellen</button>
    <div id="createStatus" style="margin-top:10px;"></div>
</div>

<div class="group">
    <h3>Backup hochladen & wiederherstellen</h3>
    <p><strong>Wichtig:</strong> Nach der Wiederherstellung wird das Ger&auml;t automatisch neu gestartet!</p>
    <form id="uploadForm" enctype="multipart/form-data">
        <input type="file" id="backupFile" name="backup" accept=".json" style="margin-top:10px;">
        <button type="button" onclick="uploadBackup()" class="button" style="background-color:#FF9800;margin-top:10px;">Backup hochladen & wiederherstellen</button>
    </form>
    <div id="uploadStatus" style="margin-top:10px;"></div>
</div>

<div class="group">
    <h3>Verf&uuml;gbare Backups</h3>
    <button onclick="refreshBackupList()" class="button" style="background-color:#2196F3;width:auto;">Liste aktualisieren</button>
    <div id="backupList" style="margin-top:15px;">
        <p style="color:#888;">Lade Backup-Liste...</p>
    </div>
</div>

<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>

<script>
function showStatus(elementId, message, isError = false) {
    const el = document.getElementById(elementId);
    el.textContent = message;
    el.style.color = isError ? '#f44336' : '#4CAF50';
}

function createBackup() {
    showStatus('createStatus', 'Erstelle Backup...', false);
    
    fetch('/api/backup/create', {
        method: 'POST'
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showStatus('createStatus', 'Backup erfolgreich erstellt: ' + data.filename, false);
            refreshBackupList();
        } else {
            showStatus('createStatus', 'Fehler: ' + (data.error || 'Unbekannter Fehler'), true);
        }
    })
    .catch(error => {
        showStatus('createStatus', 'Fehler beim Erstellen: ' + error, true);
    });
}

function uploadBackup() {
    const fileInput = document.getElementById('backupFile');
    const file = fileInput.files[0];
    
    if (!file) {
        showStatus('uploadStatus', 'Bitte w&auml;hlen Sie eine Backup-Datei aus!', true);
        return;
    }
    
    if (!confirm('WARNUNG: Das Ger&auml;t wird nach der Wiederherstellung neu gestartet! Fortfahren?')) {
        return;
    }
    
    showStatus('uploadStatus', 'Lade Backup hoch...', false);
    
    const formData = new FormData();
    formData.append('backup', file);
    
    fetch('/api/backup/upload', {
        method: 'POST',
        body: formData
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showStatus('uploadStatus', 'Backup erfolgreich hochgeladen! Wiederherstellung wird gestartet...', false);
            // Restore the backup
            setTimeout(() => {
                restoreBackup(data.filename);
            }, 1000);
        } else {
            showStatus('uploadStatus', 'Fehler: ' + (data.error || 'Upload fehlgeschlagen'), true);
        }
    })
    .catch(error => {
        showStatus('uploadStatus', 'Fehler beim Upload: ' + error, true);
    });
}

function restoreBackup(filename) {
    if (!confirm('Backup "' + filename + '" wirklich wiederherstellen? Das Ger&auml;t wird neu gestartet!')) {
        return;
    }
    
    showStatus('uploadStatus', 'Stelle Backup wieder her...', false);
    
    fetch('/api/backup/restore', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify({filename: filename})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showStatus('uploadStatus', 'Wiederherstellung erfolgreich! Ger&auml;t startet neu...', false);
            setTimeout(() => {
                window.location.href = '/';
            }, 3000);
        } else {
            showStatus('uploadStatus', 'Fehler: ' + (data.error || 'Wiederherstellung fehlgeschlagen'), true);
        }
    })
    .catch(error => {
        showStatus('uploadStatus', 'Fehler bei Wiederherstellung: ' + error, true);
    });
}

function downloadBackup(filename) {
    window.location.href = '/api/backup/download?file=' + encodeURIComponent(filename);
}

function refreshBackupList() {
    const listDiv = document.getElementById('backupList');
    listDiv.innerHTML = '<p style="color:#888;">Lade Backup-Liste...</p>';
    
    fetch('/api/backup/list')
    .then(response => response.json())
    .then(data => {
        if (data.backups && data.backups.length > 0) {
            let html = '<table><tr><th>Dateiname</th><th>Zeitstempel</th><th>Gr&ouml;&szlig;e</th><th>Aktionen</th></tr>';
            
            data.backups.forEach(backup => {
                const sizeKB = (backup.size / 1024).toFixed(2);
                html += '<tr>';
                html += '<td>' + backup.filename + '</td>';
                html += '<td>' + backup.timestamp + '</td>';
                html += '<td>' + sizeKB + ' KB</td>';
                html += '<td>';
                html += '<button onclick="downloadBackup(\'' + backup.filename + '\')" class="button" style="width:auto;padding:5px 10px;margin:2px;">Download</button>';
                html += '<button onclick="restoreBackup(\'' + backup.filename + '\')" class="button" style="width:auto;padding:5px 10px;margin:2px;background-color:#FF9800;">Wiederherstellen</button>';
                html += '</td>';
                html += '</tr>';
            });
            
            html += '</table>';
            listDiv.innerHTML = html;
        } else {
            listDiv.innerHTML = '<p style="color:#888;">Keine Backups vorhanden.</p>';
        }
    })
    .catch(error => {
        listDiv.innerHTML = '<p style="color:#f44336;">Fehler beim Laden der Liste: ' + error + '</p>';
    });
}

// Load backup list on page load
refreshBackupList();
</script>
)rawliteral";

const char HTML_FIRMWARE_UPDATE[] PROGMEM = R"rawliteral(
<h1>Firmware Update</h1>
<div class="group">
    <h3>Firmware hochladen</h3>
    <p>W&auml;hlen Sie eine Firmware-Datei (.bin) aus, um die Panelclock zu aktualisieren.</p>
    <p style="color:#ff8c00;"><strong>Warnung:</strong> W&auml;hrend des Updates wird das Ger&auml;t neu gestartet. Unterbrechen Sie nicht die Stromversorgung!</p>
    
    <form id="uploadForm" method="POST" action="/update" enctype="multipart/form-data">
        <label for="firmwareFile">Firmware-Datei ausw&auml;hlen:</label>
        <input type="file" id="firmwareFile" name="firmware" accept=".bin" required style="margin-top:10px;margin-bottom:10px;">
        <div id="progressContainer" style="display:none;margin-top:20px;">
            <label>Upload-Fortschritt:</label>
            <div style="width:100%;background-color:#333;border-radius:4px;margin-top:5px;overflow:hidden;">
                <div id="progressBar" style="width:0%;height:30px;background-color:#4CAF50;text-align:center;line-height:30px;color:white;transition:width 0.3s;">0%</div>
            </div>
            <p id="statusMessage" style="margin-top:10px;color:#4CAF50;"></p>
        </div>
        <input type="submit" value="Firmware hochladen" id="uploadButton" class="button-danger">
    </form>
</div>

<div class="footer-link"><a href="/">&laquo; Zur&uuml;ck zum Hauptmen&uuml;</a></div>

<script>
document.getElementById('uploadForm').addEventListener('submit', function(e) {
    e.preventDefault();
    
    const fileInput = document.getElementById('firmwareFile');
    const file = fileInput.files[0];
    
    if (!file) {
        alert('Bitte w\u00e4hlen Sie eine Datei aus.');
        return;
    }
    
    if (!file.name.endsWith('.bin')) {
        alert('Bitte w\u00e4hlen Sie eine .bin Datei aus.');
        return;
    }
    
    const progressContainer = document.getElementById('progressContainer');
    const progressBar = document.getElementById('progressBar');
    const statusMessage = document.getElementById('statusMessage');
    const uploadButton = document.getElementById('uploadButton');
    
    progressContainer.style.display = 'block';
    uploadButton.disabled = true;
    statusMessage.textContent = 'Upload wird gestartet...';
    statusMessage.style.color = '#4CAF50';
    
    const xhr = new XMLHttpRequest();
    let uploadCompleted = false;
    
    // Function to show success message with countdown and auto-reload
    function showSuccessAndReload() {
        progressBar.style.backgroundColor = '#4CAF50';
        
        let countdown = 10;
        statusMessage.innerHTML = '<strong>Upload erfolgreich!</strong><br>Ger&auml;t wird neu gestartet...<br>Seite wird in <span id="countdown">10</span> Sekunden automatisch neu geladen.';
        statusMessage.style.color = '#4CAF50';
        statusMessage.style.fontSize = '16px';
        
        // Update countdown every second
        const countdownInterval = setInterval(function() {
            countdown--;
            const countdownElement = document.getElementById('countdown');
            if (countdownElement && countdown > 0) {
                countdownElement.textContent = countdown;
            } else {
                clearInterval(countdownInterval);
            }
        }, 1000);
        
        // Reload page after 10 seconds
        setTimeout(function() {
            window.location.href = '/';
        }, 10000);
    }
    
    xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
            const percentComplete = Math.round((e.loaded / e.total) * 100);
            progressBar.style.width = percentComplete + '%';
            progressBar.textContent = percentComplete + '%';
            statusMessage.textContent = 'Hochladen... ' + percentComplete + '%';
        }
    });
    
    xhr.addEventListener('load', function() {
        if (xhr.status === 200) {
            uploadCompleted = true;
            progressBar.style.width = '100%';
            progressBar.textContent = '100%';
            showSuccessAndReload();
        } else {
            progressBar.style.backgroundColor = '#f44336';
            statusMessage.textContent = 'Fehler beim Upload: ' + xhr.responseText;
            statusMessage.style.color = '#f44336';
            uploadButton.disabled = false;
        }
    });
    
    xhr.addEventListener('error', function() {
        // If upload completed successfully but connection lost (device rebooting), show success message
        if (uploadCompleted) {
            showSuccessAndReload();
        } else {
            progressBar.style.backgroundColor = '#f44336';
            statusMessage.textContent = 'Netzwerkfehler beim Upload';
            statusMessage.style.color = '#f44336';
            uploadButton.disabled = false;
        }
    });
    
    xhr.addEventListener('abort', function() {
        statusMessage.textContent = 'Upload abgebrochen';
        statusMessage.style.color = '#ff8c00';
        uploadButton.disabled = false;
    });
    
    const formData = new FormData();
    formData.append('firmware', file);
    
    xhr.open('POST', '/update', true);
    xhr.send(formData);
});
</script>
)rawliteral";

#endif // WEB_PAGES_HPP
