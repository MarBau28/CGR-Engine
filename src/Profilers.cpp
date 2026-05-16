#include "../include/Profilers.h"
#include <glad/glad.h>

// CPU Profiler
void CpuProfiler::Begin() { startTime = std::chrono::high_resolution_clock::now(); }

void CpuProfiler::End() {
    const auto endTime = std::chrono::high_resolution_clock::now();
    elapsedMs          = std::chrono::duration<double, std::milli>(endTime - startTime).count();
}

// GPU Profiler
void GpuProfiler::Init() {
    glGenQueries(2, queries);
    // Dummy query to initialize back-buffer and prevent read errors on frame 1
    glBeginQuery(GL_TIME_ELAPSED, queries[0]);
    glEndQuery(GL_TIME_ELAPSED);
    glBeginQuery(GL_TIME_ELAPSED, queries[1]);
    glEndQuery(GL_TIME_ELAPSED);
}

void GpuProfiler::Begin() const { glBeginQuery(GL_TIME_ELAPSED, queries[queryFrame]); }

void GpuProfiler::End() { glEndQuery(GL_TIME_ELAPSED); }

void GpuProfiler::Update() {
    unsigned int available = 0;

    // Check if GPU has finished writing the data
    glGetQueryObjectuiv(queries[1 - queryFrame], GL_QUERY_RESULT_AVAILABLE, &available);
    if (available) {
        GLuint64 timeNs = 0;
        glGetQueryObjectui64v(queries[1 - queryFrame], GL_QUERY_RESULT, &timeNs);
        elapsedMs = static_cast<double>(timeNs) / 1000000.0; // convert to milliseconds

        // Only swap if data was successfully received
        queryFrame = 1 - queryFrame;
    }
}

void GpuProfiler::Destroy() const { glDeleteQueries(2, queries); }