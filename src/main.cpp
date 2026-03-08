#include "../include/Config.h"
#include "algorithm"
#include "filesystem"
#include "glad/glad.h"
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

// global constants
constexpr std::string_view ObstacleTexture            = "starry-galaxy_tex.png";
constexpr std::string_view WoodTexture                = "wood_tex.png";
constexpr std::string_view DefaultVertShaderName      = "default.vert";
constexpr std::string_view DefaultFragShaderName      = "default.frag";
constexpr std::string_view PostProcessFragShaderName  = "postprocess.frag";
constexpr std::string_view lucyMeshFileName           = "stanford_lucy.glb";
constexpr std::string_view stanfordDragonMeshFileName = "stanford_dragon.glb";
constexpr float modelScalar                           = 1.0f;
constexpr float modelRotation                         = 1.5f;
constexpr int cameraMode                              = CAMERA_ORBITAL;
constexpr int obstacleCount                           = 500;
constexpr float innerRadius                           = 10.0f; // obstacle cloud dimension
constexpr float outerRadius                           = 50.0f; // obstacle cloud dimension

// global variables
Vector3 lightPos = {0.0f, 5.0f, 0.0f};

int main() {
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
    const std::string vertPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultVertShaderName);
    const std::string fragPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultFragShaderName);
    const std::string postFragPath =
        std::string(Config::Paths::Shaders) + std::string(PostProcessFragShaderName);
    const Shader postShader = LoadShader(nullptr, postFragPath.c_str());
    const Shader pbrShader  = LoadShader(vertPath.c_str(), fragPath.c_str());

    // Generate obstacle base Meshes
    Mesh baseMesh = GenMeshCube(0.75f, 0.75f, 0.75f);
    GenMeshTangents(&baseMesh);

    // load default texture
    const std::string texturePath =
        std::string(Config::Paths::Textures) + std::string(ObstacleTexture);
    const Texture2D obstacleTexture = LoadTexture(texturePath.c_str());

    // Populate the obstacle array as a cloud of objects
    std::vector<Obstacle> obstacles;
    int attempts = 0;

    while (obstacles.size() < obstacleCount && attempts < 5000) {
        attempts++;

        // Generate basic physical properties
        float scale = static_cast<float>(GetRandomValue(10, 30)) / 10.0f;
        float radius =
            static_cast<float>(GetRandomValue(innerRadius * 10, outerRadius * 10)) / 10.0f;
        float angle  = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;
        float height = static_cast<float>(GetRandomValue(static_cast<int>(scale) * 10, 200)) /
                       10.0f; // Scale acts as minHeight

        Vector3 testPos = {cosf(angle) * radius, height, sinf(angle) * radius};

        // If generated object hits something, skip to the next attempt
        bool isCollidingWithExistingObject = std::ranges::any_of(obstacles, [&](const auto &obs) {
            return Vector3Distance(testPos, obs.position) < (scale + obs.boundingRadius);
        });
        if (isCollidingWithExistingObject)
            continue;

        // Build the Object
        Obstacle obs{};
        obs.position       = testPos;
        obs.boundingRadius = scale;
        obs.model          = LoadModelFromMesh(baseMesh);

        // Materials
        obs.model.materials[0].shader                            = pbrShader;
        obs.model.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = obstacleTexture;

        // Calculate matrices once
        Vector3 rotAxis = Vector3Normalize({static_cast<float>(GetRandomValue(0, 100)),
                                            static_cast<float>(GetRandomValue(0, 100)),
                                            static_cast<float>(GetRandomValue(0, 100))});
        auto rotAngle   = static_cast<float>(GetRandomValue(0, 360));

        // apply random transformations
        obs.model.transform =
            MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale),
                                          MatrixRotate(rotAxis, rotAngle * DEG2RAD)),
                           MatrixTranslate(testPos.x, testPos.y, testPos.z));
        obs.normalMatrix = MatrixTranspose(MatrixInvert(obs.model.transform));

        obstacles.push_back(obs);
    }

    // floor Mesh Setup
    Mesh floorMesh   = GenMeshPlane(500.0f, 500.0f, 100, 100);
    Model floorModel = LoadModelFromMesh(floorMesh);

    const std::string woodTexPath = std::string(Config::Paths::Textures) + std::string(WoodTexture);
    Texture2D floorTexture        = LoadTexture(woodTexPath.c_str());
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
        floorModel.materials[i].shader                            = pbrShader;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = floorTexture;
        floorModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    // light mesh
    const Mesh lightningSphereMesh   = GenMeshSphere(0.5f, 16, 16);
    const Model lightningSourceModel = LoadModelFromMesh(lightningSphereMesh);

    // define uniform location variables (vertex)
    int modelMatShaderLoc  = GetShaderLocation(pbrShader, "modelMat");
    int normalMatShaderLoc = GetShaderLocation(pbrShader, "normalMat");

    // define uniform location variables (fragment)
    int lightPosShaderLoc = GetShaderLocation(pbrShader, "lightPos");
    int viewPosShaderLoc  = GetShaderLocation(pbrShader, "viewPos");

    // define uniform location variables (post processing fragment)
    int resolutionShaderLoc = GetShaderLocation(postShader, "resolution");

    // Generate the Framebuffer ID
    unsigned int FboId = rlLoadFramebuffer();
    if (FboId == 0) {
        TraceLog(LOG_ERROR, "Failed to create custom FBO");
    }

    // Generate the Color and Depth Attachment Texture in GPU memory for FBO
    unsigned int colorTexId =
        rlLoadTexture(nullptr, screenWidth, screenHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    unsigned int depthTexId = rlLoadTextureDepth(screenWidth, screenHeight, true);

    // Bind the FBO and attach the textures
    rlEnableFramebuffer(FboId);
    rlFramebufferAttach(FboId, colorTexId, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D,
                        0);
    rlFramebufferAttach(FboId, depthTexId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);

    // Verify FBO completeness and unbind to return to the default screen buffer
    if (!rlFramebufferComplete(FboId)) {
        TraceLog(LOG_ERROR, "Custom FBO is incomplete!");
    }
    rlDisableFramebuffer();

    // Wrap raw OpenGL color texture ID in Raylib Texture2D struct for FBO
    Texture2D fboOutputTexture = {colorTexId, screenWidth, screenHeight, 1,
                                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

    // MAIN GAME LOOP
    while (!WindowShouldClose()) {
        // Update
        UpdateCamera(&camera, cameraMode);
        auto currentWidth  = static_cast<float>(GetScreenWidth());
        auto currentHeight = static_cast<float>(GetScreenHeight());

        // move light a bit
        // double time = GetTime();
        // lightPos.x  = cosf(static_cast<float>(time)) * 3.0f;
        // lightPos.z  = sinf(static_cast<float>(time)) * 3.0f;

        // Draw
        BeginDrawing();
        {
            // FBO Off-Screen rendering
            rlEnableFramebuffer(FboId); // Redirect GPU output to custom FBO
            ClearBackground(Config::EngineSettings::BackgroundColor); // Background color
            rlClearScreenBuffers(); // Manually clear the FBOs color and depth buffers

            // apply Camera's MVP matrices
            BeginMode3D(camera);
            {
                // Set fragment uniforms
                SetShaderValue(pbrShader, lightPosShaderLoc, &lightPos, SHADER_UNIFORM_VEC3);
                SetShaderValue(pbrShader, viewPosShaderLoc, &camera.position, SHADER_UNIFORM_VEC3);

                // Reset transform state for un-rotated objects (after obstacles manipulate them)
                SetShaderValueMatrix(pbrShader, modelMatShaderLoc, MatrixIdentity());
                SetShaderValueMatrix(pbrShader, normalMatShaderLoc, MatrixIdentity());

                // Draw static models
                DrawModel(floorModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
                DrawModel(lightningSourceModel, lightPos, 2.0f, WHITE);

                // Draw obstacles
                for (const auto &obs : obstacles) {
                    // Set vertex uniforms with pre-calculated matrices
                    SetShaderValueMatrix(pbrShader, modelMatShaderLoc, obs.model.transform);
                    SetShaderValueMatrix(pbrShader, normalMatShaderLoc, obs.normalMatrix);
                    DrawModel(obs.model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
                }

                // debug grid for floor
                // DrawGrid(100, 1.0f);
            }
            EndMode3D(); // stop applying 3D math

            // Restore GPU output back to the default screen buffer
            rlDisableFramebuffer();

            // Define source rectangle.
            const Rectangle sourceRec = {0.0f, 0.0f, static_cast<float>(fboOutputTexture.width),
                                         -static_cast<float>(fboOutputTexture.height)};
            const Rectangle destRec   = {0.0f, 0.0f, currentWidth, currentHeight};

            // post processing uniform passing
            float res[2] = {currentWidth, currentHeight};
            SetShaderValue(postShader, resolutionShaderLoc, res, SHADER_UNIFORM_VEC2);

            // Post-processing fragment Shader
            BeginShaderMode(postShader);
            {
                // Draw FBO texture to screen
                DrawTexturePro(fboOutputTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
            }
            EndShaderMode();

            // Draw UI
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // cleanup
    rlUnloadFramebuffer(FboId);
    rlUnloadTexture(colorTexId);
    UnloadShader(pbrShader);
    UnloadShader(postShader);
    for (auto &obs : obstacles) {
        UnloadModel(obs.model);
    }
    UnloadMesh(baseMesh);
    UnloadTexture(obstacleTexture);
    UnloadModel(lightningSourceModel);
    UnloadMesh(lightningSphereMesh);
    UnloadModel(floorModel);
    UnloadMesh(floorMesh);
    UnloadTexture(floorTexture);
    glDeleteRenderbuffers(1, &depthTexId);

    CloseWindow();

    return 0;
}