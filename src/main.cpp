#include "../include/Config.h"
#include "algorithm"
#include "filesystem"
#include "format"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "vector"

// global constants
inline constexpr std::string_view obstacleTextureName         = "starry-galaxy_tex.png";
inline constexpr std::string_view woodTextureName             = "wood_tex.png";
inline constexpr std::string_view geometryVertexShaderName    = "geometry_pass.vert";
inline constexpr std::string_view geometryFragmentShaderName  = "geometry_pass.frag";
inline constexpr std::string_view lightingFragmentShaderName  = "lighting_pass.frag";
inline constexpr std::string_view postFragmentShaderName      = "postprocess.frag";
inline constexpr std::string_view instancedVertexShaderName   = "geometry_pass_instanced.vert";
inline constexpr std::string_view instancedFragmentShaderName = "geometry_pass_instanced.frag";
inline constexpr float modelScalar                            = 1.0f;
inline constexpr float modelRotation                          = 1.5f;
inline constexpr int cameraMode                               = CAMERA_ORBITAL;
inline constexpr int obstacleCount                            = 1000;
inline constexpr float ObjectSphereRadius                     = 100.0f; // obstacle cloud size
inline constexpr int activeLightCount                         = 250;
inline constexpr float minLightThreshold                      = 0.03f;
inline constexpr float specularStrength                       = 1.0f;
inline constexpr int generalMaterialShininess                 = 48;
inline constexpr float attenuationConstant                    = 1.0f;
inline constexpr float attenuationLinear                      = 0.09f;
inline constexpr float attenuationQuadratic                   = 0.032f;
inline constexpr uint numberOfStyles                          = 4;

// global variables
Vector3 lightPositions[activeLightCount];
Vector3 lightColors[activeLightCount];
std::vector<Matrix> instancedTransforms;
std::vector<float> instancedStyleIds;
std::vector<Matrix> instancedLightTransforms;
std::vector<Vector3> occupiedLightPositions;
bool setFrameLimit          = true;
bool useMultipleLightColors = false;
bool enableStyleSplit       = true;

int main() {
    // BASIC SETUP
    // -------------------------------------------------------------------------------------------

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

    const Shader geometryPassShader =
        LoadShader(geometryVertPath.c_str(), geometryFragPath.c_str());
    const Shader instancedShader = LoadShader(instancedVertPath.c_str(), instancedFragPath.c_str());
    const Shader lightingPassShader = LoadShader(nullptr, lightingFragPath.c_str());
    const Shader postShader         = LoadShader(nullptr, postFragPath.c_str());

    // OBJECT GENERATION
    // -------------------------------------------------------------------------------------------

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

    // reserve obstacle and light mesh arrays
    instancedTransforms.reserve(obstacleCount);
    instancedStyleIds.reserve(obstacleCount);
    instancedLightTransforms.reserve(activeLightCount);
    occupiedLightPositions.reserve(activeLightCount);

    // Obstacle generation
    for (int i = 0; i < obstacleCount; i++) {
        float scale = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;

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

        // Push directly to contiguous arrays
        instancedTransforms.push_back(transform);
        instancedStyleIds.push_back(styleId);
    }

    int actualObstacleCount = static_cast<int>(std::ssize(instancedTransforms));

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
            // set color and position
            lightColors[lightsGenerated]    = {Config::EngineSettings::MainLightColor.x / 255.0f,
                                               Config::EngineSettings::MainLightColor.y / 255.0f,
                                               Config::EngineSettings::MainLightColor.z / 255.0f};
            lightPositions[lightsGenerated] = validPos;

            // Generate physical debug sphere transform for DrawMeshInstanced
            Matrix transform = MatrixMultiply(MatrixScale(1.5f, 1.5f, 1.5f),
                                              MatrixTranslate(validPos.x, validPos.y, validPos.z));

            instancedLightTransforms.push_back(transform);
            occupiedLightPositions.push_back(validPos);
            lightsGenerated++;
        }
    }

    // INSTANCING: VAO & VBO SETUP
    // -------------------------------------------------------------------------------------------

    // Tell Raylib where the instance matrix attribute is located
    instancedShader.locs[SHADER_LOC_MATRIX_MODEL] =
        GetShaderLocationAttrib(instancedShader, "instanceTransform");

    // Upload the Style ID array to the GPU V-RAM as a VBO
    unsigned int styleIdVboId = rlLoadVertexBuffer(
        instancedStyleIds.data(), static_cast<int>(actualObstacleCount * sizeof(float)), false);

    // Bind VBO to the baseMesh's VAO
    rlEnableVertexArray(baseMesh.vaoId);
    {
        rlEnableVertexBuffer(styleIdVboId);

        // Define data format
        rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(10);

        // attribute advances per instance
        rlSetVertexAttributeDivisor(10, 1);

        rlDisableVertexBuffer();
    }
    rlDisableVertexArray();

    // Set up the shared Material for the instanced draw call
    Material instancedMaterial                          = LoadMaterialDefault();
    instancedMaterial.shader                            = instancedShader;
    instancedMaterial.maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

    // Light VBO Setup
    std::vector instancedLightStyleIds(activeLightCount, 0.0f); // 0.0f = Debug style
    unsigned int lightStyleIdVboId =
        rlLoadVertexBuffer(instancedLightStyleIds.data(), activeLightCount * sizeof(float), false);

    rlEnableVertexArray(lightningSphereMesh.vaoId);
    {
        rlEnableVertexBuffer(lightStyleIdVboId);
        rlSetVertexAttribute(10, 1, RL_FLOAT, false, 0, 0);
        rlEnableVertexAttribute(10);
        rlSetVertexAttributeDivisor(10, 1);
        rlDisableVertexBuffer();
    }
    rlDisableVertexArray();

    // Set up the shared Material for the instanced draw call
    Material instancedLightMaterial = LoadMaterialDefault();
    instancedLightMaterial.shader   = instancedShader;

    // G-BUFFER INITIALIZATION
    // -------------------------------------------------------------------------------------------

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

    // Canvas for Pass 2 (Lighting) to draw onto for post-processing effects
    RenderTexture2D litSceneTarget = LoadRenderTexture(renderWidth, renderHeight);

    // SHADER LOCATION AND UNIFORM SETUP
    // -------------------------------------------------------------------------------------------

    // define uniform location variables (geometry vertex)
    int modelMatLoc  = GetShaderLocation(geometryPassShader, "modelMat");
    int normalMatLoc = GetShaderLocation(geometryPassShader, "normalMat");

    // define uniform locations variable (geometry fragment)
    int styleIdLoc = GetShaderLocation(geometryPassShader, "styleId");

    // define uniform location variables (instance geometry shaders)
    int isLightLocInstanced = GetShaderLocation(instancedShader, "isLightSource");

    // define uniform location variables (lighting fragment)
    int lightPosArrayLoc      = GetShaderLocation(lightingPassShader, "lightPositions");
    int lightColorArrayLoc    = GetShaderLocation(lightingPassShader, "lightColors");
    int LightingResolutionLoc = GetShaderLocation(lightingPassShader, "resolution");
    int normalTexLoc          = GetShaderLocation(lightingPassShader, "gNormalTex");
    int depthTexLoc =
        GetShaderLocation(lightingPassShader, "gDepthTex"); // Replaced Position with Depth
    int invViewProjLoc = GetShaderLocation(lightingPassShader, "invViewProj"); // New matrix uniform
    int postViewPosLoc = GetShaderLocation(lightingPassShader, "viewPos");
    int postBackgroundColorLoc  = GetShaderLocation(lightingPassShader, "backgroundColor");
    int intensityLoc            = GetShaderLocation(lightingPassShader, "lightIntensity");
    int ambientLoc              = GetShaderLocation(lightingPassShader, "ambientLightStrength");
    int specularLoc             = GetShaderLocation(lightingPassShader, "specularStrength");
    int shininessLoc            = GetShaderLocation(lightingPassShader, "shininess");
    int activeLightsLoc         = GetShaderLocation(lightingPassShader, "activeLights");
    int maxLightRadiusLoc       = GetShaderLocation(lightingPassShader, "maxLightRadius");
    int attenuationConstantLoc  = GetShaderLocation(lightingPassShader, "attenuationConstant");
    int attenuationLinearLoc    = GetShaderLocation(lightingPassShader, "attenuationLinear");
    int attenuationQuadraticLoc = GetShaderLocation(lightingPassShader, "attenuationQuadratic");

    // define uniform location variables (post-process fragment)
    int postResolutionLoc = GetShaderLocation(postShader, "resolution");

    // calculate maximum lightRadius based on light intensity to safe shader calcs
    constexpr float c =
        attenuationConstant - (Config::EngineSettings::LightIntensity / minLightThreshold);
    const float maxRadius = (-attenuationLinear + std::sqrt(attenuationLinear * attenuationLinear -
                                                            4.0f * attenuationQuadratic * c)) /
                            (2.0f * attenuationQuadratic);

    constexpr Vector3 BackgroundColor = {Config::EngineSettings::BackgroundColor.x / 255.0f,
                                         Config::EngineSettings::BackgroundColor.y / 255.0f,
                                         Config::EngineSettings::BackgroundColor.z / 255.0f};

    // set static shader values for postProcessingShader
    SetShaderValue(lightingPassShader, postBackgroundColorLoc, &BackgroundColor,
                   SHADER_UNIFORM_VEC3);
    int safeLightCount = std::min(activeLightCount, 500);
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
    SetShaderValue(lightingPassShader, specularLoc, &specularStrength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(lightingPassShader, shininessLoc, &generalMaterialShininess, SHADER_UNIFORM_INT);

    // MAIN GAME LOOP
    // -------------------------------------------------------------------------------------------

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

        // Draw
        BeginDrawing();
        {
            // GEOMETRY PASS
            // -----------------------------------------------------------------------------------

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

                // Draw Lights using instanced shader
                int isLight = 1;
                SetShaderValue(instancedShader, isLightLocInstanced, &isLight, SHADER_UNIFORM_INT);
                DrawMeshInstanced(lightningSphereMesh, instancedLightMaterial,
                                  instancedLightTransforms.data(), activeLightCount);

                // draw obstacles using instanced shader
                isLight = 0;
                SetShaderValue(instancedShader, isLightLocInstanced, &isLight, SHADER_UNIFORM_INT);
                DrawMeshInstanced(baseMesh, instancedMaterial, instancedTransforms.data(),
                                  actualObstacleCount);
            }
            EndMode3D(); // stop applying 3D math

            // Restore GPU output back to the default screen buffer
            rlEnableColorBlend();
            rlDisableFramebuffer();
            rlViewport(
                0, 0, static_cast<int>(currentWidth),
                static_cast<int>(currentHeight)); // Restore the viewport to actual window size

            // LIGHTING PASS
            // --------------------------------------------------------------------------------

            // Calculate the Inverse View-Projection Matrix
            double aspect   = static_cast<double>(renderWidth) / static_cast<double>(renderHeight);
            Matrix proj     = MatrixPerspective(camera.fovy * DEG2RAD, aspect, 0.01, 1000.0);
            Matrix view     = GetCameraMatrix(camera);
            Matrix viewProj = MatrixMultiply(view, proj);
            Matrix invViewProj = MatrixInvert(viewProj);

            BeginTextureMode(litSceneTarget);
            ClearBackground(BLANK);
            BeginShaderMode(lightingPassShader);
            {
                // Pass lighting arrays, camera and screen position
                SetShaderValue(lightingPassShader, LightingResolutionLoc, internalRes,
                               SHADER_UNIFORM_VEC2);
                SetShaderValueV(lightingPassShader, lightPosArrayLoc, lightPositions,
                                SHADER_UNIFORM_VEC3, activeLightCount);
                SetShaderValueV(lightingPassShader, lightColorArrayLoc, lightColors,
                                SHADER_UNIFORM_VEC3, activeLightCount);
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

            // POST-PROCESSING PASS
            // -----------------------------------------------------------------------------------

            ClearBackground(BLACK);

            BeginShaderMode(postShader);
            {
                // Pass Uniforms
                SetShaderValue(postShader, postResolutionLoc, dynamicRes, SHADER_UNIFORM_VEC2);

                // Draw finished Scene into post-Processing shader
                DrawTexturePro(litSceneTarget.texture, sourceRec, screenDestRec, {0, 0}, 0.0f,
                               WHITE);
            }
            EndShaderMode();

            // Text overlay
            DrawText(std::format("Light-Count: {}", activeLightCount).c_str(), 10, 40, fontSize / 2,
                     RAYWHITE);
            DrawText(std::format("Obstacle-Count: {}", obstacleCount).c_str(), 10, 70, fontSize / 2,
                     RAYWHITE);

            // Draw UI
            DrawFPS(static_cast<int>(currentWidth) - 100, 10);
        }
        EndDrawing();
    }

    // CLEANUP
    // -------------------------------------------------------------------------------------------

    // Unload Custom VBOs from GPU Memory
    rlUnloadVertexBuffer(styleIdVboId);
    rlUnloadVertexBuffer(lightStyleIdVboId);

    rlUnloadFramebuffer(FboId);
    rlUnloadTexture(albedoTexId);
    rlUnloadTexture(normalTexId);
    rlUnloadTexture(depthTexId);
    UnloadRenderTexture(litSceneTarget);

    // Unload Shaders
    UnloadShader(geometryPassShader);
    UnloadShader(instancedShader);
    UnloadShader(lightingPassShader);
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