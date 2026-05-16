#include "../include/InputController.h"
#include "../include/Config.h"
#include <limits>

InputController::InputController() {
    previousCameraState  = CameraState::Orbital;
    previousCameraPos    = Config::EngineSettings::CameraPosition;
    previousCameraTarget = Config::EngineSettings::CameraViewDirection;
}

InputEventFlags InputController::ProcessInputs(EngineState &state, CameraController &camCtrl,
                                               const bool isBenchmarking,
                                               const int actualGeneratedLights,
                                               const int maxLodMeshes) {
    InputEventFlags flags;

    if (IsKeyPressed(KEY_C)) {
        camCtrl.CycleState();
    }
    // The CameraController self-regulates its locked state during benchmarks
    camCtrl.Update();
    Camera3D &camera = camCtrl.GetCamera();

    if (IsKeyPressed(KEY_F11) && !isBenchmarking) {
        flags.triggerBenchmarkStart = true;
    }

    // Intercept manual controls if a deterministic benchmark is running
    if (isBenchmarking)
        return flags;

    if (IsKeyPressed(KEY_R)) {
        state.useNprRoom = !state.useNprRoom;

        if (state.useNprRoom) {
            previousCameraState  = camCtrl.GetState();
            previousCameraPos    = camera.position;
            previousCameraTarget = camera.target;

            camCtrl.SetState(CameraState::Static);
            camera.position       = {0.0f, 50.0f, 150.0f};
            camera.target         = {0.0f, 50.0f, 0.0f};
            state.currentLodIndex = 0;
            state.enableOutlines  = false;
        } else {
            camCtrl.SetState(previousCameraState);
            camera.position = previousCameraPos;
            camera.target   = previousCameraTarget;
        }

        flags.triggerSceneRebuild = true;
    }

    if (IsKeyPressed(KEY_O) && !state.useNprRoom)
        state.enableOutlines = !state.enableOutlines;
    if (IsKeyPressed(KEY_K))
        state.enableKuwahara = !state.enableKuwahara;
    if (IsKeyPressed(KEY_G))
        state.enableGooch = !state.enableGooch;
    if (IsKeyPressed(KEY_T))
        state.enableToon = !state.enableToon;

    if (IsKeyPressed(KEY_RIGHT) && state.kuwaharaRadius < 8)
        state.kuwaharaRadius++;
    if (IsKeyPressed(KEY_LEFT) && state.kuwaharaRadius > 1)
        state.kuwaharaRadius--;
    kuwIntInput.Update(KEY_UP, KEY_DOWN, state.kuwaharaIntensity, 0.5f, 5.0f, 1.0f, 20.0f);

    if (IsKeyPressed(KEY_TAB)) {
        int nextPath           = (static_cast<int>(state.activeRenderPath) + 1) % 3;
        state.activeRenderPath = static_cast<RenderPath>(nextPath);
    }

    if (IsKeyPressed(KEY_H)) {
        state.use16BitHDR          = !state.use16BitHDR;
        flags.triggerHdrFboRebuild = true;
    }

    if (IsKeyPressed(KEY_P))
        state.requestScreenshot = true;
    if (IsKeyPressed(KEY_L))
        state.useLightSingularity = !state.useLightSingularity;

    if (IsKeyPressed(KEY_M) && !state.useNprRoom) {
        state.useClusteredStyles  = !state.useClusteredStyles;
        flags.triggerSceneRebuild = true;
    }

    if (IsKeyPressed(KEY_X) && !state.useNprRoom) {
        state.currentLodIndex = (state.currentLodIndex + 1) % maxLodMeshes;
    }
    if (IsKeyPressed(KEY_Z) && !state.useNprRoom) {
        state.currentLodIndex = std::max(0, (state.currentLodIndex - 1));
    }

    const float previousRadius = state.objectSphereRadius;
    radiusInput.Update(KEY_NINE, KEY_KP_9, KEY_SEVEN, KEY_KP_7, state.objectSphereRadius, 5.0f,
                       50.0f, 1.0f, 1000.0f);
    if (state.objectSphereRadius != previousRadius && !state.useNprRoom) {
        flags.triggerSceneRebuild = true;
    }

    obsInput.Update(KEY_SIX, KEY_KP_6, KEY_FOUR, KEY_KP_4, state.activeObstacleCount, 100, 5000.0f,
                    1, Config::EngineSettings::MAX_OBSTACLES);

    const int previousLightCount = state.activeLightCount;
    const int maxAllowedLights   = state.useNprRoom ? 500 : actualGeneratedLights;
    lightInput.Update(KEY_EIGHT, KEY_KP_8, KEY_FIVE, KEY_KP_5, state.activeLightCount, 10, 200.0f,
                      1, maxAllowedLights);

    if (state.useNprRoom && state.activeLightCount != previousLightCount) {
        flags.triggerSceneRebuild = true;
    }

    intensityInput.Update(KEY_TWO, KEY_KP_2, KEY_ZERO, KEY_KP_0, state.lightIntensity, 0.1f, 2.0f,
                          0.0f, std::numeric_limits<float>::max());
    ambientInput.Update(KEY_THREE, KEY_KP_3, KEY_ONE, KEY_KP_1, state.ambientLightStrength, 0.05f,
                        1.0f, 0.0f, std::numeric_limits<float>::max());

    return flags;
}