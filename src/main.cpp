#include "../include/Config.h"
#include "algorithm"
#include "filesystem"
#include "format"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "vector"

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
inline constexpr int cameraMode                                 = CAMERA_PERSPECTIVE;
inline constexpr int obstacleCount                              = 5000;
inline constexpr float ObjectSphereRadius                       = 200.0f; // obstacle cloud size
inline constexpr int activeLightCount                           = 1000;
inline constexpr float minLightThreshold                        = 0.03f;
inline constexpr float attenuationConstant                      = 1.0f;
inline constexpr float attenuationLinear                        = 0.09f;
inline constexpr float attenuationQuadratic                     = 0.032f;
inline constexpr uint numberOfStyles                            = 4;

// Global variables
// -------------------------------------------------------------------------------------------------

// Immutable object Data
Vector3 lightPositions[activeLightCount];
std::vector<Vector3> occupiedLightPositions;
std::vector<Matrix> masterObstacleTransforms;
std::vector<float> masterObstacleStyleIds;
std::vector<BoundingSphere> masterObstacleSpheres;
std::vector<Matrix> masterLightProxyTransforms;  // Tiny spheres for visuals
std::vector<Matrix> masterLightVolumeTransforms; // Massive spheres for lighting math

// Render object Data (Uploaded/Updated in GPU every frame)
std::vector<Matrix> visibleObstacleTransforms;
std::vector<float> visibleObstacleStyleIds;

// Settings
bool setFrameLimit           = true;
bool enableStyleSplit        = true;
bool useDeferredLightVolumes = true;

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

    // calculate maximum lightRadius based on light intensity to safe shader calcs
    constexpr float c =
        attenuationConstant - (Config::EngineSettings::LightIntensity / minLightThreshold);
    const float maxRadius = (-attenuationLinear + std::sqrt(attenuationLinear * attenuationLinear -
                                                            (4.0f * attenuationQuadratic * c))) /
                            (2.0f * attenuationQuadratic);

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
    masterObstacleTransforms.reserve(obstacleCount);
    masterObstacleStyleIds.reserve(obstacleCount);
    masterObstacleSpheres.reserve(obstacleCount);
    visibleObstacleTransforms.reserve(obstacleCount);
    visibleObstacleStyleIds.reserve(obstacleCount);
    occupiedLightPositions.reserve(activeLightCount);
    masterLightProxyTransforms.reserve(activeLightCount);
    masterLightVolumeTransforms.reserve(activeLightCount);

    // Obstacle generation
    for (int i = 0; i < obstacleCount; i++) {
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
    while (lightsGenerated < activeLightCount && overallAttempts < 10000) {
        overallAttempts++;
        constexpr float maxHeight = ObjectSphereRadius / 2;

        if (Vector3 validPos; TryFindEmptyLightSpace(0.5f, 2.0f, maxHeight, validPos)) {
            // set position (Needed for Uber-Shader comparison)
            lightPositions[lightsGenerated] = validPos;

            // Generate physical debug sphere transform
            Matrix proxyTransform = MatrixMultiply(
                MatrixScale(1.5f, 1.5f, 1.5f), MatrixTranslate(validPos.x, validPos.y, validPos.z));

            // Generate invisible light volume transform (Scale maxRadius)
            Matrix volumeTransform =
                MatrixMultiply(MatrixScale(-maxRadius * 2.0f, -maxRadius * 2.0f, -maxRadius * 2.0f),
                               MatrixTranslate(validPos.x, validPos.y, validPos.z));

            masterLightProxyTransforms.push_back(proxyTransform);
            masterLightVolumeTransforms.push_back(volumeTransform);

            occupiedLightPositions.push_back(validPos);
            lightsGenerated++;
        }
    }

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
        rlLoadVertexBuffer(masterObstacleStyleIds.data(), obstacleCount * sizeof(float), true);

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
    std::vector instancedLightStyleIds(activeLightCount, 0.0f); // 0.0f = No style
    unsigned int lightStyleIdVboId =
        rlLoadVertexBuffer(instancedLightStyleIds.data(), activeLightCount * sizeof(float), false);

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

    // Allocate Texture Memory block Target 0: Albedo (8-bit RGBA)
    unsigned int albedoTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    // Allocate Texture Memory blocks Target 2: Normal (32-bit Float RGBA)
    unsigned int normalTexId =
        rlLoadTexture(nullptr, renderWidth, renderHeight, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    // Depth texture for Z-sorting (and position reconstruction)
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
                         PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};
    Texture2D gDepth  = {depthTexId, renderWidth, renderHeight, 1,
                         PIXELFORMAT_UNCOMPRESSED_GRAYSCALE};

    // Canvas for Pass 2 to draw onto (Either Uber-Shader or Light Volume accumulation)
    RenderTexture2D litSceneTarget = LoadRenderTexture(renderWidth, renderHeight);

    // Canvas for Pass 3 to draw onto (NPR Resolve pass, before final post-processing)
    RenderTexture2D resolveTarget = LoadRenderTexture(renderWidth, renderHeight);

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
    constexpr int safeLightCount      = std::min(activeLightCount, 500);

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
    SetShaderValue(lightingPassShader, activeLightsLoc, &safeLightCount, SHADER_UNIFORM_INT);
    SetShaderValue(lightingPassShader, maxLightRadiusLoc, &maxRadius, SHADER_UNIFORM_FLOAT);
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

    SetShaderValue(lightVolumeShader, lvMaxRadiusLoc, &maxRadius, SHADER_UNIFORM_FLOAT);
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

        // Key Bindings
        if (IsKeyPressed(KEY_F)) {
            setFrameLimit     = !setFrameLimit;
            frameLimitUpdated = true;
        }
        if (IsKeyPressed(KEY_TAB)) {
            useDeferredLightVolumes = !useDeferredLightVolumes;
        }

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

        // Execute CPU Culling
        visibleObstacleTransforms.clear();
        visibleObstacleStyleIds.clear();

        for (int i = 0; i < obstacleCount; i++) {
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
                // Draw Static Models (Floor) using standard shader
                SetShaderValueMatrix(geometryPassShader, modelMatLoc, MatrixIdentity());
                SetShaderValueMatrix(geometryPassShader, normalMatLoc, MatrixIdentity());
                float generalStyleId = 1;
                SetShaderValue(geometryPassShader, styleIdLoc, &generalStyleId,
                               SHADER_UNIFORM_FLOAT);
                DrawModel(floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

                // Draw Lights proxies using instanced shader
                int isLight = 1;
                SetShaderValue(instancedShader, isLightLocInstanced, &isLight, SHADER_UNIFORM_INT);
                DrawMeshInstanced(lightningSphereMesh, instancedLightMaterial,
                                  masterLightProxyTransforms.data(),
                                  useDeferredLightVolumes ? activeLightCount : safeLightCount);

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
            rlViewport(
                0, 0, static_cast<int>(currentWidth),
                static_cast<int>(currentHeight)); // Restore the viewport to actual window size

            // LIGHTING PASS & RESOLVE
            // -------------------------------------------------------------------------------------

            if (!useDeferredLightVolumes) {
                // ARCHITECTURE A: UBER-SHADER

                BeginTextureMode(litSceneTarget);
                ClearBackground(BLANK);
                BeginShaderMode(lightingPassShader);
                {
                    // Pass lighting arrays, camera and screen position
                    SetShaderValue(lightingPassShader, LightingResolutionLoc, internalRes,
                                   SHADER_UNIFORM_VEC2);
                    SetShaderValueV(lightingPassShader, lightPosArrayLoc, lightPositions,
                                    SHADER_UNIFORM_VEC3, safeLightCount);
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

                        // Draw all 500 big light volumes for additive blending
                        DrawMeshInstanced(lightningSphereMesh, lightVolumeMaterial,
                                          masterLightVolumeTransforms.data(), activeLightCount);
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

            // Text overlay
            DrawText("Standard HyDra Scene", 10, 10, fontSize / 2, RAYWHITE);
            DrawText(std::format("Render-Mode: {}", useDeferredLightVolumes
                                                        ? "Deferred (Multi-Pass with Light Volumes)"
                                                        : "Deferred (Uber-Shader)")
                         .c_str(),
                     10, 40, fontSize / 2, RAYWHITE);
            DrawText(std::format("Light-Count: {}",
                                 useDeferredLightVolumes ? activeLightCount : safeLightCount)
                         .c_str(),
                     10, 70, fontSize / 2, RAYWHITE);
            DrawText(
                std::format("Obstacle-Count: {} / {} (Culled)", actualVisibleCount, obstacleCount)
                    .c_str(),
                10, 100, fontSize / 2, RAYWHITE);

            // Draw UI
            DrawFPS(static_cast<int>(currentWidth) - 100, 10);
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