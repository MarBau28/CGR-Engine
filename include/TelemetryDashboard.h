#pragma once

#include "CameraController.h"
#include "EngineState.h"
#include "Profilers.h"
#include "SceneManager.h"

class TelemetryDashboard {
  public:
    // Takes state as a non-const reference solely so it can reset the screenshot flag
    static void Draw(EngineState &state, const CpuProfiler &cpuProf, const GpuProfiler &geomProf,
                     const GpuProfiler &lightProf, const CameraController &camCtrl,
                     const SceneManager &sceneManager, int currentMeshTriangleCount);
};