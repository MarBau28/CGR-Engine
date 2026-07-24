#pragma once

#include "CameraController.h"
#include "EngineState.h"
#include "InputHandler.h"

struct InputEventFlags {
    bool triggerBenchmarkStart    = false;
    bool triggerSceneRebuild      = false;
    bool triggerHdrFboRebuild     = false;
    bool triggerResolutionRebuild = false;
    int cycleBenchmarkSuite       = 0; // -1 = previous suite, +1 = next suite
};

class InputController {
  public:
    InputController();
    
    InputEventFlags ProcessInputs(EngineState &state, CameraController &camCtrl,
                                  bool isBenchmarking, int actualGeneratedLights, int maxLodMeshes);

  private:
    ContinuousInput<int> obsInput;
    ContinuousInput<int> lightInput;
    ContinuousInput<float> intensityInput;
    ContinuousInput<float> ambientInput;
    ContinuousInput<float> radiusInput;
    ContinuousInput<float> kuwIntInput;

    CameraState previousCameraState;
    Vector3 previousCameraPos{};
    Vector3 previousCameraTarget{};
};