#pragma once

#include "raylib.h"
#include <string_view>

namespace Config::Paths {
    inline constexpr std::string_view Assets   = "../assets/";
    inline constexpr std::string_view Textures = "../assets/textures/";
    inline constexpr std::string_view Shaders  = "../assets/shaders/";
}

namespace Config::Assets {
    inline constexpr std::string_view ObstacleTexture = "starry-galaxy_tex.png";
    inline constexpr std::string_view WoodTexture     = "wood_tex.png";
}

namespace Config::Shaders {
    inline constexpr std::string_view GBufferVert          = "g-buffer.vert";
    inline constexpr std::string_view GBufferFrag          = "g-buffer.frag";
    inline constexpr std::string_view DeferredUberFrag     = "deferred_uber.frag";
    inline constexpr std::string_view PostProcessFrag      = "postprocess.frag";
    inline constexpr std::string_view GBufferInstancedVert = "g-buffer_instanced.vert";
    inline constexpr std::string_view GBufferInstancedFrag = "g-buffer_instanced.frag";
    inline constexpr std::string_view DeferredVolumeVert   = "deferred_volume.vert";
    inline constexpr std::string_view DeferredVolumeFrag   = "deferred_volume.frag";
    inline constexpr std::string_view DeferredResolveFrag  = "deferred_resolve.frag";
    inline constexpr std::string_view ForwardInstancedVert = "forward_instanced.vert";
    inline constexpr std::string_view ForwardBlinnFrag     = "forward_blinn.frag";
    inline constexpr std::string_view ForwardGoochFrag     = "forward_gooch.frag";
    inline constexpr std::string_view ForwardToonFrag      = "forward_toon.frag";
    inline constexpr std::string_view ForwardOutlineVert   = "forward_outline.vert";
    inline constexpr std::string_view ForwardOutlineFrag   = "forward_outline.frag";
    inline constexpr std::string_view ForwardUnlitFrag     = "forward_unlit.frag";
}

namespace Config::EngineSettings {
    // Screen and Camera
    inline constexpr int ScreenWidth             = 1920;
    inline constexpr int ScreenHeight            = 1080;
    inline constexpr float CameraFOV             = 45.0f;
    inline constexpr Vector3 CameraOrientation   = {0.0f, 1.0f, 0.0f};
    inline constexpr Vector3 CameraViewDirection = {0.0f, 0.0f, 0.0f};
    inline constexpr Vector3 CameraPosition      = {150.0f, 150.0f, 150.0f};

    // Light Math
    inline constexpr Vector3 MainLightColor     = {255.0f, 242.0f, 217.0f};
    inline constexpr float LightIntensity       = 3.0f;
    inline constexpr float AmbientLightStrength = 0.25f;
    inline constexpr float MinLightThreshold    = 0.05f;
    inline constexpr float AttenuationConstant  = 1.0f;
    inline constexpr float AttenuationLinear    = 0.09f;
    inline constexpr float AttenuationQuadratic = 0.032f;

    // Architecture Limits
    inline constexpr int MAX_OBSTACLES = 50000;
    inline constexpr int MAX_LIGHTS    = 5000;

    // Misc Defaults
    inline constexpr Vector3 BackgroundColor = {36.0f, 39.0f, 58.0f};
    inline constexpr int TargetFPS           = 30;
    inline constexpr float UiScale           = 0.8f;
}

// Engine State Initializers
namespace Config::DefaultState {
    inline constexpr bool Use16BitHDR         = true;
    inline constexpr int ActiveObstacleCount  = 5000;
    inline constexpr int ActiveLightCount     = 250;
    inline constexpr bool EnableOutlines      = false;
    inline constexpr bool EnableKuwahara      = false;
    inline constexpr bool EnableGooch         = true;
    inline constexpr bool EnableToon          = true;
    inline constexpr int KuwaharaRadius       = 4;
    inline constexpr float KuwaharaIntensity  = 4.0f;
    inline constexpr float ObjectSphereRadius = 200.0f;
    inline constexpr bool UseClusteredStyles  = false;
    inline constexpr bool UseLightSingularity = false;
    inline constexpr int CurrentLodIndex      = 0;
    inline constexpr bool UseNprRoom          = false;
}