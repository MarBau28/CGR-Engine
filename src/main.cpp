#include "../include/Config.h"
#include "../include/InputHandler.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <glad/glad.h>
#include <vector>

// Structs
// -------------------------------------------------------------------------------------------------

// Render Architecture Enum
enum class RenderPath { DeferredUber, DeferredVolume, Forward };

// Camera Architecture Enum
enum class CameraState { Static, Orbital, Free };

struct BoundingSphere {
    Vector3 center;
    float radius;
};

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

    // Extract the 6 planes from the combined View-Projection matrix
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

        for (auto &plane : planes) {
            plane.Normalize();
        }
    }

    // Evaluates if a sphere is completely behind any of the 6 planes
    [[nodiscard]] bool IsSphereVisible(const BoundingSphere &sphere) const {
        return std::ranges::all_of(planes, [&](const FrustumPlane &plane) {
            const float dist = Vector3DotProduct(plane.normal, sphere.center) + plane.distance;
            // If the negative distance exceeds the radius, the sphere is completely outside
            return dist >= -sphere.radius;
        });
    }
};

struct GpuProfiler {
    unsigned int queries[2]{0, 0};
    int queryFrame   = 0;
    double elapsedMs = 0.0;

    void Init() {
        glGenQueries(2, queries);
        // Dummy query to initialize back-buffer and prevent read errors on frame 1
        glBeginQuery(GL_TIME_ELAPSED, queries[0]);
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, queries[1]);
        glEndQuery(GL_TIME_ELAPSED);
    }

    void Begin() const { glBeginQuery(GL_TIME_ELAPSED, queries[queryFrame]); }

    static void End() { glEndQuery(GL_TIME_ELAPSED); }

    void Update() {
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

    void Destroy() const { glDeleteQueries(2, queries); }
};

// Global Constants
// -------------------------------------------------------------------------------------------------

inline constexpr std::string_view obstacleTextureName = "starry-galaxy_tex.png";
inline constexpr std::string_view woodTextureName     = "wood_tex.png";

// Unified Shader Names
inline constexpr std::string_view gBuffeVertexShaderName             = "g-buffer.vert";
inline constexpr std::string_view gBuffeFragmentShaderName           = "g-buffer.frag";
inline constexpr std::string_view deferredUberFragmentShaderName     = "deferred_uber.frag";
inline constexpr std::string_view postFragmentShaderName             = "postprocess.frag";
inline constexpr std::string_view gBufferInstancedVertexShaderName   = "g-buffer_instanced.vert";
inline constexpr std::string_view gBufferInstancedFragmentShaderName = "g-buffer_instanced.frag";
inline constexpr std::string_view deferredVolumeVertexShaderName     = "deferred_volume.vert";
inline constexpr std::string_view deferredVolumeFragmentShaderName   = "deferred_volume.frag";
inline constexpr std::string_view deferredResolveFragmentShaderName  = "deferred_resolve.frag";
inline constexpr std::string_view forwardInstVertexShaderName        = "forward_instanced.vert";
inline constexpr std::string_view forwardBlinnFragmentShaderName     = "forward_blinn.frag";
inline constexpr std::string_view forwardGoochFragmentShaderName     = "forward_gooch.frag";
inline constexpr std::string_view forwardToonFragmentShaderName      = "forward_toon.frag";
inline constexpr std::string_view forwardOutlineVertexShaderName     = "forward_outline.vert";
inline constexpr std::string_view forwardOutlineFragmentShaderName   = "forward_outline.frag";

// General settings
inline constexpr float minLightThreshold    = 0.05f;
inline constexpr float attenuationConstant  = 1.0f;
inline constexpr float attenuationLinear    = 0.09f;
inline constexpr float attenuationQuadratic = 0.032f;
inline constexpr int MAX_OBSTACLES          = 50000;
inline constexpr int MAX_LIGHTS             = 5000;
inline constexpr float UiScale              = 0.8f;

// Global variables
// -------------------------------------------------------------------------------------------------

// Immutable object Data
Vector3 masterLightPositions[MAX_LIGHTS];
std::vector<Vector3> occupiedLightPositions;
std::vector<Matrix> masterObstacleTransforms;
std::vector<float> masterObstacleStyleIds;
std::vector<BoundingSphere> masterObstacleSpheres;
std::vector<Matrix> masterLightProxyTransforms; // Tiny spheres for visuals

// Render object Data (Uploaded/Updated in GPU every frame)
std::vector<Matrix> visibleObstacleTransforms;
std::vector<float> visibleObstacleStyleIds;
std::vector<Matrix> visibleLightProxyTransforms;
std::vector<Matrix> visibleLightVolumeTransforms;
std::vector<Vector3> visibleLightPositions;

// Forward Rendering Specific Bins (for culling)
std::vector<Matrix> fwdTransformsBlinn;
std::vector<Matrix> fwdTransformsGooch;
std::vector<Matrix> fwdTransformsToon;
std::vector<Matrix> fwdTransformsOutline;

// Profiling Data
GpuProfiler geomProfiler;
GpuProfiler lightProfiler;

// Settings
bool use16BitHDR           = true;
int activeObstacleCount    = 5000;
int activeLightCount       = 250;
int actualGeneratedLights  = 0;
float lightIntensity       = Config::EngineSettings::LightIntensity;
float ambientLightStrength = Config::EngineSettings::AmbientLightStrength;
bool requestScreenshot     = false;
auto activeRenderPath      = RenderPath::Forward;
auto currentCameraState    = CameraState::Orbital;
bool enableOutlines        = false;
bool enableKuwahara        = false;
bool enableGooch           = true;
bool enableToon            = true;
int kuwaharaRadius         = 4;
float kuwaharaIntensity    = 4.0f;
float objectSphereRadius   = 200.0f; // obstacle cloud size
bool useClusteredStyles    = false;
bool useLightSingularity   = false;
int currentLodIndex        = 0;
bool useNprRoom            = false;

// SCENE GENERATION HELPERS
// -------------------------------------------------------------------------------------------------

void GenerateScene(const float sphereRadius, const bool clustered) {
    // set random seed to guarantee equal benchmarking across runs
    SetRandomSeed(1337);

    // Zero-allocation clear
    masterObstacleTransforms.clear();
    masterObstacleStyleIds.clear();
    masterObstacleSpheres.clear();
    occupiedLightPositions.clear();
    masterLightProxyTransforms.clear();

    // Obstacle Generation
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        constexpr float baseRadius = 0.6495f;
        const float scale          = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;

        // Uniform Disk Math
        const float u      = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
        const float radius = sqrtf(u) * sphereRadius;
        const float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;

        const float minHeight = scale;
        const float maxHeight = sphereRadius / 2.0f;
        const float height    = static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                                  static_cast<int>(maxHeight) * 10)) /
                             10.0f;

        const Vector3 pos = {cosf(angle) * radius, height, sinf(angle) * radius};

        // Spatial Clustering vs Random for StyleID
        float styleId = 1.0f;
        if (clustered) {
            if (pos.x >= 0.0f && pos.z >= 0.0f)
                styleId = 1.0f; // Quadrant 1: Blinn
            else if (pos.x < 0.0f && pos.z >= 0.0f)
                styleId = 2.0f; // Quadrant 2: Gooch
            else if (pos.x < 0.0f && pos.z < 0.0f)
                styleId = 3.0f; // Quadrant 3: Toon
            else
                styleId = 4.0f; // Quadrant 4: Fallback and Kuwahara
        } else {
            styleId = static_cast<float>(GetRandomValue(1, 4));
        }

        // Calculate Transform Matrix
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

    // Light Generation
    int lightsGenerated = 0;

    // Standard Light Generation
    while (lightsGenerated < MAX_LIGHTS) {
        const float u             = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
        const float radius        = sqrtf(u) * sphereRadius;
        const float angle         = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
        constexpr float minHeight = 2.0f;
        const float maxHeight     = sphereRadius / 2.0f;
        const float height = static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                               static_cast<int>(maxHeight) * 10)) /
                             10.0f;

        Vector3 validPos = {cosf(angle) * radius, height, sinf(angle) * radius};

        masterLightPositions[lightsGenerated] = validPos;
        Matrix proxyTransform                 = MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f),
                                                               MatrixTranslate(validPos.x, validPos.y, validPos.z));

        masterLightProxyTransforms.push_back(proxyTransform);
        occupiedLightPositions.push_back(validPos);
        lightsGenerated++;
    }

    actualGeneratedLights = lightsGenerated;
    activeLightCount      = std::min(activeLightCount, actualGeneratedLights);
}

void GenerateNprRoomScene(const int lightCount) {
    masterObstacleTransforms.clear();
    masterObstacleStyleIds.clear();
    masterObstacleSpheres.clear();
    occupiedLightPositions.clear();
    masterLightProxyTransforms.clear();

    constexpr float baseScale = 1.0f / 0.75f; // Because baseMesh is 0.75
    constexpr float vW        = 100.0f;       // Visual Width/Depth
    constexpr float vH        = 100.0f;       // Visual Height
    constexpr float vT        = 1.0f;         // Visual Thickness

    // Pre-calculate scaled dimensions
    constexpr float sW   = vW * baseScale;
    constexpr float sH   = vH * baseScale;
    constexpr float sT   = vT * baseScale;
    constexpr float padW = (vW + vT * 2.0f) * baseScale;

    // Floor - Style 1 (Blinn)
    masterObstacleTransforms.push_back(
        MatrixMultiply(MatrixScale(padW, sT, padW), MatrixTranslate(0, -vT / 2.0f, 0)));
    masterObstacleStyleIds.push_back(1.0f);
    masterObstacleSpheres.push_back({{0, 0, 0}, vW * 0.75f});

    // Ceiling - Style 1 (Blinn)
    masterObstacleTransforms.push_back(
        MatrixMultiply(MatrixScale(padW, sT, padW), MatrixTranslate(0, vH + vT / 2.0f, 0)));
    masterObstacleStyleIds.push_back(1.0f);
    masterObstacleSpheres.push_back({{0, vH, 0}, vW * 0.75f});

    // Back Wall - Style 2 (Gooch)
    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(padW, sH, sT), MatrixTranslate(0, vH / 2.0f, -vW / 2.0f - vT / 2.0f)));
    masterObstacleStyleIds.push_back(2.0f);
    masterObstacleSpheres.push_back({{0, vH / 2.0f, -vW / 2.0f}, vW * 0.75f});

    // Left Wall - Style 3 (Toon)
    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(sT, sH, sW), MatrixTranslate(-vW / 2.0f - vT / 2.0f, vH / 2.0f, 0)));
    masterObstacleStyleIds.push_back(3.0f);
    masterObstacleSpheres.push_back({{-vW / 2.0f, vH / 2.0f, 0}, vW * 0.75f});

    // Right Wall - Style 4 (Kuwahara / Fallback)
    masterObstacleTransforms.push_back(MatrixMultiply(
        MatrixScale(sT, sH, sW), MatrixTranslate(vW / 2.0f + vT / 2.0f, vH / 2.0f, 0)));
    masterObstacleStyleIds.push_back(4.0f);
    masterObstacleSpheres.push_back({{vW / 2.0f, vH / 2.0f, 0}, vW * 0.75f});

    // 3D Fibonacci Sphere Light Distribution
    const float phi = PI * (3.0f - sqrtf(5.0f)); // Golden angle

    for (int i = 0; i < lightCount; ++i) {
        constexpr float usableRadius = 40.0f;
        const float y                = 1.0f - (static_cast<float>(i) /
                                static_cast<float>(lightCount > 1 ? lightCount - 1 : 1)) *
                                   2.0f;
        const float radius = sqrtf(1.0f - y * y) * usableRadius;
        const float theta  = phi * static_cast<float>(i);

        Vector3 pos = {cosf(theta) * radius,
                       (y * usableRadius) + (vH / 2.0f), // Center vertically at Y=50
                       sinf(theta) * radius};

        masterLightPositions[i] = pos;
        masterLightProxyTransforms.push_back(
            MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f), MatrixTranslate(pos.x, pos.y, pos.z)));
        occupiedLightPositions.push_back(pos);
    }
    actualGeneratedLights = lightCount;
}

int main() {
    // BASIC SETUP
    // ---------------------------------------------------------------------------------------------

    // Context & Window Creation
    constexpr int renderWidth      = Config::EngineSettings::ScreenWidth;
    constexpr int renderHeight     = Config::EngineSettings::ScreenHeight;
    constexpr float internalRes[2] = {static_cast<float>(renderWidth),
                                      static_cast<float>(renderHeight)};
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_MAXIMIZED);
    InitWindow(renderWidth, renderHeight, "HyDra");

    // Initialize Hardware Profilers
    geomProfiler.Init();
    lightProfiler.Init();

    // Camera Setup
    Camera3D camera   = {0};
    camera.position   = Config::EngineSettings::CameraPosition;      // camera spot
    camera.target     = Config::EngineSettings::CameraViewDirection; // view direction
    camera.up         = Config::EngineSettings::CameraOrientation;   // up vector
    camera.fovy       = Config::EngineSettings::CameraFOV;           // Field of View
    camera.projection = CAMERA_PERSPECTIVE;                          // standard 3D-depth projection

    // Load and link Shaders
    const std::string geometryVertPath =
        std::string(Config::Paths::Shaders) + std::string(gBuffeVertexShaderName);
    const std::string geometryFragPath =
        std::string(Config::Paths::Shaders) + std::string(gBuffeFragmentShaderName);
    const std::string lightingFragPath =
        std::string(Config::Paths::Shaders) + std::string(deferredUberFragmentShaderName);
    const std::string postFragPath =
        std::string(Config::Paths::Shaders) + std::string(postFragmentShaderName);
    const std::string instancedVertPath =
        std::string(Config::Paths::Shaders) + std::string(gBufferInstancedVertexShaderName);
    const std::string instancedFragPath =
        std::string(Config::Paths::Shaders) + std::string(gBufferInstancedFragmentShaderName);
    const std::string lightVolumeVertPath =
        std::string(Config::Paths::Shaders) + std::string(deferredVolumeVertexShaderName);
    const std::string lightVolumeFragPath =
        std::string(Config::Paths::Shaders) + std::string(deferredVolumeFragmentShaderName);
    const std::string nprResolveFragPath =
        std::string(Config::Paths::Shaders) + std::string(deferredResolveFragmentShaderName);

    const std::string forwardInstVertPath =
        std::string(Config::Paths::Shaders) + std::string(forwardInstVertexShaderName);
    const std::string forwardBlinnFragPath =
        std::string(Config::Paths::Shaders) + std::string(forwardBlinnFragmentShaderName);
    const std::string forwardGoochFragPath =
        std::string(Config::Paths::Shaders) + std::string(forwardGoochFragmentShaderName);
    const std::string forwardToonFragPath =
        std::string(Config::Paths::Shaders) + std::string(forwardToonFragmentShaderName);
    const std::string forwardOutlineVertPath =
        std::string(Config::Paths::Shaders) + std::string(forwardOutlineVertexShaderName);
    const std::string forwardOutlineFragPath =
        std::string(Config::Paths::Shaders) + std::string(forwardOutlineFragmentShaderName);
    const std::string forwardUnlitFragPath =
        std::string(Config::Paths::Shaders) + "forward_unlit.frag";

    const Shader geometryPassShader =
        LoadShader(geometryVertPath.c_str(), geometryFragPath.c_str());
    const Shader instancedShader = LoadShader(instancedVertPath.c_str(), instancedFragPath.c_str());
    const Shader lightingPassShader = LoadShader(nullptr, lightingFragPath.c_str());
    const Shader postShader         = LoadShader(nullptr, postFragPath.c_str());
    const Shader lightVolumeShader =
        LoadShader(lightVolumeVertPath.c_str(), lightVolumeFragPath.c_str());
    const Shader nprResolveShader = LoadShader(nullptr, nprResolveFragPath.c_str());

    const Shader forwardBlinnShader =
        LoadShader(forwardInstVertPath.c_str(), forwardBlinnFragPath.c_str());
    const Shader forwardGoochShader =
        LoadShader(forwardInstVertPath.c_str(), forwardGoochFragPath.c_str());
    const Shader forwardToonShader =
        LoadShader(forwardInstVertPath.c_str(), forwardToonFragPath.c_str());
    const Shader forwardOutlineShader =
        LoadShader(forwardOutlineVertPath.c_str(), forwardOutlineFragPath.c_str());
    const Shader forwardUnlitShader =
        LoadShader(forwardInstVertPath.c_str(), forwardUnlitFragPath.c_str());

    // Bind instanceTransform attribute for all forward instanced vertex shaders
    forwardBlinnShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardBlinnShader, "instanceTransform");
    forwardGoochShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardGoochShader, "instanceTransform");
    forwardToonShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardToonShader, "instanceTransform");
    forwardOutlineShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardOutlineShader, "instanceTransform");
    forwardUnlitShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(forwardUnlitShader, "instanceTransform");

    // OBJECT GENERATION
    // ---------------------------------------------------------------------------------------------

    // Generate obstacle base Meshes
    std::vector<Mesh> lodMeshes;

    // LOD 0: Primitive Cube (12 Triangles)
    lodMeshes.push_back(GenMeshCube(0.75f, 0.75f, 0.75f));
    // LOD 1: Low-Poly Sphere (128 Triangles)
    lodMeshes.push_back(GenMeshSphere(0.75f, 8, 8));
    // LOD 2: Mid-Poly Sphere (512 Triangles)
    lodMeshes.push_back(GenMeshSphere(0.75f, 16, 16));
    // LOD 3: High-Poly Sphere (2,048 Triangles)
    lodMeshes.push_back(GenMeshSphere(0.75f, 32, 32));
    // LOD 4: Extreme-Poly Sphere (8,192 Triangles)
    lodMeshes.push_back(GenMeshSphere(0.75f, 64, 64));

    // Generate Tangents for all LODs
    for (auto &mesh : lodMeshes) {
        GenMeshTangents(&mesh);
    }

    // set object texture
    const std::string texturePath =
        std::string(Config::Paths::Textures) + std::string(obstacleTextureName);
    const Texture2D obstacleTexture = LoadTexture(texturePath.c_str());

    // floor Mesh Setup with Texture
    Mesh floorMesh   = GenMeshPlane(1000.0f, 1000.0f, 100, 100);
    Model floorModel = LoadModelFromMesh(floorMesh);
    const std::string woodTexPath =
        std::string(Config::Paths::Textures) + std::string(woodTextureName);
    Texture2D floorTexture = LoadTexture(woodTexPath.c_str());
    SetTextureWrap(floorTexture, TEXTURE_WRAP_REPEAT); // Force the texture repeat

    // Scale UV coordinates of the mesh so the texture tiles 100 times
    for (int i = 0; i < floorMesh.vertexCount; i++) {
        floorMesh.texcoords[i * 2] *= 100.0f;     // Scale U
        floorMesh.texcoords[i * 2 + 1] *= 100.0f; // Scale V
    }
    UpdateMeshBuffer(floorMesh, 1, floorMesh.texcoords,
                     floorMesh.vertexCount * static_cast<int>(2 * sizeof(float)), 0);

    // apply texture to floor
    for (int i = 0; i < floorModel.materialCount; i++) {
        floorModel.materials[i].shader                            = geometryPassShader;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    // light mesh
    Mesh lightningSphereMesh                 = GenMeshSphere(0.5f, 16, 16);
    Model lightningSourceModel               = LoadModelFromMesh(lightningSphereMesh);
    lightningSourceModel.materials[0].shader = geometryPassShader;

    // reserve and pre-allocate obstacle and light mesh arrays
    masterObstacleTransforms.reserve(MAX_OBSTACLES);
    masterObstacleStyleIds.reserve(MAX_OBSTACLES);
    masterObstacleSpheres.reserve(MAX_OBSTACLES);
    visibleObstacleTransforms.reserve(MAX_OBSTACLES);
    visibleObstacleStyleIds.reserve(MAX_OBSTACLES);
    occupiedLightPositions.reserve(MAX_LIGHTS);
    masterLightProxyTransforms.reserve(MAX_LIGHTS);
    visibleLightProxyTransforms.reserve(MAX_LIGHTS);
    visibleLightPositions.reserve(MAX_LIGHTS);
    visibleLightVolumeTransforms.reserve(MAX_LIGHTS);

    // Bootstrap initial scene generation
    GenerateScene(objectSphereRadius, useClusteredStyles);

    // set light color
    Vector3 lightColor = {Config::EngineSettings::MainLightColor.x / 255.0f,
                          Config::EngineSettings::MainLightColor.y / 255.0f,
                          Config::EngineSettings::MainLightColor.z / 255.0f};

    // INSTANCING: VAO & VBO SETUP
    // ---------------------------------------------------------------------------------------------

    // Tell Raylib where the instance matrix attribute is located
    instancedShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(instancedShader, "instanceTransform");

    // Upload the Style ID array to the GPU V-RAM as a dynamic VBO
    unsigned int styleIdVboId =
        rlLoadVertexBuffer(masterObstacleStyleIds.data(), MAX_OBSTACLES * sizeof(float), true);

    // Bind VBO to every LOD Mesh's VAO
    for (const auto &mesh : lodMeshes) {
        rlEnableVertexArray(mesh.vaoId);
        {
            rlEnableVertexBuffer(styleIdVboId);
            rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0); // Define data format
            rlEnableVertexAttribute(10);
            rlSetVertexAttributeDivisor(10, 1);
            rlDisableVertexBuffer();
        }
        rlDisableVertexArray();
    }

    // Set up the shared Material for the instanced draw call
    Material instancedMaterial                          = LoadMaterialDefault();
    instancedMaterial.shader                            = instancedShader;
    instancedMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    // Light VBO Setup
    std::vector instancedLightStyleIds(MAX_LIGHTS, 0.0f); // 0.0f = No style
    unsigned int lightStyleIdVboId =
        rlLoadVertexBuffer(instancedLightStyleIds.data(), MAX_LIGHTS * sizeof(float), false);

    rlEnableVertexArray(lightningSphereMesh.vaoId);
    {
        rlEnableVertexBuffer(lightStyleIdVboId);
        rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0); // Define data format
        rlEnableVertexAttribute(10);
        rlSetVertexAttributeDivisor(10, 1);
        rlDisableVertexBuffer();
    }
    rlDisableVertexArray();

    // Set up the shared Material for the instanced draw call
    Material instancedLightMaterial = LoadMaterialDefault();
    instancedLightMaterial.shader   = instancedShader;

    // G-BUFFER INITIALIZATION
    // ---------------------------------------------------------------------------------------------

    unsigned int FboId = rlLoadFramebuffer();
    if (FboId == 0)
        TraceLog(LOG_ERROR, "Failed to create FBO");

    // Target 0: Albedo (8-bit RGBA)
    unsigned int albedoTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    // Target 1: Normal + StyleID (16-bit Float RGBA)
    unsigned int normalTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    // Target 2: Depth
    unsigned int depthTexId = rlLoadTextureDepth(renderWidth, renderHeight, false);

    // Bind FBO and attach all textures
    rlEnableFramebuffer(FboId);
    rlFramebufferAttach(FboId, albedoTexId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, normalTexId, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, depthTexId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    // Verify completeness and unbind
    if (!rlFramebufferComplete(FboId))
        TraceLog(LOG_ERROR, "G-Buffer FBO is incomplete.");
    rlDisableFramebuffer();

    // Wrap texture for use in the lighting pass
    Texture2D gAlbedo = {albedoTexId, renderWidth, renderHeight, 1,
                         PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D gNormal = {normalTexId, renderWidth, renderHeight, 1,
                         PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D gDepth  = {depthTexId, renderWidth, renderHeight, 1,
                         PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};

    SetTextureFilter(gAlbedo, TEXTURE_FILTER_POINT);
    SetTextureFilter(gNormal, TEXTURE_FILTER_POINT);
    SetTextureFilter(gDepth, TEXTURE_FILTER_POINT);

    // Canvas for Pass 2: Light Volume accumulation (16-bit HDR Float)
    RenderTexture2D litSceneTarget = LoadRenderTexture(renderWidth, renderHeight);
    rlUnloadTexture(litSceneTarget.texture.id);
    litSceneTarget.texture.id =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    litSceneTarget.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    rlEnableFramebuffer(litSceneTarget.id);
    rlFramebufferAttach(litSceneTarget.id, litSceneTarget.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();

    // Canvas for Pass 3: NPR Resolve (16-bit HDR Float)
    RenderTexture2D resolveTarget = LoadRenderTexture(renderWidth, renderHeight);
    rlUnloadTexture(resolveTarget.texture.id);
    resolveTarget.texture.id =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    resolveTarget.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;

    rlEnableFramebuffer(resolveTarget.id);
    rlFramebufferAttach(resolveTarget.id, resolveTarget.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlDisableFramebuffer();

    // Material for drawing the instanced light volumes
    Material lightVolumeMaterial = LoadMaterialDefault();
    lightVolumeMaterial.shader   = lightVolumeShader;

    // Bind the G-Buffer textures into the Material slots
    lightVolumeMaterial.maps[MATERIAL_MAP_ALBEDO].texture    = gAlbedo;
    lightVolumeMaterial.maps[MATERIAL_MAP_NORMAL].texture    = gNormal;
    lightVolumeMaterial.maps[MATERIAL_MAP_ROUGHNESS].texture = gDepth;

    // Route the Material slots to shader uniform names
    lightVolumeShader.locs[SHADER_LOC_MAP_ALBEDO] =
        GetShaderLocation(lightVolumeShader, "texture0");
    lightVolumeShader.locs[SHADER_LOC_MAP_NORMAL] =
        GetShaderLocation(lightVolumeShader, "gNormalTex");
    lightVolumeShader.locs[SHADER_LOC_MAP_ROUGHNESS] =
        GetShaderLocation(lightVolumeShader, "gDepthTex");

    // SHADER LOCATION AND UNIFORM SETUP
    // ---------------------------------------------------------------------------------------------

    // shared constants
    constexpr Vector3 BackgroundColor = {Config::EngineSettings::BackgroundColor.x / 255.0f,
                                         Config::EngineSettings::BackgroundColor.y / 255.0f,
                                         Config::EngineSettings::BackgroundColor.z / 255.0f};

    // geometry and instanced passes
    int modelMatLoc         = GetShaderLocation(geometryPassShader, "modelMat");
    int normalMatLoc        = GetShaderLocation(geometryPassShader, "normalMat");
    int styleIdLoc          = GetShaderLocation(geometryPassShader, "styleId");
    int isLightLocInstanced = GetShaderLocation(instancedShader, "isLightSource");

    // uber shader pass
    int lightPosArrayLoc         = GetShaderLocation(lightingPassShader, "lightPositions");
    int lightColorLoc            = GetShaderLocation(lightingPassShader, "lightColor");
    int LightingResolutionLoc    = GetShaderLocation(lightingPassShader, "resolution");
    int normalTexLoc             = GetShaderLocation(lightingPassShader, "gNormalTex");
    int depthTexLoc              = GetShaderLocation(lightingPassShader, "gDepthTex");
    int invViewProjLoc           = GetShaderLocation(lightingPassShader, "invViewProj");
    int postViewPosLoc           = GetShaderLocation(lightingPassShader, "viewPos");
    int postBackgroundColorLoc   = GetShaderLocation(lightingPassShader, "backgroundColor");
    int intensityLoc             = GetShaderLocation(lightingPassShader, "lightIntensity");
    int ambientLoc               = GetShaderLocation(lightingPassShader, "ambientLightStrength");
    int activeLightsLoc          = GetShaderLocation(lightingPassShader, "activeLights");
    int maxLightRadiusLoc        = GetShaderLocation(lightingPassShader, "maxLightRadius");
    int attenuationConstantLoc   = GetShaderLocation(lightingPassShader, "attenuationConstant");
    int attenuationLinearLoc     = GetShaderLocation(lightingPassShader, "attenuationLinear");
    int attenuationQuadraticLoc  = GetShaderLocation(lightingPassShader, "attenuationQuadratic");
    int uberEnableOutlinesLoc    = GetShaderLocation(lightingPassShader, "enableOutlines");
    int uberEnableKuwaharaLoc    = GetShaderLocation(lightingPassShader, "enableKuwahara");
    int uberEnableGoochLoc       = GetShaderLocation(lightingPassShader, "enableGooch");
    int uberEnableToonLoc        = GetShaderLocation(lightingPassShader, "enableToon");
    int uberKuwaharaRadiusLoc    = GetShaderLocation(lightingPassShader, "kuwaharaRadius");
    int uberKuwaharaIntensityLoc = GetShaderLocation(lightingPassShader, "kuwaharaIntensity");

    SetShaderValue(lightingPassShader, postBackgroundColorLoc, &BackgroundColor,
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(lightingPassShader, attenuationConstantLoc, &attenuationConstant,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, attenuationLinearLoc, &attenuationLinear,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, attenuationQuadraticLoc, &attenuationQuadratic,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, intensityLoc, &Config::EngineSettings::LightIntensity,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, ambientLoc, &Config::EngineSettings::AmbientLightStrength,
                   SHADER_UNIFORM_FLOAT);

    // light volume pass
    int lvResolutionLoc        = GetShaderLocation(lightVolumeShader, "resolution");
    int lvViewPosLoc           = GetShaderLocation(lightVolumeShader, "viewPos");
    int lvLightColorLoc        = GetShaderLocation(lightVolumeShader, "lightColor");
    int lvIntensityLoc         = GetShaderLocation(lightVolumeShader, "lightIntensity");
    int lvMaxRadiusLoc         = GetShaderLocation(lightVolumeShader, "maxLightRadius");
    int lvAttenuationConstLoc  = GetShaderLocation(lightVolumeShader, "attenuationConstant");
    int lvAttenuationLinearLoc = GetShaderLocation(lightVolumeShader, "attenuationLinear");
    int lvAttenuationQuadLoc   = GetShaderLocation(lightVolumeShader, "attenuationQuadratic");
    int lvInvViewProjLoc       = GetShaderLocation(lightVolumeShader, "invViewProj");
    int lvAlbedoTexLoc         = GetShaderLocation(lightVolumeShader, "texture0");
    int lvNormalTexLoc         = GetShaderLocation(lightVolumeShader, "gNormalTex");
    int lvDepthTexLoc          = GetShaderLocation(lightVolumeShader, "gDepthTex");
    int lvEnableGoochLoc       = GetShaderLocation(lightVolumeShader, "enableGooch");
    int lvEnableToonLoc        = GetShaderLocation(lightVolumeShader, "enableToon");

    SetShaderValue(lightVolumeShader, lvAttenuationConstLoc, &attenuationConstant,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvAttenuationLinearLoc, &attenuationLinear,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvAttenuationQuadLoc, &attenuationQuadratic,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightVolumeShader, lvIntensityLoc, &Config::EngineSettings::LightIntensity,
                   SHADER_UNIFORM_FLOAT);

    // Tell the shader to locate the instance matrix attribute for the light volumes
    lightVolumeShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(lightVolumeShader, "instanceTransform");

    // deferred volume resolve pass
    int nprResolutionLoc        = GetShaderLocation(nprResolveShader, "resolution");
    int nprViewPosLoc           = GetShaderLocation(nprResolveShader, "viewPos");
    int nprBgColorLoc           = GetShaderLocation(nprResolveShader, "backgroundColor");
    int nprAmbientLoc           = GetShaderLocation(nprResolveShader, "ambientLightStrength");
    int nprInvViewProjLoc       = GetShaderLocation(nprResolveShader, "invViewProj");
    int nprLitSceneTexLoc       = GetShaderLocation(nprResolveShader, "litSceneTex");
    int nprAlbedoTexLoc         = GetShaderLocation(nprResolveShader, "texture0");
    int nprNormalTexLoc         = GetShaderLocation(nprResolveShader, "gNormalTex");
    int nprDepthTexLoc          = GetShaderLocation(nprResolveShader, "gDepthTex");
    int resEnableOutlinesLoc    = GetShaderLocation(nprResolveShader, "enableOutlines");
    int resEnableKuwaharaLoc    = GetShaderLocation(nprResolveShader, "enableKuwahara");
    int resEnableGoochLoc       = GetShaderLocation(nprResolveShader, "enableGooch");
    int resEnableToonLoc        = GetShaderLocation(nprResolveShader, "enableToon");
    int resKuwaharaRadiusLoc    = GetShaderLocation(nprResolveShader, "kuwaharaRadius");
    int resKuwaharaIntensityLoc = GetShaderLocation(nprResolveShader, "kuwaharaIntensity");

    SetShaderValue(nprResolveShader, nprBgColorLoc, &BackgroundColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(nprResolveShader, nprAmbientLoc, &Config::EngineSettings::AmbientLightStrength,
                   SHADER_UNIFORM_FLOAT);

    // post-process pass
    int postResolutionLoc = GetShaderLocation(postShader, "resolution");

    // Blinn Shader Locations
    int fwdBlinnViewPosLoc     = GetShaderLocation(forwardBlinnShader, "viewPos");
    int fwdBlinnIntensityLoc   = GetShaderLocation(forwardBlinnShader, "lightIntensity");
    int fwdBlinnAmbientLoc     = GetShaderLocation(forwardBlinnShader, "ambientLightStrength");
    int fwdBlinnActiveLightLoc = GetShaderLocation(forwardBlinnShader, "activeLights");
    int fwdBlinnMaxRadiusLoc   = GetShaderLocation(forwardBlinnShader, "maxLightRadius");
    int fwdBlinnAttConstLoc    = GetShaderLocation(forwardBlinnShader, "attenuationConstant");
    int fwdBlinnAttLinLoc      = GetShaderLocation(forwardBlinnShader, "attenuationLinear");
    int fwdBlinnAttQuadLoc     = GetShaderLocation(forwardBlinnShader, "attenuationQuadratic");
    int fwdBlinnLightPosLoc    = GetShaderLocation(forwardBlinnShader, "lightPositions");
    int fwdBlinnLightColorLoc  = GetShaderLocation(forwardBlinnShader, "lightColor");

    SetShaderValue(forwardBlinnShader, fwdBlinnAttConstLoc, &attenuationConstant,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardBlinnShader, fwdBlinnAttLinLoc, &attenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardBlinnShader, fwdBlinnAttQuadLoc, &attenuationQuadratic,
                   SHADER_UNIFORM_FLOAT);

    // Gooch Shader Locations (Note: Gooch lacks ambient and base lightColor per your shader)
    int fwdGoochViewPosLoc     = GetShaderLocation(forwardGoochShader, "viewPos");
    int fwdGoochIntensityLoc   = GetShaderLocation(forwardGoochShader, "lightIntensity");
    int fwdGoochAmbientLoc     = GetShaderLocation(forwardGoochShader, "ambientLightStrength");
    int fwdGoochActiveLightLoc = GetShaderLocation(forwardGoochShader, "activeLights");
    int fwdGoochMaxRadiusLoc   = GetShaderLocation(forwardGoochShader, "maxLightRadius");
    int fwdGoochAttConstLoc    = GetShaderLocation(forwardGoochShader, "attenuationConstant");
    int fwdGoochAttLinLoc      = GetShaderLocation(forwardGoochShader, "attenuationLinear");
    int fwdGoochAttQuadLoc     = GetShaderLocation(forwardGoochShader, "attenuationQuadratic");
    int fwdGoochLightPosLoc    = GetShaderLocation(forwardGoochShader, "lightPositions");

    SetShaderValue(forwardGoochShader, fwdGoochAttConstLoc, &attenuationConstant,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardGoochShader, fwdGoochAttLinLoc, &attenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardGoochShader, fwdGoochAttQuadLoc, &attenuationQuadratic,
                   SHADER_UNIFORM_FLOAT);

    // Toon Shader Locations
    int fwdToonViewPosLoc     = GetShaderLocation(forwardToonShader, "viewPos");
    int fwdToonIntensityLoc   = GetShaderLocation(forwardToonShader, "lightIntensity");
    int fwdToonAmbientLoc     = GetShaderLocation(forwardToonShader, "ambientLightStrength");
    int fwdToonActiveLightLoc = GetShaderLocation(forwardToonShader, "activeLights");
    int fwdToonMaxRadiusLoc   = GetShaderLocation(forwardToonShader, "maxLightRadius");
    int fwdToonAttConstLoc    = GetShaderLocation(forwardToonShader, "attenuationConstant");
    int fwdToonAttLinLoc      = GetShaderLocation(forwardToonShader, "attenuationLinear");
    int fwdToonAttQuadLoc     = GetShaderLocation(forwardToonShader, "attenuationQuadratic");
    int fwdToonLightPosLoc    = GetShaderLocation(forwardToonShader, "lightPositions");
    int fwdToonLightColorLoc  = GetShaderLocation(forwardToonShader, "lightColor");

    SetShaderValue(forwardToonShader, fwdToonAttConstLoc, &attenuationConstant,
                   SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardToonShader, fwdToonAttLinLoc, &attenuationLinear, SHADER_UNIFORM_FLOAT);
    SetShaderValue(forwardToonShader, fwdToonAttQuadLoc, &attenuationQuadratic,
                   SHADER_UNIFORM_FLOAT);

    // Outline Shader Locations
    int fwdOutlineViewPosLoc = GetShaderLocation(forwardOutlineShader, "viewPos");

    // FORWARD RENDERING MATERIAL SETUP
    // ---------------------------------------------------------------------------------------------
    Material fwdBlinnMaterial                          = LoadMaterialDefault();
    fwdBlinnMaterial.shader                            = forwardBlinnShader;
    fwdBlinnMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    Material fwdGoochMaterial                          = LoadMaterialDefault();
    fwdGoochMaterial.shader                            = forwardGoochShader;
    fwdGoochMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    Material fwdToonMaterial                          = LoadMaterialDefault();
    fwdToonMaterial.shader                            = forwardToonShader;
    fwdToonMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    Material fwdOutlineMaterial = LoadMaterialDefault();
    fwdOutlineMaterial.shader   = forwardOutlineShader;

    // Floor & Light Proxy Materials for Forward
    Material fwdFloorMaterial                          = LoadMaterialDefault();
    fwdFloorMaterial.shader                            = forwardBlinnShader;
    fwdFloorMaterial.maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;

    Material fwdLightProxyMaterial = LoadMaterialDefault();
    fwdLightProxyMaterial.shader   = forwardUnlitShader; // Unlit

    // MAIN GAME LOOP
    // ---------------------------------------------------------------------------------------------

    // Define source and destination rectangles for FBO
    constexpr Rectangle sourceRec = {0.0f, 0.0f, static_cast<float>(renderWidth),
                                     -static_cast<float>(renderHeight)};

    // FBO Target. Exactly defined res so the Lighting pass matches G-Buffer
    constexpr Rectangle fboDestRec = {0.0f, 0.0f, static_cast<float>(renderWidth),
                                      static_cast<float>(renderHeight)};

    // initialize the key binding controllers for continuous inputs
    ContinuousInput<int> obsInput;
    ContinuousInput<int> lightInput;
    ContinuousInput<float> intensityInput;
    ContinuousInput<float> ambientInput;
    ContinuousInput<float> radiusInput;
    ContinuousInput<float> kuwIntInput;

    // State Tracking
    CameraState previousCameraState = currentCameraState;
    Vector3 previousCameraPos       = camera.position;
    Vector3 previousCameraTarget    = camera.target;

    while (!WindowShouldClose()) {
        // CAMERA
        // -----------------------------------------------------------------------------------------

        if (IsKeyPressed(KEY_C)) {
            int nextCam        = (static_cast<int>(currentCameraState) + 1) % 3;
            currentCameraState = static_cast<CameraState>(nextCam);

            if (currentCameraState == CameraState::Free) {
                DisableCursor(); // Locks to window center and hides cursor
            } else {
                EnableCursor();
            }
        }

        if (currentCameraState == CameraState::Orbital) {
            UpdateCamera(&camera, CAMERA_ORBITAL);
        } else if (currentCameraState == CameraState::Free) {
            // Decoupled Camera Controller for Custom Speed
            float baseSpeed    = 50.0f; // movement speed
            float turnSpeed    = 0.05f; // Mouse sensitivity
            float zoomSpeed    = 5.0f;  // Scroll speed
            float sprintMult   = IsKeyDown(KEY_LEFT_SHIFT) ? 3.0f : 1.0f;
            float currentSpeed = baseSpeed * sprintMult * GetFrameTime();

            // Calculate Local Vectors for True 3D Flight
            Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Vector3 right   = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
            Vector3 up      = Vector3Normalize(Vector3CrossProduct(right, forward));

            // Accumulate 3D Velocity
            Vector3 velocity = {0.0f, 0.0f, 0.0f};

            if (IsKeyDown(KEY_W))
                velocity = Vector3Add(velocity, Vector3Scale(forward, currentSpeed));
            if (IsKeyDown(KEY_S))
                velocity = Vector3Subtract(velocity, Vector3Scale(forward, currentSpeed));
            if (IsKeyDown(KEY_D))
                velocity = Vector3Add(velocity, Vector3Scale(right, currentSpeed));
            if (IsKeyDown(KEY_A))
                velocity = Vector3Subtract(velocity, Vector3Scale(right, currentSpeed));
            if (IsKeyDown(KEY_E))
                velocity = Vector3Add(velocity, Vector3Scale(up, currentSpeed));
            if (IsKeyDown(KEY_Q))
                velocity = Vector3Subtract(velocity, Vector3Scale(up, currentSpeed));

            // Apply Translation Manually
            camera.position = Vector3Add(camera.position, velocity);
            camera.target   = Vector3Add(camera.target, velocity);

            // Delegate Rotation and Zoom
            Vector3 rotation = {0.0f, 0.0f, 0.0f};
            auto [x, y]      = GetMouseDelta();
            rotation.x       = x * turnSpeed;
            rotation.y       = y * turnSpeed;
            float zoom       = GetMouseWheelMove() * zoomSpeed * -1;

            UpdateCameraPro(&camera, {0.0f, 0.0f, 0.0f}, rotation, zoom);
        }

        auto currentWidth             = static_cast<float>(GetScreenWidth());
        auto currentHeight            = static_cast<float>(GetScreenHeight());
        float dynamicRes[2]           = {currentWidth, currentHeight};
        const Rectangle screenDestRec = {0.0f, 0.0f, currentWidth, currentHeight};

        // KEY BINDINGS
        // -----------------------------------------------------------------------------------------

        // NPR Room Toggle
        if (IsKeyPressed(KEY_R)) {
            useNprRoom = !useNprRoom;

            if (useNprRoom) {
                // Save Scene State
                previousCameraState  = currentCameraState;
                previousCameraPos    = camera.position;
                previousCameraTarget = camera.target;

                // Lock Controls & Variables
                currentCameraState = CameraState::Static;
                camera.position    = {0.0f, 50.0f, 150.0f};
                camera.target      = {0.0f, 50.0f, 0.0f};
                currentLodIndex    = 0;
                enableOutlines     = false;

                GenerateNprRoomScene(activeLightCount);
            } else {
                // Restore Scene State
                currentCameraState = previousCameraState;
                camera.position    = previousCameraPos;
                camera.target      = previousCameraTarget;

                GenerateScene(objectSphereRadius, useClusteredStyles);
            }

            // Sync GPU Buffers
            int activeLimit = static_cast<int>(masterObstacleStyleIds.size());
            rlUpdateVertexBuffer(styleIdVboId, masterObstacleStyleIds.data(),
                                 activeLimit * static_cast<int>(sizeof(float)), 0);
        }

        // NPR and Profiling Toggles
        if (IsKeyPressed(KEY_O) && !useNprRoom)
            enableOutlines = !enableOutlines;
        if (IsKeyPressed(KEY_K))
            enableKuwahara = !enableKuwahara;
        if (IsKeyPressed(KEY_G))
            enableGooch = !enableGooch;
        if (IsKeyPressed(KEY_T))
            enableToon = !enableToon;
        if (IsKeyPressed(KEY_RIGHT) && kuwaharaRadius < 8)
            kuwaharaRadius++;
        if (IsKeyPressed(KEY_LEFT) && kuwaharaRadius > 1)
            kuwaharaRadius--;

        // Adjust Kuwahara Intensity: Tap: +/- 0.5 | Hold: +/- 5.0 per sec
        kuwIntInput.Update(KEY_UP, KEY_DOWN, kuwaharaIntensity, 0.5f, 5.0f, 1.0f, 20.0f);

        if (IsKeyPressed(KEY_TAB)) {
            // Cycle through the 3 rendering architectures
            int nextPath     = (static_cast<int>(activeRenderPath) + 1) % 3;
            activeRenderPath = static_cast<RenderPath>(nextPath);

            const char *pathNames[] = {"Deferred (Uber-Shader)", "Deferred (Light Volumes)",
                                       "Forward (Baseline)"};
            TraceLog(LOG_INFO, "PIPELINE SWAPPED: %s", pathNames[nextPath]);
        }

        // HDR to LDR Pipeline Toggle
        if (IsKeyPressed(KEY_H)) {
            use16BitHDR      = !use16BitHDR;
            int targetFormat = use16BitHDR ? PIXELFORMAT_UNCOMPRESSED_R16G16B16A16
                                           : PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

            // Rebuild the Accumulation Target
            rlUnloadTexture(litSceneTarget.texture.id);
            litSceneTarget.texture.id =
                rlLoadTexture(nullptr, renderWidth, renderHeight, targetFormat, 1);
            litSceneTarget.texture.format = targetFormat;
            rlEnableFramebuffer(litSceneTarget.id);
            rlFramebufferAttach(litSceneTarget.id, litSceneTarget.texture.id,
                                RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
            rlDisableFramebuffer();

            // Rebuild the Resolve Target
            rlUnloadTexture(resolveTarget.texture.id);
            resolveTarget.texture.id =
                rlLoadTexture(nullptr, renderWidth, renderHeight, targetFormat, 1);
            resolveTarget.texture.format = targetFormat;

            rlEnableFramebuffer(resolveTarget.id);
            rlFramebufferAttach(resolveTarget.id, resolveTarget.texture.id,
                                RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
            rlDisableFramebuffer();

            TraceLog(LOG_INFO, "PIPELINE BIT-DEPTH SWAPPED: %s",
                     use16BitHDR ? "16-Bit HDR" : "8-Bit LDR");
        }

        // Screenshot Hook
        if (IsKeyPressed(KEY_P)) {
            requestScreenshot = true;
        }

        // Clustering Hook
        if (IsKeyPressed(KEY_M) && !useNprRoom) {
            useClusteredStyles = !useClusteredStyles;
            GenerateScene(objectSphereRadius, useClusteredStyles);

            int activeLimit = static_cast<int>(masterObstacleStyleIds.size());
            rlUpdateVertexBuffer(styleIdVboId, masterObstacleStyleIds.data(),
                                 activeLimit * static_cast<int>(sizeof(float)), 0);
        }

        // Light singularity Toggle
        if (IsKeyPressed(KEY_L)) {
            useLightSingularity = !useLightSingularity;
        }

        // High poly Mesh Toggle +
        if (IsKeyPressed(KEY_X) && !useNprRoom) {
            currentLodIndex = (currentLodIndex + 1) % static_cast<int>(lodMeshes.size());
        }
        // High poly Mesh Toggle -
        if (IsKeyPressed(KEY_Z) && !useNprRoom) {
            currentLodIndex = std::max(0, (currentLodIndex - 1));
        }

        // Adjust Sphere Radius: Tap: +/- 5.0f | Hold: +/- 50.0f per second
        float previousRadius = objectSphereRadius;
        radiusInput.Update(KEY_NINE, KEY_KP_9, KEY_SEVEN, KEY_KP_7, objectSphereRadius, 5.0f, 50.0f,
                           1.0f, 1000.0f);

        // Synchronous Regeneration Hook
        if (objectSphereRadius != previousRadius && !useNprRoom) {
            GenerateScene(objectSphereRadius, useClusteredStyles);

            // Re-stream static VBO limits
            int activeLimit = static_cast<int>(masterObstacleStyleIds.size());
            rlUpdateVertexBuffer(styleIdVboId, masterObstacleStyleIds.data(),
                                 activeLimit * static_cast<int>(sizeof(float)), 0);
        }

        // Adjust Obstacles: Tap: +/- 100 | Hold: +/- 5000 per second
        obsInput.Update(KEY_SIX, KEY_KP_6, KEY_FOUR, KEY_KP_4, activeObstacleCount, 100, 5000.0f, 1,
                        MAX_OBSTACLES);

        // Adjust Lights: Tap: +/- 10  | Hold: +/- 200 per second
        int previousLightCount = activeLightCount;
        int maxAllowedLights   = useNprRoom ? 500 : actualGeneratedLights;
        lightInput.Update(KEY_EIGHT, KEY_KP_8, KEY_FIVE, KEY_KP_5, activeLightCount, 10, 200.0f, 1,
                          maxAllowedLights);

        // Generate new NPR-Room with updated light count
        if (useNprRoom && activeLightCount != previousLightCount) {
            GenerateNprRoomScene(activeLightCount); // Re-calculate 3D grid layout instantly
        }

        // Adjust Light Intensity: Tap: +/- 0.1 | Hold: +/- 2.0 per second
        intensityInput.Update(KEY_TWO, KEY_KP_2, KEY_ZERO, KEY_KP_0, lightIntensity, 0.1f, 2.0f,
                              0.0f, std::numeric_limits<float>::max());

        // Adjust Ambient Strength: Tap: +/- 0.05 | Hold: +/- 1.0 per second
        ambientInput.Update(KEY_THREE, KEY_KP_3, KEY_ONE, KEY_KP_1, ambientLightStrength, 0.05f,
                            1.0f, 0.0f, std::numeric_limits<float>::max());

        // CULLING MATH & MATRIX SETUP
        // -----------------------------------------------------------------------------------------

        // Calculate the Inverse View-Projection Matrix
        double aspect      = static_cast<double>(currentWidth) / static_cast<double>(currentHeight);
        Matrix proj        = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01, 1000.0);
        Matrix view        = GetCameraMatrix(camera);
        Matrix viewProj    = MatrixMultiply(view, proj);
        Matrix invViewProj = MatrixInvert(viewProj); // Calculated now, used in lighting pass

        // Extract Frustum Planes & forward binning
        Frustum cameraFrustum{};
        cameraFrustum.Extract(viewProj);

        // Execute CPU Culling for obstacles
        visibleObstacleTransforms.clear();
        visibleObstacleStyleIds.clear();

        // Clear Forward Bins
        fwdTransformsBlinn.clear();
        fwdTransformsGooch.clear();
        fwdTransformsToon.clear();
        fwdTransformsOutline.clear();

        // Prevent indexing stale memory
        int currentObstacleCount =
            useNprRoom ? static_cast<int>(masterObstacleTransforms.size()) : activeObstacleCount;

        for (int i = 0; i < currentObstacleCount; i++) {
            if (cameraFrustum.IsSphereVisible(masterObstacleSpheres[i])) {
                // Populate base deferred arrays
                visibleObstacleTransforms.push_back(masterObstacleTransforms[i]);
                visibleObstacleStyleIds.push_back(masterObstacleStyleIds[i]);

                // Forward Rendering: CPU Dynamic Routing
                // Route Gooch vs Fallback
                if (int currentStyle = static_cast<int>(masterObstacleStyleIds[i]);
                    currentStyle == 2) {
                    if (enableGooch)
                        fwdTransformsGooch.push_back(masterObstacleTransforms[i]);
                    else
                        fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);
                } // Route Toon vs Fallback
                else if (currentStyle == 3) {
                    if (enableToon)
                        fwdTransformsToon.push_back(masterObstacleTransforms[i]);
                    else
                        fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);

                    // Outlines are architecturally isolated from the surface shader
                    fwdTransformsOutline.push_back(masterObstacleTransforms[i]);
                }
                // Baseline Blinn (Style 1) and Kuwahara (Style 4, unsupported in Forward)
                else {
                    fwdTransformsBlinn.push_back(masterObstacleTransforms[i]);
                }
            }
        }

        int actualVisibleCount = static_cast<int>(std::ssize(visibleObstacleTransforms));

        // Stream Custom Attributes to GPU
        if (actualVisibleCount > 0) {
            rlUpdateVertexBuffer(styleIdVboId, visibleObstacleStyleIds.data(),
                                 actualVisibleCount * static_cast<int>(sizeof(float)), 0);
        }

        // calculate maximum lightRadius based on light intensity to safe shader calcs
        float c                = attenuationConstant - (lightIntensity / minLightThreshold);
        float dynamicMaxRadius = 0.0f;
        if (lightIntensity > 0.0f) {
            dynamicMaxRadius =
                (-attenuationLinear + std::sqrt(attenuationLinear * attenuationLinear -
                                                (4.0f * attenuationQuadratic * c))) /
                (2.0f * attenuationQuadratic);
        }

        // Execute CPU Culling for Lights
        visibleLightProxyTransforms.clear();
        visibleLightVolumeTransforms.clear();
        visibleLightPositions.clear();

        if (useLightSingularity) {
            // Singularity light Distribution
            Vector3 singularityPos = {0.0f, 10.0f, 0.0f}; // Hovering above center

            if (BoundingSphere singularitySphere = {singularityPos, dynamicMaxRadius};
                cameraFrustum.IsSphereVisible(singularitySphere)) {
                // Pre-calculate the identical matrices once
                Matrix volTransform = MatrixMultiply(
                    MatrixScale(-dynamicMaxRadius * 2.0f, -dynamicMaxRadius * 2.0f,
                                -dynamicMaxRadius * 2.0f),
                    MatrixTranslate(singularityPos.x, singularityPos.y, singularityPos.z));
                Matrix proxyTransform = MatrixMultiply(
                    MatrixScale(1.5f, 1.5f, 1.5f),
                    MatrixTranslate(singularityPos.x, singularityPos.y, singularityPos.z));

                // Flood arrays with overlapping geometry
                for (int i = 0; i < activeLightCount; i++) {
                    visibleLightVolumeTransforms.push_back(volTransform);
                    visibleLightProxyTransforms.push_back(proxyTransform);
                    visibleLightPositions.push_back(singularityPos);
                }
            }
        } else {
            // standard light distribution
            for (int i = 0; i < activeLightCount; i++) {
                // Cull the light Volumes against Frustum using dynamic radius
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

                // Cull the light proxy bulbs
                if (BoundingSphere proxySphere = {masterLightPositions[i], 0.75f};
                    cameraFrustum.IsSphereVisible(proxySphere)) {
                    visibleLightProxyTransforms.push_back(masterLightProxyTransforms[i]);
                }
            }
        }

        int actualVisibleVolumeCount = static_cast<int>(visibleLightVolumeTransforms.size());
        int actualVisibleProxyCount  = static_cast<int>(visibleLightProxyTransforms.size());

        // Determine active mesh
        Mesh &currentMesh = lodMeshes[currentLodIndex];

        // Draw
        BeginDrawing();
        {
            // GEOMETRY PASS
            // -------------------------------------------------------------------------------------
            if (activeRenderPath == RenderPath::DeferredUber ||
                activeRenderPath == RenderPath::DeferredVolume) {
                // FBO Off-Screen rendering
                rlEnableFramebuffer(FboId); // Redirect GPU output to custom FBO
                rlViewport(0, 0, renderWidth,
                           renderHeight); // Force camera to render at set resolution
                rlActiveDrawBuffers(2);   // Activate all G-Buffer color attachments simultaneously
                ClearBackground(BLANK);
                rlDisableColorBlend(); // prevent auto blending

                // Hardware profiling
                rlDrawRenderBatchActive(); // flush state changes
                geomProfiler.Begin();      // Start Geometry timer

                // Draw models
                BeginMode3D(camera);
                {
                    // force back viewport to internal FBO resolution
                    rlViewport(0, 0, renderWidth, renderHeight);

                    // Draw Static Models (Floor) using standard shader
                    if (!useNprRoom) {
                        SetShaderValueMatrix(geometryPassShader, modelMatLoc, MatrixIdentity());
                        SetShaderValueMatrix(geometryPassShader, normalMatLoc, MatrixIdentity());
                        float generalStyleId = 1;
                        SetShaderValue(geometryPassShader, styleIdLoc, &generalStyleId,
                                       SHADER_UNIFORM_FLOAT);
                        DrawModel(floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
                    }

                    // Draw Lights proxies using instanced shader (+ culled)
                    int isLight = 1;
                    SetShaderValue(instancedShader, isLightLocInstanced, &isLight,
                                   SHADER_UNIFORM_INT);
                    if (actualVisibleProxyCount > 0) {
                        DrawMeshInstanced(lightningSphereMesh, instancedLightMaterial,
                                          visibleLightProxyTransforms.data(),
                                          std::min(actualVisibleProxyCount, 500));
                    }

                    // draw obstacles using instanced shader
                    isLight = 0;
                    SetShaderValue(instancedShader, isLightLocInstanced, &isLight,
                                   SHADER_UNIFORM_INT);
                    if (actualVisibleCount > 0) {
                        DrawMeshInstanced(currentMesh, instancedMaterial,
                                          visibleObstacleTransforms.data(), actualVisibleCount);
                    }
                }
                EndMode3D(); // stop applying 3D math

                // Hardware profiling
                rlDrawRenderBatchActive(); // Force push all deferred draws to GPU
                GpuProfiler::End();        // stop hardware timer

                // Restore GPU output back to the default screen buffer
                rlEnableColorBlend();
                rlDisableFramebuffer();

                // Restore the viewport to actual window size
                rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
            } else {
                // Reset timer display for Forward rendering
                geomProfiler.elapsedMs = 0.0;
            }

            // LIGHTING PASS & RESOLVE
            // -------------------------------------------------------------------------------------

            int outInt   = enableOutlines ? 1 : 0;
            int kuwInt   = enableKuwahara ? 1 : 0;
            int goochInt = enableGooch ? 1 : 0;
            int toonInt  = enableToon ? 1 : 0;

            if (activeRenderPath == RenderPath::DeferredUber) {
                // ARCHITECTURE A: UBER-SHADER

                BeginTextureMode(litSceneTarget);
                ClearBackground(BLANK);

                // Hardware profiling
                rlDrawRenderBatchActive(); // flush
                lightProfiler.Begin();     // start lighting timer

                BeginShaderMode(lightingPassShader);
                {
                    // set dynamic light data via uniforms
                    SetShaderValue(lightingPassShader, intensityLoc, &lightIntensity,
                                   SHADER_UNIFORM_FLOAT);
                    SetShaderValue(lightingPassShader, ambientLoc, &ambientLightStrength,
                                   SHADER_UNIFORM_FLOAT);
                    SetShaderValue(lightingPassShader, maxLightRadiusLoc, &dynamicMaxRadius,
                                   SHADER_UNIFORM_FLOAT);

                    // Pass lighting arrays, camera and screen position
                    int safeVisibleLightCount = std::min(actualVisibleVolumeCount, 500);
                    SetShaderValue(lightingPassShader, LightingResolutionLoc, internalRes,
                                   SHADER_UNIFORM_VEC2);
                    SetShaderValue(lightingPassShader, activeLightsLoc, &safeVisibleLightCount,
                                   SHADER_UNIFORM_INT);
                    SetShaderValueV(lightingPassShader, lightPosArrayLoc,
                                    visibleLightPositions.data(), SHADER_UNIFORM_VEC3,
                                    safeVisibleLightCount);
                    SetShaderValue(lightingPassShader, lightColorLoc, &lightColor,
                                   SHADER_UNIFORM_VEC3);
                    SetShaderValue(lightingPassShader, postViewPosLoc, &camera.position,
                                   SHADER_UNIFORM_VEC3);
                    SetShaderValueMatrix(lightingPassShader, invViewProjLoc, invViewProj);

                    // Bind G-Buffer textures
                    SetShaderValueTexture(lightingPassShader, normalTexLoc, gNormal);
                    SetShaderValueTexture(lightingPassShader, depthTexLoc, gDepth);

                    // set dynamic switches for NPR Effects
                    SetShaderValue(lightingPassShader, uberEnableOutlinesLoc, &outInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(lightingPassShader, uberEnableKuwaharaLoc, &kuwInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(lightingPassShader, uberEnableGoochLoc, &goochInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(lightingPassShader, uberEnableToonLoc, &toonInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(lightingPassShader, uberKuwaharaRadiusLoc, &kuwaharaRadius,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(lightingPassShader, uberKuwaharaIntensityLoc, &kuwaharaIntensity,
                                   SHADER_UNIFORM_FLOAT);

                    // Draw the G-Buffer Albedo to lighting shader
                    DrawTexturePro(gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                }
                EndShaderMode();

                // Hardware profiling
                rlDrawRenderBatchActive(); // flush for timer
                GpuProfiler::End();        // End timer

                EndTextureMode();

            } else if (activeRenderPath == RenderPath::DeferredVolume) {
                // ARCHITECTURE B: MULTI-PASS DEFERRED (LIGHT VOLUMES)

                // Pass 2: Light Volume Accumulation
                BeginTextureMode(litSceneTarget);
                ClearBackground(BLANK); // Clear canvas to black

                // Hardware profiling
                rlDrawRenderBatchActive(); // flush for timer
                lightProfiler.Begin();     // End timer

                BeginBlendMode(BLEND_ADDITIVE); // Enable hardware light accumulation
                rlDisableDepthMask();           // Prevent spheres from occluding each other

                BeginMode3D(camera);
                {
                    BeginShaderMode(lightVolumeShader);
                    {
                        // set dynamic light values using uniforms
                        SetShaderValue(lightVolumeShader, lvIntensityLoc, &lightIntensity,
                                       SHADER_UNIFORM_FLOAT);
                        SetShaderValue(lightVolumeShader, lvMaxRadiusLoc, &dynamicMaxRadius,
                                       SHADER_UNIFORM_FLOAT);

                        SetShaderValue(lightVolumeShader, lvResolutionLoc, internalRes,
                                       SHADER_UNIFORM_VEC2);
                        SetShaderValue(lightVolumeShader, lvViewPosLoc, &camera.position,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValue(lightVolumeShader, lvLightColorLoc, &lightColor,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValueMatrix(lightVolumeShader, lvInvViewProjLoc, invViewProj);

                        SetShaderValueTexture(lightVolumeShader, lvAlbedoTexLoc, gAlbedo);
                        SetShaderValueTexture(lightVolumeShader, lvNormalTexLoc, gNormal);
                        SetShaderValueTexture(lightVolumeShader, lvDepthTexLoc, gDepth);

                        SetShaderValue(lightVolumeShader, lvEnableGoochLoc, &goochInt,
                                       SHADER_UNIFORM_INT);
                        SetShaderValue(lightVolumeShader, lvEnableToonLoc, &toonInt,
                                       SHADER_UNIFORM_INT);

                        // Draw all Lights safely via back-faces
                        if (actualVisibleVolumeCount > 0) {
                            DrawMeshInstanced(lightningSphereMesh, lightVolumeMaterial,
                                              visibleLightVolumeTransforms.data(),
                                              actualVisibleVolumeCount);
                        }
                    }
                    EndShaderMode();
                }
                EndMode3D();
                rlDrawRenderBatchActive(); // flush volume draw

                // Restore hardware states
                rlEnableDepthMask();
                EndBlendMode();
                EndTextureMode();

                // Pass 3: NPR Resolve (Spatial Filters)
                BeginTextureMode(resolveTarget);
                ClearBackground(BLANK);
                BeginShaderMode(nprResolveShader);
                {
                    SetShaderValue(nprResolveShader, nprAmbientLoc, &ambientLightStrength,
                                   SHADER_UNIFORM_FLOAT);
                    SetShaderValue(nprResolveShader, nprResolutionLoc, internalRes,
                                   SHADER_UNIFORM_VEC2);
                    SetShaderValue(nprResolveShader, nprViewPosLoc, &camera.position,
                                   SHADER_UNIFORM_VEC3);
                    SetShaderValueMatrix(nprResolveShader, nprInvViewProjLoc, invViewProj);

                    // Bind the accumulated lighting canvas and the G-Buffer
                    SetShaderValueTexture(nprResolveShader, nprLitSceneTexLoc,
                                          litSceneTarget.texture);
                    SetShaderValueTexture(nprResolveShader, nprAlbedoTexLoc, gAlbedo);
                    SetShaderValueTexture(nprResolveShader, nprNormalTexLoc, gNormal);
                    SetShaderValueTexture(nprResolveShader, nprDepthTexLoc, gDepth);

                    // set dynamic switches for NPR Effects
                    SetShaderValue(nprResolveShader, resEnableOutlinesLoc, &outInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(nprResolveShader, resEnableKuwaharaLoc, &kuwInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(nprResolveShader, resEnableGoochLoc, &goochInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(nprResolveShader, resEnableToonLoc, &toonInt,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(nprResolveShader, resKuwaharaRadiusLoc, &kuwaharaRadius,
                                   SHADER_UNIFORM_INT);
                    SetShaderValue(nprResolveShader, resKuwaharaIntensityLoc, &kuwaharaIntensity,
                                   SHADER_UNIFORM_FLOAT);

                    // Draw full-screen quad to execute resolve
                    DrawTexturePro(gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                }
                EndShaderMode();

                // Hardware Profiling
                rlDrawRenderBatchActive(); // flush for timer
                GpuProfiler::End();        // end timer

                EndTextureMode();
            } else if (activeRenderPath == RenderPath::Forward) {
                // ARCHITECTURE C: FORWARD RENDERING

                // Explicit State Sanitization
                rlEnableDepthTest();
                rlEnableDepthMask();
                rlEnableBackfaceCulling();
                rlSetCullFace(RL_CULL_FACE_BACK);

                // FBO binding & Viewport forcing mirrors the G-Buffer architecture
                rlEnableFramebuffer(litSceneTarget.id);
                rlViewport(0, 0, renderWidth, renderHeight);
                rlActiveDrawBuffers(1);

                Color bgCol = {
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.x),
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.y),
                    static_cast<unsigned char>(Config::EngineSettings::BackgroundColor.z), 255};
                ClearBackground(bgCol);

                // Hardware Profiling
                rlDrawRenderBatchActive(); // flush
                lightProfiler.Begin();     // start timer

                BeginMode3D(camera);
                {
                    // Force internal viewport to guarantee projection matrix matches
                    rlViewport(0, 0, renderWidth, renderHeight);

                    int safeVisibleLightCount = std::min(actualVisibleVolumeCount, 500);
                    int blinnCount            = static_cast<int>(fwdTransformsBlinn.size());
                    int goochCount            = static_cast<int>(fwdTransformsGooch.size());
                    int toonCount             = static_cast<int>(fwdTransformsToon.size());

                    // UNLIT PASS (Light Proxies)
                    if (actualVisibleProxyCount > 0) {
                        BeginShaderMode(forwardUnlitShader);
                        DrawMeshInstanced(lightningSphereMesh, fwdLightProxyMaterial,
                                          visibleLightProxyTransforms.data(),
                                          std::min(actualVisibleProxyCount, 500));
                        EndShaderMode();
                    }

                    // BLINN-PHONG PASS (Includes Floor)
                    BeginShaderMode(forwardBlinnShader);
                    {
                        SetShaderValue(forwardBlinnShader, fwdBlinnViewPosLoc, &camera.position,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValue(forwardBlinnShader, fwdBlinnIntensityLoc, &lightIntensity,
                                       SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardBlinnShader, fwdBlinnAmbientLoc,
                                       &ambientLightStrength, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardBlinnShader, fwdBlinnMaxRadiusLoc, &dynamicMaxRadius,
                                       SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardBlinnShader, fwdBlinnActiveLightLoc,
                                       &safeVisibleLightCount, SHADER_UNIFORM_INT);
                        SetShaderValue(forwardBlinnShader, fwdBlinnLightColorLoc, &lightColor,
                                       SHADER_UNIFORM_VEC3);

                        if (safeVisibleLightCount > 0) {
                            SetShaderValueV(forwardBlinnShader, fwdBlinnLightPosLoc,
                                            visibleLightPositions.data(), SHADER_UNIFORM_VEC3,
                                            safeVisibleLightCount);
                        }

                        if (!useNprRoom) {
                            Matrix floorIdentity = MatrixIdentity();
                            DrawMeshInstanced(floorMesh, fwdFloorMaterial, &floorIdentity, 1);
                        }

                        if (blinnCount > 0) {
                            DrawMeshInstanced(currentMesh, fwdBlinnMaterial,
                                              fwdTransformsBlinn.data(), blinnCount);
                        }
                    }
                    EndShaderMode();

                    // GOOCH PASS
                    if (goochCount > 0) {
                        BeginShaderMode(forwardGoochShader);
                        SetShaderValue(forwardGoochShader, fwdGoochViewPosLoc, &camera.position,
                                       SHADER_UNIFORM_VEC3);
                        SetShaderValue(forwardGoochShader, fwdGoochIntensityLoc, &lightIntensity,
                                       SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardGoochShader, fwdGoochMaxRadiusLoc, &dynamicMaxRadius,
                                       SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardGoochShader, fwdGoochAmbientLoc,
                                       &ambientLightStrength, SHADER_UNIFORM_FLOAT);
                        SetShaderValue(forwardGoochShader, fwdGoochActiveLightLoc,
                                       &safeVisibleLightCount, SHADER_UNIFORM_INT);

                        if (safeVisibleLightCount > 0) {
                            SetShaderValueV(forwardGoochShader, fwdGoochLightPosLoc,
                                            visibleLightPositions.data(), SHADER_UNIFORM_VEC3,
                                            safeVisibleLightCount);
                        }

                        DrawMeshInstanced(currentMesh, fwdGoochMaterial, fwdTransformsGooch.data(),
                                          goochCount);
                        EndShaderMode();
                    }

                    // TOON PASS
                    if (toonCount > 0) {
                        BeginShaderMode(forwardToonShader);
                        {
                            SetShaderValue(forwardToonShader, fwdToonViewPosLoc, &camera.position,
                                           SHADER_UNIFORM_VEC3);
                            SetShaderValue(forwardToonShader, fwdToonIntensityLoc, &lightIntensity,
                                           SHADER_UNIFORM_FLOAT);
                            SetShaderValue(forwardToonShader, fwdToonAmbientLoc,
                                           &ambientLightStrength, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(forwardToonShader, fwdToonMaxRadiusLoc,
                                           &dynamicMaxRadius, SHADER_UNIFORM_FLOAT);
                            SetShaderValue(forwardToonShader, fwdToonActiveLightLoc,
                                           &safeVisibleLightCount, SHADER_UNIFORM_INT);
                            SetShaderValue(forwardToonShader, fwdToonLightColorLoc, &lightColor,
                                           SHADER_UNIFORM_VEC3);

                            if (safeVisibleLightCount > 0) {
                                SetShaderValueV(forwardToonShader, fwdToonLightPosLoc,
                                                visibleLightPositions.data(), SHADER_UNIFORM_VEC3,
                                                safeVisibleLightCount);
                            }

                            DrawMeshInstanced(currentMesh, fwdToonMaterial,
                                              fwdTransformsToon.data(), toonCount);
                        }
                        EndShaderMode();

                        // Inverted Hull Outline
                        if (int outlineCount = static_cast<int>(fwdTransformsOutline.size());
                            enableOutlines && outlineCount > 0) {
                            BeginShaderMode(forwardOutlineShader);
                            {
                                SetShaderValue(forwardOutlineShader, fwdOutlineViewPosLoc,
                                               &camera.position, SHADER_UNIFORM_VEC3);
                                rlDisableBackfaceCulling();

                                DrawMeshInstanced(currentMesh, fwdOutlineMaterial,
                                                  fwdTransformsOutline.data(), outlineCount);

                                rlEnableBackfaceCulling();
                            }
                            EndShaderMode();
                        }
                    }
                }
                EndMode3D();

                rlDrawRenderBatchActive(); // flush for timer
                GpuProfiler::End();        // end timer

                // Fetch from internal FBO and restore window-scale viewport
                rlDisableFramebuffer();
                rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());
            }

            // POST-PROCESSING PASS
            // -------------------------------------------------------------------------------------

            ClearBackground(BLACK);

            BeginShaderMode(postShader);
            {
                // Pass Uniforms
                SetShaderValue(postShader, postResolutionLoc, dynamicRes, SHADER_UNIFORM_VEC2);

                // Dynamically route the correct texture to the final output based on architecture
                Texture2D finalRender;
                if (activeRenderPath == RenderPath::DeferredVolume) {
                    finalRender = resolveTarget.texture;
                } else {
                    // Both Uber-Shader and Forward write their final composite to litSceneTarget
                    finalRender = litSceneTarget.texture;
                }

                // Draw finished Scene into post-Processing shader
                DrawTexturePro(finalRender, sourceRec, screenDestRec, {0, 0}, 0.0f, WHITE);
            }
            EndShaderMode();

            // TEXT OVERLAY & UI (Telemetry Dashboard)
            // -------------------------------------------------------------------------------------

            // Dynamic Scaling & Layout (Baseline 1080p)
            float uiScale = (currentHeight / 1080.0f) * UiScale;
            int fontLg    = static_cast<int>(24 * uiScale);
            int fontMd    = static_cast<int>(18 * uiScale);
            int fontSm    = static_cast<int>(14 * uiScale);
            int pad       = static_cast<int>(20 * uiScale);
            int spacing   = static_cast<int>(26 * uiScale);

            int panelWidth  = static_cast<int>(500 * uiScale);
            int panelHeight = static_cast<int>(1100 * uiScale);
            int panelX      = pad;
            int panelY      = pad;

            // UI Color-Palette
            Color colBg       = {20, 24, 28, 245};    // Dark slate
            Color colBorder   = {60, 65, 75, 255};    // Subtle border outline
            Color colText     = {235, 235, 240, 255}; // Soft white
            Color colMuted    = {150, 155, 165, 255}; // Muted gray
            Color colAccent   = {170, 205, 250, 255}; // Pastel blue
            Color colAction   = {255, 240, 165, 255}; // Pastel Yellow
            Color colWarning  = {255, 200, 140, 255}; // Pastel Orange
            Color colGood     = {175, 235, 180, 255}; // Pastel green
            Color colBad      = {245, 165, 165, 255}; // Pastel red
            Color colSpecial  = {200, 180, 245, 255}; // Pastel purple
            Color colSpecial2 = {160, 240, 245, 255}; // Pastel teal
            Color colSpecial3 = {255, 195, 190, 255}; // Pastel peach

            // Panel Background
            DrawRectangle(panelX, panelY, panelWidth, panelHeight, colBg);
            DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, colBorder);

            // Column Layout Coordinates
            int textX = panelX + pad;
            int valueX =
                textX + static_cast<int>(250 * uiScale); // Hard vertical boundary for values
            int textY = panelY + pad;

            // Header
            DrawText("HYDRA ENGINE DASHBOARD", textX, textY, fontLg, colAccent);
            textY += spacing + pad;

            // Pipeline Section
            DrawText("ARCHITECTURE", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            DrawText("Pipeline-Mode", textX, textY, fontMd, colText);
            auto modeString = "Unknown";
            if (activeRenderPath == RenderPath::DeferredUber)
                modeString = "Deferred (Uber-Shader)";
            else if (activeRenderPath == RenderPath::DeferredVolume)
                modeString = "Deferred (Light Volumes)";
            else if (activeRenderPath == RenderPath::Forward)
                modeString = "Forward (Baseline)";
            DrawText(modeString, valueX, textY, fontMd, colSpecial);
            textY += spacing;

            DrawText("FBO Depth", textX, textY, fontMd, colText);
            DrawText(use16BitHDR ? "16-Bit HDR (Float)" : "8-Bit LDR (Clamped)", valueX, textY,
                     fontMd, use16BitHDR ? colGood : colBad);
            textY += spacing;

            DrawText("Test Environment", textX, textY, fontMd, colText);
            DrawText(useNprRoom ? "NPR Room (Synthetic)" : "Open World (Organic)", valueX, textY,
                     fontMd, useNprRoom ? colWarning : colSpecial2);
            textY += spacing + pad;

            // NPR State Section
            DrawText("NPR PIPELINE STATE", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            DrawText("Outlines [O]", textX, textY, fontMd, colText);
            DrawText(enableOutlines ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
                     enableOutlines ? colGood : colBad);
            textY += spacing;

            DrawText("Kuwahara [K]", textX, textY, fontMd, colText);
            DrawText(enableKuwahara ? TextFormat("ENABLED (Rad: %d | Int: %.1f)", kuwaharaRadius,
                                                 kuwaharaIntensity)
                                    : "DISABLED",
                     valueX, textY, fontMd, enableKuwahara ? colGood : colBad);
            textY += spacing;

            DrawText("Gooch [G]", textX, textY, fontMd, colText);
            DrawText(enableGooch ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
                     enableGooch ? colGood : colBad);
            textY += spacing;

            DrawText("Toon [T]", textX, textY, fontMd, colText);
            DrawText(enableToon ? "ENABLED" : "DISABLED", valueX, textY, fontMd,
                     enableToon ? colGood : colBad);
            textY += spacing + pad;

            // Scene Data Section (State & Variables)
            DrawText("SCENE DATA", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            int currentFps = GetFPS();
            Color fpsColor = currentFps >= 60 ? colGood : (currentFps >= 30 ? colWarning : colBad);
            DrawText("Framerate", textX, textY, fontMd, colText);
            DrawText(TextFormat("%d FPS", currentFps), valueX, textY, fontMd, fpsColor);
            textY += spacing;

            if (activeRenderPath == RenderPath::Forward) {
                DrawText("GPU Geometry", textX, textY, fontMd, colText);
                DrawText("N/A (Unified)", valueX, textY, fontMd, colMuted);
                textY += spacing;

                DrawText("GPU Forward (All)", textX, textY, fontMd, colText);
                DrawText(TextFormat("%.3f ms", lightProfiler.elapsedMs), valueX, textY, fontMd,
                         colSpecial3);
                textY += spacing;
            } else {
                DrawText("GPU Geometry", textX, textY, fontMd, colText);
                DrawText(TextFormat("%.3f ms", geomProfiler.elapsedMs), valueX, textY, fontMd,
                         colSpecial3);
                textY += spacing;

                DrawText("GPU Light + NPR", textX, textY, fontMd, colText);
                DrawText(TextFormat("%.3f ms", lightProfiler.elapsedMs), valueX, textY, fontMd,
                         colSpecial3);
                textY += spacing;
            }

            DrawText("Camera-Mode", textX, textY, fontMd, colText);
            auto cameraModeString = "Unknown";
            if (currentCameraState == CameraState::Orbital)
                cameraModeString = "Orbital";
            else if (currentCameraState == CameraState::Free)
                cameraModeString = "Free";
            else if (currentCameraState == CameraState::Static)
                cameraModeString = "Static";
            DrawText(cameraModeString, valueX, textY, fontMd, colSpecial2);
            textY += spacing;

            DrawText("Distribution", textX, textY, fontMd, colText);
            DrawText(useClusteredStyles ? "Quadrants (Clustered)" : "Uniform (Random)", valueX,
                     textY, fontMd, colSpecial2);
            textY += spacing;

            DrawText("Light Topology", textX, textY, fontMd, colText);
            DrawText(useLightSingularity ? "Singularity (Stacked)" : "Uniform (Scattered)", valueX,
                     textY, fontMd, colSpecial2);
            textY += spacing;

            DrawText("Geometry Level", textX, textY, fontMd, colText);
            DrawText(TextFormat("LOD %d (%d Tris/Obj)", currentLodIndex, currentMesh.triangleCount),
                     valueX, textY, fontMd, colWarning);
            textY += spacing;

            int activeTriangles = currentMesh.triangleCount * actualVisibleCount;
            DrawText("Active Triangles", textX, textY, fontMd, colText);
            DrawText(TextFormat("%d", activeTriangles), valueX, textY, fontMd, colWarning);
            textY += spacing;

            DrawText("Cloud Size", textX, textY, fontMd, colText);
            DrawText(TextFormat("%.2f", objectSphereRadius), valueX, textY, fontMd, colText);
            textY += spacing;

            DrawText("Obstacles", textX, textY, fontMd, colText);
            DrawText(std::format("{} / {}", actualVisibleCount, currentObstacleCount).c_str(),
                     valueX, textY, fontMd, colText);
            textY += spacing;

            int drawnLights = 0;
            if (activeRenderPath == RenderPath::DeferredVolume) {
                drawnLights = actualVisibleVolumeCount; // Multi-Pass draws all culled lights
            } else {
                // Both Uber-Shader and Forward use the 500-light array limit
                drawnLights = std::min(actualVisibleVolumeCount, 500);
            }

            DrawText("Lights Evaluated", textX, textY, fontMd, colText);
            DrawText(std::format("{} / {}", drawnLights, activeLightCount).c_str(), valueX, textY,
                     fontMd, colText);
            textY += spacing;

            if (activeRenderPath != RenderPath::DeferredVolume && actualVisibleVolumeCount > 500) {
                DrawText("WARNING: Shader Array Limit Exceeded!", textX, textY, fontSm, colBad);
                textY += static_cast<int>(18 * uiScale);
            }

            DrawText("Light Intensity", textX, textY, fontMd, colText);
            DrawText(TextFormat("%.2f", lightIntensity), valueX, textY, fontMd, colText);
            textY += spacing;

            DrawText("Ambient Strength", textX, textY, fontMd, colText);
            DrawText(TextFormat("%.2f", ambientLightStrength), valueX, textY, fontMd, colText);
            textY += spacing + pad;

            // Controls Section
            DrawText("CONTROLS", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            DrawText("Toggle Pipeline", textX, textY, fontMd, colText);
            DrawText("[TAB]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Toggle HDR/LDR", textX, textY, fontMd, colText);
            DrawText("[H]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Toggle \"NPR-Room\"", textX, textY, fontMd, colText);
            DrawText("[R]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Cloud Size", textX, textY, fontMd, colText);
            DrawText("[7 / 9]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Obstacles Count", textX, textY, fontMd, colText);
            DrawText("[4 / 6]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Lights Count", textX, textY, fontMd, colText);
            DrawText("[5 / 8]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Light Intensity", textX, textY, fontMd, colText);
            DrawText("[0 / 2]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Ambient Light", textX, textY, fontMd, colText);
            DrawText("[1 / 3]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Kuwahara Radius", textX, textY, fontMd, colText);
            DrawText("[Left / Right]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Kuwahara Intensity", textX, textY, fontMd, colText);
            DrawText("[Up / Down]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Capture Screen", textX, textY, fontMd, colText);
            DrawText("[P]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Switch Camera", textX, textY, fontMd, colText);
            DrawText("[C]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Switch Distribution", textX, textY, fontMd, colText);
            DrawText("[M]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Toggle Light-Collapse", textX, textY, fontMd, colText);
            DrawText("[L]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Geometry LOD", textX, textY, fontMd, colText);
            DrawText("[Z / X]", valueX, textY, fontMd, colAction);
            textY += spacing;

            // screenshot execution
            if (requestScreenshot) {
                rlDrawRenderBatchActive();

                namespace fs = std::filesystem;

                // Dynamically escape CMake environment
                fs::path currentPath = fs::current_path();
                if (currentPath.filename().string().find("cmake-build") != std::string::npos) {
                    currentPath = currentPath.parent_path(); // Step up to the project root
                }

                fs::path outputDir = currentPath / "outputs" / "screenshots";

                if (!fs::exists(outputDir)) {
                    std::error_code ec;
                    fs::create_directories(outputDir, ec);
                    if (ec)
                        TraceLog(LOG_ERROR, "FAILED TO CREATE DIRECTORY: %s", ec.message().c_str());
                }

                auto now     = std::chrono::system_clock::now();
                auto now_sec = std::chrono::floor<std::chrono::seconds>(now);
                std::chrono::zoned_time local_time{std::chrono::current_zone(), now_sec};
                std::string fileName =
                    std::format("hydra_capture_{:%Y%m%d_%H%M%S}.png", local_time);
                fs::path fullPath = outputDir / fileName;

                // High-DPI Bypass: Read absolute physical pixels, ignoring OS logical scaling
                int physWidth            = GetRenderWidth();
                int physHeight           = GetRenderHeight();
                unsigned char *rawPixels = rlReadScreenPixels(physWidth, physHeight);

                // Construct a Raylib Image from the raw pixel buffer
                Image capture = {rawPixels, physWidth, physHeight, 1,
                                 PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

                // Export and clean up memory
                ExportImage(capture, fullPath.string().c_str());
                UnloadImage(capture);
                TraceLog(LOG_INFO, "SCREENSHOT SAVED SUCCESSFULLY TO: %s",
                         fullPath.string().c_str());
                requestScreenshot = false;
            }

            // Update Hardware Profilers before concluding the frame
            geomProfiler.Update();
            lightProfiler.Update();
        }
        EndDrawing();
    }

    // CLEANUP
    // ---------------------------------------------------------------------------------------------

    // Unload Hardware profilers
    geomProfiler.Destroy();
    lightProfiler.Destroy();

    // Unload Materials
    UnloadMaterial(instancedMaterial);
    UnloadMaterial(instancedLightMaterial);
    UnloadMaterial(lightVolumeMaterial);
    UnloadMaterial(fwdBlinnMaterial);
    UnloadMaterial(fwdGoochMaterial);
    UnloadMaterial(fwdToonMaterial);
    UnloadMaterial(fwdOutlineMaterial);
    UnloadMaterial(fwdFloorMaterial);
    UnloadMaterial(fwdLightProxyMaterial);

    // Unload Custom VBOs
    rlUnloadVertexBuffer(styleIdVboId);
    rlUnloadVertexBuffer(lightStyleIdVboId);

    // Unload FBOs and associated Textures
    rlUnloadFramebuffer(FboId);
    rlUnloadTexture(albedoTexId);
    rlUnloadTexture(normalTexId);
    rlUnloadTexture(depthTexId);
    UnloadRenderTexture(litSceneTarget);
    UnloadRenderTexture(resolveTarget);

    // Unload Shaders
    UnloadShader(geometryPassShader);
    UnloadShader(instancedShader);
    UnloadShader(lightingPassShader);
    UnloadShader(lightVolumeShader);
    UnloadShader(nprResolveShader);
    UnloadShader(forwardBlinnShader);
    UnloadShader(forwardGoochShader);
    UnloadShader(forwardToonShader);
    UnloadShader(forwardOutlineShader);
    UnloadShader(forwardUnlitShader);
    UnloadShader(postShader);

    // Standard unloads
    for (auto &mesh : lodMeshes) {
        UnloadMesh(mesh);
    }
    UnloadTexture(obstacleTexture);
    UnloadTexture(floorTexture);
    UnloadModel(lightningSourceModel);
    UnloadModel(floorModel);

    CloseWindow();

    return 0;
}