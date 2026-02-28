#include "../include/Config.h"
#include "filesystem"
#include "raylib.h"
#include "raymath.h"

// global constants
constexpr std::string_view SquareTexture         = "square_tex.png";
constexpr std::string_view DefaultVertShaderName = "default.vert";
constexpr std::string_view DefaultFragShaderName = "default.frag";
constexpr Vector3 lightPos                       = {1.0f, 3.0f, 2.0f};
constexpr Vector3 cubePosition                   = {0.0f, 1.0f, 0.0f};

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

    // Geometry and Textures (Replaces VAO, VBO, EBO and texture loading)
    const Mesh cubeMesh = GenMeshCube(2.0f, 2.0f, 2.0f);
    const Model myModel = LoadModelFromMesh(cubeMesh);

    // light mesh (small cube)
    const Mesh lightningSphere         = GenMeshSphere(0.5f, 16, 16);
    const Model myLightningSourceModel = LoadModelFromMesh(lightningSphere);

    // Load texture and assign it to the model's default material
    const std::string texturePath =
        std::string(Config::Paths::Textures) + std::string(SquareTexture);
    const Texture2D myTexture                              = LoadTexture(texturePath.c_str());
    myModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = myTexture;

    // Load custom Shaders
    const std::string vertPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultVertShaderName);
    const std::string fragPath =
        std::string(Config::Paths::Shaders) + std::string(DefaultFragShaderName);
    const Shader customShader = LoadShader(vertPath.c_str(), fragPath.c_str()); // Linking Shaders

    // attach shaders to model
    myModel.materials[0].shader = customShader;

    // define uniform location variables
    int modelMatShaderLoc  = GetShaderLocation(customShader, "modelMat");
    int normalMatShaderLoc = GetShaderLocation(customShader, "normalMat");
    int lightPosShaderLoc  = GetShaderLocation(customShader, "lightPos");
    int viewPosShaderLoc   = GetShaderLocation(customShader, "viewPos");

    // Game Loop
    while (!WindowShouldClose()) {
        // Update
        UpdateCamera(&camera, CAMERA_ORBITAL);

        // Draw
        BeginDrawing();
        ClearBackground(Config::EngineSettings::BackgroundColor);

        // apply Camera's MVP matrices
        BeginMode3D(camera);

        // generate the normal matrices for normal transform to world space
        Matrix modelMatrix  = myModel.transform;
        Matrix normalMatrix = MatrixTranspose(MatrixInvert(modelMatrix));

        // Set vertex uniforms
        SetShaderValueMatrix(customShader, modelMatShaderLoc, modelMatrix);
        SetShaderValueMatrix(customShader, normalMatShaderLoc, normalMatrix);

        // Set fragment uniforms
        SetShaderValue(customShader, lightPosShaderLoc, &lightPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(customShader, viewPosShaderLoc, &camera.position, SHADER_UNIFORM_VEC3);

        // Draws the model at World Position (0,0,0)
        DrawModel(myModel, cubePosition, 1.0f, WHITE);
        DrawModel(myLightningSourceModel, lightPos, 1.0f, WHITE);

        // debug grid for floor
        DrawGrid(100, 1.0f);

        EndMode3D(); // stop applying 3D math

        // Draw UI
        DrawFPS(10, 10);

        EndDrawing();
    }

    // cleanup
    UnloadShader(customShader);
    UnloadTexture(myTexture);
    UnloadModel(myModel);
    CloseWindow();

    return 0;
}