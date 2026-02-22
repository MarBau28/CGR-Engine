#include "../include/Config.h"
#include "filesystem"
#include "raylib.h"

// global constants
constexpr std::string_view TestTexture = "square_tst.png";

int main() {
    // Context & Window Creation
    constexpr int screenWidth  = Config::EngineSettings::ScreenWidth;
    constexpr int screenHeight = Config::EngineSettings::ScreenHeight;
    InitWindow(screenWidth, screenHeight, "HyDra");
    SetTargetFPS(60);

    // Camera Setup
    Camera3D camera   = {0};
    camera.position   = Config::EngineSettings::CameraPosition; // camera spot
    camera.target     = {0.0f, 0.0f, 0.0f};                     // view direction
    camera.up         = {0.0f, 1.0f, 0.0f};                     // up vector
    camera.fovy       = Config::EngineSettings::CameraFOV;      // Field of View
    camera.projection = CAMERA_PERSPECTIVE;                     // Adds 3D depth

    // Geometry and Textures (Replaces VAO, VBO, EBO and texture loading)
    const Mesh cubeMesh = GenMeshCube(2.0f, 2.0f, 2.0f);
    const Model myModel = LoadModelFromMesh(cubeMesh);

    // Load texture and assign it to the model's default material
    const std::string texturePath = std::string(Config::Paths::Textures) + std::string(TestTexture);
    const Texture2D myTexture     = LoadTexture(texturePath.c_str());
    myModel.materials[0].maps[MATERIAL_MAP_ALBEDO].texture = myTexture;

    // Game Loop
    while (!WindowShouldClose()) {
        // Update
        UpdateCamera(&camera, CAMERA_ORBITAL); // slowly spins the camera around the target

        // Draw
        BeginDrawing();
        ClearBackground({18, 33, 43, 255}); // background color

        // apply Camera's MVP matrices
        BeginMode3D(camera);

        // Draws the model at World Position (0,0,0)
        DrawModel(myModel, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);

        // debug grid for floor
        DrawGrid(10, 1.0f);

        EndMode3D(); // stop applying 3D math

        // Draw UI
        DrawFPS(10, 10);

        EndDrawing();
    }

    // cleanup
    UnloadTexture(myTexture);
    UnloadModel(myModel);
    CloseWindow();

    return 0;
}