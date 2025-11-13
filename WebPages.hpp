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
a {color:#4CAF50;}
</style></head><body><div class="container">
)rawliteral";
const char HTML_PAGE_FOOTER[] PROGMEM = R"rawliteral(</div></body></html>)rawliteral";

const char HTML_INDEX[] PROGMEM = R"rawliteral(
<h1>Panelclock Hauptmen&uuml;</h1>
<a href="/config_base" class="button button-danger">Grundkonfiguration (mit Neustart)</a>
<a href="/config_location" class="button">Mein Standort</a>
<a href="/config_modules" class="button">Anzeige-Module (Live-Update)</a>
<a href="/config_hardware" class="button">Optionale Hardware</a>
<!-- File manager button added to main menu -->
<a href="/fs" class="button">Dateimanager</a>
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
  <button class="tablinks" onclick="openTab(event, 'Timezone')" id="defaultOpen">Zeitzone</button>
  <button class="tablinks" onclick="openTab(event, 'Wetter')">Wetter</button>
  <button class="tablinks" onclick="openTab(event, 'Tankstellen')">Tankstellen</button>
  <button class="tablinks" onclick="openTab(event, 'Kalender')">Kalender</button>
  <button class="tablinks" onclick="openTab(event, 'Darts')">Darts</button>
  <button class="tablinks" onclick="openTab(event, 'Fritzbox')">Fritz!Box</button>
</div>

<form action="/save_modules" method="POST" onsubmit="collectStationIds()">

<div id="Timezone" class="tabcontent">
    <div class="group">
        <h3>Zeitzone</h3>
        <label for="timezone">Zeitzone</label><select id="timezone" name="timezone">{tz_options}</select>
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
        <label for="weatherDailyForecastDays">Anzahl der Vorschau-Tage (0-8)</label><input type="number" id="weatherDailyForecastDays" name="weatherDailyForecastDays" value="{weatherDailyForecastDays}" min="0" max="8">
    </div>
    <div class="group">
        <h3>Stunden-Vorschau Details</h3>
        <label for="weatherHourlyMode">Typ der Stunden-Anzeige</label>
        <select id="weatherHourlyMode" name="weatherHourlyMode" onchange="toggleHourlyMode()">
            <option value="0" {weatherHourlyMode_0_selected}>Definierte Zeitfenster</option>
            <option value="1" {weatherHourlyMode_1_selected}>Festes Intervall</option>
        </select>
        <div id="hourlyModeSlots">
            <label for="weatherHourlySlotMorning">'Morgen' endet um (Stunde)</label><input type="number" id="weatherHourlySlotMorning" name="weatherHourlySlotMorning" value="{weatherHourlySlotMorning}" min="0" max="23">
            <label for="weatherHourlySlotNoon">'Mittag' endet um (Stunde)</label><input type="number" id="weatherHourlySlotNoon" name="weatherHourlySlotNoon" value="{weatherHourlySlotNoon}" min="0" max="23">
            <label for="weatherHourlySlotEvening">'Abend' endet um (Stunde)</label><input type="number" id="weatherHourlySlotEvening" name="weatherHourlySlotEvening" value="{weatherHourlySlotEvening}" min="0" max="23">
        </div>
        <div id="hourlyModeInterval" style="display:none;">
            <label for="weatherHourlyInterval">Alle X Stunden anzeigen</label><input type="number" id="weatherHourlyInterval" name="weatherHourlyInterval" value="{weatherHourlyInterval}" min="1" max="12">
        </div>
    </div>
    <div class="group">
        <h3>Offizielle Wetter-Warnungen</h3>
        <input type="checkbox" id="weatherAlertsEnabled" name="weatherAlertsEnabled" {weatherAlertsEnabled_checked}><label for="weatherAlertsEnabled" style="display:inline;">Warnungen anzeigen (unterbricht normale Anzeige)</label><br>
        <label for="weatherAlertsDisplaySec">Anzeigedauer der Warnung (Sekunden)</label><input type="number" id="weatherAlertsDisplaySec" name="weatherAlertsDisplaySec" value="{weatherAlertsDisplaySec}" min="1">
        <label for="weatherAlertsRepeatMin">Wiederholung der Warnung (Minuten)</label><input type="number" id="weatherAlertsRepeatMin" name="weatherAlertsRepeatMin" value="{weatherAlertsRepeatMin}" min="0">
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
        <label for="calendarScrollMs">Scroll-Geschwindigkeit (ms)</label><input type="number" id="calendarScrollMs" name="calendarScrollMs" value="{calendarScrollMs}">
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

<div id="Darts" class="tabcontent">
    <div class="group">
        <h3>Darts-Ranglisten</h3>
        <input type="checkbox" id="dartsOomEnabled" name="dartsOomEnabled" {dartsOomEnabled_checked}><label for="dartsOomEnabled" style="display:inline;">Order of Merit anzeigen</label><br>
        <input type="checkbox" id="dartsProTourEnabled" name="dartsProTourEnabled" {dartsProTourEnabled_checked}><label for="dartsProTourEnabled" style="display:inline;">Pro Tour anzeigen</label><br>
        <label for="dartsDisplaySec">Anzeigedauer (Sekunden)</label><input type="number" id="dartsDisplaySec" name="dartsDisplaySec" value="{dartsDisplaySec}">
        <label for="trackedDartsPlayers">Lieblingsspieler (kommagetrennt)</label><input type="text" id="trackedDartsPlayers" name="trackedDartsPlayers" value="{trackedDartsPlayers}">
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

function toggleHourlyMode() {
    var mode = document.getElementById('weatherHourlyMode').value;
    document.getElementById('hourlyModeSlots').style.display = (mode == 0) ? 'block' : 'none';
    document.getElementById('hourlyModeInterval').style.display = (mode == 1) ? 'block' : 'none';
}

document.addEventListener('DOMContentLoaded', function() {
    openTab(null, 'Timezone');
    toggleHourlyMode();
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
<div class="group">
    <h3>LED Panel Vorschau</h3>
    <div style="text-align: center;">
        <canvas id="panelSimulator" width="576" height="288" style="background-color: #000; border: 2px solid #444; border-radius: 8px; max-width: 100%;"></canvas>
    </div>
    <div style="margin-top: 15px; text-align: center;">
        <button id="connectBtn" class="button" onclick="toggleConnection()">Verbinden</button>
        <span id="statusText" style="margin-left: 15px; color: #bbb;">Getrennt</span>
    </div>
</div>

<div class="group">
    <h3>Log-Ausgabe</h3>
    <div style="position: relative;">
        <pre id="logOutput" style="background: #1a1a1a; color: #0f0; padding: 15px; border-radius: 4px; height: 400px; overflow-y: auto; font-family: monospace; font-size: 12px; border: 1px solid #444;"></pre>
        <button onclick="clearLogs()" class="button" style="margin-top: 10px; width: auto;">Logs löschen</button>
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
const LED_SIZE = 2.5;
const LED_SPACING = 3;

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
    if (ws && ws.readyState === WebSocket.OPEN) {
        ws.close();
    } else {
        connect();
    }
}

function connect() {
    statusText.textContent = 'Verbinde...';
    statusText.style.color = '#ffaa00';
    connectBtn.disabled = true;
    
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = protocol + '//' + window.location.hostname + ':81/';
    
    ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';
    
    ws.onopen = function() {
        statusText.textContent = 'Verbunden';
        statusText.style.color = '#4CAF50';
        connectBtn.textContent = 'Trennen';
        connectBtn.disabled = false;
        addLog('[System] WebSocket verbunden');
    };
    
    ws.onclose = function() {
        statusText.textContent = 'Getrennt';
        statusText.style.color = '#bbb';
        connectBtn.textContent = 'Verbinden';
        connectBtn.disabled = false;
        addLog('[System] WebSocket getrennt');
    };
    
    ws.onerror = function(err) {
        statusText.textContent = 'Fehler';
        statusText.style.color = '#f44336';
        connectBtn.disabled = false;
        addLog('[System] WebSocket Fehler: ' + err);
    };
    
    ws.onmessage = function(event) {
        if (typeof event.data === 'string') {
            // Text message - log data
            try {
                const msg = JSON.parse(event.data);
                if (msg.type === 'log') {
                    addLog(msg.data);
                }
            } catch (e) {
                addLog('[Error] Failed to parse log message');
            }
        } else {
            // Binary message - panel data (RLE compressed)
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
        // Read RLE: [count][high_byte][low_byte]
        let count = data[pos++];
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
initCanvas();
</script>
)rawliteral";

#endif // WEB_PAGES_HPP


#endif // WEB_PAGES_HPP