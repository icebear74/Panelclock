/**
 * @file HUB75Driver.cpp
 * @brief HUB75 LED Matrix Display Driver Implementation
 */

#include "HUB75Driver.hpp"

HUB75Driver::HUB75Driver(DisplayConfig& config) 
    : _config(config), _dmaDisplay(nullptr), _virtualDisp(nullptr) {
}

HUB75Driver::~HUB75Driver() {
    delete _virtualDisp;
    delete _dmaDisplay;
}

bool HUB75Driver::begin() {
    Serial.println("[HUB75Driver] Initializing display...");
    
    // Get pin configuration
    auto pins = _config.getHUB75Pins();
    
    // Setup HUB75 pins
    HUB75_I2S_CFG::i2s_pins hub75Pins = {
        (int8_t)pins.R1, (int8_t)pins.G1, (int8_t)pins.B1,
        (int8_t)pins.R2, (int8_t)pins.G2, (int8_t)pins.B2,
        (int8_t)pins.A,  (int8_t)pins.B,  (int8_t)pins.C,
        (int8_t)pins.D,  (int8_t)pins.E,
        (int8_t)pins.LAT, (int8_t)pins.OE, (int8_t)pins.CLK
    };
    
    // Create display configuration
    HUB75_I2S_CFG mxconfig(
        PANEL_RES_X, 
        PANEL_RES_Y, 
        VDISP_NUM_ROWS * VDISP_NUM_COLS, 
        hub75Pins
    );
    
    mxconfig.double_buff = false;
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
    mxconfig.clkphase = false;
    
    // Initialize DMA display
    _dmaDisplay = new MatrixPanel_I2S_DMA(mxconfig);
    if (!_dmaDisplay->begin()) {
        Serial.println("[HUB75Driver] ERROR: DMA display initialization failed!");
        return false;
    }
    
    // Set initial brightness
    _dmaDisplay->setBrightness8(_config.getBrightness());
    _dmaDisplay->clearScreen();
    
    // Setup virtual display for chaining
    _virtualDisp = new VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>(
        VDISP_NUM_ROWS,
        VDISP_NUM_COLS,
        PANEL_RES_X,
        PANEL_RES_Y
    );
    _virtualDisp->setDisplay(*_dmaDisplay);
    
    Serial.printf("[HUB75Driver] Display initialized: %dx%d pixels\n", 
                 getWidth(), getHeight());
    return true;
}

void HUB75Driver::showTestPattern() {
    Serial.println("[HUB75Driver] Showing test pattern...");
    
    const uint16_t w = getWidth();
    const uint16_t h = getHeight();
    
    // Clear to black
    _virtualDisp->fillScreen(0);
    
    // Draw color bars
    const uint16_t barHeight = h / 8;
    const uint16_t colors[] = {
        0xF800,  // Red
        0xFBE0,  // Orange
        0xFFE0,  // Yellow
        0x07E0,  // Green
        0x07FF,  // Cyan
        0x001F,  // Blue
        0xF81F,  // Magenta
        0xFFFF   // White
    };
    
    for (int i = 0; i < 8; i++) {
        _virtualDisp->fillRect(0, i * barHeight, w, barHeight, colors[i]);
    }
    
    // Draw border
    _virtualDisp->drawRect(0, 0, w, h, 0xFFFF);
    _virtualDisp->drawRect(1, 1, w-2, h-2, 0xFFFF);
    
    // Draw "DISPLAY-ESP" text in center (if space)
    _virtualDisp->setCursor(w/2 - 40, h/2 - 4);
    _virtualDisp->setTextColor(0xFFFF);
    _virtualDisp->print("READY");
    
    Serial.println("[HUB75Driver] Test pattern displayed");
}

void HUB75Driver::updateFrame(const uint8_t* rgb888Buffer, size_t width, size_t height) {
    if (!rgb888Buffer || width != getWidth() || height != getHeight()) {
        return;
    }
    
    // Convert RGB888 to RGB565 and display
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;
            uint8_t r = rgb888Buffer[idx];
            uint8_t g = rgb888Buffer[idx + 1];
            uint8_t b = rgb888Buffer[idx + 2];
            
            uint16_t color = rgb888to565(r, g, b);
            _virtualDisp->drawPixel(x, y, color);
        }
    }
}

void HUB75Driver::setBrightness(uint8_t brightness) {
    if (_dmaDisplay) {
        _dmaDisplay->setBrightness8(brightness);
    }
}

void HUB75Driver::clear() {
    if (_virtualDisp) {
        _virtualDisp->fillScreen(0);
    }
}

uint16_t HUB75Driver::rgb888to565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
