#pragma once

#include "raylib.h"
#include "string_view"

namespace Config::Paths {
    inline constexpr std::string_view Assets   = "../assets/";
    inline constexpr std::string_view Textures = "../assets/textures/";
    inline constexpr std::string_view Shaders  = "../assets/shaders/";
    inline constexpr std::string_view Models   = "../assets/models/";
}

namespace Config::EngineSettings {
    // Screen and Camera
    inline constexpr int ScreenWidth             = 1920;
    inline constexpr int ScreenHeight            = 1080;
    inline constexpr float CameraFOV             = 45.0f;
    inline constexpr Vector3 CameraOrientation   = {0.0f, 1.0f, 0.0f};
    inline constexpr Vector3 CameraViewDirection = {0.0f, 0.0f, 0.0f};
    inline constexpr Vector3 CameraPosition      = {150.0f, 150.0f, 150.0f};

    // Light settings
    inline constexpr Vector3 MainLightColor     = {255.0f, 242.0f, 217.0f};
    inline constexpr float LightIntensity       = 1.0f;
    inline constexpr float AmbientLightStrength = 0.25f;

    // Misc
    inline constexpr Vector3 BackgroundColor = {36.0f, 39.0f, 58.0f};
    inline constexpr int TargetFPS           = 30;
}