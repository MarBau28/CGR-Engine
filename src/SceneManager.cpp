#include "../include/SceneManager.h"
#include "../include/BenchmarkOrchestrator.h"
#include "../include/CameraController.h"
#include "../include/Config.h"
#include <algorithm>
#include <cmath>
#include <raymath.h>

namespace {
    // Internal spatial math structures
    struct FrustumPlane {
        Vector3 normal;
        float distance;

        void Normalize() {
            const float length = Vector3Length(normal);
            normal             = Vector3Scale(normal, 1.0f / length);
            distance /= length;
        }
    };

    struct Frustum {
        FrustumPlane planes[6];

        void Extract(const Matrix &vp) {
            // Left Plane
            planes[0].normal.x = vp.m3 + vp.m0;
            planes[0].normal.y = vp.m7 + vp.m4;
            planes[0].normal.z = vp.m11 + vp.m8;
            planes[0].distance = vp.m15 + vp.m12;

            // Right Plane
            planes[1].normal.x = vp.m3 - vp.m0;
            planes[1].normal.y = vp.m7 - vp.m4;
            planes[1].normal.z = vp.m11 - vp.m8;
            planes[1].distance = vp.m15 - vp.m12;

            // Bottom Plane
            planes[2].normal.x = vp.m3 + vp.m1;
            planes[2].normal.y = vp.m7 + vp.m5;
            planes[2].normal.z = vp.m11 + vp.m9;
            planes[2].distance = vp.m15 + vp.m13;

            // Top Plane
            planes[3].normal.x = vp.m3 - vp.m1;
            planes[3].normal.y = vp.m7 - vp.m5;
            planes[3].normal.z = vp.m11 - vp.m9;
            planes[3].distance = vp.m15 - vp.m13;

            // Near Plane
            planes[4].normal.x = vp.m3 + vp.m2;
            planes[4].normal.y = vp.m7 + vp.m6;
            planes[4].normal.z = vp.m11 + vp.m10;
            planes[4].distance = vp.m15 + vp.m14;

            // Far Plane
            planes[5].normal.x = vp.m3 - vp.m2;
            planes[5].normal.y = vp.m7 - vp.m6;
            planes[5].normal.z = vp.m11 - vp.m10;
            planes[5].distance = vp.m15 - vp.m14;

            for (auto &plane : planes)
                plane.Normalize();
        }

        [[nodiscard]] bool IsSphereVisible(const BoundingSphere &sphere) const {
            return std::ranges::all_of(planes, [&](const FrustumPlane &plane) {
                const float dist = Vector3DotProduct(plane.normal, sphere.center) + plane.distance;
                return dist >= -sphere.radius;
            });
        }
    };
}

SceneManager::SceneManager() {
    masterObstacleTransforms.reserve(Config::EngineSettings::MAX_OBSTACLES);
    masterObstacleStyleIds.reserve(Config::EngineSettings::MAX_OBSTACLES);
    masterObstacleSpheres.reserve(Config::EngineSettings::MAX_OBSTACLES);
    masterLightPositions.reserve(Config::EngineSettings::MAX_LIGHTS);
    masterLightProxyTransforms.reserve(Config::EngineSettings::MAX_LIGHTS);

    visibleObstacleTransforms.reserve(Config::EngineSettings::MAX_OBSTACLES);
    visibleObstacleStyleIds.reserve(Config::EngineSettings::MAX_OBSTACLES);
    visibleLightProxyTransforms.reserve(Config::EngineSettings::MAX_LIGHTS);
    visibleLightVolumeTransforms.reserve(Config::EngineSettings::MAX_LIGHTS);
    visibleLightPositions.reserve(Config::EngineSettings::MAX_LIGHTS);

    fwdTransformsBlinn.reserve(Config::EngineSettings::MAX_OBSTACLES);
    fwdTransformsGooch.reserve(Config::EngineSettings::MAX_OBSTACLES);
    fwdTransformsToon.reserve(Config::EngineSettings::MAX_OBSTACLES);
    fwdTransformsOutline.reserve(Config::EngineSettings::MAX_OBSTACLES);
}

void SceneManager::RebuildScene(const EngineState &state) {
    if (state.useNprRoom) {
        GenerateNprRoomScene(state.activeLightCount);
    } else {
        GenerateStandardScene(state.objectSphereRadius, state.useClusteredStyles);
    }
}

void SceneManager::GenerateStandardScene(const float sphereRadius, const bool clustered) {
    SetRandomSeed(1337);

    masterObstacleTransforms.clear();
    masterObstacleStyleIds.clear();
    masterObstacleSpheres.clear();
    masterLightPositions.clear();
    masterLightProxyTransforms.clear();

    for (int i = 0; i < Config::EngineSettings::MAX_OBSTACLES; i++) {
        constexpr float baseRadius = 0.6495f;
        const float scale          = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;

        const float u      = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
        const float radius = sqrtf(u) * sphereRadius;
        const float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;

        const float minHeight = scale;
        const float maxHeight = sphereRadius / 2.0f;
        const float height    = static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                                  static_cast<int>(maxHeight) * 10)) /
                             10.0f;

        const Vector3 pos = {cosf(angle) * radius, height, sinf(angle) * radius};

        float styleId;
        if (clustered) {
            if (pos.x >= 0.0f && pos.z >= 0.0f)
                styleId = 1.0f;
            else if (pos.x < 0.0f && pos.z >= 0.0f)
                styleId = 2.0f;
            else if (pos.x < 0.0f && pos.z < 0.0f)
                styleId = 3.0f;
            else
                styleId = 4.0f;
        } else {
            styleId = static_cast<float>(GetRandomValue(1, 4));
        }

        const Vector3 rotAxis = Vector3Normalize({static_cast<float>(GetRandomValue(0, 100)),
                                                  static_cast<float>(GetRandomValue(0, 100)),
                                                  static_cast<float>(GetRandomValue(0, 100))});
        const auto rotAngle   = static_cast<float>(GetRandomValue(0, 360));

        Matrix transform = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale),
                                                         MatrixRotate(rotAxis, rotAngle * DEG2RAD)),
                                          MatrixTranslate(pos.x, pos.y, pos.z));

        masterObstacleTransforms.push_back(transform);
        masterObstacleStyleIds.push_back(styleId);
        masterObstacleSpheres.push_back({pos, baseRadius * scale});
    }

    int lightsGenerated = 0;
    while (lightsGenerated < Config::EngineSettings::MAX_LIGHTS) {
        const float u             = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
        const float radius        = sqrtf(u) * sphereRadius;
        const float angle         = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
        constexpr float minHeight = 2.0f;
        const float maxHeight     = sphereRadius / 2.0f;
        const float height = static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                               static_cast<int>(maxHeight) * 10)) /
                             10.0f;

        Vector3 validPos = {cosf(angle) * radius, height, sinf(angle) * radius};
        masterLightPositions.push_back(validPos);

        Matrix proxyTransform = MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f),
                                               MatrixTranslate(validPos.x, validPos.y, validPos.z));
        masterLightProxyTransforms.push_back(proxyTransform);

        lightsGenerated++;
    }

    actualGeneratedLights = lightsGenerated;
}

void SceneManager::GenerateNprRoomScene(const int lightCount) {
    masterObstacleTransforms.clear();
    masterObstacleStyleIds.clear();
    masterObstacleSpheres.clear();
    masterLightPositions.clear();
    masterLightProxyTransforms.clear();

    constexpr float baseScale = 1.0f / 0.75f;
    constexpr float vW        = 100.0f;
    constexpr float vH        = 100.0f;
    constexpr float vT        = 1.0f;

    constexpr float sW   = vW * baseScale;
    constexpr float sH   = vH * baseScale;
    constexpr float sT   = vT * baseScale;
    constexpr float padW = (vW + vT * 2.0f) * baseScale;

    masterObstacleTransforms.push_back(
        MatrixMultiply(MatrixScale(padW, sT, padW), MatrixTranslate(0, -vT / 2.0f, 0)));
    masterObstacleStyleIds.push_back(1.0f);
    masterObstacleSpheres.push_back({{0, 0, 0}, vW * 0.75f});

    masterObstacleTransforms.push_back(
        MatrixMultiply(MatrixScale(padW, sT, padW), MatrixTranslate(0, vH + vT / 2.0f, 0)));
    masterObstacleStyleIds.push_back(1.0f);
    masterObstacleSpheres.push_back({{0, vH, 0}, vW * 0.75f});

    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(padW, sH, sT), MatrixTranslate(0, vH / 2.0f, -vW / 2.0f - vT / 2.0f)));
    masterObstacleStyleIds.push_back(2.0f);
    masterObstacleSpheres.push_back({{0, vH / 2.0f, -vW / 2.0f}, vW * 0.75f});

    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(sT, sH, sW), MatrixTranslate(-vW / 2.0f - vT / 2.0f, vH / 2.0f, 0)));
    masterObstacleStyleIds.push_back(3.0f);
    masterObstacleSpheres.push_back({{-vW / 2.0f, vH / 2.0f, 0}, vW * 0.75f});

    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(sT, sH, sW), MatrixTranslate(vW / 2.0f + vT / 2.0f, vH / 2.0f, 0)));
    masterObstacleStyleIds.push_back(4.0f);
    masterObstacleSpheres.push_back({{vW / 2.0f, vH / 2.0f, 0}, vW * 0.75f});

    const float phi = PI * (3.0f - std::sqrt(5.0f));

    for (int i = 0; i < lightCount; ++i) {
        constexpr float usableRadius = 40.0f;
        const float y                = 1.0f - (static_cast<float>(i) /
                                static_cast<float>(lightCount > 1 ? lightCount - 1 : 1)) *
                                   2.0f;
        const float radius = std::sqrt(1.0f - y * y) * usableRadius;
        const float theta  = phi * static_cast<float>(i);

        Vector3 pos = {cosf(theta) * radius, (y * usableRadius) + (vH / 2.0f),
                       sinf(theta) * radius};

        masterLightPositions.push_back(pos);
        masterLightProxyTransforms.push_back(
            MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f), MatrixTranslate(pos.x, pos.y, pos.z)));
    }
    actualGeneratedLights = lightCount;
}

void SceneManager::UpdateVisibility(CameraController &camera, const EngineState &state) {
    double aspect =
        static_cast<double>(state.renderWidth) / static_cast<double>(state.renderHeight);
    Matrix proj     = MatrixPerspective(camera.GetCamera().fovy * DEG2RAD, aspect,
                                        Config::EngineSettings::CameraNearPlane,
                                        Config::EngineSettings::CameraFarPlane);
    Matrix view     = GetCameraMatrix(camera.GetCamera());
    Matrix viewProj = MatrixMultiply(view, proj);

    Frustum cameraFrustum{};
    cameraFrustum.Extract(viewProj);

    // Cull Obstacles
    visibleObstacleTransforms.clear();
    visibleObstacleStyleIds.clear();
    fwdTransformsBlinn.clear();
    fwdTransformsGooch.clear();
    fwdTransformsToon.clear();
    fwdTransformsOutline.clear();

    int currentObstacleCount = GetTotalObstacleCount(state);

    for (int i = 0; i < currentObstacleCount; i++) {
        if (cameraFrustum.IsSphereVisible(masterObstacleSpheres[i])) {
            visibleObstacleTransforms.push_back(masterObstacleTransforms[i]);
            visibleObstacleStyleIds.push_back(masterObstacleStyleIds[i]);

            if (state.activeRenderPath == RenderPath::Forward) {
                if (int currentStyle = static_cast<int>(masterObstacleStyleIds[i]);
                    currentStyle == 2) {
                    if (state.enableGooch)
                        fwdTransformsGooch.push_back(masterObstacleTransforms[i]);
                    else
                        fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);
                } else if (currentStyle == 3) {
                    if (state.enableToon)
                        fwdTransformsToon.push_back(masterObstacleTransforms[i]);
                    else
                        fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);
                    fwdTransformsOutline.push_back(masterObstacleTransforms[i]);
                } else {
                    fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);
                }
            }
        }
    }

    // Cull Lights
    visibleLightProxyTransforms.clear();
    visibleLightVolumeTransforms.clear();
    visibleLightPositions.clear();

    // Use runtime state variable
    float c = Config::EngineSettings::AttenuationConstant -
              (state.lightIntensity / Config::EngineSettings::MinLightThreshold);
    float dynamicMaxRadius = 0.0f;

    if (state.lightIntensity > 0.0f) {
        dynamicMaxRadius = (-Config::EngineSettings::AttenuationLinear +
                            std::sqrt(Config::EngineSettings::AttenuationLinear *
                                          Config::EngineSettings::AttenuationLinear -
                                      (4.0f * Config::EngineSettings::AttenuationQuadratic * c))) /
                           (2.0f * Config::EngineSettings::AttenuationQuadratic);
    }

    if (state.useLightSingularity) {
        Vector3 singularityPos = {0.0f, 10.0f, 0.0f};
        if (BoundingSphere singularitySphere = {singularityPos, dynamicMaxRadius};
            cameraFrustum.IsSphereVisible(singularitySphere)) {
            Matrix volTransform = MatrixMultiply(
                MatrixScale(-dynamicMaxRadius * 2.0f, -dynamicMaxRadius * 2.0f,
                            -dynamicMaxRadius * 2.0f),
                MatrixTranslate(singularityPos.x, singularityPos.y, singularityPos.z));
            Matrix proxyTransform = MatrixMultiply(
                MatrixScale(1.5f, 1.5f, 1.5f),
                MatrixTranslate(singularityPos.x, singularityPos.y, singularityPos.z));

            for (int i = 0; i < state.activeLightCount; i++) {
                visibleLightVolumeTransforms.push_back(volTransform);
                visibleLightProxyTransforms.push_back(proxyTransform);
                visibleLightPositions.push_back(singularityPos);
            }
        }
    } else {
        for (int i = 0; i < state.activeLightCount; i++) {
            if (BoundingSphere volumeSphere = {masterLightPositions[i], dynamicMaxRadius};
                cameraFrustum.IsSphereVisible(volumeSphere)) {
                Matrix volumeTransform = MatrixMultiply(
                    MatrixScale(-dynamicMaxRadius * 2.0f, -dynamicMaxRadius * 2.0f,
                                -dynamicMaxRadius * 2.0f),
                    MatrixTranslate(masterLightPositions[i].x, masterLightPositions[i].y,
                                    masterLightPositions[i].z));
                visibleLightVolumeTransforms.push_back(volumeTransform);
                visibleLightPositions.push_back(masterLightPositions[i]);
            }
            if (BoundingSphere proxySphere = {masterLightPositions[i], 0.75f};
                cameraFrustum.IsSphereVisible(proxySphere)) {
                visibleLightProxyTransforms.push_back(masterLightProxyTransforms[i]);
            }
        }
    }
}

int SceneManager::GetTotalObstacleCount(const EngineState &state) const {
    return state.useNprRoom ? static_cast<int>(masterObstacleTransforms.size())
                            : state.activeObstacleCount;
}

float SceneManager::CalculateTheoreticalOverdraw(const EngineState &state) const {
    const int instanceCount           = GetTotalObstacleCount(state);
    constexpr float avgObstacleRadius = 0.6495f * 2.0f;
    constexpr float singleVolume =
        (4.0f / 3.0f) * PI * (avgObstacleRadius * avgObstacleRadius * avgObstacleRadius);
    const float spawnVolume =
        (2.0f / 3.0f) * PI *
        (state.objectSphereRadius * state.objectSphereRadius * state.objectSphereRadius);

    if (spawnVolume <= 0.0f)
        return 0.0f;
    return (singleVolume * static_cast<float>(instanceCount)) / spawnVolume;
}