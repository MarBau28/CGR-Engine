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
    baseFilename = defaultFilename;
    return true;
}

void CsvTelemetryWriter::AppendRow(const std::string &suiteStr, const std::string &archName,
                                   const double avgCpu, const double avgGeom, const double avgLight,
                                   const double avgTotal, const int currentInstances,
                                   const int currentLod, const int currentTris,
                                   const int currentLights, const int lightTop, const int styleTop,
                                   const int currentKuwRad, const double currentOverdraw) {
    metrics.push_back({suiteStr, archName, avgCpu, avgGeom, avgLight, avgTotal, currentInstances,
                       currentLod, currentTris, currentLights, lightTop, styleTop, currentKuwRad,
                       currentOverdraw});
}

void CsvTelemetryWriter::Close() const {
    if (metrics.empty()) {
        std::cerr << "Telemetry Warning: Attempted to save empty benchmark." << std::endl;
        return;
    }

    // Ensure isolated output directory exists
    const std::string outputDir = "outputs/benchmarks";
    if (!fs::exists(outputDir)) {
        fs::create_directories(outputDir);
    }

    // Generate deterministic, timestamped filename
    auto now       = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");

    // Strip .csv from baseFilename if it exists, then append timestamp
    std::string cleanBase = baseFilename;
    if (size_t extPos = cleanBase.find(".csv"); extPos != std::string::npos)
        cleanBase.erase(extPos);

    std::string filename = outputDir + "/" + cleanBase + "_" + ss.str() + ".csv";

    // Flush to disk
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Telemetry Error: Failed to open file for writing: " << filename << std::endl;
        return;
    }

    // Write CSV Header
    file << "Suite,Architecture,AvgCpuMs,AvgGeomMs,AvgLightMs,AvgTotalMs,Instances,LOD,Triangles,"
            "Lights,LightTopology,StyleTopology,KuwaharaRadius,OverdrawFactor\n";

    // Write Payload
    for (const auto &[suiteStr, archName, avgCpu, avgGeom, avgLight, avgTotal, currentInstances,
                      currentLod, currentTris, currentLights, lightTop, styleTop, currentKuwRad,
                      currentOverdraw] : metrics) {
        file << suiteStr << "," << archName << "," << avgCpu << "," << avgGeom << "," << avgLight
             << "," << avgTotal << "," << currentInstances << "," << currentLod << ","
             << currentTris << "," << currentLights << "," << lightTop << "," << styleTop << ","
             << currentKuwRad << "," << currentOverdraw << "\n";
    }

    file.close();
    std::cout << "Telemetry Success: Saved benchmark data to " << filename << std::endl;
}