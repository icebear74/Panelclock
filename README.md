# Panelclock - Dual ESP32-S3 Architecture

Multi-functional ESP32-S3 LED matrix clock with dual-processor architecture.

## Project Structure

This repository contains **two separate ESP32-S3 projects**:

### `/control` - Control-ESP
The main controller with all business logic:
- WiFi connectivity and web interface
- All information modules (Weather, Calendar, Darts, etc.)
- Data fetching and processing
- Renders frames in RGB888 (24-bit color)
- Sends frames to Display-ESP via SPI
- OTA updates for both ESPs

**Hardware**: Seeed Studio XIAO ESP32-S3 with PSRAM

[See control/readme.md for details](control/readme.md)

### `/display` - Display-ESP
Dedicated display driver:
- Receives RGB888 frames via SPI
- Drives HUB75 LED matrix (9 panels, 192x96 pixels)
- No WiFi - offline operation
- Configuration from Control-ESP
- OTA updates via Control-ESP

**Hardware**: ESP32-S3 Mini with PSRAM

[See display/README.md for details](display/README.md)

## Architecture Benefits

- **Memory Relief**: Split functionality frees up Control-ESP memory
- **24-bit Color**: Full RGB888 instead of RGB565
- **Faster Display**: Dedicated ESP for rendering
- **Simple Updates**: Single web interface controls both ESPs
- **Resilient**: Display-ESP persists config, works independently

## Connection

Control-ESP and Display-ESP are connected via SPI:

```
Control-ESP (XIAO)     Display-ESP (Mini)
GPIO9 (MOSI)    →      GPIO35 (MOSI)
GPIO8 (MISO)    ←      GPIO37 (MISO)
GPIO7 (SCK)     →      GPIO36 (SCK)
GPIO6 (CS)      →      GPIO34 (CS)
GND             -      GND
```

## Getting Started

### 1. Build and Flash Control-ESP
```bash
cd control
# Open Panelclock.ino in Arduino IDE
# Select Board: "Seeed Studio XIAO ESP32S3"
# Upload
```

### 2. Build and Flash Display-ESP
```bash
cd display
# Open DisplayMain.ino in Arduino IDE
# Select Board: "ESP32-S3 Dev Module"
# Enable PSRAM in Tools menu
# Upload
```

### 3. Wire the Connection
Connect the 4 SPI wires + GND between the two ESPs.

### 4. Power On
- Display-ESP shows test pattern
- Control-ESP connects to WiFi
- Control-ESP sends frames to Display-ESP
- Display updates automatically

## Development Status

- [x] Architecture design completed
- [x] Repository restructured
- [x] Display-ESP basic structure
- [x] Test pattern working
- [ ] SPI protocol implementation
- [ ] RLE compression/decompression
- [ ] Control-ESP modifications
- [ ] Full integration testing

## License

See original Panelclock license in control/

## Original Project

This is a restructured version of the Panelclock project to support dual-ESP architecture.
Original single-ESP code is in the `control/` directory.
