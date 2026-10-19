#pragma once
#include <cstddef>
#include <cstring>

// Shared protocol definition: mirrored in the player include directory.
// Keep this outside Settings_t so IVideoRenderer's binary layout is unchanged.
struct AmbilightSettings {
    bool enabled = true;
    int intensity = 53;
    int blur = 55;
    int edge = 27;
    int spread = 55;
    int fadeStart = 15;
    int fadeCurve = 35;
    int brightness = 100;
    int contrast = 100;
    int vibrance = 100;
    int saturation = 100;
    int debanding = 0;
    int directions = 15; // top=1, right=2, bottom=4, left=8
};

struct AmbilightSettingDefinition {
    const char* key;
    const wchar_t* label;
    int minimum, maximum, initial;
    int AmbilightSettings::*member;
};
inline constexpr AmbilightSettingDefinition AmbilightSettingDefinitions[] = {
    {"ambilightIntensity", L"Intensity", 0, 100, 53, &AmbilightSettings::intensity},
    {"ambilightBlur", L"Blur", 0, 100, 55, &AmbilightSettings::blur},
    {"ambilightEdge", L"Edge size", 2, 50, 27, &AmbilightSettings::edge},
    {"ambilightSpread", L"Spread", 0, 400, 55, &AmbilightSettings::spread},
    {"ambilightFadeStart", L"Spread fade start", -50, 100, 15, &AmbilightSettings::fadeStart},
    {"ambilightFadeCurve", L"Spread fade curve", 1, 100, 35, &AmbilightSettings::fadeCurve},
    {"ambilightBrightness", L"Brightness", 0, 200, 100, &AmbilightSettings::brightness},
    {"ambilightContrast", L"Contrast", 0, 200, 100, &AmbilightSettings::contrast},
    {"ambilightVibrance", L"Colors", 0, 200, 100, &AmbilightSettings::vibrance},
    {"ambilightSaturation", L"Saturation", 0, 200, 100, &AmbilightSettings::saturation},
    {"ambilightDebanding", L"Debanding (noise)", 0, 100, 0, &AmbilightSettings::debanding},
    {"ambilightDirections", L"Directions", 0, 15, 15, &AmbilightSettings::directions},
};
inline constexpr size_t AmbilightSettingCount = sizeof(AmbilightSettingDefinitions) / sizeof(AmbilightSettingDefinitions[0]);
inline constexpr size_t AmbilightSliderCount = AmbilightSettingCount - 1;
inline const AmbilightSettingDefinition* FindAmbilightSetting(const char* key) {
    for (const auto& definition : AmbilightSettingDefinitions) {
        if (!strcmp(key, definition.key)) return &definition;
    }
    return nullptr;
}
