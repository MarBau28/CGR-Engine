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
    inline constexpr int ScreenWidth             = 1920;
    inline constexpr int ScreenHeight            = 1080;
    inline constexpr float CameraFOV             = 45.0f;
    inline constexpr Vector3 CameraOrientation   = {0.0f, 1.0f, 0.0f};
    inline constexpr Vector3 CameraViewDirection = {0.0f, 0.0f, 0.0f};
    inline constexpr Vector3 CameraPosition      = {40.0f, 40.0f, 40.0f};
    inline constexpr int TargetFPS               = 60;
}