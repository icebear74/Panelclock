#ifndef DATAMODULE_HPP
#define DATAMODULE_HPP

#include <Arduino.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <GFXcanvas.h> // for GFXcanvas16
#include <cmath>

class DataModule {
public:
  DataModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, int timeAreaHeight)
    : u8g2(u8g2), canvas(canvas), timeAreaH(timeAreaHeight) {}

  // set station / price data (main will call after fetching)
  void setData(const String &station,
               float e5_, float e10_, float diesel_,
               float e5L, float e5H, float e10L, float e10H, float dL, float dH) {
    stationname = station;
    e5 = e5_; e10 = e10_; diesel = diesel_;
    e5Low = e5L; e5High = e5H;
    e10Low = e10L; e10High = e10H;
    dieselLow = dL; dieselHigh = dH;
  }

  // Draw the data area (frame + contents)
  void draw() {
    canvas.fillScreen(0);
    canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), rgb565(50,50,50));
    u8g2.begin(canvas);

    // heading
    u8g2.setFont(u8g2_font_8x13_tf);
    u8g2.setForegroundColor(WHITE);
    u8g2.setCursor(40, 40 - timeAreaH);
    u8g2.print("Benzinpreise");

    // station name
    u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
    u8g2.setForegroundColor(rgb565(128,128,128));
    u8g2.setCursor(0, 47 - timeAreaH);
    u8g2.print(stationname);

    // prices: baselines adjusted relative to the overall display (main uses same baseline scheme)
    u8g2.setForegroundColor(YELLOW);
    // E5
    int e5_baseline = 60 - timeAreaH;
    u8g2.setFont(u8g2_font_6x13_me);
    u8g2.setCursor(2, e5_baseline);
    u8g2.print("E5");
    drawPrice(50, e5_baseline, e5Low, GREEN);
    uint16_t e5col = calcColor(e5, e5Low, e5High);
    drawPrice(100, e5_baseline, e5, e5col);
    drawPrice(150, e5_baseline, e5High, RED);

    // E10
    int e10_baseline = 73 - timeAreaH;
    u8g2.setForegroundColor(YELLOW);
    u8g2.setFont(u8g2_font_6x13_me);
    u8g2.setCursor(2, e10_baseline);
    u8g2.print("E10");
    drawPrice(50, e10_baseline, e10Low, GREEN);
    uint16_t e10col = calcColor(e10, e10Low, e10High);
    drawPrice(100, e10_baseline, e10, e10col);
    drawPrice(150, e10_baseline, e10High, RED);

    // Diesel
    int diesel_baseline = 86 - timeAreaH;
    u8g2.setForegroundColor(YELLOW);
    u8g2.setFont(u8g2_font_6x13_me);
    u8g2.setCursor(2, diesel_baseline);
    u8g2.print("Diesel");
    drawPrice(50, diesel_baseline, dieselLow, GREEN);
    uint16_t dcol = calcColor(diesel, dieselLow, dieselHigh);
    drawPrice(100, diesel_baseline, diesel, dcol);
    drawPrice(150, diesel_baseline, dieselHigh, RED);
  }

  // accessors for buffer if main wants to copy to PSRAM
  uint16_t* getBuffer() { return canvas.getBuffer(); }
  int width() const { return canvas.width(); }
  int height() const { return canvas.height(); }

private:
  U8G2_FOR_ADAFRUIT_GFX &u8g2;
  GFXcanvas16 &canvas;
  int timeAreaH;

  // data
  String stationname;
  float e5=0, e10=0, diesel=0;
  float e5Low=99.999, e5High=0, e10Low=99.999, e10High=0, dieselLow=99.999, dieselHigh=0;

  // colors
  static constexpr uint16_t WHITE = 0xFFFF;
  static constexpr uint16_t YELLOW = 0xFFE0;
  static constexpr uint16_t GREEN = 0x07E0;
  static constexpr uint16_t RED = 0xF800;

  static inline uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
    return ((r/8)<<11)|((g/4)<<5)|(b/8);
  }

  // compute gradient color like previous logic; returns RGB565 color
  static uint16_t calcColor(float value, float low, float high) {
    if (low == high) return 0xFFFF; // WHITE
    int diff = (int)round(((high - value) / (high - low) * 100));
    uint8_t rval = 255, gval = 255;
    if (diff <= 50) { rval = 255; gval = map(diff, 0, 50, 0, 255); }
    else { gval = 255; rval = map(diff, 50, 100, 255, 0); }
    return rgb565(rval, gval, 0);
  }

  // drawPrice: renders price with 3 decimals; third decimal small and superscript
  void drawPrice(int x, int baselineY, float price, uint16_t color) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.3f", price);
    String s = String(buf);
    int dotPos = s.indexOf('.');
    String mainPart;
    String thirdDigit;
    if (dotPos >= 0 && (int)s.length() >= dotPos + 4) {
      mainPart = s.substring(0, dotPos + 3);   // e.g. "1.45"
      thirdDigit = s.substring(dotPos + 3, dotPos + 4);
    } else {
      mainPart = s;
      thirdDigit = "";
    }

    // approximate widths used in main sketch
    const int MAIN_W = 6;
    const int SMALL_W = 4;
    const int SMALL_SUPER_OFFSET = 6;

    u8g2.setForegroundColor(color);
    u8g2.setFont(u8g2_font_6x13_me);
    u8g2.setCursor(x, baselineY);
    u8g2.print(mainPart);

    int wMain = mainPart.length() * MAIN_W;

    if (thirdDigit.length() > 0) {
      u8g2.setFont(u8g2_font_tom_thumb_4x6_tf);
      u8g2.setCursor(x + wMain, baselineY - SMALL_SUPER_OFFSET);
      u8g2.print(thirdDigit);
    }

    int wSmall = (thirdDigit.length() > 0) ? (thirdDigit.length() * SMALL_W) : 0;
    u8g2.setFont(u8g2_font_6x13_me);
    u8g2.setCursor(x + wMain + wSmall + 2, baselineY);
    u8g2.print(" €");
  }
};

#endif // DATAMODULE_HPP