#pragma once

#include "CameraController.h"
#include "EngineState.h"
#include "Profilers.h"
#include "Telemetry.h"
#include <vector>

// The specific thesis evaluation suites
enum class BenchmarkSuite {
    Inactive,
    SuiteA_Geom,        // Rasterization Stress
    SuiteB_DrawCall,    // Vertex/CPU Limit
    SuiteC_Entropy,     // Warp Divergence vs State Change
    SuiteD_FillRate,    // BRDF Scalability
    SuiteE_Singularity, // 16-bit HDR Bandwidth Death
    SuiteF_Spatial,     // Kuwahara Filter Cost
    SuiteG_Synthesis,   // Real-World Mixed Load
    SuiteH_Overdraw,    // The Overdraw Crucible
    Complete
};

class BenchmarkOrchestrator {
  public:
    // Requires references to ALL measurement devices
    BenchmarkOrchestrator(CsvTelemetryWriter &telemetry, CpuProfiler &cpuProf,
                          GpuProfiler &geomProf, GpuProfiler &lightProf, CameraController &camCtrl);

    void Start(BenchmarkSuite suite);

    // Called at the end of the frame. Handles warmup, recording, and phase transitions.
    void UpdateAndRecord(double totalFrameTimeMs, int activeTris, double currentOverdraw);

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool
    DidStateChangeThisFrame() const; // Signals main.cpp to trigger GenerateScene()
    [[nodiscard]] const EngineState &GetCurrentState() const;

  private:
    CsvTelemetryWriter &telemetryWriter;
    CpuProfiler &cpuProfiler;
    GpuProfiler &geomProfiler;
    GpuProfiler &lightProfiler;
    CameraController &cameraController;

    BenchmarkSuite currentSuite;
    EngineState currentState;

    int warmupFrames           = 60;
    int recordFrames           = 500;
    int currentFrameCount      = 0;
    bool isWarmingUp           = false;
    bool stateChangedThisFrame = false;

    // Phase progression
    int sweepIndex = 0;
    std::vector<int> currentSweepArray; // Holds the variable being scaled (e.g., LODs, Instances)

    // Accumulators
    double accumCpu, accumGeom, accumLight, accumTotal;
    int recordedFrames;

    void ApplySuiteState();
    void EndBenchmark();
};