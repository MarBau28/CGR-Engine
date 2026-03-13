#include "../include/Config.h"
#include "algorithm"
#include "filesystem"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "vector"

// Structure to hold static geometry data
struct Obstacle {
    Model model;
    Matrix normalMatrix;
    Vector3 position;
    float boundingRadius;
};

// Tracks physical volume of every Object
struct BoundingVolume {
    Vector3 position;
    float radius;
};

// global constants
inline constexpr std::string_view obstacleTextureName        = "starry-galaxy_tex.png";
inline constexpr std::string_view woodTextureName            = "wood_tex.png";
inline constexpr std::string_view geomitryVertexShaderName   = "geometry_pass.vert";
inline constexpr std::string_view geomitryFragmentShaderName = "geometry_pass.frag";
inline constexpr std::string_view lightingFragmentShaderName = "lighting_pass.frag";
inline constexpr std::string_view postFragmentShaderName     = "postprocess.frag";
inline constexpr float modelScalar                           = 1.0f;
inline constexpr float modelRotation                         = 1.5f;
inline constexpr int cameraMode                              = CAMERA_ORBITAL;
inline constexpr int obstacleCount                           = 500;
inline constexpr float outerRadius                           = 50.0f; // obstacle cloud size
inline constexpr int activeLightCount                        = 50;
inline constexpr float minLightThreshold                     = 0.03f;
inline constexpr float specularStrength                      = 1.0f;
inline constexpr int generalMaterialShininess                = 96;
inline constexpr float attenuationConstant                   = 1.0f;
inline constexpr float attenuationLinear                     = 0.09f;
inline constexpr float attenuationQuadratic                  = 0.032f;
inline constexpr bool useMultipleLightColors                 = false;

// global variables
Vector3 lightPositions[activeLightCount];
Vector3 lightColors[activeLightCount];
Color lightMeshColors[activeLightCount];
Vector3 lightOrbitOffsets[activeLightCount];
std::vector<BoundingVolume> occupiedVolumes;

int main() {
    // BASIC SETUP
    // -------------------------------------------------------------------------------------------

    // Context & Window Creation
    constexpr int screenWidth  = Config::EngineSettings::ScreenWidth;
    constexpr int screenHeight = Config::EngineSettings::ScreenHeight;
    InitWindow(screenWidth, screenHeight, "HyDra");
    SetTargetFPS(Config::EngineSettings::TargetFPS);

    // Camera Setup
    Camera3D camera   = {0};
    camera.position   = Config::EngineSettings::CameraPosition;      // camera spot
    camera.target     = Config::EngineSettings::CameraViewDirection; // view direction
    camera.up         = Config::EngineSettings::CameraOrientation;   // up vector
    camera.fovy       = Config::EngineSettings::CameraFOV;           // Field of View
    camera.projection = CAMERA_PERSPECTIVE;                          // standard 3D-depth projection

    // Load  and link Shaders
    const std::string geomitryVertPath =
        std::string(Config::Paths::Shaders) + std::string(geomitryVertexShaderName);
    const std::string geomitryFragPath =
        std::string(Config::Paths::Shaders) + std::string(geomitryFragmentShaderName);
    const std::string lightingFragPath =
        std::string(Config::Paths::Shaders) + std::string(lightingFragmentShaderName);
    const std::string postFragPath =
        std::string(Config::Paths::Shaders) + std::string(postFragmentShaderName);

    const Shader geomitryPassShader =
        LoadShader(geomitryVertPath.c_str(), geomitryFragPath.c_str());
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
    Mesh floorMesh   = GenMeshPlane(500.0f, 500.0f, 100, 100);
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
        floorModel.materials[i].shader                            = geomitryPassShader;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    // light mesh
    Mesh lightningSphereMesh                 = GenMeshSphere(0.5f, 16, 16);
    Model lightningSourceModel               = LoadModelFromMesh(lightningSphereMesh);
    lightningSourceModel.materials[0].shader = geomitryPassShader;

    // Pre-allocate vectors
    std::vector<Obstacle> obstacles;
    obstacles.reserve(obstacleCount);
    occupiedVolumes.reserve(obstacleCount + activeLightCount);

    // Populate the obstacle array as a cloud of objects
    auto TryFindEmptySpace = [&](float radiusToClear, const float minHeight, const float maxHeight,
                                 Vector3 &outPos) -> bool {
        for (int attempts = 0; attempts < 50; attempts++) {
            const float radius = static_cast<float>(GetRandomValue(0, outerRadius * 10)) / 10.0f;
            const float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
            const float height =
                static_cast<float>(GetRandomValue(static_cast<int>(minHeight) * 10,
                                                  static_cast<int>(maxHeight) * 10)) /
                10.0f;

            Vector3 testPos = {cosf(angle) * radius, height, sinf(angle) * radius};

            // Check collision
            const bool isColliding = std::ranges::any_of(occupiedVolumes, [&](const auto &vol) {
                return Vector3Distance(testPos, vol.position) < (radiusToClear + vol.radius);
            });

            if (!isColliding) {
                outPos = testPos;
                return true;
            }
        }
        return false; // Failed to find space in 50 attempts
    };

    // generate Obstacles
    int overallAttempts = 0;
    while (obstacles.size() < obstacleCount && overallAttempts < 10000) {
        overallAttempts++;

        float scale = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;

        if (Vector3 validPos; TryFindEmptySpace(scale, scale, 20.0f, validPos)) {
            // Build Object
            Obstacle obs{};
            obs.position       = validPos;
            obs.boundingRadius = scale;
            obs.model          = LoadModelFromMesh(baseMesh);

            // Materials
            obs.model.materials[0].shader                            = geomitryPassShader;
            obs.model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

            // Calculate normal matrices
            Vector3 rotAxis = Vector3Normalize({static_cast<float>(GetRandomValue(0, 100)),
                                                static_cast<float>(GetRandomValue(0, 100)),
                                                static_cast<float>(GetRandomValue(0, 100))});

            // apply random transformations
            auto rotAngle = static_cast<float>(GetRandomValue(0, 360));
            obs.model.transform =
                MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale),
                                              MatrixRotate(rotAxis, rotAngle * DEG2RAD)),
                               MatrixTranslate(validPos.x, validPos.y, validPos.z));
            obs.normalMatrix = MatrixTranspose(MatrixInvert(obs.model.transform));

            obstacles.push_back(obs);
            occupiedVolumes.push_back({validPos, scale});
        }
    }

    // Generate Lights
    int lightsGenerated = 0;
    overallAttempts     = 0;
    while (lightsGenerated < activeLightCount && overallAttempts < 10000) {
        overallAttempts++;

        if (Vector3 validPos; TryFindEmptySpace(0.5f, 2.0f, 15.0f, validPos)) {
            // Generate random light colors
            float t = std::clamp(Config::EngineSettings::LightIntensity / 15.0f, 0.0f, 1.0f);
            const auto [x, y, z]         = useMultipleLightColors
                                               ? Vector3{static_cast<float>(GetRandomValue(100, 255)),
                                                 static_cast<float>(GetRandomValue(100, 255)),
                                                 static_cast<float>(GetRandomValue(100, 255))}
                                               : Config::EngineSettings::MainLightColor;
            lightColors[lightsGenerated] = {x / 255.0f, y / 255.0f, z / 255.0f};
            auto lerpColor               = [&](const float channel) {
                return static_cast<unsigned char>(std::lerp(channel, 255.0f, t));
            };
            lightMeshColors[lightsGenerated] = {lerpColor(x), lerpColor(y), lerpColor(z), 255};

            // Store the initial polar coordinates and convert to Cartesian coordinates
            float radius = sqrtf(validPos.x * validPos.x + validPos.z * validPos.z);
            float height = validPos.y;
            float angle  = atan2f(validPos.z, validPos.x);
            lightOrbitOffsets[lightsGenerated] = {radius, height, angle};
            lightPositions[lightsGenerated]    = validPos;
            occupiedVolumes.push_back({validPos, 0.5f});
            lightsGenerated++;
        }
    }

    // G-BUFFER INITIALIZATION
    // -------------------------------------------------------------------------------------------

    unsigned int FboId = rlLoadFramebuffer();
    if (FboId == 0)
        TraceLog(LOG_ERROR, "Failed to create FBO");

    // Allocate Texture Memory block Target 0: Albedo (8-bit RGBA)
    unsigned int albedoTexId =
        rlLoadTexture(nullptr, screenWidth, screenHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    // Allocate Texture Memory blocks Target 1 & 2: Normal and Position (32-bit Float RGBA)
    unsigned int normalTexId =
        rlLoadTexture(nullptr, screenWidth, screenHeight, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    unsigned int positionTexId =
        rlLoadTexture(nullptr, screenWidth, screenHeight, PIXELFORMAT_UNCOMPRESSED_R32G32B32A32, 1);
    // Depth texture for Z-sorting
    unsigned int depthTexId = rlLoadTextureDepth(screenWidth, screenHeight, false);

    // Bind FBO and attach all textures
    rlEnableFramebuffer(FboId);
    rlFramebufferAttach(FboId, albedoTexId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, normalTexId, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, positionTexId, RL_ATTACHMENT_COLOR_CHANNEL2, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, depthTexId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);

    // Verify completeness and unbind
    if (!rlFramebufferComplete(FboId))
        TraceLog(LOG_ERROR, "G-Buffer FBO is incomplete.");
    rlDisableFramebuffer();

    // Wrap texture for use in the lighting pass
    Texture2D gAlbedo   = {albedoTexId, screenWidth, screenHeight, 1,
                           PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D gNormal   = {normalTexId, screenWidth, screenHeight, 1,
                           PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};
    Texture2D gPosition = {positionTexId, screenWidth, screenHeight, 1,
                           PIXELFORMAT_UNCOMPRESSED_R32G32B32A32};

    // Canvas for Pass 2 (Lighting) to draw onto for post-processing effects
    RenderTexture2D litSceneTarget = LoadRenderTexture(screenWidth, screenHeight);

    // SHADER LOCATION AND UNIFORM SETUP
    // -------------------------------------------------------------------------------------------

    // define uniform location variables (geomitry vertex)
    int modelMatLoc  = GetShaderLocation(geomitryPassShader, "modelMat");
    int normalMatLoc = GetShaderLocation(geomitryPassShader, "normalMat");

    // define uniform locations variable (geomitry fragment)
    int isLightLoc = GetShaderLocation(geomitryPassShader, "isLightSource");

    // define uniform location variables (lighting fragment)
    int lightPosArrayLoc        = GetShaderLocation(lightingPassShader, "lightPositions");
    int lightColorArrayLoc      = GetShaderLocation(lightingPassShader, "lightColors");
    int LightingResolutionLoc   = GetShaderLocation(lightingPassShader, "resolution");
    int normalTexLoc            = GetShaderLocation(lightingPassShader, "gNormalTex");
    int positionTexLoc          = GetShaderLocation(lightingPassShader, "gPositionTex");
    int postViewPosLoc          = GetShaderLocation(lightingPassShader, "viewPos");
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
    SetShaderValue(lightingPassShader, activeLightsLoc, &activeLightCount, SHADER_UNIFORM_INT);
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

    while (!WindowShouldClose()) {
        // Update Camera and Screen
        UpdateCamera(&camera, cameraMode);
        auto currentWidth  = static_cast<float>(GetScreenWidth());
        auto currentHeight = static_cast<float>(GetScreenHeight());
        float res[2]       = {currentWidth, currentHeight};

        // Define source rectangle
        const Rectangle sourceRec = {0.0f, 0.0f, static_cast<float>(gAlbedo.width),
                                     -static_cast<float>(gAlbedo.height)};
        const Rectangle destRec   = {0.0f, 0.0f, currentWidth, currentHeight};

        // move the lights
        // double time = GetTime();
        // for (int i = 0; i < activeLightCount; i++) {
        //     // Give each light a slightly different speed based on its index
        //     float speedOffset = static_cast<float>(i) * 0.1f;
        //     float currentAngle =
        //         lightOrbitOffsets[i].z + static_cast<float>(time) * (0.5f + speedOffset);
        //
        //     // X and Z use polar coordinates to orbit. Y uses a sine wave to bob up and down
        //     around. lightPositions[i].x = cosf(currentAngle) * lightOrbitOffsets[i].x;
        //     lightPositions[i].z = sinf(currentAngle) * lightOrbitOffsets[i].x;
        //     lightPositions[i].y =
        //         lightOrbitOffsets[i].y +
        //         sinf(static_cast<float>(time) * 2.0f + static_cast<float>(i)) * 2.0f;
        // }

        // Draw
        BeginDrawing();
        {
            // GEOMETRY PASS
            // -----------------------------------------------------------------------------------

            // FBO Off-Screen rendering
            rlEnableFramebuffer(FboId); // Redirect GPU output to custom FBO
            rlActiveDrawBuffers(3);     // Activate all 3 G-Buffer color attachments simultaneously
            ClearBackground(BLANK);     // Background color
            rlClearScreenBuffers();     // Manually clear the FBOs color and depth buffers

            // Draw models
            BeginMode3D(camera);
            {
                // Reset transform state for un-rotated objects (after obstacles manipulate them)
                SetShaderValueMatrix(geomitryPassShader, modelMatLoc, MatrixIdentity());
                SetShaderValueMatrix(geomitryPassShader, normalMatLoc, MatrixIdentity());

                // Draw static models
                DrawModel(floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

                // Draw Lights
                int isLight = 1;
                SetShaderValue(geomitryPassShader, isLightLoc, &isLight, SHADER_UNIFORM_INT);
                for (int i = 0; i < activeLightCount; i++) {
                    DrawModel(lightningSourceModel, lightPositions[i], 1.5f, lightMeshColors[i]);
                }
                isLight = 0; // reset switch
                SetShaderValue(geomitryPassShader, isLightLoc, &isLight, SHADER_UNIFORM_INT);

                // Draw obstacles
                for (const auto &obs : obstacles) {
                    // Set vertex uniforms with pre-calculated matrices
                    SetShaderValueMatrix(geomitryPassShader, modelMatLoc, obs.model.transform);
                    SetShaderValueMatrix(geomitryPassShader, normalMatLoc, obs.normalMatrix);
                    DrawModel(obs.model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
                }

                // debug grid for floor
                // DrawGrid(100, 1.0f);
            }
            EndMode3D(); // stop applying 3D math

            // Restore GPU output back to the default screen buffer
            rlDisableFramebuffer();

            // LIGHTING PASS
            // -----------------------------------------------------------------------------------

            BeginTextureMode(litSceneTarget);
            ClearBackground(BLANK);
            BeginShaderMode(lightingPassShader);
            {
                // Pass lighting arrays, camera and screen position
                SetShaderValue(lightingPassShader, LightingResolutionLoc, res, SHADER_UNIFORM_VEC2);
                SetShaderValueV(lightingPassShader, lightPosArrayLoc, lightPositions,
                                SHADER_UNIFORM_VEC3, activeLightCount);
                SetShaderValueV(lightingPassShader, lightColorArrayLoc, lightColors,
                                SHADER_UNIFORM_VEC3, activeLightCount);
                SetShaderValue(lightingPassShader, postViewPosLoc, &camera.position,
                               SHADER_UNIFORM_VEC3);

                // Bind G-Buffer textures
                SetShaderValueTexture(lightingPassShader, normalTexLoc, gNormal);
                SetShaderValueTexture(lightingPassShader, positionTexLoc, gPosition);

                // Draw the G-Buffer Albedo to lighting shader
                DrawTexturePro(gAlbedo, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
            }
            EndShaderMode();
            EndTextureMode();

            // POST-PROCESSING PASS
            // -----------------------------------------------------------------------------------

            BeginShaderMode(postShader);
            {
                // Pass Uniforms
                SetShaderValue(postShader, postResolutionLoc, res, SHADER_UNIFORM_VEC2);

                // Draw finished Scene into post-Processing shader
                DrawTexturePro(litSceneTarget.texture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
            }
            EndShaderMode();

            // Draw UI
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // CLEANUP
    // -------------------------------------------------------------------------------------------

    rlUnloadFramebuffer(FboId);
    rlUnloadTexture(albedoTexId);
    rlUnloadTexture(normalTexId);
    rlUnloadTexture(positionTexId);
    rlUnloadTexture(depthTexId);
    UnloadRenderTexture(litSceneTarget);
    UnloadShader(geomitryPassShader);
    UnloadShader(lightingPassShader);
    UnloadShader(postShader);
    for (Obstacle &obs : obstacles) {
        UnloadModel(obs.model);
    }
    UnloadMesh(baseMesh);
    UnloadTexture(obstacleTexture);
    UnloadModel(lightningSourceModel);
    UnloadModel(floorModel);
    UnloadTexture(floorTexture);

    CloseWindow();

    return 0;
}