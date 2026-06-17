#include "../include/CsvTelemetryWriter.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

bool CsvTelemetryWriter::Initialize(const std::string &defaultFilename) {
    metrics.clear();
    // Pre-Allocating reasonable bounds minimizes runtime allocation overhead during capture phases
    metrics.reserve(50000);
    baseFilename = defaultFilename;
    return true;
}

void CsvTelemetryWriter::AppendRow(const std::string &suiteStr, const std::string &archName,
                                   const float stepValue, const int frameNum,
                                   const double cpuLogicMs, const double cpuRenderMs,
                                   const double geomMs, const double lightMs,
                                   const double totalGpuMs, const double totalFrameMs,
                                   const double fps, const int currentInstances,
                                   const int currentTris, const double currentOverdraw) {
    metrics.push_back(FrameMetrics{suiteStr, archName, stepValue, frameNum, cpuLogicMs, cpuRenderMs,
                                   geomMs, lightMs, totalGpuMs, totalFrameMs, fps, currentInstances,
                                   currentTris, currentOverdraw});
}

void CsvTelemetryWriter::Close(const std::string &currentSuiteName) const {
    if (metrics.empty()) {
        std::cerr << "[TELEMETRY] Write aborted: Metric buffer is empty." << std::endl;
        return;
    }

    namespace fs         = std::filesystem;
    fs::path currentPath = fs::current_path();

    // Escape CMake build directories
    if (currentPath.filename().string().find("cmake-build") != std::string::npos) {
        currentPath = currentPath.parent_path();
    }

    fs::path outputDir = currentPath / "outputs" / "benchmarks";

    if (!fs::exists(outputDir)) {
        std::error_code ec;
        fs::create_directories(outputDir, ec);
    }

    std::string cleanBase = baseFilename;
    if (size_t extPos = cleanBase.find(".csv"); extPos != std::string::npos) {
        cleanBase.erase(extPos);
    }

    std::string fileName = std::format("{}_{}.csv", cleanBase, currentSuiteName);
    fs::path fullPath    = outputDir / fileName;

    std::ofstream file(fullPath);
    if (!file.is_open()) {
        std::cerr << "[TELEMETRY] IO Error: Failed to acquire file handle for " << fullPath.string()
                  << std::endl;
        return;
    }

    file << "Suite,Architecture,StepValue,FrameNumber,CpuLogicMs,CpuRenderMs,GeomMs,LightMs,"
            "TotalGpuMs,TotalFrameMs,FPS,Instances,Triangles,OverdrawFactor\n";

    for (const auto &[suiteId, architecture, stepValue, frameNumber, cpuLogicMs, cpuRenderMs,
                      geomMs, lightMs, totalGpuMs, totalFrameMs, fps, instanceCount, triangleCount,
                      theoreticalOverdraw] : metrics) {
        file << suiteId << "," << architecture << "," << stepValue << "," << frameNumber << ","
             << cpuLogicMs << "," << cpuRenderMs << "," << geomMs << "," << lightMs << ","
             << totalGpuMs << "," << totalFrameMs << "," << fps << "," << instanceCount << ","
             << triangleCount << "," << theoreticalOverdraw << "\n";
    }

    file.close();
    std::cout << "[TELEMETRY] Matrix flushed successfully to: " << fullPath.string() << std::endl;
}