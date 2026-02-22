#include <raylib.h>

int main() {

    // 1. Context & Window Creation
    constexpr int screenWidth = 1920;
    constexpr int screenHeight = 1080;
    InitWindow(screenWidth, screenHeight, "MinimalCGREngine - Raylib");
    SetTargetFPS(60);

    // 2. The Game Loop
    while (!WindowShouldClose()) {

        // 3. Begin Drawing
        // locks the OpenGL state and prepares the frame.
        BeginDrawing();

        // Set a Background color
        ClearBackground({18, 33, 43, 255});

        // geometry and texture drawing will go here

        // 4. End Drawing
        EndDrawing();
    }

    // 5. Cleanup
    CloseWindow();

    return 0;
}