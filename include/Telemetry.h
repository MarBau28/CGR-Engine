#pragma once

#include <fstream>
#include <string>
#include <vector>

struct FrameMetrics {
    std::string suiteId;
    std::string architecture;
    float stepValue;
    int frameNumber;
    double cpuLogicMs;
    double cpuRenderMs;
    double geomMs;
    double lightMs;
    double totalGpuMs;
    double totalFrameMs;
    double fps;
    int instanceCount;
    int triangleCount;
    double theoreticalOverdraw;
};

class CsvTelemetryWriter {
  public:
    bool Initialize(const std::string &defaultFilename);
    void AppendRow(const std::string &suiteStr, const std::string &archName, float stepValue,
                   int frameNum, double cpuLogicMs, double cpuRenderMs, double geomMs,
                   double lightMs, double totalGpuMs, double totalFrameMs, double fps,
                   int currentInstances, int currentTris, double currentOverdraw);
    void Close() const;

  private:
    std::string baseFilename;
    std::vector<FrameMetrics> metrics;
};