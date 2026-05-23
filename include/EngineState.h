#pragma once

#include "Config.h"

// Architectural Rendering Paths
enum class RenderPath { DeferredUber, DeferredVolume, Forward };

// Centralized state container for the current frame
struct EngineState {
    RenderPath activeRenderPath = RenderPath::Forward;

    // Core Metrics
    bool use16BitHDR           = Config::DefaultState::Use16BitHDR;
    int activeObstacleCount    = Config::DefaultState::ActiveObstacleCount;
    int activeLightCount       = Config::DefaultState::ActiveLightCount;
    float lightIntensity       = Config::EngineSettings::LightIntensity;
    float ambientLightStrength = Config::EngineSettings::AmbientLightStrength;
    int renderWidth            = Config::EngineSettings::ScreenWidth;
    int renderHeight           = Config::EngineSettings::ScreenHeight;

    // NPR Toggles
    bool enableOutlines = Config::DefaultState::EnableOutlines;
    bool enableKuwahara = Config::DefaultState::EnableKuwahara;
    bool enableGooch    = Config::DefaultState::EnableGooch;
    bool enableToon     = Config::DefaultState::EnableToon;

    // Spatial & Stylistic Modifiers
    int kuwaharaRadius       = Config::DefaultState::KuwaharaRadius;
    float kuwaharaIntensity  = Config::DefaultState::KuwaharaIntensity;
    float objectSphereRadius = Config::DefaultState::ObjectSphereRadius;
    int currentLodIndex      = Config::DefaultState::CurrentLodIndex;

    // Architecture & Distribution Flags
    bool useClusteredStyles  = Config::DefaultState::UseClusteredStyles;
    bool useLightSingularity = Config::DefaultState::UseLightSingularity;
    bool useNprRoom          = Config::DefaultState::UseNprRoom;
    bool renderFloor         = Config::DefaultState::RenderFloor;

    // System Flags
    bool requestScreenshot = false;
};