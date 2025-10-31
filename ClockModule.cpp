#include "ClockModule.hpp"
#include <esp_heap_caps.h> // NEU: Für detaillierte Heap-Informationen

ClockModule::ClockModule(U8G2_FOR_ADAFRUIT_GFX &u8g2, GFXcanvas16 &canvas, GeneralTimeConverter& timeConverter)
  : u8g2(u8g2), canvas(canvas), timeConverter(timeConverter) {}

void ClockModule::setTime(const struct tm &t) {
  timeinfo = t;
}

void ClockModule::setSensorState(bool displayIsOn, time_t onTime, time_t offTime, float onPercentage) {
  isDisplayOn = displayIsOn;
  lastOnEventTime = onTime;
  lastOffEventTime = offTime;
  onPercentageValue = onPercentage;
}

void ClockModule::tick() {
  unsigned long now = millis();
  if (now - lastRssiUpdate > 30000 || lastRssiUpdate == 0) {
    if (WiFi.status() == WL_CONNECTED) {
      lastRssi = WiFi.RSSI();
    } else {
      lastRssi = -100; // kein WLAN
    }
    lastRssiUpdate = now;
  }
}

void ClockModule::draw() {
  canvas.fillScreen(0);
  canvas.drawRect(0, 0, canvas.width() - 1, canvas.height(), rgb565(128,128,128));
  drawWifiStrengthBar();
  u8g2.begin(canvas);
  u8g2.setFontMode(0);
  u8g2.setFontDirection(0);

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setForegroundColor(rgb565(255, 255, 255));
  u8g2.setCursor(7, 8);
  
  // =================================================================
  // ===================== PATCH: SPEICHER-ANZEIGE =====================
  // =================================================================
  char memBuf[50];
  multi_heap_info_t heap_info;
  heap_caps_get_info(&heap_info, MALLOC_CAP_INTERNAL);

  snprintf(memBuf, sizeof(memBuf), "G%.1f F%.1f M%.1f %d",
      (float)ESP.getHeapSize() / 1024.0,
      (float)ESP.getFreeHeap() / 1024.0,
      (float)ESP.getMaxAllocHeap() / 1024.0,
      heap_info.free_blocks);
  u8g2.print(memBuf);
  // ======================== ENDE PATCH ===============================


  u8g2.setForegroundColor(MAGENTA);
  u8g2.setBackgroundColor(BLACK);
  u8g2.setFont(u8g2_font_fub20_tf);
  u8g2.setCursor(7, 29);
  u8g2.print(&timeinfo, "%H:%M:%S");

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setForegroundColor(YELLOW);
  u8g2.setCursor(123, 18);
  u8g2.print(&timeinfo, "%d.%m.%Y");

  u8g2.setForegroundColor(rgb565(0,255,0));
  u8g2.setCursor(123, 9);
  switch (timeinfo.tm_wday) {
    case 0: u8g2.print("Sonntag"); break;
    case 1: u8g2.print("Montag"); break;
    case 2: u8g2.print("Dienstag"); break;
    case 3: u8g2.print("Mittwoch"); break;
    case 4: u8g2.print("Donnerstag"); break;
    case 5: u8g2.print("Freitag"); break;
    case 6: u8g2.print("Samstag"); break;
    default: break;
  }

  u8g2.setCursor(123, 27);
  u8g2.setForegroundColor(CYAN);
  u8g2.print(&timeinfo, "T:%j ");
  {
    char wkbuf[8];
    int kw = isoWeekNumber(timeinfo);
    if (kw < 1) kw = 1;
    snprintf(wkbuf, sizeof(wkbuf), "KW:%02d", kw);
    u8g2.print(wkbuf);
  }
}

// ENTFERNT: getBuffer(), width(), height()

uint16_t ClockModule::rgb565(uint8_t r,uint8_t g,uint8_t b){
  return ((r/8)<<11)|((g/4)<<5)|(b/8);
}

void ClockModule::drawWifiStrengthBar() {
  int x0 = 1;
  int x1 = 2;
  int yTop = 1;
  int yBot = canvas.height() - 2;

  for (int y = yTop; y <= yBot; y++) {
    float rel = float(yBot - y) / float(yBot - yTop);
    uint8_t r_bg = 10 + uint8_t(20 * (1.0 - rel));
    uint8_t g_bg = 10 + uint8_t(80 * rel);
    uint16_t col_bg = rgb565(r_bg, g_bg, 0);
    canvas.drawPixel(x0, y, col_bg);
    canvas.drawPixel(x1, y, col_bg);
  }

  int rssi = lastRssi;
  if (rssi < -100) rssi = -100;
  if (rssi > -40) rssi = -40;
  float frac = float(rssi + 100) / 60.0f;
  int yRssiTop = yBot - round(frac * (yBot - yTop));

  for (int y = yRssiTop; y <= yBot; y++) {
    float columnRel = float(yBot - y) / float(yBot - yRssiTop);
    uint8_t r_fg = 220 - uint8_t(120 * columnRel);
    uint8_t g_fg = 60 + uint8_t(195 * columnRel);
    uint16_t col_fg = rgb565(r_fg, g_fg, 0);
    canvas.drawPixel(x0, y, col_fg);
    canvas.drawPixel(x1, y, col_fg);
  }

  for (int y = 1; y <= yBot; y++) {
    canvas.drawPixel(x0+2, y, rgb565(128, 128, 128));
  }
}

int ClockModule::isoWeekNumber(const struct tm &t) {
  struct tm tmp = t;
  time_t tt = mktime(&tmp);
  if (tt == (time_t)-1) return 0;
  struct tm normalized;
  localtime_r(&tt, &normalized);
  int wday = normalized.tm_wday == 0 ? 7 : normalized.tm_wday;
  int daysToThursday = 4 - wday;
  time_t thursday_tt = tt + (time_t)daysToThursday * 24 * 3600;
  struct tm thursday_tm;
  localtime_r(&thursday_tt, &thursday_tm);
  int isoYear = thursday_tm.tm_year + 1900;
  struct tm jan1 = {0};
  jan1.tm_year = isoYear - 1900;
  jan1.tm_mon = 0;
  jan1.tm_mday = 1;
  jan1.tm_hour = 0; jan1.tm_min = 0; jan1.tm_sec = 0;
  time_t jan1_tt = mktime(&jan1);
  if (jan1_tt == (time_t)-1) return 0;
  struct tm jan1_tm; localtime_r(&jan1_tt, &jan1_tm);
  int jan1_wday = jan1_tm.tm_wday == 0 ? 7 : jan1_tm.tm_wday;
  int week1_monday_offset = (jan1_wday <= 4) ? (1 - jan1_wday) : (8 - jan1_wday);
  time_t week1_monday_tt = jan1_tt + (time_t)week1_monday_offset * 24 * 3600;
  int weekno = (int)((thursday_tt - week1_monday_tt) / (7 * 24 * 3600)) + 1;
  return weekno;
}