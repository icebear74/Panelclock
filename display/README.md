# Panelclock Display-ESP

Display driver firmware for ESP32-S3 in dual-ESP Panelclock architecture.

## Overview

This is the **Display-ESP** component of the Panelclock system. It handles:
- Receiving RGB888 frames via SPI from Control-ESP
- Driving HUB75 LED matrix panels (9 panels, 192x96 resolution)
- Configuration management (stored in NVS)
- No WiFi, no business logic - pure display rendering

## Hardware Requirements

- **ESP32-S3** with PSRAM (recommended: ESP32-S3 Mini)
- **HUB75 RGB LED Matrix**: 9x panels (64x32 each) = 192x96 total
- **SPI Connection** to Control-ESP (4 wires: MOSI, MISO, SCK, CS)

## Pin Configuration

### HUB75 Pins (Default)
- R1=1, G1=2, B1=4
- R2=5, G2=6, B2=7
- A=15, B=16, C=17, D=18, E=3
- CLK=19, LAT=20, OE=21

### SPI Slave Pins
- MISO=37 (to Control-ESP)
- MOSI=35 (from Control-ESP)
- SCK=36
- CS=34

## Features

- [x] HUB75 display driver with test pattern
- [x] NVS configuration management
- [x] SPI slave receiver (basic structure)
- [ ] RLE decompression
- [ ] Full SPI protocol implementation
- [ ] OTA update receiver
- [ ] Connection quality monitoring

## Building

Use Arduino IDE or PlatformIO with ESP32-S3 board selected.

### Dependencies
- ESP32-HUB75-MatrixPanel-DMA
- ArduinoJson

## Status Display

On startup, shows a test pattern with color bars and "READY" text.

When connected to Control-ESP, displays frames received via SPI.

## Serial Monitor

Debug output available at 115200 baud:
- Initialization status
- Frame statistics (count, errors, FPS)
- Configuration updates
- Error messages
