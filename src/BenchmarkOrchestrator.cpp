#include "../include/BenchmarkOrchestrator.h"
#include <iostream>

BenchmarkOrchestrator::BenchmarkOrchestrator(CsvTelemetryWriter &telemetry, CpuProfiler &cpuProf,
                                             GpuProfiler &geomProf, GpuProfiler &lightProf,
                                             CameraController &camCtrl)
    : telemetryWriter(telemetry), cpuProfiler(cpuProf), geomProfiler(geomProf),
      lightProfiler(lightProf), cameraController(camCtrl), currentSuite(BenchmarkSuite::Inactive),
      accumCpu(0), accumGeom(0), accumLight(0), accumTotal(0), recordedFrames(0) {}

void BenchmarkOrchestrator::Start(BenchmarkSuite suite) {
    if (currentSuite != BenchmarkSuite::Inactive)
        return;

    currentSuite = suite;
    sweepIndex   = 0;
    currentSweepArray.clear();

    // Lock camera for exact view-matrix reproduction across tests
    cameraController.SetLocked(true);
    cameraController.SetDeterministicState({50.0f, 30.0f, 50.0f}, {0.0f, 0.0f, 0.0f}, 45.0f);

    // Setup scaling parameters for the requested suite
    switch (suite) {
    case BenchmarkSuite::SuiteA_Geom:
        currentSweepArray = {0, 1, 2, 3, 4};
        break; // LOD sweep
    case BenchmarkSuite::SuiteB_DrawCall:
        currentSweepArray = {1000, 5000, 10000, 25000, 50000};
        break; // Instances
    case BenchmarkSuite::SuiteC_Entropy:
        currentSweepArray = {0, 1};
        break; // 0=Random, 1=Clustered
    case BenchmarkSuite::SuiteD_FillRate:
    case BenchmarkSuite::SuiteE_Singularity:
        currentSweepArray = {10, 50, 100, 250, 500};
        break; // Light count / Collapsed light count
    case BenchmarkSuite::SuiteF_Spatial:
        currentSweepArray = {2, 4, 6, 8};
        break; // Kuwahara radii
    case BenchmarkSuite::SuiteG_Synthesis:
        currentSweepArray = {1000, 5000, 10000, 25000};
        break; // Mixed load scaling
    case BenchmarkSuite::SuiteH_Overdraw:
        currentSweepArray = {250, 200, 150, 100, 50};
        break; // Decreasing cloud radius
    default:
        EndBenchmark();
        return;
    }

    ApplySuiteState();
    std::cout << "Benchmark Started: Suite ID " << static_cast<int>(suite) << "\n";
}

void BenchmarkOrchestrator::ApplySuiteState() {
    // Reset baseline to prevent cross-contamination
    currentState.enableOutlines      = false;
    currentState.enableKuwahara      = false;
    currentState.enableGooch         = false;
    currentState.enableToon          = false;
    currentState.useNprRoom          = false;
    currentState.useClusteredStyles  = false;
    currentState.useLightSingularity = false;

    // Set static defaults
    currentState.activeObstacleCount = 5000;
    currentState.activeLightCount    = 100;
    currentState.currentLodIndex     = 0;
    currentState.objectSphereRadius  = 200.0f;
    currentState.kuwaharaRadius      = 4;

    const int currentSweepValue = currentSweepArray[sweepIndex];

    switch (currentSuite) {
    case BenchmarkSuite::SuiteA_Geom:
        currentState.activeRenderPath = RenderPath::DeferredUber;
        currentState.currentLodIndex  = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteB_DrawCall:
        currentState.activeRenderPath    = RenderPath::Forward;
        currentState.activeObstacleCount = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteC_Entropy:
        currentState.activeRenderPath   = RenderPath::DeferredUber;
        currentState.useNprRoom         = true;
        currentState.useClusteredStyles = (currentSweepValue == 1);
        break;
    case BenchmarkSuite::SuiteD_FillRate:
        currentState.activeRenderPath    = RenderPath::DeferredVolume;
        currentState.activeObstacleCount = 1;
        currentState.activeLightCount    = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteE_Singularity:
        currentState.activeRenderPath    = RenderPath::DeferredVolume;
        currentState.activeObstacleCount = 1;
        currentState.useLightSingularity = true;
        currentState.activeLightCount    = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteF_Spatial:
        currentState.activeRenderPath = RenderPath::DeferredUber;
        currentState.enableKuwahara   = true;
        currentState.kuwaharaRadius   = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteG_Synthesis:
        currentState.activeRenderPath    = RenderPath::DeferredUber;
        currentState.currentLodIndex     = 1;
        currentState.enableGooch         = true;
        currentState.enableToon          = true;
        currentState.enableKuwahara      = true;
        currentState.activeObstacleCount = currentSweepValue;
        break;
    case BenchmarkSuite::SuiteH_Overdraw:
        currentState.activeRenderPath    = RenderPath::DeferredUber;
        currentState.activeObstacleCount = 10000;
        currentState.objectSphereRadius  = static_cast<float>(currentSweepValue);
        break;
    default:
        break;
    }

    currentFrameCount     = 0;
    isWarmingUp           = true;
    stateChangedThisFrame = true;

    accumCpu       = 0.0;
    accumGeom      = 0.0;
    accumLight     = 0.0;
    accumTotal     = 0.0;
    recordedFrames = 0;
}

void BenchmarkOrchestrator::UpdateAndRecord(const double totalFrameTimeMs, const int activeTris,
                                            const double currentOverdraw) {
    if (currentSuite == BenchmarkSuite::Inactive)
        return;

    stateChangedThisFrame = false; // Reset the flag immediately
    currentFrameCount++;

    if (isWarmingUp) {
        if (currentFrameCount >= warmupFrames) {
            isWarmingUp       = false;
            currentFrameCount = 0;
        }
    } else {
        // Recording Phase
        accumCpu += cpuProfiler.elapsedMs;
        accumGeom += geomProfiler.elapsedMs;
        accumLight += lightProfiler.elapsedMs;
        accumTotal += totalFrameTimeMs;
        recordedFrames++;

        if (currentFrameCount >= recordFrames) {
            const double avgCpu   = accumCpu / recordedFrames;
            const double avgGeom  = accumGeom / recordedFrames;
            const double avgLight = accumLight / recordedFrames;
            const double avgTotal = accumTotal / recordedFrames;

            const std::string archStr =
                currentState.activeRenderPath == RenderPath::Forward
                    ? "forward"
                    : (currentState.activeRenderPath == RenderPath::DeferredUber
                           ? "Deferred_Uber"
                           : "Deferred_Volume");

            const int lightTop = currentState.useLightSingularity ? 1 : 0;
            const int styleTop =
                currentState.useNprRoom ? 2 : (currentState.useClusteredStyles ? 1 : 0);

            telemetryWriter.AppendRow(
                "Suite_" + std::to_string(static_cast<int>(currentSuite)), archStr, avgCpu, avgGeom,
                avgLight, avgTotal, currentState.activeObstacleCount, currentState.currentLodIndex,
                activeTris, currentState.activeLightCount, lightTop, styleTop,
                currentState.kuwaharaRadius, currentOverdraw);

            sweepIndex++;
            if (sweepIndex >= currentSweepArray.size()) {
                EndBenchmark();
            } else {
                ApplySuiteState(); // Steps to the next phase
            }
        }
    }
}

void BenchmarkOrchestrator::EndBenchmark() {
    telemetryWriter.Close();
    cameraController.SetLocked(false);
    currentSuite = BenchmarkSuite::Complete; // Signal main loop it finished
    std::cout << "Benchmark Suite Complete.\n";
}

bool BenchmarkOrchestrator::IsActive() const {
    return currentSuite != BenchmarkSuite::Inactive && currentSuite != BenchmarkSuite::Complete;
}
bool BenchmarkOrchestrator::DidStateChangeThisFrame() const { return stateChangedThisFrame; }
const EngineState &BenchmarkOrchestrator::GetCurrentState() const { return currentState; }