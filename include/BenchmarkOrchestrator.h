#pragma once

#include "CameraController.h"
#include "CsvTelemetryWriter.h"
#include "EngineState.h"
#include "Profilers.h"
#include <queue>
#include <string>
#include <vector>

enum class BenchmarkSuite {
    Inactive,
    Suite_5_2_1_LodMicroGeom,
    Suite_5_2_2_ObjectCount,
    Suite_5_2_3_OverdrawDensity,
    Suite_5_3_1_ResolutionScaling,
    Suite_5_3_2_BaseBandwidthTax,
    Suite_5_4_1_LightCount,
    Suite_5_4_2_1_LightIntensity,
    Suite_5_4_2_2_LightSingularity,
    Suite_5_4_3_ShadingOverdraw,
    Suite_5_5_1_1_SpatialEntropy,
    Suite_5_5_1_2_StyleCombinatorics,
    Suite_5_5_2_KernelBandwidth,
    Suite_5_6_Pass1_GeometryBaseline,
    Suite_5_6_Pass2_ShadingTax,
    Suite_5_6_Pass3_ParityFlythrough,
    Suite_5_6_Pass4_DeferredMaxFidelity,
    Complete
};

enum class BenchPhase { Warmup, Capture };

struct FrameRecord {
    int frameNumber;
    double cpuLogicMs;
    double cpuRenderMs;
    double totalFrameTimeMs;
    double fps;
    int activeInstances;
    int activeTris;
    double currentOverdraw;
    RenderPath activeRenderPath;
    std::string architecture;
    float stepValue;
    BenchmarkSuite suite;
};

class BenchmarkOrchestrator {
  public:
    BenchmarkOrchestrator(CsvTelemetryWriter &telemetry, CpuProfiler &cpuLogicProfiler,
                          CpuProfiler &cpuRenderProfiler, GpuProfiler &geomProf,
                          GpuProfiler &lightProf, GpuProfiler &masterGpuProf,
                          CameraController &camCtrl);

    void Start(BenchmarkSuite suite);
    void UpdateAndRecord(double totalFrameTimeMs, int activeTris, double currentOverdraw);
    void EndBenchmark();
    void InjectPerFrameState() const;

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool DidStateChangeThisFrame() const;
    [[nodiscard]] const EngineState &GetCurrentState() const;

  private:
    void ApplySuiteState();
    void AdvanceState();

    CsvTelemetryWriter &telemetryWriter;
    CpuProfiler &cpuLogicProfiler;
    CpuProfiler &cpuRenderProfiler;
    GpuProfiler &geomProfiler;
    GpuProfiler &lightProfiler;
    GpuProfiler &masterGpuProfiler;
    CameraController &cameraController;

    BenchmarkSuite currentSuite;
    BenchPhase currentPhase;
    EngineState currentState;

    bool stateChangedThisFrame;
    
    double phaseStartTime;
    double warmupDuration;
    double captureDuration;
    int frameCounter;

    bool useFrameLimit   = true;
    int targetFrameCount = 1000;

    std::vector<float> stepValues;
    int currentStepIndex;

    std::vector<RenderPath> targetPipelines;
    int currentPipelineIndex;
    std::queue<FrameRecord> pendingFrames;
    void FlushTelemetryQueue();

    static std::string GetSuiteName(BenchmarkSuite suite);
};