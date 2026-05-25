#include "../include/Profilers.h"
#include <ctime>
#include <glad/glad.h>

// CPU Profiler
void CpuProfiler::Begin() {
    struct timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    startTimeNs = ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void CpuProfiler::End() {
    struct timespec ts{};
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    const long long endTimeNs = ts.tv_sec * 1000000000LL + ts.tv_nsec;

    elapsedMs = static_cast<double>(endTimeNs - startTimeNs) / 1000000.0;
}

// GPU Profiler
void GpuProfiler::Init() {
    glGenQueries(MAX_IN_FLIGHT, startQueries);
    glGenQueries(MAX_IN_FLIGHT, endQueries);

    // Inject dummy timestamps to initialize the OpenGL driver state
    for (int i = 0; i < MAX_IN_FLIGHT; ++i) {
        glQueryCounter(startQueries[i], GL_TIMESTAMP);
        glQueryCounter(endQueries[i], GL_TIMESTAMP);
    }
}

void GpuProfiler::AwaitCapacity() const {
    // By using GL_QUERY_RESULT, the driver handles the stall
    if (pendingCount >= MAX_IN_FLIGHT) {
        GLuint64 dummyTime = 0;
        glGetQueryObjectui64v(endQueries[oldestFrame], GL_QUERY_RESULT, &dummyTime);
    }
}

void GpuProfiler::Begin() const {
    // Inject start timestamp marker into the GPU command queue
    glQueryCounter(startQueries[currentFrame], GL_TIMESTAMP);
}

void GpuProfiler::End() {
    // Inject the end timestamp marker into the GPU command queue
    glQueryCounter(endQueries[currentFrame], GL_TIMESTAMP);

    currentFrame = (currentFrame + 1) % MAX_IN_FLIGHT;
    pendingCount++;
}

bool GpuProfiler::IsOldestReady() const {
    if (pendingCount == 0)
        return false;

    GLuint available = 0;
    // Command queue executes chronologically; if the end query is ready, the start query is too.
    glGetQueryObjectuiv(endQueries[oldestFrame], GL_QUERY_RESULT_AVAILABLE, &available);
    return available != 0;
}

bool GpuProfiler::TryGetOldestResult(double &outElapsedMs, const bool forceWait) {
    if (pendingCount == 0)
        return false;

    if (!forceWait) {
        GLuint available = 0;
        glGetQueryObjectuiv(endQueries[oldestFrame], GL_QUERY_RESULT_AVAILABLE, &available);
        if (!available)
            return false;
    }

    GLuint64 startTimeNs = 0;
    GLuint64 endTimeNs   = 0;

    // during teardown/flush
    glGetQueryObjectui64v(startQueries[oldestFrame], GL_QUERY_RESULT, &startTimeNs);
    glGetQueryObjectui64v(endQueries[oldestFrame], GL_QUERY_RESULT, &endTimeNs);

    // Calculate the delta and convert nanoseconds to milliseconds
    outElapsedMs   = static_cast<double>(endTimeNs - startTimeNs) / 1000000.0;
    lastMeasuredMs = outElapsedMs;

    oldestFrame = (oldestFrame + 1) % MAX_IN_FLIGHT;
    pendingCount--;

    return true;
}

void GpuProfiler::Destroy() const {
    glDeleteQueries(MAX_IN_FLIGHT, startQueries);
    glDeleteQueries(MAX_IN_FLIGHT, endQueries);
}

void GpuProfiler::Reset() {
    currentFrame   = 0;
    oldestFrame    = 0;
    pendingCount   = 0;
    lastMeasuredMs = 0.0;

    // Re-inject fresh timestamp markers to purge any stale driver state
    for (int i = 0; i < MAX_IN_FLIGHT; ++i) {
        glQueryCounter(startQueries[i], GL_TIMESTAMP);
        glQueryCounter(endQueries[i], GL_TIMESTAMP);
    }
}