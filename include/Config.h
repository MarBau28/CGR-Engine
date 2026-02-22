#pragma once

#include "raylib.h"
#include "string_view"

namespace Config::Paths {
    inline constexpr std::string_view Assets   = "../assets/";
    inline constexpr std::string_view Textures = "../assets/textures/";
    inline constexpr std::string_view Shaders  = "../assets/shaders/";
}

namespace Config::EngineSettings {
    inline constexpr int ScreenWidth        = 1920;
    inline constexpr int ScreenHeight       = 1080;
    inline constexpr float CameraFOV        = 45.0f;
    inline constexpr Vector3 CameraPosition = {4.0f, 4.0f, 4.0f};
}