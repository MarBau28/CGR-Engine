#pragma once

#include <string>
#include <vector>

// Defines a single aggregated measurement point per test suite
struct BenchmarkResult {
    std::string suiteStr;
    std::string archName;
    double avgCpu;
    double avgGeom;
    double avgLight;
    double avgTotal;
    int currentInstances;
    int currentLod;
    int currentTris;
    int currentLights;
    int lightTop;
    int styleTop;
    int currentKuwRad;
    double currentOverdraw;
};

class CsvTelemetryWriter {
  public:
    // Acts as a setup/clear mechanism to maintain the existing API
    bool Initialize(const std::string &defaultFilename);

    // Buffers data in memory instead of immediately writing to disk
    void AppendRow(const std::string &suiteStr, const std::string &archName, double avgCpu,
                   double avgGeom, double avgLight, double avgTotal, int currentInstances,
                   int currentLod, int currentTris, int currentLights, int lightTop, int styleTop,
                   int currentKuwRad, double currentOverdraw);

    // Flushes buffered data to disk with a timestamped filename
    void Close() const;

  private:
    std::vector<BenchmarkResult> metrics;
    std::string baseFilename;
};