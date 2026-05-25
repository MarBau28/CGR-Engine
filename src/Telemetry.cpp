#include "../include/Telemetry.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

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

void CsvTelemetryWriter::Close() const {
    if (metrics.empty()) {
        std::cerr << "[TELEMETRY] Write aborted: Metric buffer is empty." << std::endl;
        return;
    }

    const std::string outputDir = "outputs/benchmarks";
    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    auto now       = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");

    std::string cleanBase = baseFilename;
    if (size_t extPos = cleanBase.find(".csv"); extPos != std::string::npos) {
        cleanBase.erase(extPos);
    }

    const std::string filename = outputDir + "/" + cleanBase + "_" + ss.str() + ".csv";

    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[TELEMETRY] IO Error: Failed to acquire file handle for " << filename
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
    std::cout << "[TELEMETRY] Matrix flushed successfully to: " << filename << std::endl;
}