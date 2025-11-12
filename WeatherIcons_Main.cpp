#include "WeatherIcons_Main.hpp"
#include "WeatherIconCache.hpp"

// ================= WEATHER ICONS WMO ==================
// Jede ICON-Variable ist ein RGB888-Array für den Zustand.
// Dummy: { 0x000000 }. Ein echtes Icon hat 48*48*3 Werte!

// ICON: CLEAR (WMO 0, Tag)
const uint8_t clear_48[] PROGMEM = { 0x000000 };
// ICON: CLEAR (WMO 0, Nacht)
const uint8_t clear_night_48[] PROGMEM = { 0x000000 };

// ICON: MAINLY CLEAR (WMO 1, Tag)
const uint8_t mainly_clear_48[] PROGMEM = { 0x000000 };
// ICON: MAINLY CLEAR (WMO 1, Nacht)
const uint8_t mainly_clear_night_48[] PROGMEM = { 0x000000 };

// ICON: PARTLY CLOUDY (WMO 2, Tag)
const uint8_t partly_cloudy_48[] PROGMEM = { 0x000000 };
// ICON: PARTLY CLOUDY (WMO 2, Nacht)
const uint8_t partly_cloudy_night_48[] PROGMEM = { 0x000000 };

// ICON: OVERCAST (WMO 3)
const uint8_t overcast_48[] PROGMEM = { 0x000000 };

// ICON: FOG (WMO 45)
const uint8_t fog_48[] PROGMEM = { 0x000000 };

// ICON: RIME FOG (WMO 48)
const uint8_t rime_fog_48[] PROGMEM = { 0x000000 };

// ICON: DRIZZLE LIGHT (WMO 51)
const uint8_t drizzle_light_48[] PROGMEM = { 0x000000 };
// ICON: DRIZZLE MODERATE (WMO 53)
const uint8_t drizzle_moderate_48[] PROGMEM = { 0x000000 };
// ICON: DRIZZLE DENSE (WMO 55)
const uint8_t drizzle_dense_48[] PROGMEM = { 0x000000 };

// ICON: FREEZING DRIZZLE LIGHT (WMO 56)
const uint8_t freezing_drizzle_light_48[] PROGMEM = { 0x000000 };
// ICON: FREEZING DRIZZLE DENSE (WMO 57)
const uint8_t freezing_drizzle_dense_48[] PROGMEM = { 0x000000 };

// ICON: RAIN LIGHT (WMO 61, Tag)
const uint8_t rain_light_48[] PROGMEM = { 0x000000 };
// ICON: RAIN LIGHT (WMO 61, Nacht)
const uint8_t rain_light_night_48[] PROGMEM = { 0x000000 };

// ICON: RAIN MODERATE (WMO 63, Tag)
const uint8_t rain_moderate_48[] PROGMEM = { 0x000000 };
// ICON: RAIN MODERATE (WMO 63, Nacht)
const uint8_t rain_moderate_night_48[] PROGMEM = { 0x000000 };

// ICON: RAIN HEAVY (WMO 65)
const uint8_t rain_heavy_48[] PROGMEM = { 0x000000 };

// ICON: FREEZING RAIN LIGHT (WMO 66)
const uint8_t freezing_rain_light_48[] PROGMEM = { 0x000000 };
// ICON: FREEZING RAIN HEAVY (WMO 67)
const uint8_t freezing_rain_heavy_48[] PROGMEM = { 0x000000 };

// ICON: SNOW LIGHT (WMO 71, Tag)
const uint8_t snow_light_48[] PROGMEM = { 0x000000 };
// ICON: SNOW LIGHT (WMO 71, Nacht)
const uint8_t snow_light_night_48[] PROGMEM = { 0x000000 };

// ICON: SNOW MODERATE (WMO 73, Tag)
const uint8_t snow_moderate_48[] PROGMEM = { 0x000000 };
// ICON: SNOW MODERATE (WMO 73, Nacht)
const uint8_t snow_moderate_night_48[] PROGMEM = { 0x000000 };

// ICON: SNOW HEAVY (WMO 75)
const uint8_t snow_heavy_48[] PROGMEM = { 0x000000 };

// ICON: SNOW GRAINS (WMO 77)
const uint8_t snow_grains_48[] PROGMEM = { 0x000000 };

// ICON: SHOWERS LIGHT (WMO 80, Tag)
const uint8_t showers_light_48[] PROGMEM = { 0x000000 };
// ICON: SHOWERS LIGHT (WMO 80, Nacht)
const uint8_t showers_light_night_48[] PROGMEM = { 0x000000 };

// ICON: SHOWERS MODERATE (WMO 81, Tag)
const uint8_t showers_moderate_48[] PROGMEM = { 0x000000 };
// ICON: SHOWERS MODERATE (WMO 81, Nacht)
const uint8_t showers_moderate_night_48[] PROGMEM = { 0x000000 };

// ICON: SHOWERS HEAVY (WMO 82)
const uint8_t showers_heavy_48[] PROGMEM = { 0x000000 };

// ICON: SNOW SHOWERS LIGHT (WMO 85, Tag)
const uint8_t snow_showers_light_48[] PROGMEM = { 0x000000 };
// ICON: SNOW SHOWERS LIGHT (WMO 85, Nacht)
const uint8_t snow_showers_light_night_48[] PROGMEM = { 0x000000 };

// ICON: SNOW SHOWERS HEAVY (WMO 86)
const uint8_t snow_showers_heavy_48[] PROGMEM = { 0x000000 };

// ICON: THUNDERSTORM (WMO 95)
const uint8_t thunderstorm_48[] PROGMEM = { 0x000000 };

// ICON: THUNDERSTORM LIGHT HAIL (WMO 96)
const uint8_t thunderstorm_light_hail_48[] PROGMEM = { 0x000000 };

// ICON: THUNDERSTORM HEAVY HAIL (WMO 99)
const uint8_t thunderstorm_heavy_hail_48[] PROGMEM = { 0x000000 };

// ICON: UNKNOWN FALLBACK
const uint8_t unknown_48[] PROGMEM = { 0x000000 };

// ---------------------- ICON-OBJEKTE GANZ AM ENDE ----------------------
// Reihenfolge identisch wie Arrays. Tag/Nacht immer beide registriert.
const WeatherIcon icon_clear_48 = { clear_48, 48, 48 };
const WeatherIcon icon_clear_night_48 = { clear_night_48, 48, 48 };
const WeatherIcon icon_mainly_clear_48 = { mainly_clear_48, 48, 48 };
const WeatherIcon icon_mainly_clear_night_48 = { mainly_clear_night_48, 48, 48 };
const WeatherIcon icon_partly_cloudy_48 = { partly_cloudy_48, 48, 48 };
const WeatherIcon icon_partly_cloudy_night_48 = { partly_cloudy_night_48, 48, 48 };
const WeatherIcon icon_overcast_48 = { overcast_48, 48, 48 };
const WeatherIcon icon_fog_48 = { fog_48, 48, 48 };
const WeatherIcon icon_rime_fog_48 = { rime_fog_48, 48, 48 };
const WeatherIcon icon_drizzle_light_48 = { drizzle_light_48, 48, 48 };
const WeatherIcon icon_drizzle_moderate_48 = { drizzle_moderate_48, 48, 48 };
const WeatherIcon icon_drizzle_dense_48 = { drizzle_dense_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_light_48 = { freezing_drizzle_light_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_dense_48 = { freezing_drizzle_dense_48, 48, 48 };
const WeatherIcon icon_rain_light_48 = { rain_light_48, 48, 48 };
const WeatherIcon icon_rain_light_night_48 = { rain_light_night_48, 48, 48 };
const WeatherIcon icon_rain_moderate_48 = { rain_moderate_48, 48, 48 };
const WeatherIcon icon_rain_moderate_night_48 = { rain_moderate_night_48, 48, 48 };
const WeatherIcon icon_rain_heavy_48 = { rain_heavy_48, 48, 48 };
const WeatherIcon icon_freezing_rain_light_48 = { freezing_rain_light_48, 48, 48 };
const WeatherIcon icon_freezing_rain_heavy_48 = { freezing_rain_heavy_48, 48, 48 };
const WeatherIcon icon_snow_light_48 = { snow_light_48, 48, 48 };
const WeatherIcon icon_snow_light_night_48 = { snow_light_night_48, 48, 48 };
const WeatherIcon icon_snow_moderate_48 = { snow_moderate_48, 48, 48 };
const WeatherIcon icon_snow_moderate_night_48 = { snow_moderate_night_48, 48, 48 };
const WeatherIcon icon_snow_heavy_48 = { snow_heavy_48, 48, 48 };
const WeatherIcon icon_snow_grains_48 = { snow_grains_48, 48, 48 };
const WeatherIcon icon_showers_light_48 = { showers_light_48, 48, 48 };
const WeatherIcon icon_showers_light_night_48 = { showers_light_night_48, 48, 48 };
const WeatherIcon icon_showers_moderate_48 = { showers_moderate_48, 48, 48 };
const WeatherIcon icon_showers_moderate_night_48 = { showers_moderate_night_48, 48, 48 };
const WeatherIcon icon_showers_heavy_48 = { showers_heavy_48, 48, 48 };
const WeatherIcon icon_snow_showers_light_48 = { snow_showers_light_48, 48, 48 };
const WeatherIcon icon_snow_showers_light_night_48 = { snow_showers_light_night_48, 48, 48 };
const WeatherIcon icon_snow_showers_heavy_48 = { snow_showers_heavy_48, 48, 48 };
const WeatherIcon icon_thunderstorm_48 = { thunderstorm_48, 48, 48 };
const WeatherIcon icon_thunderstorm_light_hail_48 = { thunderstorm_light_hail_48, 48, 48 };
const WeatherIcon icon_thunderstorm_heavy_hail_48 = { thunderstorm_heavy_hail_48, 48, 48 };
const WeatherIcon icon_unknown_48 = { unknown_48, 48, 48 };

// Global instances
WeatherIconSet globalWeatherIconSet;
WeatherIconCache globalWeatherIconCache;

// Register all main weather icons
void registerWeatherIcons() {
    // Set unknown icon first
    globalWeatherIconSet.setUnknown(&icon_unknown_48);
    
    // Register WMO weather icons with day/night variants
    globalWeatherIconSet.registerIcon("clear", &icon_clear_48, &icon_clear_night_48);
    globalWeatherIconSet.registerIcon("mainly_clear", &icon_mainly_clear_48, &icon_mainly_clear_night_48);
    globalWeatherIconSet.registerIcon("partly_cloudy", &icon_partly_cloudy_48, &icon_partly_cloudy_night_48);
    globalWeatherIconSet.registerIcon("overcast", &icon_overcast_48);
    globalWeatherIconSet.registerIcon("fog", &icon_fog_48);
    globalWeatherIconSet.registerIcon("rime_fog", &icon_rime_fog_48);
    globalWeatherIconSet.registerIcon("drizzle_light", &icon_drizzle_light_48);
    globalWeatherIconSet.registerIcon("drizzle_moderate", &icon_drizzle_moderate_48);
    globalWeatherIconSet.registerIcon("drizzle_dense", &icon_drizzle_dense_48);
    globalWeatherIconSet.registerIcon("freezing_drizzle_light", &icon_freezing_drizzle_light_48);
    globalWeatherIconSet.registerIcon("freezing_drizzle_dense", &icon_freezing_drizzle_dense_48);
    globalWeatherIconSet.registerIcon("rain_light", &icon_rain_light_48, &icon_rain_light_night_48);
    globalWeatherIconSet.registerIcon("rain_moderate", &icon_rain_moderate_48, &icon_rain_moderate_night_48);
    globalWeatherIconSet.registerIcon("rain_heavy", &icon_rain_heavy_48);
    globalWeatherIconSet.registerIcon("freezing_rain_light", &icon_freezing_rain_light_48);
    globalWeatherIconSet.registerIcon("freezing_rain_heavy", &icon_freezing_rain_heavy_48);
    globalWeatherIconSet.registerIcon("snow_light", &icon_snow_light_48, &icon_snow_light_night_48);
    globalWeatherIconSet.registerIcon("snow_moderate", &icon_snow_moderate_48, &icon_snow_moderate_night_48);
    globalWeatherIconSet.registerIcon("snow_heavy", &icon_snow_heavy_48);
    globalWeatherIconSet.registerIcon("snow_grains", &icon_snow_grains_48);
    globalWeatherIconSet.registerIcon("showers_light", &icon_showers_light_48, &icon_showers_light_night_48);
    globalWeatherIconSet.registerIcon("showers_moderate", &icon_showers_moderate_48, &icon_showers_moderate_night_48);
    globalWeatherIconSet.registerIcon("showers_heavy", &icon_showers_heavy_48);
    globalWeatherIconSet.registerIcon("snow_showers_light", &icon_snow_showers_light_48, &icon_snow_showers_light_night_48);
    globalWeatherIconSet.registerIcon("snow_showers_heavy", &icon_snow_showers_heavy_48);
    globalWeatherIconSet.registerIcon("thunderstorm", &icon_thunderstorm_48);
    globalWeatherIconSet.registerIcon("thunderstorm_light_hail", &icon_thunderstorm_light_hail_48);
    globalWeatherIconSet.registerIcon("thunderstorm_heavy_hail", &icon_thunderstorm_heavy_hail_48);
    globalWeatherIconSet.registerIcon("unknown", &icon_unknown_48);
}