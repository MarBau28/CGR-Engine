#include "../include/TelemetryDashboard.h"
#include "../include/Config.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <rlgl.h>

void TelemetryDashboard::Draw(EngineState &state, const CpuProfiler &cpuProf,
                              const GpuProfiler &geomProf, const GpuProfiler &lightProf,
                              const CameraController &camCtrl, const SceneManager &sceneManager,
                              int currentMeshTriangleCount) {
    // GENERAL UI SETUP
    // ---------------------------------------------------------------------------------------------

    auto currentHeight = static_cast<float>(GetScreenHeight());
    float uiScale      = (currentHeight / 1080.0f) * Config::EngineSettings::UiScale;

    int fontLg  = static_cast<int>(24 * uiScale);
    int fontMd  = static_cast<int>(18 * uiScale);
    int fontSm  = static_cast<int>(14 * uiScale);
    int pad     = static_cast<int>(20 * uiScale);
    int spacing = static_cast<int>(26 * uiScale);

    int panelWidth  = static_cast<int>(550 * uiScale);
    int panelHeight = static_cast<int>(1250 * uiScale);
    int panelX      = pad;
    int panelY      = pad;

    Color colBg       = {20, 24, 28, 245};
    Color colBorder   = {60, 65, 75, 255};
    Color colText     = {235, 235, 240, 255};
    Color colMuted    = {150, 155, 165, 255};
    Color colAccent   = {170, 205, 250, 255};
    Color colAction   = {255, 240, 165, 255};
    Color colWarning  = {255, 200, 140, 255};
    Color colGood     = {175, 235, 180, 255};
    Color colBad      = {245, 165, 165, 255};
    Color colSpecial  = {200, 180, 245, 255};
    Color colSpecial2 = {160, 240, 245, 255};
    Color colSpecial3 = {255, 195, 190, 255};

    DrawRectangle(panelX, panelY, panelWidth, panelHeight, colBg);
    DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, colBorder);

    int textX  = panelX + pad;
    int valueX = textX + static_cast<int>(250 * uiScale);
    int textY  = panelY + pad;

    DrawText("HYDRA ENGINE DASHBOARD", textX, textY, fontLg, colAccent);
    textY += spacing + pad;

    // ARCHITECTURE
    // ---------------------------------------------------------------------------------------------

    DrawText("ARCHITECTURE", textX, textY, fontSm, colMuted);
    textY += static_cast<int>(18 * uiScale);

    DrawText("Pipeline-Mode", textX, textY, fontMd, colText);
    auto modeString = "Unknown";
    if (state.activeRenderPath == RenderPath::DeferredUber)
        modeString = "Deferred (Uber-Shader)";
    else if (state.activeRenderPath == RenderPath::DeferredVolume)
        modeString = "Deferred (Light Volumes)";
    else if (state.activeRenderPath == RenderPath::Forward)
        modeString = "forward (Baseline)";
    DrawText(modeString, valueX, textY, fontMd, colSpecial);
    textY += spacing;

    DrawText("Render Resolution", textX, textY, fontMd, colText);
    DrawText(TextFormat("%d x %d", state.renderWidth, state.renderHeight), valueX, textY, fontMd,
             colWarning);
    textY += spacing;

    DrawText("FBO Depth", textX, textY, fontMd, colText);
    DrawText(state.use16BitHDR ? "16-Bit HDR (Float)" : "8-Bit LDR (Clamped)", valueX, textY,
             fontMd, state.use16BitHDR ? colGood : colBad);
    textY += spacing;

    DrawText("Test Environment", textX, textY, fontMd, colText);
    DrawText(state.useNprRoom ? "NPR Room (Synthetic)" : "Open World (Organic)", valueX, textY,
             fontMd, state.useNprRoom ? colWarning : colSpecial2);
    textY += spacing + pad;

    DrawText("NPR PIPELINE STATE", textX, textY, fontSm, colMuted);
    textY += static_cast<int>(18 * uiScale);

    DrawText("Outlines [O]", textX, textY, fontMd, colText);
    DrawText(state.enableOutlines ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
             state.enableOutlines ? colGood : colBad);
    textY += spacing;

    DrawText("Kuwahara [K]", textX, textY, fontMd, colText);
    DrawText(state.enableKuwahara ? TextFormat("ENABLED (Rad: %d | Int: %.1f)",
                                               state.kuwaharaRadius, state.kuwaharaIntensity)
                                  : "DISABLED",
             valueX, textY, fontMd, state.enableKuwahara ? colGood : colBad);
    textY += spacing;

    DrawText("Gooch [G]", textX, textY, fontMd, colText);
    DrawText(state.enableGooch ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
             state.enableGooch ? colGood : colBad);
    textY += spacing;

    DrawText("Toon [T]", textX, textY, fontMd, colText);
    DrawText(state.enableToon ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
             state.enableToon ? colGood : colBad);
    textY += spacing + pad;

    // SCENE DATA
    // ---------------------------------------------------------------------------------------------

    DrawText("SCENE DATA", textX, textY, fontSm, colMuted);
    textY += static_cast<int>(18 * uiScale);

    int currentFps = GetFPS();
    Color fpsColor = currentFps >= 60 ? colGood : (currentFps >= 30 ? colWarning : colBad);
    DrawText("Framerate", textX, textY, fontMd, colText);
    DrawText(TextFormat("%d FPS", currentFps), valueX, textY, fontMd, fpsColor);
    textY += spacing;

    if (state.activeRenderPath == RenderPath::Forward) {
        DrawText("GPU Geometry", textX, textY, fontMd, colText);
        DrawText("N/A (Unified)", valueX, textY, fontMd, colMuted);
        textY += spacing;

        DrawText("GPU forward (All)", textX, textY, fontMd, colText);
        DrawText(TextFormat("%.3f ms", lightProf.elapsedMs), valueX, textY, fontMd, colSpecial3);
        textY += spacing;
    } else {
        DrawText("GPU Geometry", textX, textY, fontMd, colText);
        DrawText(TextFormat("%.3f ms", geomProf.elapsedMs), valueX, textY, fontMd, colSpecial3);
        textY += spacing;

        DrawText("GPU Light + NPR", textX, textY, fontMd, colText);
        DrawText(TextFormat("%.3f ms", lightProf.elapsedMs), valueX, textY, fontMd, colSpecial3);
        textY += spacing;
    }

    DrawText("CPU Bench", textX, textY, fontMd, colText);
    DrawText(TextFormat("%.3f ms", cpuProf.elapsedMs), valueX, textY, fontMd, colSpecial3);
    textY += spacing;

    DrawText("Camera-Mode", textX, textY, fontMd, colText);
    auto cameraModeString = "Unknown";
    if (camCtrl.GetState() == CameraState::Orbital)
        cameraModeString = "Orbital";
    else if (camCtrl.GetState() == CameraState::Free)
        cameraModeString = "Free";
    else if (camCtrl.GetState() == CameraState::Static)
        cameraModeString = "Static";
    DrawText(cameraModeString, valueX, textY, fontMd, colSpecial2);
    textY += spacing;

    DrawText("Floor Rendering", textX, textY, fontMd, colText);
    DrawText(state.renderFloor ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
             state.renderFloor ? colGood : colBad);
    textY += spacing;

    DrawText("Distribution", textX, textY, fontMd, colText);
    DrawText(state.useClusteredStyles ? "Quadrants (Clustered)" : "Uniform (Random)", valueX, textY,
             fontMd, colSpecial2);
    textY += spacing;

    DrawText("Light Topology", textX, textY, fontMd, colText);
    DrawText(state.useLightSingularity ? "Singularity (Stacked)" : "Uniform (Scattered)", valueX,
             textY, fontMd, colSpecial2);
    textY += spacing;

    DrawText("Geometry Level", textX, textY, fontMd, colText);
    DrawText(TextFormat("LOD %d (%d Tris/Obj)", state.currentLodIndex, currentMeshTriangleCount),
             valueX, textY, fontMd, colWarning);
    textY += spacing;

    int actualVisibleCount = static_cast<int>(sceneManager.GetVisibleObstacleTransforms().size());
    int activeTriangles    = currentMeshTriangleCount * actualVisibleCount;
    DrawText("Active Triangles", textX, textY, fontMd, colText);
    DrawText(TextFormat("%d", activeTriangles), valueX, textY, fontMd, colText);
    textY += spacing;

    DrawText("Cloud Size", textX, textY, fontMd, colText);
    DrawText(TextFormat("%.2f", state.objectSphereRadius), valueX, textY, fontMd, colText);
    textY += spacing;

    int currentObstacleCount = state.useNprRoom
                                   ? static_cast<int>(sceneManager.GetMasterStyleIds().size())
                                   : state.activeObstacleCount;
    DrawText("Obstacles", textX, textY, fontMd, colText);
    DrawText(std::format("{} / {}", actualVisibleCount, currentObstacleCount).c_str(), valueX,
             textY, fontMd, colText);
    textY += spacing;

    int actualVisibleVolumeCount =
        static_cast<int>(sceneManager.GetVisibleLightVolumeTransforms().size());
    int drawnLights = (state.activeRenderPath == RenderPath::DeferredVolume)
                          ? actualVisibleVolumeCount
                          : std::min(actualVisibleVolumeCount, 500);
    DrawText("Lights Evaluated", textX, textY, fontMd, colText);
    DrawText(std::format("{} / {}", drawnLights, state.activeLightCount).c_str(), valueX, textY,
             fontMd, colText);
    textY += spacing;

    if (state.activeRenderPath != RenderPath::DeferredVolume && actualVisibleVolumeCount > 500) {
        DrawText("WARNING: Shader Array Limit Exceeded!", textX, textY, fontSm, colBad);
        textY += static_cast<int>(18 * uiScale);
    }

    DrawText("Light Intensity", textX, textY, fontMd, colText);
    DrawText(TextFormat("%.2f", state.lightIntensity), valueX, textY, fontMd, colText);
    textY += spacing;

    DrawText("Ambient Strength", textX, textY, fontMd, colText);
    DrawText(TextFormat("%.2f", state.ambientLightStrength), valueX, textY, fontMd, colText);
    textY += spacing + pad;

    // CONTROLS
    // ---------------------------------------------------------------------------------------------

    DrawText("CONTROLS", textX, textY, fontSm, colMuted);
    textY += static_cast<int>(18 * uiScale);

    DrawText("Toggle Pipeline", textX, textY, fontMd, colText);
    DrawText("[TAB]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Scale Resolution", textX, textY, fontMd, colText);
    DrawText("[PgDn / PgUp]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Toggle HDR/LDR", textX, textY, fontMd, colText);
    DrawText("[H]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Toggle \"NPR-Room\"", textX, textY, fontMd, colText);
    DrawText("[R]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Toggle Floor", textX, textY, fontMd, colText);
    DrawText("[F]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Cloud Size", textX, textY, fontMd, colText);
    DrawText("[7 / 9]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Obstacles Count", textX, textY, fontMd, colText);
    DrawText("[4 / 6]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Lights Count", textX, textY, fontMd, colText);
    DrawText("[5 / 8]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Light Intensity", textX, textY, fontMd, colText);
    DrawText("[0 / 2]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Ambient Light", textX, textY, fontMd, colText);
    DrawText("[1 / 3]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Kuwahara Radius", textX, textY, fontMd, colText);
    DrawText("[Left / Right]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Kuwahara Intensity", textX, textY, fontMd, colText);
    DrawText("[Up / Down]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Capture Screen", textX, textY, fontMd, colText);
    DrawText("[P]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Switch Camera", textX, textY, fontMd, colText);
    DrawText("[C]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Switch Distribution", textX, textY, fontMd, colText);
    DrawText("[M]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Toggle Light-Collapse", textX, textY, fontMd, colText);
    DrawText("[L]", valueX, textY, fontMd, colAction);
    textY += spacing;
    DrawText("Geometry LOD", textX, textY, fontMd, colText);
    DrawText("[Z / X]", valueX, textY, fontMd, colAction);

    // SCREENSHOT HANDLING
    // ---------------------------------------------------------------------------------------------

    if (state.requestScreenshot) {
        rlDrawRenderBatchActive();
        namespace fs         = std::filesystem;
        fs::path currentPath = fs::current_path();
        if (currentPath.filename().string().find("cmake-build") != std::string::npos)
            currentPath = currentPath.parent_path();
        fs::path outputDir = currentPath / "outputs" / "screenshots";

        if (!fs::exists(outputDir)) {
            std::error_code ec;
            fs::create_directories(outputDir, ec);
            if (ec)
                TraceLog(LOG_ERROR, "FAILED TO CREATE DIRECTORY: %s", ec.message().c_str());
        }

        auto now     = std::chrono::system_clock::now();
        auto now_sec = std::chrono::floor<std::chrono::seconds>(now);
        std::chrono::zoned_time local_time{std::chrono::current_zone(), now_sec};
        std::string fileName = std::format("hydra_capture_{:%Y%m%d_%H%M%S}.png", local_time);
        fs::path fullPath    = outputDir / fileName;

        int physWidth            = GetRenderWidth();
        int physHeight           = GetRenderHeight();
        unsigned char *rawPixels = rlReadScreenPixels(physWidth, physHeight);
        Image capture = {rawPixels, physWidth, physHeight, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

        ExportImage(capture, fullPath.string().c_str());
        UnloadImage(capture);
        TraceLog(LOG_INFO, "SCREENSHOT SAVED SUCCESSFULLY TO: %s", fullPath.string().c_str());
        state.requestScreenshot = false;
    }
}