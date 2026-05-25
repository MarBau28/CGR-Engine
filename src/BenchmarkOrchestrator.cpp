#include "../include/BenchmarkOrchestrator.h"
#include <glad/glad.h>
#include <iostream>
#include <raylib.h>
#include <rlgl.h>

BenchmarkOrchestrator::BenchmarkOrchestrator(CsvTelemetryWriter &telemetry,
                                             CpuProfiler &cpuLogicProfiler,
                                             CpuProfiler &cpuRenderProfiler, GpuProfiler &geomProf,
                                             GpuProfiler &lightProf, GpuProfiler &masterGpuProf,
                                             CameraController &camCtrl)
    : telemetryWriter(telemetry), cpuLogicProfiler(cpuLogicProfiler),
      cpuRenderProfiler(cpuRenderProfiler), geomProfiler(geomProf), lightProfiler(lightProf),
      masterGpuProfiler(masterGpuProf), cameraController(camCtrl),
      currentSuite(BenchmarkSuite::Inactive), currentPhase(BenchPhase::Warmup),
      stateChangedThisFrame(false), phaseStartTime(0.0), warmupDuration(3.0), captureDuration(5.0),
      frameCounter(0), currentStepIndex(0), currentPipelineIndex(0) {}

void BenchmarkOrchestrator::Start(BenchmarkSuite suite) {
    if (currentSuite != BenchmarkSuite::Inactive)
        return;

    currentSuite         = suite;
    currentStepIndex     = 0;
    currentPipelineIndex = 0;
    stepValues.clear();

    // Default architectural sweep; dynamically constrained per suite definition
    targetPipelines = {RenderPath::Forward, RenderPath::DeferredUber, RenderPath::DeferredVolume};
    cameraController.SetLocked(true);

    switch (suite) {
    // TODO: Implement specific independent variable population per thesis definition
    default:
        EndBenchmark();
        return;
    }

    ApplySuiteState();
    std::cout << "[ORCHESTRATOR] Benchmark sequence initialized. Suite ID: "
              << static_cast<int>(suite) << "\n";
}

void BenchmarkOrchestrator::ApplySuiteState() {
    // Zero-state initialization to prevent variable carryover across architectural sweeps
    currentState.activeLightCount     = 0;
    currentState.ambientLightStrength = 1.0f;
    currentState.enableOutlines       = false;
    currentState.enableKuwahara       = false;
    currentState.enableGooch          = false;
    currentState.enableToon           = false;
    currentState.useClusteredStyles   = false;
    currentState.use16BitHDR          = true;

    currentState.activeRenderPath = targetPipelines[currentPipelineIndex];

    // Reset phase clocks and structural flags
    warmupDuration        = 10.0;
    phaseStartTime        = GetTime();
    currentPhase          = BenchPhase::Warmup;
    frameCounter          = 0;
    stateChangedThisFrame = true;

    if (currentSuite == BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline ||
        currentSuite == BenchmarkSuite::Suite_5_5_Pass2_ShadingTax ||
        currentSuite == BenchmarkSuite::Suite_5_5_Pass3_ParityFlythrough ||
        currentSuite == BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity) {
        // Spatial Sweep: Measures the entire path chronologically
        useFrameLimit   = false;
        captureDuration = 15.0;
    } else {
        // Parameter Sweep: Measures a mathematically identical sample size
        useFrameLimit    = true;
        targetFrameCount = 1000;
    }

    // TODO: Implement suite-specific variable mutation and camera transformations here

    // Reset phase clocks and structural flags
    // warmupDuration        = 3.0;
    // captureDuration       = 5.0;
    // phaseStartTime        = GetTime();
    // currentPhase          = BenchPhase::Warmup;
    // frameCounter          = 0;
    // stateChangedThisFrame = true;
}

void BenchmarkOrchestrator::AdvanceState() {
    currentPipelineIndex++;

    if (currentPipelineIndex >= targetPipelines.size()) {
        currentPipelineIndex = 0;
        currentStepIndex++;

        if (currentStepIndex >= stepValues.size()) {
            EndBenchmark();
            return;
        }
    }

    ApplySuiteState();
}

void BenchmarkOrchestrator::UpdateAndRecord(const double totalFrameTimeMs, const int activeTris,
                                            const double currentOverdraw) {
    if (currentSuite == BenchmarkSuite::Inactive)
        return;

    stateChangedThisFrame    = false;
    const double currentTime = GetTime();
    const double elapsedTime = currentTime - phaseStartTime;

    if (currentPhase == BenchPhase::Warmup) {
        if (elapsedTime >= warmupDuration) {
            rlDrawRenderBatchActive();
            glFinish();

            // Drain all remaining queries forced to completion by glFinish
            geomProfiler.Reset();
            lightProfiler.Reset();
            masterGpuProfiler.Reset();

            currentPhase   = BenchPhase::Capture;
            phaseStartTime = GetTime();
            frameCounter   = 0;
            // Purge queue to ensure zero carryover
            while (!pendingFrames.empty())
                pendingFrames.pop();
        }
    } else if (currentPhase == BenchPhase::Capture) {
        frameCounter++;

        const std::string archStr =
            currentState.activeRenderPath == RenderPath::Forward
                ? "Forward"
                : (currentState.activeRenderPath == RenderPath::DeferredUber ? "Deferred_Uber"
                                                                             : "Deferred_Volume");

        const double safeTime   = std::max(totalFrameTimeMs, 0.001);
        const double currentFps = 1000.0 / safeTime;

        // Queue up the raw CPU metrics for this frame
        pendingFrames.push(FrameRecord{
            frameCounter, cpuLogicProfiler.elapsedMs, cpuRenderProfiler.elapsedMs, totalFrameTimeMs,
            currentFps, currentState.activeObstacleCount, activeTris, currentOverdraw,
            currentState.activeRenderPath, archStr, stepValues[currentStepIndex], currentSuite});

        // Process resolved frames opportunistically (Non-Blocking)
        while (!pendingFrames.empty()) {
            const auto &[recFrameNumber, recCpuLogicMs, recCpuRenderMs, recTotalFrameTimeMs, recFps,
                         recActiveInstances, recActiveTris, recCurrentOverdraw, recActiveRenderPath,
                         recArchitecture, recStepValue, recSuite] = pendingFrames.front();
            const bool isDeferred = (recActiveRenderPath != RenderPath::Forward);

            // Check if hardware queries are naturally ready
            const bool geomReady       = !isDeferred || geomProfiler.IsOldestReady();
            const bool lightReady      = lightProfiler.IsOldestReady();
            const bool masterReady     = masterGpuProfiler.IsOldestReady();
            const bool allQueriesReady = (geomReady && lightReady && masterReady);
            const bool emergencyStallRequired =
                (pendingFrames.size() >= GpuProfiler::MAX_IN_FLIGHT);

            // Wait for all three profilers to report naturally, OR force the wait to clear capacity
            if (allQueriesReady || emergencyStallRequired) {
                double geomMs = 0.0, lightMs = 0.0, totalGpuMs = 0.0;

                // Pass 'forceWait' down to TryGetOldestResult so the thread explicitly blocks
                // and correctly waits for the OpenGL queries to resolve before extracting.
                if (isDeferred)
                    geomProfiler.TryGetOldestResult(geomMs, emergencyStallRequired);

                lightProfiler.TryGetOldestResult(lightMs, emergencyStallRequired);
                masterGpuProfiler.TryGetOldestResult(totalGpuMs, emergencyStallRequired);

                telemetryWriter.AppendRow("Suite_" + std::to_string(static_cast<int>(recSuite)),
                                          recArchitecture, recStepValue, recFrameNumber,
                                          recCpuLogicMs, recCpuRenderMs, geomMs, lightMs,
                                          totalGpuMs, recTotalFrameTimeMs, recFps,
                                          recActiveInstances, recActiveTris, recCurrentOverdraw);

                pendingFrames.pop();
            } else {
                break; // GPU data for the oldest frame is not ready; Pause writes
            }
        }

        // Phase completion and flush
        bool isPhaseComplete = false;

        if (useFrameLimit) {
            isPhaseComplete = (frameCounter >= targetFrameCount);
        } else {
            isPhaseComplete = (elapsedTime >= captureDuration);
        }

        if (isPhaseComplete) {
            // Gracefully blocks to drain remaining frames at the end of a suite
            FlushTelemetryQueue();
            AdvanceState();
        }
    }
}

void BenchmarkOrchestrator::FlushTelemetryQueue() {
    // Force a blocking wait to extract all remaining frames before state mutates
    while (!pendingFrames.empty()) {
        const auto &[frameNumber, cpuLogicMs, cpuRenderMs, totalFrameTimeMs, fps, activeInstances,
                     activeTris, currentOverdraw, activeRenderPath, architecture, stepValue,
                     suite] = pendingFrames.front();

        const bool isDeferred = (activeRenderPath != RenderPath::Forward);

        double geomMs = 0.0, lightMs = 0.0, totalGpuMs = 0.0;

        if (isDeferred)
            geomProfiler.TryGetOldestResult(geomMs, true);
        lightProfiler.TryGetOldestResult(lightMs, true);
        masterGpuProfiler.TryGetOldestResult(totalGpuMs, true);

        telemetryWriter.AppendRow("Suite_" + std::to_string(static_cast<int>(suite)), architecture,
                                  stepValue, frameNumber, cpuLogicMs, cpuRenderMs, geomMs, lightMs,
                                  totalGpuMs, totalFrameTimeMs, fps, activeInstances, activeTris,
                                  currentOverdraw);

        pendingFrames.pop();
    }
}

void BenchmarkOrchestrator::EndBenchmark() {
    telemetryWriter.Close();
    cameraController.SetLocked(false);
    currentSuite = BenchmarkSuite::Complete;
    std::cout << "[ORCHESTRATOR] Suite execution completed. Telemetry flushed.\n";
}

bool BenchmarkOrchestrator::IsActive() const {
    return currentSuite != BenchmarkSuite::Inactive && currentSuite != BenchmarkSuite::Complete;
}
bool BenchmarkOrchestrator::DidStateChangeThisFrame() const { return stateChangedThisFrame; }
const EngineState &BenchmarkOrchestrator::GetCurrentState() const { return currentState; }