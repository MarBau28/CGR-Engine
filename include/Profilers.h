#pragma once
#include <glad/glad.h>

class CpuProfiler {
  public:
    long long startTimeNs = 0;
    double elapsedMs      = 0.0;

    void Begin();
    void End();
};

class GpuProfiler {
  public:
    static constexpr int MAX_IN_FLIGHT = 16;

    void Init();
    void AwaitCapacity() const;
    void Begin() const;
    void End();
    void Reset();

    // Checks if the oldest pending query has finished
    [[nodiscard]] bool IsOldestReady() const;

    // Retrieves the data and advances the ring buffer
    bool TryGetOldestResult(double &outElapsedMs, bool forceWait);

    [[nodiscard]] double GetLastMeasuredMs() const { return lastMeasuredMs; }

    void Destroy() const;

  private:
    GLuint startQueries[MAX_IN_FLIGHT] = {};
    GLuint endQueries[MAX_IN_FLIGHT]   = {};
    int currentFrame                   = 0;
    int oldestFrame                    = 0;
    int pendingCount                   = 0;
    double lastMeasuredMs              = 0.0;
};
