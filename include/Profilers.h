#pragma once

#include <chrono>

struct CpuProfiler {
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
    double elapsedMs = 0.0;

    void Begin();
    void End();
};

struct GpuProfiler {
    unsigned int queries[2]{0, 0};
    int queryFrame   = 0;
    double elapsedMs = 0.0;

    void Init();
    void Begin() const;
    static void End();
    void Update();
    void Destroy() const;
};
