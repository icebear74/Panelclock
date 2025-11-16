[View this document in German (Deutsch)](README.de.md)

---

# Panelclock - Multifunctional ESP32 LED Matrix Clock

Panelclock is a flexible and modular project for an ESP32-based clock running on a HUB75 RGB LED matrix panel. It is much more than just a clock; it's a versatile information display that can be extended with a variety of modules.

The system is designed for 24/7 operation, featuring a sophisticated web interface for configuration, Over-the-Air (OTA) update capabilities, and robust memory management that relies on PSRAM.

## Core Architecture

The heart of the project is a modular architecture, orchestrated by the central `Application` class. Each unit of information that can be displayed on the panel (like the time, calendar, sports results) is encapsulated as a standalone `DrawableModule`.

- **Application (`Application.cpp/.hpp`):** The conductor of the entire project. It initializes all managers and modules and controls the main loop.
- **PanelManager (`PanelManager.cpp/.hpp`):** The display controller. It manages the list of active modules, cycles through them in a predefined rotation, and is responsible for rendering the final image to the LED panel.
- **DrawableModule (`DrawableModule.hpp`):** A virtual base class (interface) implemented by all display modules. It defines the fundamental functions a module must have, such as `draw()`, `logicTick()`, and `getDisplayDuration()`.
- **WebClientModule (`WebClientModule.cpp/.hpp`):** A central, asynchronous manager for all HTTP requests. Modules register their required web resources here, and the WebClient efficiently handles fetching and caching the data in the background.
- **WebServer & Configuration (`webconfig.cpp/.hpp`, `WebServerManager.cpp/.hpp`):** The project hosts a comprehensive web interface that allows for full configuration of all modules without recompiling. Settings are stored on the device's LittleFS.
- **BackupManager (`BackupManager.cpp/.hpp`):** Manages complete system backups including all configurations, module data, and certificates. Supports automatic daily backups, manual backup creation, download/upload via web interface, and emergency recovery in Access Point mode. See [BACKUP_SYSTEM.md](BACKUP_SYSTEM.md) for details.

## Key Features

- **Modular Display:** Easily extendable with new modules. The displayed information rotates automatically.
- **Web Configuration:** An integrated web server allows for convenient configuration of Wi-Fi, timezone, module settings, and more via a browser.
- **OTA Updates:** The firmware can be updated "Over-the-Air" through the web interface.
- **Backup & Restore:** Complete system backups including all configurations, module data, and certificates. Automatic daily backups with web-based download/upload and emergency recovery in Access Point mode. See [BACKUP_SYSTEM.md](BACKUP_SYSTEM.md) for details.
- **Efficient Resource Management:** Heavy use of PSRAM for web data, JSON parsing, player lists, and string operations to conserve the ESP32's scarce internal memory. All modules use PsramString and PsramVector to minimize heap fragmentation.
- **Asynchronous Data Fetching:** All external data is loaded in the background without blocking the display or other operations.
- **Presence Detection:** The `MwaveSensorModule` can automatically turn off the display when no one is present and reactivate it, saving power.
- **Robust Time Conversion:** A custom `GeneralTimeConverter` ensures reliable conversion from UTC to local time, including correct Daylight Saving Time (DST) rules.

## Modules

The Panelclock system consists of a series of specialized modules:

### Standard Modules
- **ClockModule:** A simple yet elegant digital clock. The core module.
- **CalendarModule:** Displays events from one or more iCal sources (.ics). Supports complex recurrence rules thanks to the `RRuleParser`.

### Information Modules
- **DartsRankingModule:** Displays the "Order of Merit" and "Pro Tour" rankings of the darts world. Tracked players can be highlighted, and for live tournaments, the player's current round is shown.
- **CuriousHolidaysModule:** Fetches and displays curious holidays for the current and next day from `kuriose-feiertage.de`.
- **TankerkoenigModule:** Retrieves current fuel prices for configured gas stations via the Tankerkönig API.
- **FritzboxModule:** Connects to an AVM FRITZ!Box to display a call list of recent missed, received, or placed calls.
- **WeatherModule:** Shows weather information and forecasts from OpenWeatherMap.
- **ThemeParkModule:** Displays wait times for attractions from selected theme parks. Data is fetched via the [Wartezeiten.APP](https://www.wartezeiten.app/) API. For each park, the name, crowd level, and highest wait times are displayed.

### System Modules
- **MwaveSensorModule:** Controls an RCWL-0516 microwave motion sensor to detect human presence in the room and toggle the display.
- **ConnectionManager:** Manages the Wi-Fi connection and ensures automatic reconnection.
- **OtaManager:** Provides the functionality for Over-the-Air updates.

## How to Add a New Module

The modular design makes it easy to extend the Panelclock's functionality. Follow these steps to create your own `DrawableModule`:

1.  **Create Header and Source Files:**
    *   Create `YourNewModule.hpp` and `YourNewModule.cpp`.
    *   In the `.hpp` file, include `DrawableModule.hpp` and have your new class inherit from it.
        ```cpp
        #include "DrawableModule.hpp"
        
        class YourNewModule : public DrawableModule {
        public:
            YourNewModule(/* Dependencies like u8g2, canvas, etc. */);
            
            // Implement the virtual functions from DrawableModule
            void draw() override;
            void logicTick() override;
            unsigned long getDisplayDuration() override;
            bool isEnabled() override;
            const char* getModuleName() const override { return "YourNewModule"; }
            // ... other optional functions like `tick()` or `resetPaging()` ...
        };
        ```

2.  **Instantiate the Module in the Application:**
    *   Open `Application.hpp` and declare a pointer to your new module.
    *   Open `Application.cpp`. In the `setup()` method, create a new instance of your module using `new`.

3.  **Register the Module with the PanelManager:**
    *   In `Application::setup()`, add your new module to the `panelManager`'s list:
        ```cpp
        panelManager.addModule(yourNewModule.get()); 
        ```

4.  **Add Configuration in the Web Interface:**
    *   **Define Structure:** In `webconfig.hpp`, extend the `struct WebConfig` with the configuration parameters for your module (e.g., a `bool enabled` switch, API keys, etc.).
    *   **Create HTML Page:** In `WebPages.hpp`, add a new `PROGMEM` string that contains the HTML form for your module's settings. Use placeholders (`%placeholder%`) that will be replaced later.
    *   **Implement Web Handler:** In `WebHandlers.cpp`, create a new handler function (e.g., `handleYourModuleConfig()`) that serves the configuration page and saves the form input.
    *   **Register Server Route:** In `WebServerManager::begin()`, register a new route for your handler (e.g., `/config/yourmodule`).

5.  **Data Fetching (if necessary):**
    *   If your module needs data from the internet, use the `WebClientModule`.
    *   Register the required URL in your module's `setConfig` method using `webClient->registerResource(...)`.
    *   Implement the `queueData()` method to trigger the data fetch and the `processData()` method to process the loaded data.

## Hardware & Dependencies

- **Controller:** ESP32 with PSRAM (e.g., a WROVER module).
- **Display:** HUB75-compatible RGB LED matrix panel.
- **Libraries:**
  - `ESP32-HUB75-LED-MATRIX-PANEL-DMA-Display` for controlling the panel.
  - `Adafruit_GFX_Library` and `U8g2_for_Adafruit_GFX` for text and graphics rendering.
  - `ArduinoJson` for processing API responses.
  - `NTPClient` for time synchronization.
  - As well as standard ESP32 libraries (`WiFi`, `HTTPClient`, etc.).

## Build & Flash

The project is structured as an Arduino sketch.
- **Partition Scheme:** A custom `partitions.csv` is used to provide enough space for the large application, OTA updates, and the LittleFS filesystem for web files.
- **PSRAM:** PSRAM must be enabled in the Arduino IDE's board settings.

---
*This README was generated based on a source code analysis and has been manually extended.*
