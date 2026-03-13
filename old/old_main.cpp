// #define STB_IMAGE_IMPLEMENTATION
//
// #include "../include/old_main.h"
// #include <GLFW/glfw3.h>
// #include <array>
// #include <filesystem>
// #include <glad/glad.h>
// #include <iostream>
//
// #include "../include/EBO.h"
// #include "../include/Shader.h"
// #include "../include/VAO.h"
// #include "../include/VBO.h"
// #include "../include/old_Texture.h"
// #include "stb_image.h"
//
// // vertex data
// // std::array<GLfloat, 36> triangleVertices = {
// //    -0.5f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
// //     0.5f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
// //     0.0f,  0.5f * float(sqrt(3)) * 2 / 3, 0.0f, 1.0f, 0.6f,  0.32f,
// //    -0.25f, 0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f, 0.17f,
// //     0.25f, 0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f, 0.17f,
// //     0.0f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
// //};
// // std::array<GLuint, 9> triangleIndices = {
// //    0,3,5,
// //    3,2,4,
// //    5,4,1
// //};
//
// std::array<GLfloat, 32> exampleObjectVertices = {
//     // X, Y, Z (Position) | R, G, B (Color) | U, V (Texture)
//     -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, // bottom left
//     -0.5f, 0.5f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, // bottom right
//     0.5f,  0.5f,  0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, // top right
//     0.5f,  -0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, // top left
// };
// std::array<GLuint, 6> exampleObjectIndices = {0, 2, 1, 0, 3, 2};
//
// void error_callback(int error, const char *description) {
//     fprintf(stderr, "Error: %s\n", description);
// }
//
// int main() {
//     glfwSetErrorCallback(error_callback);
//
//     // configure and initialize openGL
//     if (!glfwInit()) {
//         std::cout << "Failed to initialize GLFW" << std::endl;
//         return -1;
//     }
//
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
//     glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
//     glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//
//     // create and configure window
//     auto windowDeleter = [](GLFWwindow *win) {
//         std::cout << "Destroying window..." << std::endl;
//         glfwDestroyWindow(win);
//     };
//     const std::unique_ptr<GLFWwindow, decltype(windowDeleter)> windowPtr(
//         glfwCreateWindow(800, 800, "MinimalCGREngine", nullptr, nullptr), windowDeleter);
//     GLFWwindow *window = windowPtr.get();
//     if (!windowPtr) {
//         std::cout << "Failed to create GLFW window" << std::endl;
//         glfwTerminate();
//         return -1;
//     }
//     glfwMakeContextCurrent(window);
//
//     // initialize GLAD and viewport
//     if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
//         std::cout << "Failed to initialize GLAD" << std::endl;
//         return -1;
//     }
//     glViewport(0, 0, 800, 800);
//
//     // create shader program by specifying the sources
//     const Shader shaderProgram("../shaders/geometry_pass.vert", "../shaders/geometry_pass.frag");
//
//     // create and configure (VOA) buffers (VBO, EBO)
//     const VAO VAO1;
//     VAO1.Bind();
//     const VBO VBO1(exampleObjectVertices.data(), sizeof(exampleObjectVertices));
//     const EBO EBO1(exampleObjectIndices.data(), sizeof(exampleObjectIndices));
//
//     VAO::LinkAttrib(VBO1, 0, 3, GL_FLOAT, 8 * sizeof(float), (void *)nullptr);
//     VAO::LinkAttrib(VBO1, 1, 3, GL_FLOAT, 8 * sizeof(float),
//                     reinterpret_cast<void *>(3 * sizeof(float)));
//     VAO::LinkAttrib(VBO1, 2, 2, GL_FLOAT, 8 * sizeof(float),
//                     reinterpret_cast<void *>(6 * sizeof(float)));
//     VAO::Unbind();
//     VBO::Unbind();
//     EBO::Unbind();
//
//     // uniform for scaling
//     const GLint scalerUniId = glGetUniformLocation(shaderProgram.ID, "scale");
//
//     // Texture
//     const Texture texture("../assets/textures/square_tst.JPG", GL_TEXTURE_2D, GL_TEXTURE0,
//     GL_RGB,
//                           GL_UNSIGNED_BYTE);
//     Texture::TexUnit(shaderProgram, 0);
//
//     // main rendering loop to run poll the buffers and draw on screen
//     while (!glfwWindowShouldClose(window)) {
//         // create background color
//         glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
//         glClear(GL_COLOR_BUFFER_BIT);
//
//         // use shaders to draw geometry / apply the texture
//         shaderProgram.ActivateProgram();
//         glUniform1f(scalerUniId, 0.5f);
//         texture.Bind();
//         VAO1.Bind();
//         glDrawElements(GL_TRIANGLES, sizeof(exampleObjectIndices) / sizeof(int), GL_UNSIGNED_INT,
//                        nullptr);
//         glfwSwapBuffers(window);
//
//         glfwPollEvents();
//     }
//
//     // cleanup Buffers, buffer configuration and shader-program
//     VAO1.Delete();
//     VBO1.Delete();
//     EBO1.Delete();
//     texture.Delete();
//     shaderProgram.DeleteProgram();
//
//     return 0;
// }