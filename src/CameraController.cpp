#include "../include/CameraController.h"
#include "../include/Config.h"
#include <raymath.h>

CameraController::CameraController() : currentState(CameraState::Orbital), isLocked(false) {
    camera.position   = Config::EngineSettings::CameraPosition;
    camera.target     = Config::EngineSettings::CameraViewDirection;
    camera.up         = Config::EngineSettings::CameraOrientation;
    camera.fovy       = Config::EngineSettings::CameraFOV;
    camera.projection = CAMERA_PERSPECTIVE;
}

void CameraController::Update() {
    if (isLocked)
        return;

    switch (currentState) {
    case CameraState::Orbital:
        UpdateOrbital();
        break;
    case CameraState::Free:
        UpdateFreeFlight();
        break;
    case CameraState::Static:
        // Intentional pass-through
        break;
    }
}

void CameraController::CycleState() {
    if (isLocked)
        return;

    int nextState = (static_cast<int>(currentState) + 1) % 3;
    SetState(static_cast<CameraState>(nextState));
}

void CameraController::SetState(const CameraState newState) {
    currentState = newState;
    if (currentState == CameraState::Free) {
        DisableCursor();
    } else {
        EnableCursor();
    }
}

CameraState CameraController::GetState() const { return currentState; }

void CameraController::SetDeterministicState(const Vector3 position, const Vector3 target,
                                             const float fov) {
    camera.position = position;
    camera.target   = target;
    camera.fovy     = fov;
}

void CameraController::SetLocked(const bool locked) { isLocked = locked; }

bool CameraController::IsLocked() const { return isLocked; }

Camera3D &CameraController::GetCamera() { return camera; }

// Subsystem Updates
// -------------------------------------------------------------------------------------------------

void CameraController::UpdateOrbital() { UpdateCamera(&camera, CAMERA_ORBITAL); }

void CameraController::UpdateFreeFlight() {
    // Velocity Configuration
    constexpr float baseSpeed = 50.0f;
    constexpr float turnSpeed = 0.05f;
    constexpr float zoomSpeed = 5.0f;
    const float sprintMult    = IsKeyDown(KEY_LEFT_SHIFT) ? 3.0f : 1.0f;
    const float currentSpeed  = baseSpeed * sprintMult * GetFrameTime();

    // Local Axis Extraction
    const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    const Vector3 right   = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    const Vector3 up      = Vector3Normalize(Vector3CrossProduct(right, forward));

    // Vector Accumulation
    Vector3 velocity = {0.0f, 0.0f, 0.0f};

    if (IsKeyDown(KEY_W))
        velocity = Vector3Add(velocity, Vector3Scale(forward, currentSpeed));
    if (IsKeyDown(KEY_S))
        velocity = Vector3Subtract(velocity, Vector3Scale(forward, currentSpeed));
    if (IsKeyDown(KEY_D))
        velocity = Vector3Add(velocity, Vector3Scale(right, currentSpeed));
    if (IsKeyDown(KEY_A))
        velocity = Vector3Subtract(velocity, Vector3Scale(right, currentSpeed));
    if (IsKeyDown(KEY_E))
        velocity = Vector3Add(velocity, Vector3Scale(up, currentSpeed));
    if (IsKeyDown(KEY_Q))
        velocity = Vector3Subtract(velocity, Vector3Scale(up, currentSpeed));

    // Translation Application
    camera.position = Vector3Add(camera.position, velocity);
    camera.target   = Vector3Add(camera.target, velocity);

    // Rotation and Field of View
    Vector3 rotation = {0.0f, 0.0f, 0.0f};
    auto [x, y]      = GetMouseDelta();
    rotation.x       = x * turnSpeed;
    rotation.y       = y * turnSpeed;
    const float zoom = GetMouseWheelMove() * zoomSpeed * -1.0f;

    UpdateCameraPro(&camera, {0.0f, 0.0f, 0.0f}, rotation, zoom);
}

void CameraController::UpdateCinematic() {
    // Parametric time variable
    const double t = (GetTime() - cinematicStartTime) * 0.15;
    // Fixed spatial scale of the flight path
    constexpr float pathScale = 150.0f;

    // Parametric equations for smooth traversal
    camera.position.x = pathScale * static_cast<float>(sin(t));
    camera.position.y = 50.0f + 25.0f * static_cast<float>(sin(t * 1.5));
    camera.position.z = pathScale * static_cast<float>(sin(t) * cos(t));

    // Look slightly ahead on the curve to simulate flight
    const double tAhead = t + 0.1;
    camera.target.x     = pathScale * static_cast<float>(sin(tAhead));
    camera.target.y     = 50.0f + 25.0f * static_cast<float>(sin(tAhead * 1.5));
    camera.target.z     = pathScale * static_cast<float>(sin(tAhead) * cos(tAhead));
}