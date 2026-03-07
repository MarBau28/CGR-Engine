#include "../include/Config.h"
#include "filesystem"
#include "glad/glad.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

// global constants
constexpr std::string_view SquareTexture              = "square_tex.png";
constexpr std::string_view DefaultVertShaderName      = "default.vert";
constexpr std::string_view DefaultFragShaderName      = "default.frag";
constexpr std::string_view PostProcessFragShaderName  = "postprocess.frag";
constexpr std::string_view lucyMeshFileName           = "stanford_lucy.glb";
constexpr std::string_view stanfordDragonMeshFileName = "stanford_dragon.glb";
constexpr Vector3 objectPosition                      = {0.0f, 1.0f, 0.0f};
constexpr float modelScalar                           = 1.0f;
constexpr float modelRotation                         = 1.5f;
constexpr int cameraMode                              = CAMERA_ORBITAL;

// global variables
Vector3 lightPos = {1.5f, 2.5f, -2.5f};

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

    // main testing Model
    Mesh simpleGeoMesh = GenMeshCube(2.0f, 2.0f, 2.0f);
    // Mesh simpleGeoMesh = GenMeshKnot(1.0f, 2.0f, 128, 32);
    // Mesh simpleGeoMesh = GenMeshSphere(1.0f, 32, 32);
    // Mesh simpleGeoMesh = GenMeshCylinder(1.0f, 2.0f, 32);
    // Mesh simpleGeoMesh = GenMeshCone(1.0f, 2.0f, 32);
    // Mesh simpleGeoMesh = GenMeshHemiSphere(1.5f, 32, 32);
    // Mesh simpleGeoMesh = GenMeshTorus(0.5f, 2.0f, 16, 32);
    GenMeshTangents(&simpleGeoMesh);
    Model mainModel = LoadModelFromMesh(simpleGeoMesh);
    // const std::string complexMesh =
    //     std::string(Config::Paths::Models) + std::string(lucyMeshFileName);
    // const std::string complexMesh =
    //     std::string(Config::Paths::Models) + std::string(stanfordDragonMeshFileName);
    // Model mainModel = LoadModel(complexMesh.c_str());

    // scale, rotate model
    Matrix scaleMat     = MatrixScale(modelScalar, modelScalar, modelScalar); // scale
    Matrix rotMat       = MatrixRotateY(PI / modelRotation);                  // rotation
    Matrix transformFix = MatrixMultiply(scaleMat, rotMat);
    mainModel.transform = MatrixMultiply(transformFix, mainModel.transform);

    // light mesh (small sphere)
    const Mesh lightningSphere       = GenMeshSphere(0.5f, 16, 16);
    const Model lightningSourceModel = LoadModelFromMesh(lightningSphere);

    // Load texture and assign it to the model's default material
    const std::string texturePath =
        std::string(Config::Paths::Textures) + std::string(SquareTexture);
    const Texture2D defaultTexture = LoadTexture(texturePath.c_str());

    // Load custom Shaders
    const std::string vertPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultVertShaderName);
    const std::string fragPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultFragShaderName);
    const std::string postFragPath =
        std::string(Config::Paths::Shaders) + std::string(PostProcessFragShaderName);

    // Linking Shaders
    const Shader postShader = LoadShader(nullptr, postFragPath.c_str());
    const Shader pbrShader  = LoadShader(vertPath.c_str(), fragPath.c_str());

    // attach shaders to model
    for (int i = 0; i < mainModel.materialCount; i++) {
        mainModel.materials[i].shader                            = pbrShader;
        mainModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = defaultTexture;
        mainModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color   = WHITE;
    }

    // define uniform location variables (vertex)
    int modelMatShaderLoc  = GetShaderLocation(pbrShader, "modelMat");
    int normalMatShaderLoc = GetShaderLocation(pbrShader, "normalMat");

    // define uniform location variables (fragment)
    int lightPosShaderLoc = GetShaderLocation(pbrShader, "lightPos");
    int viewPosShaderLoc  = GetShaderLocation(pbrShader, "viewPos");

    // define uniform location variables (post processing fragment)
    int resolutionShaderLoc = GetShaderLocation(postShader, "resolution");

    // Generate the Framebuffer ID
    unsigned int customFboId = rlLoadFramebuffer();
    if (customFboId == 0) {
        TraceLog(LOG_ERROR, "Failed to create custom FBO");
    }

    // Generate the Color and Depth Attachment Texture in GPU memory for FBO
    unsigned int colorTexId =
        rlLoadTexture(nullptr, screenWidth, screenHeight, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    unsigned int depthTexId = rlLoadTextureDepth(screenWidth, screenHeight, true);

    // Bind the FBO and attach the textures
    rlEnableFramebuffer(customFboId);
    rlFramebufferAttach(customFboId, colorTexId, RL_ATTACHMENT_COLOR_CHANNEL0,
                        RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(customFboId, depthTexId, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER,
                        0);

    // Verify FBO completeness and unbind to return to the default screen buffer
    if (!rlFramebufferComplete(customFboId)) {
        TraceLog(LOG_ERROR, "Custom FBO is incomplete!");
    }
    rlDisableFramebuffer();

    // Wrap raw OpenGL color texture ID in Raylib Texture2D struct for FBO
    Texture2D fboOutputTexture = {colorTexId, screenWidth, screenHeight, 1,
                                  PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};

    // Game Loop
    while (!WindowShouldClose()) {
        // Update
        UpdateCamera(&camera, cameraMode);
        auto currentWidth  = static_cast<float>(GetScreenWidth());
        auto currentHeight = static_cast<float>(GetScreenHeight());

        // move light a bit
        double time = GetTime();
        lightPos.x  = cosf(static_cast<float>(time)) * 3.0f;
        lightPos.z  = sinf(static_cast<float>(time)) * 3.0f;

        // Draw
        BeginDrawing();
        {
            // FBO Off-Screen rendering
            rlEnableFramebuffer(customFboId); // Redirect GPU output to custom FBO
            ClearBackground(Config::EngineSettings::BackgroundColor); // Background color
            rlClearScreenBuffers(); // Manually clear the FBOs color and depth buffers

            // apply Camera's MVP matrices
            BeginMode3D(camera);
            {
                // generate the normal matrices for normal transform to world space
                Matrix modelMatrix  = mainModel.transform;
                Matrix normalMatrix = MatrixTranspose(MatrixInvert(modelMatrix));

                // Set vertex uniforms
                SetShaderValueMatrix(pbrShader, modelMatShaderLoc, modelMatrix);
                SetShaderValueMatrix(pbrShader, normalMatShaderLoc, normalMatrix);

                // Set fragment uniforms
                SetShaderValue(pbrShader, lightPosShaderLoc, &lightPos, SHADER_UNIFORM_VEC3);
                SetShaderValue(pbrShader, viewPosShaderLoc, &camera.position, SHADER_UNIFORM_VEC3);

                // Draw the model
                rlDisableBackfaceCulling();
                DrawModel(mainModel, objectPosition, 1.0f, WHITE);
                rlEnableBackfaceCulling();
                DrawModel(lightningSourceModel, lightPos, 1.0f, WHITE);

                // debug grid for floor
                DrawGrid(100, 1.0f);
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

            // Enable Post-processing fragment Shader
            BeginShaderMode(postShader);
            {
                // Draw FBO texture to screen
                DrawTexturePro(fboOutputTexture, sourceRec, destRec, {0, 0}, 0.0f, WHITE);
            }
            // Disable Post-processing fragment Shader
            EndShaderMode();

            // Draw UI
            DrawFPS(10, 10);
        }
        EndDrawing();
    }

    // cleanup
    rlUnloadFramebuffer(customFboId);
    rlUnloadTexture(colorTexId);
    UnloadShader(pbrShader);
    UnloadShader(postShader);
    UnloadTexture(defaultTexture);
    UnloadModel(mainModel);
    UnloadModel(lightningSourceModel);
    glDeleteRenderbuffers(1, &depthTexId);

    CloseWindow();

    return 0;
}