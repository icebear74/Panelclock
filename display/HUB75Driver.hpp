/**
 * @file HUB75Driver.hpp
 * @brief HUB75 LED Matrix Display Driver
 * 
 * Manages the physical HUB75 RGB LED matrix panel
 * - Initialize display hardware with config from Control-ESP
 * - Render RGB888 framebuffer to HUB75 output
 * - Test pattern generation
 */

#ifndef HUB75_DRIVER_HPP
#define HUB75_DRIVER_HPP

#include <Arduino.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include "DisplayConfig.hpp"

// Panel configuration (from Control-ESP architecture)
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define VDISP_NUM_ROWS 3
#define VDISP_NUM_COLS 3
#define PANEL_CHAIN_TYPE CHAIN_TOP_LEFT_DOWN

class HUB75Driver {
public:
    HUB75Driver(DisplayConfig& config);
    ~HUB75Driver();
    
    bool begin();
    void showTestPattern();
    void updateFrame(const uint8_t* rgb888Buffer, size_t width, size_t height);
    void setBrightness(uint8_t brightness);
    void clear();
    
    // Getters
    uint16_t getWidth() const { return PANEL_RES_X * VDISP_NUM_COLS; }
    uint16_t getHeight() const { return PANEL_RES_Y * VDISP_NUM_ROWS; }
    
private:
    DisplayConfig& _config;
    MatrixPanel_I2S_DMA* _dmaDisplay;
    VirtualMatrixPanel_T<PANEL_CHAIN_TYPE>* _virtualDisp;
    
    // RGB888 to RGB565 conversion
    uint16_t rgb888to565(uint8_t r, uint8_t g, uint8_t b);
};

#endif // HUB75_DRIVER_HPP
