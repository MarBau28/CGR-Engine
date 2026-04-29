#include "../include/Config.h"
#include "../include/InputHandler.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <format>
#include <vector>

// Structs
// -------------------------------------------------------------------------------------------------

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

// Global Constants
// -------------------------------------------------------------------------------------------------

inline constexpr std::string_view obstacleTextureName           = "starry-galaxy_tex.png";
inline constexpr std::string_view woodTextureName               = "wood_tex.png";
inline constexpr std::string_view geometryVertexShaderName      = "geometry_pass.vert";
inline constexpr std::string_view geometryFragmentShaderName    = "geometry_pass.frag";
inline constexpr std::string_view lightingFragmentShaderName    = "lighting_pass.frag";
inline constexpr std::string_view postFragmentShaderName        = "postprocess.frag";
inline constexpr std::string_view instancedVertexShaderName     = "geometry_pass_instanced.vert";
inline constexpr std::string_view instancedFragmentShaderName   = "geometry_pass_instanced.frag";
inline constexpr std::string_view lightVolumeVertexShaderName   = "light_volume.vert";
inline constexpr std::string_view lightVolumeFragmentShaderName = "light_volume.frag";
inline constexpr std::string_view nprResolveFragmentShaderName  = "npr_resolve.frag";
inline constexpr float modelScalar                              = 1.0f;
inline constexpr float modelRotation                            = 1.5f;
inline constexpr int cameraMode                                 = CAMERA_ORBITAL;
inline constexpr int obstacleCount                              = 5000;
inline constexpr float ObjectSphereRadius                       = 200.0f; // obstacle cloud size
inline constexpr float minLightThreshold                        = 0.05f;
inline constexpr float attenuationConstant                      = 1.0f;
inline constexpr float attenuationLinear                        = 0.09f;
inline constexpr float attenuationQuadratic                     = 0.032f;
inline constexpr uint numberOfStyles                            = 4;
inline constexpr int MAX_OBSTACLES                              = 30000;
inline constexpr int MAX_LIGHTS                                 = 5000;
inline constexpr float UiScale                                  = 0.8f;

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

// Settings
bool setFrameLimit           = true;
bool enableStyleSplit        = true;
bool useDeferredLightVolumes = true;
bool use16BitHDR             = true;
int activeObstacleCount      = 5000;
int activeLightCount         = 500;
int actualGeneratedLights    = 0;
float lightIntensity         = Config::EngineSettings::LightIntensity;
float ambientLightStrength   = Config::EngineSettings::AmbientLightStrength;
bool requestScreenshot       = false;

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

    // Camera Setup
    Camera3D camera   = {0};
    camera.position   = Config::EngineSettings::CameraPosition;      // camera spot
    camera.target     = Config::EngineSettings::CameraViewDirection; // view direction
    camera.up         = Config::EngineSettings::CameraOrientation;   // up vector
    camera.fovy       = Config::EngineSettings::CameraFOV;           // Field of View
    camera.projection = CAMERA_PERSPECTIVE;                          // standard 3D-depth projection

    // Load  and link Shaders
    const std::string geometryVertPath =
        std::string(Config::Paths::Shaders) + std::string(geometryVertexShaderName);
    const std::string geometryFragPath =
        std::string(Config::Paths::Shaders) + std::string(geometryFragmentShaderName);
    const std::string lightingFragPath =
        std::string(Config::Paths::Shaders) + std::string(lightingFragmentShaderName);
    const std::string postFragPath =
        std::string(Config::Paths::Shaders) + std::string(postFragmentShaderName);
    const std::string instancedVertPath =
        std::string(Config::Paths::Shaders) + std::string(instancedVertexShaderName);
    const std::string instancedFragPath =
        std::string(Config::Paths::Shaders) + std::string(instancedFragmentShaderName);
    const std::string lightVolumeVertPath =
        std::string(Config::Paths::Shaders) + std::string(lightVolumeVertexShaderName);
    const std::string lightVolumeFragPath =
        std::string(Config::Paths::Shaders) + std::string(lightVolumeFragmentShaderName);
    const std::string nprResolveFragPath =
        std::string(Config::Paths::Shaders) + std::string(nprResolveFragmentShaderName);

    const Shader geometryPassShader =
        LoadShader(geometryVertPath.c_str(), geometryFragPath.c_str());
    const Shader instancedShader = LoadShader(instancedVertPath.c_str(), instancedFragPath.c_str());
    const Shader lightingPassShader = LoadShader(nullptr, lightingFragPath.c_str());
    const Shader postShader         = LoadShader(nullptr, postFragPath.c_str());
    const Shader lightVolumeShader =
        LoadShader(lightVolumeVertPath.c_str(), lightVolumeFragPath.c_str());
    const Shader nprResolveShader = LoadShader(nullptr, nprResolveFragPath.c_str());

    // OBJECT GENERATION
    // ---------------------------------------------------------------------------------------------

    // Generate obstacle base Meshes with Texture
    Mesh baseMesh = GenMeshCube(0.75f, 0.75f, 0.75f);
    GenMeshTangents(&baseMesh);
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

    // Obstacle generation
    for (int i = 0; i < MAX_OBSTACLES; i++) {
        constexpr float baseRadius = 0.6495f;
        float scale                = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;

        // Uniform Circular Distribution Math
        float u      = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f; // 0.0 to 1.0
        float radius = sqrtf(u) * ObjectSphereRadius;
        float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;

        float minHeight           = scale;
        constexpr float maxHeight = ObjectSphereRadius / 2;
        float height = static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                         static_cast<int>(maxHeight) * 10)) /
                       10.0f;

        Vector3 pos = {cosf(angle) * radius, height, sinf(angle) * radius};

        // Determine Style ID
        float styleId = enableStyleSplit ? static_cast<float>(GetRandomValue(1, 4)) : 1.0f;

        // Calculate Transform Matrix
        Vector3 rotAxis = Vector3Normalize({static_cast<float>(GetRandomValue(0, 100)),
                                            static_cast<float>(GetRandomValue(0, 100)),
                                            static_cast<float>(GetRandomValue(0, 100))});
        auto rotAngle   = static_cast<float>(GetRandomValue(0, 360));

        Matrix transform = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale),
                                                         MatrixRotate(rotAxis, rotAngle * DEG2RAD)),
                                          MatrixTranslate(pos.x, pos.y, pos.z));

        // Push directly to obstacle arrays
        masterObstacleTransforms.push_back(transform);
        masterObstacleStyleIds.push_back(styleId);
        masterObstacleSpheres.push_back({pos, baseRadius * scale});
    }

    // Light generation
    auto TryFindEmptyLightSpace = [&](const float radiusToClear, const float minHeight,
                                      const float maxHeight, Vector3 &outPos) -> bool {
        for (int attempts = 0; attempts < 50; attempts++) {
            const float u = static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f; // 0.0 to 1.0
            const float radius = sqrtf(u) * ObjectSphereRadius;
            const float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
            const float height =
                static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                  static_cast<int>(maxHeight) * 10)) /
                10.0f;

            Vector3 testPos = {cosf(angle) * radius, height, sinf(angle) * radius};

            const bool isColliding =
                std::ranges::any_of(occupiedLightPositions, [&](const auto &existingPos) {
                    return Vector3Distance(testPos, existingPos) < (radiusToClear + 0.5f);
                });

            if (!isColliding) {
                outPos = testPos;
                return true;
            }
        }
        return false;
    };

    int lightsGenerated = 0;
    int overallAttempts = 0;
    while (lightsGenerated < MAX_LIGHTS && overallAttempts < 10000) {
        overallAttempts++;
        constexpr float maxHeight = ObjectSphereRadius / 2;

        if (Vector3 validPos; TryFindEmptyLightSpace(0.5f, 2.0f, maxHeight, validPos)) {
            // set position (Needed for Uber-Shader comparison)
            masterLightPositions[lightsGenerated] = validPos;

            // Generate physical debug sphere transform
            Matrix proxyTransform = MatrixMultiply(
                MatrixScale(1.5f, 1.5f, 1.5f), MatrixTranslate(validPos.x, validPos.y, validPos.z));

            masterLightProxyTransforms.push_back(proxyTransform);
            occupiedLightPositions.push_back(validPos);
            lightsGenerated++;
        }
    }

    // stacking 50 lights on top of each other for Light-Volume clamping test
    // while (lightsGenerated < 50) {
    //     Vector3 stackedPos = {0.0f, 5.0f, 0.0f};
    //     // use 50 lights for this test
    //     masterLightPositions[lightsGenerated] = stackedPos;
    //     Matrix proxyTransform =
    //         MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f),
    //                        MatrixTranslate(stackedPos.x, stackedPos.y, stackedPos.z));
    //     Matrix volumeTransform =
    //         MatrixMultiply(MatrixScale(-maxRadius * 2.0f, -maxRadius * 2.0f, -maxRadius * 2.0f),
    //                        MatrixTranslate(stackedPos.x, stackedPos.y, stackedPos.z));
    //
    //     masterLightProxyTransforms.push_back(proxyTransform);
    //     masterLightVolumeTransforms.push_back(volumeTransform);
    //
    //     lightsGenerated++;
    // }

    actualGeneratedLights = lightsGenerated;
    activeLightCount      = std::min(activeLightCount, actualGeneratedLights);

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

    // Bind VBO to the baseMesh's VAO
    rlEnableVertexArray(baseMesh.vaoId);
    {
        rlEnableVertexBuffer(styleIdVboId);
        rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0); // Define data format
        rlEnableVertexAttribute(10);
        rlSetVertexAttributeDivisor(10, 1);
        rlDisableVertexBuffer();
    }
    rlDisableVertexArray();

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
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
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
                         PIXELFORMAT_UNCOMPRESSED_R16G16B16A16};
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

    // lighting pass
    int lightPosArrayLoc        = GetShaderLocation(lightingPassShader, "lightPositions");
    int lightColorLoc           = GetShaderLocation(lightingPassShader, "lightColor");
    int LightingResolutionLoc   = GetShaderLocation(lightingPassShader, "resolution");
    int normalTexLoc            = GetShaderLocation(lightingPassShader, "gNormalTex");
    int depthTexLoc             = GetShaderLocation(lightingPassShader, "gDepthTex");
    int invViewProjLoc          = GetShaderLocation(lightingPassShader, "invViewProj");
    int postViewPosLoc          = GetShaderLocation(lightingPassShader, "viewPos");
    int postBackgroundColorLoc  = GetShaderLocation(lightingPassShader, "backgroundColor");
    int intensityLoc            = GetShaderLocation(lightingPassShader, "lightIntensity");
    int ambientLoc              = GetShaderLocation(lightingPassShader, "ambientLightStrength");
    int activeLightsLoc         = GetShaderLocation(lightingPassShader, "activeLights");
    int maxLightRadiusLoc       = GetShaderLocation(lightingPassShader, "maxLightRadius");
    int attenuationConstantLoc  = GetShaderLocation(lightingPassShader, "attenuationConstant");
    int attenuationLinearLoc    = GetShaderLocation(lightingPassShader, "attenuationLinear");
    int attenuationQuadraticLoc = GetShaderLocation(lightingPassShader, "attenuationQuadratic");

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

    // npr resolve pass
    int nprResolutionLoc  = GetShaderLocation(nprResolveShader, "resolution");
    int nprViewPosLoc     = GetShaderLocation(nprResolveShader, "viewPos");
    int nprBgColorLoc     = GetShaderLocation(nprResolveShader, "backgroundColor");
    int nprAmbientLoc     = GetShaderLocation(nprResolveShader, "ambientLightStrength");
    int nprInvViewProjLoc = GetShaderLocation(nprResolveShader, "invViewProj");
    int nprLitSceneTexLoc = GetShaderLocation(nprResolveShader, "litSceneTex");
    int nprAlbedoTexLoc   = GetShaderLocation(nprResolveShader, "texture0");
    int nprNormalTexLoc   = GetShaderLocation(nprResolveShader, "gNormalTex");
    int nprDepthTexLoc    = GetShaderLocation(nprResolveShader, "gDepthTex");

    SetShaderValue(nprResolveShader, nprBgColorLoc, &BackgroundColor, SHADER_UNIFORM_VEC3);
    SetShaderValue(nprResolveShader, nprAmbientLoc, &Config::EngineSettings::AmbientLightStrength,
                   SHADER_UNIFORM_FLOAT);

    // post-process pass
    int postResolutionLoc = GetShaderLocation(postShader, "resolution");

    // MAIN GAME LOOP
    // ---------------------------------------------------------------------------------------------

    // Define source and destination rectangles for FBO
    constexpr Rectangle sourceRec = {0.0f, 0.0f, static_cast<float>(renderWidth),
                                     -static_cast<float>(renderHeight)};

    // FBO Target. Exactly defined res so the Lighting pass matches G-Buffer
    constexpr Rectangle fboDestRec = {0.0f, 0.0f, static_cast<float>(renderWidth),
                                      static_cast<float>(renderHeight)};

    bool frameLimitUpdated = false;

    // initialize the key binding controllers for continuous inputs
    ContinuousInput<int> obsInput;
    ContinuousInput<int> lightInput;
    ContinuousInput<float> intensityInput;
    ContinuousInput<float> ambientInput;

    while (!WindowShouldClose()) {
        // Update Camera and Screen
        UpdateCamera(&camera, cameraMode);
        auto currentWidth   = static_cast<float>(GetScreenWidth());
        auto currentHeight  = static_cast<float>(GetScreenHeight());
        float dynamicRes[2] = {currentWidth, currentHeight};
        int fontSize        = static_cast<int>(currentHeight) / 30;

        if (frameLimitUpdated) {
            SetTargetFPS(setFrameLimit ? Config::EngineSettings::TargetFPS : 0);
            frameLimitUpdated = false;
        }

        // Screen Target. Forces final image to fill the window
        const Rectangle screenDestRec = {0.0f, 0.0f, currentWidth, currentHeight};

        // KEY BINDINGS
        // -----------------------------------------------------------------------------------------

        if (IsKeyPressed(KEY_F)) {
            setFrameLimit     = !setFrameLimit;
            frameLimitUpdated = true;
        }
        if (IsKeyPressed(KEY_TAB)) {
            useDeferredLightVolumes = !useDeferredLightVolumes;
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

        // Adjust Obstacles: Tap: +/- 100 | Hold: +/- 1000 per second
        obsInput.Update(KEY_D, KEY_A, activeObstacleCount, 100, 1000.0f, 1, MAX_OBSTACLES);

        // Adjust Lights: Tap: +/- 10  | Hold: +/- 100 per second
        lightInput.Update(KEY_W, KEY_S, activeLightCount, 10, 100.0f, 1, actualGeneratedLights);

        // Adjust Light Intensity: Tap: +/- 0.1 | Hold: +/- 2.0 per second
        intensityInput.Update(KEY_UP, KEY_DOWN, lightIntensity, 0.1f, 2.0f, 0.0f,
                              std::numeric_limits<float>::max());

        // Adjust Ambient Strength: Tap: +/- 0.05 | Hold: +/- 1.0 per second
        ambientInput.Update(KEY_RIGHT, KEY_LEFT, ambientLightStrength, 0.05f, 1.0f, 0.0f,
                            std::numeric_limits<float>::max());

        // CULLING MATH & MATRIX SETUP
        // -----------------------------------------------------------------------------------------

        // Calculate the Inverse View-Projection Matrix
        double aspect      = static_cast<double>(currentWidth) / static_cast<double>(currentHeight);
        Matrix proj        = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01, 1000.0);
        Matrix view        = GetCameraMatrix(camera);
        Matrix viewProj    = MatrixMultiply(view, proj);
        Matrix invViewProj = MatrixInvert(viewProj); // Calculated now, used in lighting pass

        // Extract Frustum Planes
        Frustum cameraFrustum{};
        cameraFrustum.Extract(viewProj);

        // Execute CPU Culling for obstacles
        visibleObstacleTransforms.clear();
        visibleObstacleStyleIds.clear();

        for (int i = 0; i < activeObstacleCount; i++) {
            if (cameraFrustum.IsSphereVisible(masterObstacleSpheres[i])) {
                visibleObstacleTransforms.push_back(masterObstacleTransforms[i]);
                visibleObstacleStyleIds.push_back(masterObstacleStyleIds[i]);
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

        int actualVisibleVolumeCount = static_cast<int>(visibleLightVolumeTransforms.size());
        int actualVisibleProxyCount  = static_cast<int>(visibleLightProxyTransforms.size());

        // Draw
        BeginDrawing();
        {
            // GEOMETRY PASS
            // -------------------------------------------------------------------------------------

            // FBO Off-Screen rendering
            rlEnableFramebuffer(FboId);                  // Redirect GPU output to custom FBO
            rlViewport(0, 0, renderWidth, renderHeight); // Force camera to render at set resolution
            rlActiveDrawBuffers(2); // Activate all G-Buffer color attachments simultaneously
            ClearBackground(BLANK);
            rlClearScreenBuffers(); // Manually clear the FBOs color and depth buffers
            rlDisableColorBlend();  // prevent auto blending

            // Draw models
            BeginMode3D(camera);
            {
                // force back viewport to internal FBO resolution
                rlViewport(0, 0, renderWidth, renderHeight);

                // Draw Static Models (Floor) using standard shader
                SetShaderValueMatrix(geometryPassShader, modelMatLoc, MatrixIdentity());
                SetShaderValueMatrix(geometryPassShader, normalMatLoc, MatrixIdentity());
                float generalStyleId = 1;
                SetShaderValue(geometryPassShader, styleIdLoc, &generalStyleId,
                               SHADER_UNIFORM_FLOAT);
                DrawModel(floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

                // Draw Lights proxies using instanced shader (+ culled)
                int isLight = 1;
                SetShaderValue(instancedShader, isLightLocInstanced, &isLight, SHADER_UNIFORM_INT);
                if (actualVisibleProxyCount > 0) {
                    DrawMeshInstanced(lightningSphereMesh, instancedLightMaterial,
                                      visibleLightProxyTransforms.data(),
                                      std::min(actualVisibleProxyCount, 500));
                }

                // draw obstacles using instanced shader
                isLight = 0;
                SetShaderValue(instancedShader, isLightLocInstanced, &isLight, SHADER_UNIFORM_INT);
                if (actualVisibleCount > 0) {
                    DrawMeshInstanced(baseMesh, instancedMaterial, visibleObstacleTransforms.data(),
                                      actualVisibleCount);
                }
            }
            EndMode3D(); // stop applying 3D math

            // Restore GPU output back to the default screen buffer
            rlEnableColorBlend();
            rlDisableFramebuffer();

            // Restore the viewport to actual window size
            rlViewport(0, 0, GetRenderWidth(), GetRenderHeight());

            // LIGHTING PASS & RESOLVE
            // -------------------------------------------------------------------------------------

            if (!useDeferredLightVolumes) {
                // ARCHITECTURE A: UBER-SHADER

                BeginTextureMode(litSceneTarget);
                ClearBackground(BLANK);
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

                    // Draw the G-Buffer Albedo to lighting shader
                    DrawTexturePro(gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                }
                EndShaderMode();
                EndTextureMode();

            } else {
                // ARCHITECTURE B: MULTI-PASS DEFERRED

                // Pass 2: Light Volume Accumulation
                BeginTextureMode(litSceneTarget);
                ClearBackground(BLANK);         // Clear canvas to black
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

                    // Draw full-screen quad to execute resolve
                    DrawTexturePro(gAlbedo, sourceRec, fboDestRec, {0, 0}, 0.0f, WHITE);
                }
                EndShaderMode();
                EndTextureMode();
            }

            // POST-PROCESSING PASS
            // -------------------------------------------------------------------------------------

            ClearBackground(BLACK);

            BeginShaderMode(postShader);
            {
                // Pass Uniforms
                SetShaderValue(postShader, postResolutionLoc, dynamicRes, SHADER_UNIFORM_VEC2);

                // Dynamically route the correct texture to the final output
                Texture2D finalRender =
                    useDeferredLightVolumes ? resolveTarget.texture : litSceneTarget.texture;

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

            int panelWidth  = static_cast<int>(440 * uiScale);
            int panelHeight = static_cast<int>(530 * uiScale);
            int panelX      = pad;
            int panelY      = pad;

            // UI Color-Palette
            Color colBg     = {20, 24, 28, 245};    // Sleek dark slate
            Color colBorder = {60, 65, 75, 255};    // Subtle border outline
            Color colText   = {235, 235, 240, 255}; // Soft white
            Color colMuted  = {150, 155, 165, 255}; // Muted gray for headers
            Color colAccent = {170, 205, 250, 255}; // Pastel blue
            Color colAction = {250, 215, 160, 255}; // Pastel gold/yellow
            Color colGood   = {175, 235, 180, 255}; // Pastel green
            Color colBad    = {245, 165, 165, 255}; // Pastel red

            // Panel Background
            DrawRectangle(panelX, panelY, panelWidth, panelHeight, colBg);
            DrawRectangleLines(panelX, panelY, panelWidth, panelHeight, colBorder);

            // Column Layout Coordinates
            int textX = panelX + pad;
            int valueX =
                textX + static_cast<int>(170 * uiScale); // Hard vertical boundary for values
            int textY = panelY + pad;

            // Header
            DrawText("HYDRA ENGINE TELEMETRY", textX, textY, fontLg, colAccent);
            textY += spacing + pad;

            // Pipeline Section
            DrawText("ARCHITECTURE", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            DrawText("Mode", textX, textY, fontMd, colText);
            DrawText(useDeferredLightVolumes ? "Deferred (Light Volumes)"
                                             : "Deferred (Uber-Shader)",
                     valueX, textY, fontMd, colText);
            textY += spacing;

            DrawText("FBO Depth", textX, textY, fontMd, colText);
            DrawText(use16BitHDR ? "16-Bit HDR (Float)" : "8-Bit LDR (Clamped)", valueX, textY,
                     fontMd, use16BitHDR ? colGood : colBad);
            textY += spacing + pad;

            // Scene Data Section (State & Variables)
            DrawText("SCENE DATA", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            int currentFps = GetFPS();
            Color fpsColor = currentFps >= 60 ? colGood : (currentFps >= 30 ? colAction : colBad);
            DrawText("Framerate", textX, textY, fontMd, colText);
            DrawText(TextFormat("%d FPS", currentFps), valueX, textY, fontMd, fpsColor);
            textY += spacing;

            DrawText("Obstacles", textX, textY, fontMd, colText);
            DrawText(std::format("{} / {}", actualVisibleCount, activeObstacleCount).c_str(),
                     valueX, textY, fontMd, colText);
            textY += spacing;

            int drawnLights = useDeferredLightVolumes ? actualVisibleVolumeCount
                                                      : std::min(actualVisibleVolumeCount, 500);
            DrawText("Lights Rendered", textX, textY, fontMd, colText);
            DrawText(std::format("{} / {}", drawnLights, activeLightCount).c_str(), valueX, textY,
                     fontMd, colText);
            textY += spacing;

            DrawText("Light Intensity", textX, textY, fontMd, colText);
            DrawText(TextFormat("%.2f", lightIntensity), valueX, textY, fontMd, colText);
            textY += spacing;

            DrawText("Ambient Strength", textX, textY, fontMd, colText);
            DrawText(TextFormat("%.2f", ambientLightStrength), valueX, textY, fontMd, colText);
            textY += spacing;

            if (!useDeferredLightVolumes && actualVisibleVolumeCount > 500) {
                DrawText("WARNING: Uber-Shader Limit Exceeded!", textX, textY, fontSm, colBad);
                textY += static_cast<int>(18 * uiScale);
            }
            textY += pad;

            // Controls Section (Mutators & Input)
            DrawText("CONTROLS", textX, textY, fontSm, colMuted);
            textY += static_cast<int>(18 * uiScale);

            DrawText("Toggle Pipeline", textX, textY, fontMd, colText);
            DrawText("[TAB]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Toggle HDR/LDR", textX, textY, fontMd, colText);
            DrawText("[H]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Adjust Obstacles", textX, textY, fontMd, colText);
            DrawText("[A / D]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Adjust Lights", textX, textY, fontMd, colText);
            DrawText("[S / W]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Adjust Intensity", textX, textY, fontMd, colText);
            DrawText("[Down / Up]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Adjust Ambient", textX, textY, fontMd, colText);
            DrawText("[Left / Right]", valueX, textY, fontMd, colAction);
            textY += spacing;

            DrawText("Capture Screen", textX, textY, fontMd, colText);
            DrawText("[P]", valueX, textY, fontMd, colAction);

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
        }
        EndDrawing();
    }

    // CLEANUP
    // ---------------------------------------------------------------------------------------------

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
    UnloadShader(postShader);

    // Standard unloads
    UnloadMesh(baseMesh);
    UnloadTexture(obstacleTexture);
    UnloadModel(lightningSourceModel);
    UnloadModel(floorModel);
    UnloadTexture(floorTexture);

    CloseWindow();

    return 0;
}