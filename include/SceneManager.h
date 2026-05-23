#pragma once

#include "EngineState.h"
#include "raylib.h"
#include <vector>

// forward declarations
class CameraController;
struct EngineState;

// Represents a spatial volume for CPU culling
struct BoundingSphere {
    Vector3 center;
    float radius;
};

class SceneManager {
  public:
    SceneManager();

    // Deterministic scene generation based on the current benchmark state
    void RebuildScene(const EngineState &state);

    // Executes CPU-side frustum culling and populates visible arrays
    void UpdateVisibility(CameraController &camera, const EngineState &state, int screenWidth,
                          int screenHeight);

    // Base Deferred Data
    [[nodiscard]] const std::vector<Matrix> &GetVisibleObstacleTransforms() const {
        return visibleObstacleTransforms;
    }
    [[nodiscard]] const std::vector<float> &GetVisibleObstacleStyleIds() const {
        return visibleObstacleStyleIds;
    }

    // Light Data
    [[nodiscard]] const std::vector<Matrix> &GetVisibleLightVolumeTransforms() const {
        return visibleLightVolumeTransforms;
    }
    [[nodiscard]] const std::vector<Matrix> &GetVisibleLightProxyTransforms() const {
        return visibleLightProxyTransforms;
    }
    [[nodiscard]] const std::vector<Vector3> &GetVisibleLightPositions() const {
        return visibleLightPositions;
    }

    // forward Rendering Specific Bins (Pre-sorted during culling)
    [[nodiscard]] const std::vector<Matrix> &GetFwdTransformsBlinn() const {
        return fwdTransformsBlinn;
    }
    [[nodiscard]] const std::vector<Matrix> &GetFwdTransformsGooch() const {
        return fwdTransformsGooch;
    }
    [[nodiscard]] const std::vector<Matrix> &GetFwdTransformsToon() const {
        return fwdTransformsToon;
    }
    [[nodiscard]] const std::vector<Matrix> &GetFwdTransformsOutline() const {
        return fwdTransformsOutline;
    }

    [[nodiscard]] int GetActualGeneratedLights() const { return actualGeneratedLights; }

    // Access to master data strictly for initial VBO uploads
    [[nodiscard]] const std::vector<float> &GetMasterStyleIds() const {
        return masterObstacleStyleIds;
    }

  private:
    // Master Data (Immutable during a single rendering frame)
    std::vector<Matrix> masterObstacleTransforms;
    std::vector<float> masterObstacleStyleIds;
    std::vector<BoundingSphere> masterObstacleSpheres;
    std::vector<Vector3> masterLightPositions;
    std::vector<Matrix> masterLightProxyTransforms;

    int actualGeneratedLights = 0;

    // Render Data (Rebuilt every frame by UpdateVisibility)
    std::vector<Matrix> visibleObstacleTransforms;
    std::vector<float> visibleObstacleStyleIds;
    std::vector<Matrix> visibleLightProxyTransforms;
    std::vector<Matrix> visibleLightVolumeTransforms;
    std::vector<Vector3> visibleLightPositions;

    // forward Bins
    std::vector<Matrix> fwdTransformsBlinn;
    std::vector<Matrix> fwdTransformsGooch;
    std::vector<Matrix> fwdTransformsToon;
    std::vector<Matrix> fwdTransformsOutline;

    // Internal Generators
    void GenerateStandardScene(float sphereRadius, bool clustered);
    void GenerateNprRoomScene(int lightCount);

    // Returns the true obstacle count depending on the active environment
    [[nodiscard]] int GetTotalObstacleCount(const EngineState &state) const;

    // Calculates the volumetric overlap of instances
    [[nodiscard]] float CalculateTheoreticalOverdraw(const EngineState &state) const;
};