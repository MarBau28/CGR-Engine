#pragma once

#include "CameraController.h"
#include "EngineState.h"
#include "Profilers.h"
#include "SceneManager.h"

class TelemetryDashboard {
  public:
    static void Draw(EngineState &state, const CpuProfiler &cpuLogicProf,
                     const CpuProfiler &cpuRenderProf, double trueFrameDeltaMs,
                     const GpuProfiler &geomProf, const GpuProfiler &lightProf,
                     const GpuProfiler &masterGpuProfiler, const CameraController &camCtrl,
                     const SceneManager &sceneManager, int currentMeshTriangleCount,
                     float currentOverdraw);
};