#pragma once

#include "CameraController.h"
#include "EngineState.h"
#include "InputHandler.h"

struct InputEventFlags {
    bool triggerBenchmarkStart = false;
    bool triggerSceneRebuild   = false;
    bool triggerHdrFboRebuild  = false;
};

class InputController {
  public:
    InputController();

    // Processes all input and returns structural event flags back to the main loop
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