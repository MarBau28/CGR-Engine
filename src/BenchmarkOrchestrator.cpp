#include "../include/BenchmarkOrchestrator.h"
#include <cmath>
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

    // BENCHMARK STEP DEFINITIONS
    // ---------------------------------------------------------------------------------------------

    switch (suite) {
    case BenchmarkSuite::Suite_5_1_1_LodMicroGeom: {
        // Maps to currentLodIndex
        stepValues = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_1_2_ObjectCount: {
        // Sweeping activeObstacleCount to saturate the CPU dispatcher
        stepValues = {1000.0f, 5000.0f, 10000.0f, 25000.0f, 50000.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_1_3_OverdrawDensity:
        // Sweeping activeObstacleCount to force massive Z-axis stacking
        stepValues = {2000.0f, 4000.0f, 8000.0f, 16000.0f, 32000.0f};
        break;
    case BenchmarkSuite::Suite_5_2_1_ResolutionScaling: {
        // 0 = 480p, 1 = 720p, 2 = 1080p, 3 = 2K, 4 = 4K
        stepValues = {0.0f, 1.0f, 2.0f, 3.0f, 4.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_2_2_BaseBandwidthTax: {
        // 0.0f = Floor Disabled (Zero Fill-Rate), 1.0f = Floor Enabled (Max Fill-Rate)
        stepValues = {0.0f, 1.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_3_1_LightCount: {
        // Sweeps activeLightCount.
        // Forward/Uber are capped at 500 per Test Definition. Volume scales to MAX_LIGHTS.
        stepValues = {10.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_3_2_1_LightIntensity: {
        // Sweeping light intensity (radius multiplier) to force massive screen-space overdraw.
        // 0.5 = minimal overlap, 20.0 = extreme RMW memory bus saturation.
        stepValues = {0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 20.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_3_2_2_LightSingularity: {
        // Sweeping light count, but all lights are forced to coordinate (0,0,0)
        // Forward/Uber capped at 500. Volume scales to 5000.
        stepValues = {10.0f, 50.0f, 100.0f, 250.0f, 500.0f, 1000.0f, 2500.0f, 5000.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_3_3_ShadingOverdraw: {
        // Sweeping Instance Count to force massive Z-axis stacking (Overdraw)
        stepValues = {2000.0f, 4000.0f, 8000.0f, 16000.0f, 32000.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_4_1_SpatialEntropy: {
        // Step mapping defines Phase and Entropy states:
        // 0.0f = Phase 1 (No Kuwahara) | Clustered Styles (Low Entropy)
        // 1.0f = Phase 1 (No Kuwahara) | Scattered Styles (High Entropy)
        // 2.0f = Phase 2 (Kuwahara)    | Clustered Styles (Low Entropy)
        // 3.0f = Phase 2 (Kuwahara)    | Scattered Styles (High Entropy)
        stepValues = {0.0f, 1.0f, 2.0f, 3.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_4_2_StyleCombinatorics: {
        // Step mapping defines the number of active styles in the high-entropy pool:
        // 0.0f = 1 Style  (100% Blinn)
        // 1.0f = 2 Styles (50% Blinn, 50% Gooch)
        // 2.0f = 3 Styles (33% Blinn, 33% Gooch, 33% Toon)
        stepValues = {0.0f, 1.0f, 2.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_4_3_KernelBandwidth: {
        // Evaluate bandwidth scaling with kernel size:
        // Radius 2, 4, 8, 12, 16
        stepValues = {2.0f, 4.0f, 8.0f, 12.0f, 16.0f};
        break;
    }
    case BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline:
    case BenchmarkSuite::Suite_5_5_Pass2_ShadingTax:
    case BenchmarkSuite::Suite_5_5_Pass3_ParityFlythrough: {
        targetPipelines = {RenderPath::Forward, RenderPath::DeferredUber,
                           RenderPath::DeferredVolume};
        stepValues      = {0.0f}; // Single step, 1000 frames
        break;
    }
    case BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity: {
        targetPipelines = {RenderPath::DeferredUber, RenderPath::DeferredVolume};
        stepValues      = {0.0f};
        break;
    }
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

    // TEST SUITE CONFIGURATIONS
    // ---------------------------------------------------------------------------------------------

    switch (currentSuite) {
    case BenchmarkSuite::Suite_5_1_1_LodMicroGeom: {
        currentState.activeObstacleCount = 2000;
        currentState.objectSphereRadius  = 50.0f;
        currentState.renderFloor         = false;
        currentState.activeLightCount    = 0; // Pure geometry baseline

        // Apply LOD Index
        currentState.currentLodIndex = static_cast<int>(stepValues[currentStepIndex]);

        // Lock camera
        cameraController.SetDeterministicState({0.0f, 25.0f, 150.0f}, // Position
                                               {0.0f, 25.0f, 0.0f},   // Target
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_1_2_ObjectCount: {
        // Apply independent variable (Instances)
        currentState.activeObstacleCount = static_cast<int>(stepValues[currentStepIndex]);

        // Dynamic Variable: Expand generation radius linearly with object count
        currentState.objectSphereRadius =
            static_cast<float>(currentState.activeObstacleCount) / 100.0f;

        // Locked Variables to strictly isolate the CPU
        currentState.renderFloor      = false;
        currentState.activeLightCount = 0;
        currentState.currentLodIndex  = 0; // 12-triangle cubes

        // Step back far enough to encompass the expanding generation radius
        const float r = currentState.objectSphereRadius;
        float d       = r / sinf((Config::EngineSettings::CameraFOV / 2.0f) * DEG2RAD);
        d += 50.0f; // Safe margin

        cameraController.SetDeterministicState({0.0f, 0.0f, d}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_1_3_OverdrawDensity: {
        // Apply independent variable (Instances)
        currentState.activeObstacleCount = static_cast<int>(stepValues[currentStepIndex]);

        // Locked Variables to strictly isolate Overdraw Density
        currentState.objectSphereRadius = 30.0f; // Fixed, tight radius
        currentState.renderFloor        = false;
        currentState.activeLightCount   = 0;
        currentState.currentLodIndex    = 1; // Balance between pixel coverage and vertex cost

        // Camera Fixed position
        const float r = currentState.objectSphereRadius;
        float d       = r / sinf((Config::EngineSettings::CameraFOV / 2.0f) * DEG2RAD);
        d += 15.0f; // Tight margin to maximize screen-space coverage

        cameraController.SetDeterministicState({0.0f, 0.0f, d}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_2_1_ResolutionScaling: {
        // Map abstract step values to absolute rendering resolutions
        switch (static_cast<int>(stepValues[currentStepIndex])) {
        case 0:
            currentState.renderWidth  = 854;
            currentState.renderHeight = 480;
            break;
        case 1:
            currentState.renderWidth  = 1280;
            currentState.renderHeight = 720;
            break;
        case 2:
            currentState.renderWidth  = 1920;
            currentState.renderHeight = 1080;
            break;
        case 3:
            currentState.renderWidth  = 2560;
            currentState.renderHeight = 1440;
            break;
        case 4:
            currentState.renderWidth  = 3840;
            currentState.renderHeight = 2160;
            break;
        default:
            currentState.renderWidth  = 1920;
            currentState.renderHeight = 1080;
            break;
        }

        // Standard Scene Baseline: Moderate geometry, baseline lighting, no stylistic entropy
        currentState.activeObstacleCount  = 5000;
        currentState.currentLodIndex      = 1;
        currentState.activeLightCount     = 200;
        currentState.renderFloor          = true;
        currentState.ambientLightStrength = 0.25f;

        // Statick locked Camera
        cameraController.SetDeterministicState({150.0f, 150.0f, 150.0f}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
    }
    case BenchmarkSuite::Suite_5_2_2_BaseBandwidthTax: {
        // Map stepValue to renderFloor boolean
        currentState.renderFloor = (stepValues[currentStepIndex] > 0.0f);

        // Isolate bandwidth
        currentState.activeObstacleCount  = 0;
        currentState.activeLightCount     = 0;
        currentState.ambientLightStrength = 1.00f;

        // Static camera pointing straight down at the floor origin
        cameraController.SetDeterministicState({0.1f, 150.0f, 0.1f}, // Position
                                               {0.0f, 0.0f, 0.0f},   // Target
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_3_1_LightCount: {
        const int targetLights = static_cast<int>(stepValues[currentStepIndex]);

        // Enforce architectural caps based on Test Suite Definitions
        if (currentState.activeRenderPath != RenderPath::DeferredVolume && targetLights > 500) {
            // Immediately advance the state machine to skip recording this configuration
            AdvanceState();
            return;
        }

        currentState.activeLightCount     = targetLights;
        currentState.activeObstacleCount  = 5000;
        currentState.renderFloor          = true;
        currentState.objectSphereRadius   = 200.0f;
        currentState.lightIntensity       = 1.0f;
        currentState.useLightSingularity  = false;
        currentState.currentLodIndex      = 1;
        currentState.ambientLightStrength = 0.0f;

        // Static camera elevated to view the entire scene
        cameraController.SetDeterministicState({300.0f, 300.0f, 300.0f}, // Position
                                               {0.0f, 0.0f, 0.0f},       // Target
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_3_2_1_LightIntensity: {
        currentState.lightIntensity = stepValues[currentStepIndex];

        // Lock light count to 250
        currentState.activeLightCount = 250;

        currentState.activeObstacleCount  = 5000;
        currentState.renderFloor          = true;
        currentState.objectSphereRadius   = 200.0f;
        currentState.ambientLightStrength = 0.0f;
        currentState.useLightSingularity  = false;
        currentState.currentLodIndex      = 1;

        cameraController.SetDeterministicState({300.0f, 300.0f, 300.0f}, // Position
                                               {0.0f, 0.0f, 0.0f},       // Target
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_3_2_2_LightSingularity: {
        const int targetLights = static_cast<int>(stepValues[currentStepIndex]);

        // Enforce architectural caps based on Test Suite Definitions
        if (currentState.activeRenderPath != RenderPath::DeferredVolume && targetLights > 500) {
            // Immediately advance the state machine to skip recording this configuration
            AdvanceState();
            return;
        }

        currentState.activeLightCount     = targetLights;
        currentState.activeObstacleCount  = 5000;
        currentState.renderFloor          = true;
        currentState.objectSphereRadius   = 200.0f;
        currentState.lightIntensity       = 1.0f;
        currentState.useLightSingularity  = true; // Force all lights to spawn at the origin
        currentState.ambientLightStrength = 0.0f;
        currentState.currentLodIndex      = 1;

        // Camera Close to light singularity
        cameraController.SetDeterministicState({0.0f, 50.0f, 50.0f}, // Position
                                               {0.0f, 0.0f, 0.0f},   // Target
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_3_3_ShadingOverdraw: {
        // Independent Variable: Geometric Density
        currentState.activeObstacleCount = static_cast<int>(stepValues[currentStepIndex]);

        // Locked Variables: Force heavy depth complexity with a heavy lighting baseline
        currentState.objectSphereRadius = 30.0f; // Fixed tight radius
        currentState.activeLightCount   = 100;   // The catalyst for Forward ALU collapse
        currentState.currentLodIndex    = 1;     // Balance vertex cost
        currentState.renderFloor        = false;
        currentState.lightIntensity     = 1.0f;

        // Camera Fixed position
        const float r = currentState.objectSphereRadius;
        float d       = r / sinf((Config::EngineSettings::CameraFOV / 2.0f) * DEG2RAD);
        d += 15.0f; // Tight margin to maximize screen-space coverage

        cameraController.SetDeterministicState({0.0f, 0.0f, d}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
    }
    case BenchmarkSuite::Suite_5_4_1_SpatialEntropy: {
        const int step = static_cast<int>(stepValues[currentStepIndex]);

        // Phase 2 (Steps 2 and 3) introduces extreme warp divergence via Kuwahara + Sobel
        if (step >= 2 && currentState.activeRenderPath == RenderPath::Forward) {
            // The Forward pipeline lacks Kuwahara; Advance the state machine
            AdvanceState();
            return;
        }

        // Scene setup
        currentState.activeObstacleCount = 15000;
        currentState.activeLightCount    = 250;
        currentState.renderFloor         = false;
        currentState.objectSphereRadius =
            175.0f; // Compress density to maximize screen-space filter workload

        // Extract spatial entropy state
        // Steps 0 and 2 are even (Clustered), Steps 1 and 3 are odd (Scattered)
        currentState.useClusteredStyles = (step % 2 == 0);

        // Apply shared NPR baseline
        currentState.enableGooch = true;
        currentState.enableToon  = true;

        // Apply Phase 2 Extreme Divergence (Kuwahara + Outlines)
        if (step >= 2) {
            currentState.enableKuwahara = true;
            currentState.kuwaharaRadius = 4;
            currentState.enableOutlines = true;
        } else {
            currentState.enableKuwahara = false;
            currentState.enableOutlines = false;
        }

        // Top-down camera to cleanly capture the X-Z plane sorting algorithm
        cameraController.SetDeterministicState({0.1f, 550.0f, 0.1f}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_4_2_StyleCombinatorics: {
        const int step = static_cast<int>(stepValues[currentStepIndex]);

        // Enforce the heavy baseline established in 5.4.1
        currentState.activeObstacleCount = 15000;
        currentState.activeLightCount    = 250;
        currentState.objectSphereRadius  = 175.0f;
        currentState.renderFloor         = false;
        currentState.useClusteredStyles  = true;

        // Force maximum spatial chaos and disable post-process kernels

        // Combinatorics Step Logic
        if (step == 0) {
            // 1 Style: 100% Blinn-Phong
            currentState.enableGooch = false;
            currentState.enableToon  = false;
        } else if (step == 1) {
            // 2 Styles: 50% Blinn, 50% Gooch
            currentState.enableGooch = true;
            currentState.enableToon  = false;
        } else if (step == 2) {
            // 3 Styles: 33% Blinn, 33% Gooch, 33% Toon
            currentState.enableGooch = true;
            currentState.enableToon  = true;
        }

        // Top-down camera for consistent viewing of the highly entropic distribution
        cameraController.SetDeterministicState({0.1f, 550.0f, 0.1f}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_4_3_KernelBandwidth: {
        const int radius = static_cast<int>(stepValues[currentStepIndex]);

        if (currentState.activeRenderPath == RenderPath::Forward) {
            // The Forward pipeline lacks Kuwahara; Advance the state machine
            AdvanceState();
            return;
        }

        currentState.activeObstacleCount = 15000;
        currentState.activeLightCount    = 250;
        currentState.objectSphereRadius  = 175.0f;

        // Forced Extreme Divergence/Resolution:
        // Force every object to use Kuwahara to saturate bandwidth
        currentState.useClusteredStyles = false;
        currentState.enableKuwahara     = true;
        currentState.kuwaharaRadius     = radius;
        currentState.renderFloor        = false;

        cameraController.SetDeterministicState({0.1f, 525.0f, 0.1f}, {0.0f, 0.0f, 0.0f},
                                               Config::EngineSettings::CameraFOV);
        break;
    }
    case BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline:
    case BenchmarkSuite::Suite_5_5_Pass2_ShadingTax:
    case BenchmarkSuite::Suite_5_5_Pass3_ParityFlythrough:
    case BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity: {
        // Base scene state (Applies to all passes)
        currentState.activeObstacleCount  = 15000;
        currentState.objectSphereRadius   = 150.0f;
        currentState.currentLodIndex      = 1;
        currentState.renderFloor          = true;
        currentState.useClusteredStyles   = false;
        currentState.ambientLightStrength = 0.25f;
        currentState.lightIntensity       = 2.0f;
        useFrameLimit                     = true;
        targetFrameCount                  = 1000;

        // PASS-SPECIFIC LOGIC
        if (currentSuite == BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline) {
            currentState.activeLightCount = 0;
            currentState.enableGooch      = false;
            currentState.enableToon       = false;
            currentState.enableOutlines   = false;
            currentState.enableKuwahara   = false;
        } else if (currentSuite == BenchmarkSuite::Suite_5_5_Pass2_ShadingTax) {
            currentState.activeLightCount = 250;
            currentState.enableGooch      = false;
            currentState.enableToon       = false;
            currentState.enableOutlines   = false;
            currentState.enableKuwahara   = false;
        } else if (currentSuite == BenchmarkSuite::Suite_5_5_Pass3_ParityFlythrough) {
            currentState.activeLightCount = 250;
            currentState.enableGooch      = true;
            currentState.enableToon       = true;
            currentState.enableOutlines   = true;
            currentState.enableKuwahara   = false;
        } else if (currentSuite == BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity) {
            currentState.activeLightCount = 250;
            currentState.enableGooch      = true;
            currentState.enableToon       = true;
            currentState.enableOutlines   = true;
            currentState.enableKuwahara   = true;
            currentState.kuwaharaRadius   = 6;
        }
        break;
    }
    default:
        break;
    }
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
    telemetryWriter.Close(GetSuiteName(currentSuite));
    cameraController.SetLocked(false);
    currentSuite = BenchmarkSuite::Complete;
    std::cout << "[ORCHESTRATOR] Suite execution completed. Telemetry flushed.\n";
}

bool BenchmarkOrchestrator::IsActive() const {
    return currentSuite != BenchmarkSuite::Inactive && currentSuite != BenchmarkSuite::Complete;
}
bool BenchmarkOrchestrator::DidStateChangeThisFrame() const { return stateChangedThisFrame; }
const EngineState &BenchmarkOrchestrator::GetCurrentState() const { return currentState; }

void BenchmarkOrchestrator::InjectPerFrameState() const {
    if (currentPhase == BenchPhase::Capture &&
        currentSuite >= BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline &&
        currentSuite <= BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity) {
        // 't' maps strictly from 0.0 to 1.0 over the course of the benchmark
        const float t = static_cast<float>(frameCounter) / static_cast<float>(targetFrameCount);

        // Convert 't' to a full 2*PI rotation
        const float angle         = t * 2.0f * PI;
        constexpr float pathScale = 200.0f;

        const float camX = pathScale * sinf(angle);
        const float camY =
            75.0f + 35.0f * sinf(angle * 2.0f); // Bob up and down to change overdraw density
        const float camZ = pathScale * sinf(angle) * cosf(angle);

        // Calculate a point slightly ahead on the curve to act as the camera target
        const float angleAhead = (t + 0.02f) * 2.0f * PI;
        const float targetX    = pathScale * sinf(angleAhead);
        const float targetY    = 75.0f + 35.0f * sinf(angleAhead * 2.0f);
        const float targetZ    = pathScale * sinf(angleAhead) * cosf(angleAhead);

        cameraController.SetDeterministicState({camX, camY, camZ}, {targetX, targetY, targetZ},
                                               Config::EngineSettings::CameraFOV);
    }
}

std::string BenchmarkOrchestrator::GetSuiteName(const BenchmarkSuite suite) {
    switch (suite) {
    case BenchmarkSuite::Suite_5_1_1_LodMicroGeom:
        return "5-1-1-LodMicroGeom";
    case BenchmarkSuite::Suite_5_1_2_ObjectCount:
        return "5-1-2-ObjectCount";
    case BenchmarkSuite::Suite_5_1_3_OverdrawDensity:
        return "5-1-3-OverdrawDensity";
    case BenchmarkSuite::Suite_5_2_1_ResolutionScaling:
        return "5-2-1-ResolutionScaling";
    case BenchmarkSuite::Suite_5_2_2_BaseBandwidthTax:
        return "5-2-2-BaseBandwidthTax";
    case BenchmarkSuite::Suite_5_3_1_LightCount:
        return "5-3-1-LightCount";
    case BenchmarkSuite::Suite_5_3_2_1_LightIntensity:
        return "5-3-2-1-LightIntensity";
    case BenchmarkSuite::Suite_5_3_2_2_LightSingularity:
        return "5-3-2-2-LightSingularity";
    case BenchmarkSuite::Suite_5_3_3_ShadingOverdraw:
        return "5-3-3-ShadingOverdraw";
    case BenchmarkSuite::Suite_5_4_1_SpatialEntropy:
        return "5-4-1-SpatialEntropy";
    case BenchmarkSuite::Suite_5_4_2_StyleCombinatorics:
        return "5-4-2-StyleCombinatorics";
    case BenchmarkSuite::Suite_5_4_3_KernelBandwidth:
        return "5-4-3-KernelBandwidth";
    case BenchmarkSuite::Suite_5_5_Pass1_GeometryBaseline:
        return "5-5-Pass1-GeometryBaseline";
    case BenchmarkSuite::Suite_5_5_Pass2_ShadingTax:
        return "5-5-Pass2-ShadingTax";
    case BenchmarkSuite::Suite_5_5_Pass3_ParityFlythrough:
        return "5-5-Pass3-ParityFlythrough";
    case BenchmarkSuite::Suite_5_5_Pass4_DeferredMaxFidelity:
        return "5-5-Pass4-DeferredMaxFidelity";
    default:
        return "Unknown-Suite";
    }
}