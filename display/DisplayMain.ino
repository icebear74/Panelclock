/**
 * @file DisplayMain.ino
 * @brief Display-ESP Main Entry Point
 * 
 * Minimal ESP32-S3 Display Driver for Panelclock
 * - Receives RGB888 frames via SPI from Control-ESP
 * - Drives HUB75 LED matrix panel
 * - No WiFi, no business logic - pure display rendering
 */

#include <Arduino.h>
#include "HUB75Driver.hpp"
#include "SpiSlaveReceiver.hpp"
#include "DisplayConfig.hpp"

// Global instances
HUB75Driver* displayDriver = nullptr;
SpiSlaveReceiver* spiReceiver = nullptr;
DisplayConfig* config = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Panelclock Display-ESP Startup ===");
    Serial.println("Version: 1.0.0-alpha");
    Serial.println("Hardware: ESP32-S3 with PSRAM");
    
    // Load configuration from NVS
    config = new DisplayConfig();
    if (!config->begin()) {
        Serial.println("WARNING: Using default configuration");
    }
    
    // Initialize HUB75 display driver
    displayDriver = new HUB75Driver(*config);
    if (!displayDriver->begin()) {
        Serial.println("FATAL: Display initialization failed!");
        while(1) { delay(1000); }
    }
    
    // Show test pattern
    displayDriver->showTestPattern();
    Serial.println("Test pattern displayed");
    
    // Initialize SPI slave receiver
    spiReceiver = new SpiSlaveReceiver(*displayDriver, *config);
    if (!spiReceiver->begin()) {
        Serial.println("FATAL: SPI slave initialization failed!");
        while(1) { delay(1000); }
    }
    
    Serial.println("=== Display-ESP Ready ===");
    Serial.println("Waiting for frames from Control-ESP...");
}

void loop() {
    // SPI receiver handles everything in interrupt/task
    // Main loop just monitors status
    static unsigned long lastStatusPrint = 0;
    
    if (millis() - lastStatusPrint >= 10000) {
        Serial.printf("[Status] Frames: %lu, Errors: %lu, FPS: %.1f\n",
                     spiReceiver->getFrameCount(),
                     spiReceiver->getErrorCount(),
                     spiReceiver->getCurrentFPS());
        lastStatusPrint = millis();
    }
    
    delay(100);
}
