#include "WeatherIcons_Main.hpp"
#include "WeatherIconCache.hpp"

// ================= WEATHER ICONS WMO ==================
// Jede ICON-Variable ist ein RGB888-Array für den Zustand.
// Dummy: { 0x000000 }. Ein echtes Icon hat 48*48*3 Werte!

// ICON: CLEAR (WMO 0, Tag)
const uint8_t clear_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: CLEAR (WMO 0, Nacht)
const uint8_t clear_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: MAINLY CLEAR (WMO 1, Tag)
const uint8_t mainly_clear_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: MAINLY CLEAR (WMO 1, Nacht)
const uint8_t mainly_clear_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: PARTLY CLOUDY (WMO 2, Tag)
const uint8_t partly_cloudy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: PARTLY CLOUDY (WMO 2, Nacht)
const uint8_t partly_cloudy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: OVERCAST (WMO 3, Tag)
const uint8_t overcast_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: OVERCAST (WMO 3, Nacht)
const uint8_t overcast_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FOG (WMO 45, Tag)
const uint8_t fog_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FOG (WMO 45, Nacht)
const uint8_t fog_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: RIME FOG (WMO 48, Tag)
const uint8_t rime_fog_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: RIME FOG (WMO 48, Nacht)
const uint8_t rime_fog_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: DRIZZLE LIGHT (WMO 51, Tag)
const uint8_t drizzle_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: DRIZZLE LIGHT (WMO 51, Nacht)
const uint8_t drizzle_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: DRIZZLE MODERATE (WMO 53, Tag)
const uint8_t drizzle_moderate_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: DRIZZLE MODERATE (WMO 53, Nacht)
const uint8_t drizzle_moderate_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: DRIZZLE DENSE (WMO 55, Tag)
const uint8_t drizzle_dense_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: DRIZZLE DENSE (WMO 55, Nacht)
const uint8_t drizzle_dense_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: FREEZING DRIZZLE LIGHT (WMO 56, Tag)
const uint8_t freezing_drizzle_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING DRIZZLE LIGHT (WMO 56, Nacht)
const uint8_t freezing_drizzle_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING DRIZZLE DENSE (WMO 57, Tag)
const uint8_t freezing_drizzle_dense_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING DRIZZLE DENSE (WMO 57, Nacht)
const uint8_t freezing_drizzle_dense_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: RAIN LIGHT (WMO 61, Tag)
const uint8_t rain_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: RAIN LIGHT (WMO 61, Nacht)
const uint8_t rain_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: RAIN MODERATE (WMO 63, Tag)
const uint8_t rain_moderate_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: RAIN MODERATE (WMO 63, Nacht)
const uint8_t rain_moderate_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: RAIN HEAVY (WMO 65, Tag)
const uint8_t rain_heavy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: RAIN HEAVY (WMO 65, Nacht)
const uint8_t rain_heavy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: FREEZING RAIN LIGHT (WMO 66, Tag)
const uint8_t freezing_rain_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING RAIN LIGHT (WMO 66, Nacht)
const uint8_t freezing_rain_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING RAIN HEAVY (WMO 67, Tag)
const uint8_t freezing_rain_heavy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: FREEZING RAIN HEAVY (WMO 67, Nacht)
const uint8_t freezing_rain_heavy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW LIGHT (WMO 71, Tag)
const uint8_t snow_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW LIGHT (WMO 71, Nacht)
const uint8_t snow_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW MODERATE (WMO 73, Tag)
const uint8_t snow_moderate_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW MODERATE (WMO 73, Nacht)
const uint8_t snow_moderate_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW HEAVY (WMO 75, Tag)
const uint8_t snow_heavy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW HEAVY (WMO 75, Nacht)
const uint8_t snow_heavy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW GRAINS (WMO 77, Tag)
const uint8_t snow_grains_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW GRAINS (WMO 77, Nacht)
const uint8_t snow_grains_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SHOWERS LIGHT (WMO 80, Tag)
const uint8_t showers_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SHOWERS LIGHT (WMO 80, Nacht)
const uint8_t showers_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SHOWERS MODERATE (WMO 81, Tag)
const uint8_t showers_moderate_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SHOWERS MODERATE (WMO 81, Nacht)
const uint8_t showers_moderate_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SHOWERS HEAVY (WMO 82, Tag)
const uint8_t showers_heavy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SHOWERS HEAVY (WMO 82, Nacht)
const uint8_t showers_heavy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW SHOWERS LIGHT (WMO 85, Tag)
const uint8_t snow_showers_light_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW SHOWERS LIGHT (WMO 85, Nacht)
const uint8_t snow_showers_light_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: SNOW SHOWERS HEAVY (WMO 86, Tag)
const uint8_t snow_showers_heavy_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: SNOW SHOWERS HEAVY (WMO 86, Nacht)
const uint8_t snow_showers_heavy_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: THUNDERSTORM (WMO 95, Tag)
const uint8_t thunderstorm_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: THUNDERSTORM (WMO 95, Nacht)
const uint8_t thunderstorm_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: THUNDERSTORM LIGHT HAIL (WMO 96, Tag)
const uint8_t thunderstorm_light_hail_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: THUNDERSTORM LIGHT HAIL (WMO 96, Nacht)
const uint8_t thunderstorm_light_hail_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: THUNDERSTORM HEAVY HAIL (WMO 99, Tag)
const uint8_t thunderstorm_heavy_hail_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };
// ICON: THUNDERSTORM HEAVY HAIL (WMO 99, Nacht)
const uint8_t thunderstorm_heavy_hail_night_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ICON: UNKNOWN FALLBACK
const uint8_t unknown_48[] PROGMEM = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A };

// ---------------------- ICON-OBJEKTE GANZ AM ENDE ----------------------
// Reihenfolge identisch wie Arrays. Tag/Nacht immer beide registriert.
const WeatherIcon icon_clear_48 = { clear_48, 48, 48 };
const WeatherIcon icon_clear_night_48 = { clear_night_48, 48, 48 };
const WeatherIcon icon_mainly_clear_48 = { mainly_clear_48, 48, 48 };
const WeatherIcon icon_mainly_clear_night_48 = { mainly_clear_night_48, 48, 48 };
const WeatherIcon icon_partly_cloudy_48 = { partly_cloudy_48, 48, 48 };
const WeatherIcon icon_partly_cloudy_night_48 = { partly_cloudy_night_48, 48, 48 };
const WeatherIcon icon_overcast_48 = { overcast_48, 48, 48 };
const WeatherIcon icon_overcast_night_48 = { overcast_night_48, 48, 48 };
const WeatherIcon icon_fog_48 = { fog_48, 48, 48 };
const WeatherIcon icon_fog_night_48 = { fog_night_48, 48, 48 };
const WeatherIcon icon_rime_fog_48 = { rime_fog_48, 48, 48 };
const WeatherIcon icon_rime_fog_night_48 = { rime_fog_night_48, 48, 48 };
const WeatherIcon icon_drizzle_light_48 = { drizzle_light_48, 48, 48 };
const WeatherIcon icon_drizzle_light_night_48 = { drizzle_light_night_48, 48, 48 };
const WeatherIcon icon_drizzle_moderate_48 = { drizzle_moderate_48, 48, 48 };
const WeatherIcon icon_drizzle_moderate_night_48 = { drizzle_moderate_night_48, 48, 48 };
const WeatherIcon icon_drizzle_dense_48 = { drizzle_dense_48, 48, 48 };
const WeatherIcon icon_drizzle_dense_night_48 = { drizzle_dense_night_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_light_48 = { freezing_drizzle_light_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_light_night_48 = { freezing_drizzle_light_night_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_dense_48 = { freezing_drizzle_dense_48, 48, 48 };
const WeatherIcon icon_freezing_drizzle_dense_night_48 = { freezing_drizzle_dense_night_48, 48, 48 };
const WeatherIcon icon_rain_light_48 = { rain_light_48, 48, 48 };
const WeatherIcon icon_rain_light_night_48 = { rain_light_night_48, 48, 48 };
const WeatherIcon icon_rain_moderate_48 = { rain_moderate_48, 48, 48 };
const WeatherIcon icon_rain_moderate_night_48 = { rain_moderate_night_48, 48, 48 };
const WeatherIcon icon_rain_heavy_48 = { rain_heavy_48, 48, 48 };
const WeatherIcon icon_rain_heavy_night_48 = { rain_heavy_night_48, 48, 48 };
const WeatherIcon icon_freezing_rain_light_48 = { freezing_rain_light_48, 48, 48 };
const WeatherIcon icon_freezing_rain_light_night_48 = { freezing_rain_light_night_48, 48, 48 };
const WeatherIcon icon_freezing_rain_heavy_48 = { freezing_rain_heavy_48, 48, 48 };
const WeatherIcon icon_freezing_rain_heavy_night_48 = { freezing_rain_heavy_night_48, 48, 48 };
const WeatherIcon icon_snow_light_48 = { snow_light_48, 48, 48 };
const WeatherIcon icon_snow_light_night_48 = { snow_light_night_48, 48, 48 };
const WeatherIcon icon_snow_moderate_48 = { snow_moderate_48, 48, 48 };
const WeatherIcon icon_snow_moderate_night_48 = { snow_moderate_night_48, 48, 48 };
const WeatherIcon icon_snow_heavy_48 = { snow_heavy_48, 48, 48 };
const WeatherIcon icon_snow_heavy_night_48 = { snow_heavy_night_48, 48, 48 };
const WeatherIcon icon_snow_grains_48 = { snow_grains_48, 48, 48 };
const WeatherIcon icon_snow_grains_night_48 = { snow_grains_night_48, 48, 48 };
const WeatherIcon icon_showers_light_48 = { showers_light_48, 48, 48 };
const WeatherIcon icon_showers_light_night_48 = { showers_light_night_48, 48, 48 };
const WeatherIcon icon_showers_moderate_48 = { showers_moderate_48, 48, 48 };
const WeatherIcon icon_showers_moderate_night_48 = { showers_moderate_night_48, 48, 48 };
const WeatherIcon icon_showers_heavy_48 = { showers_heavy_48, 48, 48 };
const WeatherIcon icon_showers_heavy_night_48 = { showers_heavy_night_48, 48, 48 };
const WeatherIcon icon_snow_showers_light_48 = { snow_showers_light_48, 48, 48 };
const WeatherIcon icon_snow_showers_light_night_48 = { snow_showers_light_night_48, 48, 48 };
const WeatherIcon icon_snow_showers_heavy_48 = { snow_showers_heavy_48, 48, 48 };
const WeatherIcon icon_snow_showers_heavy_night_48 = { snow_showers_heavy_night_48, 48, 48 };
const WeatherIcon icon_thunderstorm_48 = { thunderstorm_48, 48, 48 };
const WeatherIcon icon_thunderstorm_night_48 = { thunderstorm_night_48, 48, 48 };
const WeatherIcon icon_thunderstorm_light_hail_48 = { thunderstorm_light_hail_48, 48, 48 };
const WeatherIcon icon_thunderstorm_light_hail_night_48 = { thunderstorm_light_hail_night_48, 48, 48 };
const WeatherIcon icon_thunderstorm_heavy_hail_48 = { thunderstorm_heavy_hail_48, 48, 48 };
const WeatherIcon icon_thunderstorm_heavy_hail_night_48 = { thunderstorm_heavy_hail_night_48, 48, 48 };
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
    globalWeatherIconSet.registerIcon("overcast", &icon_overcast_48, &icon_overcast_night_48);
    globalWeatherIconSet.registerIcon("fog", &icon_fog_48, &icon_fog_night_48);
    globalWeatherIconSet.registerIcon("rime_fog", &icon_rime_fog_48, &icon_rime_fog_night_48);
    globalWeatherIconSet.registerIcon("drizzle_light", &icon_drizzle_light_48, &icon_drizzle_light_night_48);
    globalWeatherIconSet.registerIcon("drizzle_moderate", &icon_drizzle_moderate_48, &icon_drizzle_moderate_night_48);
    globalWeatherIconSet.registerIcon("drizzle_dense", &icon_drizzle_dense_48, &icon_drizzle_dense_night_48);
    globalWeatherIconSet.registerIcon("freezing_drizzle_light", &icon_freezing_drizzle_light_48, &icon_freezing_drizzle_light_night_48);
    globalWeatherIconSet.registerIcon("freezing_drizzle_dense", &icon_freezing_drizzle_dense_48, &icon_freezing_drizzle_dense_night_48);
    globalWeatherIconSet.registerIcon("rain_light", &icon_rain_light_48, &icon_rain_light_night_48);
    globalWeatherIconSet.registerIcon("rain_moderate", &icon_rain_moderate_48, &icon_rain_moderate_night_48);
    globalWeatherIconSet.registerIcon("rain_heavy", &icon_rain_heavy_48, &icon_rain_heavy_night_48);
    globalWeatherIconSet.registerIcon("freezing_rain_light", &icon_freezing_rain_light_48, &icon_freezing_rain_light_night_48);
    globalWeatherIconSet.registerIcon("freezing_rain_heavy", &icon_freezing_rain_heavy_48, &icon_freezing_rain_heavy_night_48);
    globalWeatherIconSet.registerIcon("snow_light", &icon_snow_light_48, &icon_snow_light_night_48);
    globalWeatherIconSet.registerIcon("snow_moderate", &icon_snow_moderate_48, &icon_snow_moderate_night_48);
    globalWeatherIconSet.registerIcon("snow_heavy", &icon_snow_heavy_48, &icon_snow_heavy_night_48);
    globalWeatherIconSet.registerIcon("snow_grains", &icon_snow_grains_48, &icon_snow_grains_night_48);
    globalWeatherIconSet.registerIcon("showers_light", &icon_showers_light_48, &icon_showers_light_night_48);
    globalWeatherIconSet.registerIcon("showers_moderate", &icon_showers_moderate_48, &icon_showers_moderate_night_48);
    globalWeatherIconSet.registerIcon("showers_heavy", &icon_showers_heavy_48, &icon_showers_heavy_night_48);
    globalWeatherIconSet.registerIcon("snow_showers_light", &icon_snow_showers_light_48, &icon_snow_showers_light_night_48);
    globalWeatherIconSet.registerIcon("snow_showers_heavy", &icon_snow_showers_heavy_48, &icon_snow_showers_heavy_night_48);
    globalWeatherIconSet.registerIcon("thunderstorm", &icon_thunderstorm_48, &icon_thunderstorm_night_48);
    globalWeatherIconSet.registerIcon("thunderstorm_light_hail", &icon_thunderstorm_light_hail_48, &icon_thunderstorm_light_hail_night_48);
    globalWeatherIconSet.registerIcon("thunderstorm_heavy_hail", &icon_thunderstorm_heavy_hail_48, &icon_thunderstorm_heavy_hail_night_48);
    globalWeatherIconSet.registerIcon("unknown", &icon_unknown_48);
}