#include "WeatherIconCache.hpp"
#include "WeatherIcons_Main.hpp"

/*
 * Färbt einen RGB888 Pixel für Nachtmodus (Blau-Dim)
 * Kann für Nachticons beim Skalieren genutzt werden.
 */
static inline void apply_night_color(uint8_t& r, uint8_t& g, uint8_t& b, float dim=0.6f) {
    r = (uint8_t)(r * dim * 0.6f);
    g = (uint8_t)(g * dim * 0.7f);
    b = (uint8_t)((b * dim) + 64);
}

const WeatherIcon* WeatherIconCache::getScaled(const std::string& name, uint8_t targetSize, bool isNight) {
    Key key{name, targetSize, isNight};
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;

    // Hole Quellicon aus dem Set (Main und Special!)
    const WeatherIcon* src = globalWeatherIconSet.getIcon(name, isNight);
    bool needs_night_transform = false;
    // Fallback: Wenn kein Nachticon da, dimmen/umfärben
    if (!src && isNight) {
        src = globalWeatherIconSet.getIcon(name, false);
        needs_night_transform = true;
    }
    if (!src) src = globalWeatherIconSet.getUnknown();
    if (!src) return nullptr;

    WeatherIcon* scaled = scaleBilinear(src, targetSize, needs_night_transform);
    if (!scaled) return nullptr;
    cache[key] = scaled;
    return scaled;
}

void WeatherIconCache::clear() {
    for (auto& entry : cache) {
        if (entry.second && entry.second->data) {
            free((void*)entry.second->data);  // free() works for ps_malloc on ESP32
        }
        if (entry.second) {
            free(entry.second);  // Also allocated with ps_malloc
        }
    }
    cache.clear();
}

// Bilinear-Skalierer: RGB888 → Zielgröße, Nachtmodus optional, alles im PSRAM
WeatherIcon* WeatherIconCache::scaleBilinear(const WeatherIcon* src, uint8_t targetSize, bool doNightTransform) {
    if (!src || !src->data || !src->width || !src->height) return nullptr;
    
    // Always copy data to PSRAM, even if no scaling is needed
    // This ensures consistent memory handling and proper PROGMEM reading
    size_t src_bytes = (size_t)src->width * src->height * 3;
    size_t target_bytes = (size_t)targetSize * targetSize * 3;
    
    uint8_t* newData = (uint8_t*)ps_malloc(target_bytes);
    if (!newData) return nullptr;
    
    if (src->width == targetSize && src->height == targetSize) {
        // No scaling needed, but copy from PROGMEM to PSRAM
        for (size_t i = 0; i < src_bytes; i++) {
            newData[i] = pgm_read_byte(src->data + i);
        }
        
        // Apply night transform if needed
        if (doNightTransform) {
            for (size_t i = 0; i < target_bytes; i += 3) {
                apply_night_color(newData[i], newData[i+1], newData[i+2], 0.5f);
            }
        }
        
        WeatherIcon* icon = (WeatherIcon*)ps_malloc(sizeof(WeatherIcon));
        if (!icon) {
            free(newData);
            return nullptr;
        }
        icon->width = targetSize;
        icon->height = targetSize;
        icon->data = newData;
        return icon;
    }
    
    // Scaling needed - free and reallocate if sizes don't match
    if (src_bytes != target_bytes) {
        free(newData);
        newData = (uint8_t*)ps_malloc(target_bytes);
        if (!newData) return nullptr;
    }

    // Integer-Bilinear: src_x/src_y berechnen, Interpolation
    for (uint8_t y = 0; y < targetSize; ++y) {
        for (uint8_t x = 0; x < targetSize; ++x) {
            float gx = (float)x * (src->width-1) / (targetSize-1);
            float gy = (float)y * (src->height-1) / (targetSize-1);
            int ix = (int)gx;
            int iy = (int)gy;
            float fx = gx - ix;
            float fy = gy - iy;
            int idxA = ((iy)   * src->width + (ix))   * 3;
            int idxB = ((iy)   * src->width + (ix+1)) * 3;
            int idxC = ((iy+1) * src->width + (ix))   * 3;
            int idxD = ((iy+1) * src->width + (ix+1)) * 3;

            for (int c=0; c<3; ++c) {
                uint8_t a = pgm_read_byte(src->data + idxA + c);
                uint8_t b = (ix+1<src->width) ? pgm_read_byte(src->data + idxB + c) : a;
                uint8_t c_ = (iy+1<src->height) ? pgm_read_byte(src->data + idxC + c) : a;
                uint8_t d = (ix+1<src->width && iy+1<src->height) ? pgm_read_byte(src->data + idxD + c) : a;
                float val = a*(1-fx)*(1-fy) + b*fx*(1-fy) + c_*(1-fx)*fy + d*fx*fy;
                uint8_t out = (uint8_t)val;
                newData[(y*targetSize + x)*3 + c] = out;
            }
            if (doNightTransform) {
                apply_night_color(
                    newData[(y*targetSize + x)*3], 
                    newData[(y*targetSize + x)*3+1], 
                    newData[(y*targetSize + x)*3+2],
                    0.5f
                );
            }
        }
    }
    WeatherIcon* icon = (WeatherIcon*)ps_malloc(sizeof(WeatherIcon));
    if (!icon) {
        free(newData);
        return nullptr;
    }
    icon->width = targetSize;
    icon->height = targetSize;
    icon->data = newData;
    return icon;
}