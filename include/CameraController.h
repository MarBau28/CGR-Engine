#pragma once

#include "raylib.h"

enum class CameraState { Static, Orbital, Free };

class CameraController {
  public:
    CameraController();

    void Update();
    void CycleState();
    void SetState(CameraState newState);
    [[nodiscard]] CameraState GetState() const;

    void SetDeterministicState(Vector3 position, Vector3 target, float fov);
    void SetLocked(bool locked);
    [[nodiscard]] bool IsLocked() const;

    Camera3D &GetCamera();

  private:
    Camera3D camera{};
    CameraState currentState;
    bool isLocked;

    // Internal Input Handlers
    void UpdateOrbital();
    void UpdateFreeFlight();
};