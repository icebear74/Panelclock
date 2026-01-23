#pragma once
#include <map>
#include <string>
static const std::map<int, std::string> wmo_code_to_icon = {
    {0, "clear"}, 
    {1, "mainly_clear"}, 
    {2, "partly_cloudy"}, 
    {3, "overcast"},
    {45, "fog"}, 
    {48, "rime_fog"},
    {51, "drizzle_light"}, 
    {53, "drizzle_moderate"}, 
    {55, "drizzle_dense"},
    {56, "freezing_drizzle_light"}, 
    {57, "freezing_drizzle_dense"},
    {61, "rain_light"}, 
    {63, "rain_moderate"}, 
    {65, "rain_heavy"},
    {66, "freezing_rain_light"}, 
    {67, "freezing_rain_heavy"},
    {71, "snow_light"}, 
    {73, "snow_moderate"}, 
    {75, "snow_heavy"}, 
    {77, "snow_grains"},
    {80, "showers_light"}, 
    {81, "showers_moderate"}, 
    {82, "showers_heavy"},
    {85, "snow_showers_light"}, 
    {86, "snow_showers_heavy"},
    {95, "thunderstorm"}, 
    {96, "thunderstorm_light_hail"}, 
    {99, "thunderstorm_heavy_hail"},
};
inline std::string mapWeatherCodeToIcon(int code, bool is_day) {
    auto it=wmo_code_to_icon.find(code);
    if(it==wmo_code_to_icon.end()) return "unknown";
    return is_day?it->second:it->second+"_night";
}