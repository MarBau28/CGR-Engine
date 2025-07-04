#define STB_IMAGE_IMPLEMENTATION

#include "../include/main.h"
#include <filesystem>
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <array>

#include "stb_image.h"
#include "../include/Texture.h"
#include "../include/Shader.h"
#include "../include/VBO.h"
#include "../include/VAO.h"
#include "../include/EBO.h"


// vertex data
//std::array<GLfloat, 36> triangleVertices = {
//    -0.5f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
//     0.5f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
//     0.0f,  0.5f * float(sqrt(3)) * 2 / 3, 0.0f, 1.0f, 0.6f,  0.32f,
//    -0.25f, 0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f, 0.17f,
//     0.25f, 0.5f * float(sqrt(3)) / 6,     0.0f, 0.9f, 0.45f, 0.17f,
//     0.0f, -0.5f * float(sqrt(3)) / 3,     0.0f, 0.8f, 0.3f,  0.02f,
//};
//std::array<GLuint, 9> triangleIndices = {
//    0,3,5,
//    3,2,4,
//    5,4,1
//};

std::array<GLfloat, 32> textureVertices = {
    -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f,
     0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 1.0f,  1.0f, 0.0f,
};
std::array<GLuint, 6> textureIndices = {
    0,2,1,
    0,3,2
};

int main() {

    // configure and initialize openGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //create and configure window
    auto windowDeleter = [](GLFWwindow* win)
    {
        glfwDestroyWindow(win);
        glfwTerminate();
    };
    const std::unique_ptr<GLFWwindow, decltype(windowDeleter)> windowPtr
    (
        glfwCreateWindow(800, 800, "MinimalCGREngine", nullptr, nullptr),
        windowDeleter
    );
    GLFWwindow* window = windowPtr.get();
    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // initialize GLAD and viewport
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    glViewport(0, 0, 800, 800);

    //create shader program by specifying the sources
    const Shader shaderProgram("../shaders/default.vert", "../shaders/default.frag");

    // create and configure (VOA) buffers (VBO, EBO)
    const VAO VAO1;
    VAO1.Bind();
    const VBO VBO1(textureVertices.data(), sizeof(textureVertices));
    const EBO EBO1(textureIndices.data(), sizeof(textureIndices));
    VBO1.Bind();
    VAO::LinkAttrib(VBO1, 0, 3, GL_FLOAT, 8 * sizeof(float), (void*)nullptr);
    VAO::LinkAttrib(VBO1, 1, 3, GL_FLOAT, 8 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    VAO::LinkAttrib(VBO1, 2, 2, GL_FLOAT, 8 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    VAO::Unbind();
    VBO::Unbind();
    EBO::Unbind();

    // uniform for scaling
    const GLint uniID = glGetUniformLocation(shaderProgram.ID, "scale");

    // Texture
    const Texture texture("../assets/textures/square_tst.JPG", GL_TEXTURE_2D, GL_TEXTURE0, GL_RGB, GL_UNSIGNED_BYTE);
    Texture::texUnit(shaderProgram, 0);

    // main rendering loop to run poll the buffers and draw on screen
    while (!glfwWindowShouldClose(window))
    {
        // create background color
        glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // use shaders to draw a triangle
        shaderProgram.ActivateProgram();
        glUniform1f(uniID, 0.5f);
        texture.Bind();
        VAO1.Bind();
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
        glfwSwapBuffers(window);

        glfwPollEvents();
    }

    // cleanup Buffers, buffer configuration and shader-program
    VAO1.Delete();
    VBO1.Delete();
    EBO1.Delete();
    texture.Delete();
    shaderProgram.DeleteProgram();

    return 0;
}

//int main() {
//
    //std::array<int, 5> arr = { 1, 2, 3, 4, 5 };
    //for (const int& elem : arr) {
    //    std::cout << elem << " ";  // Output: 1 2 3 4 5
    //}

    //const size_t size {10};
    /*double* temperatures { new double[size] {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0, 100.0} };

    for (double& temp : temperatures) {
        std::cout << "temperature: " << temp << std::endl;
    }*/
    //std::string test { "sfsd" };
    //int test2 { 1 + 2 + 3 };
    //short int test3 { 1 };
    //char test4 { 2 };
    //auto test5 { test3 + test4 };
    //test2 = 55.9;
    /*char message[]{ 'h', 'e', 'l', 'l', 'o' };

    std::cout << message << std::endl;
    std::cout << std::size(message) << std::endl;*/

    /*std::string myValue = "42";

    std::string result = "My value is " + myValue + ".";

    std::cout << result << std::endl;*/

    /*int test{ 10 };
    const int& refTest{ test };
    test = 20;

    std::cout << test << std::endl;
    std::cout << refTest << std::endl;*/

    //int a{ 1 };
    //int b{ 2 };
    //int c{ 3 };
    //int d{ 4 };
    //std::string s1 { "TEST1" };
    //std::string s2 { "TEST2" };

    //int func = [&a, &b](int one, int two) -> int {
    //    return one + two + a + b;
    //}(c, d);

    ////std::cout << func << std::endl;

    ////std::cout << add(a, b) << std::endl;

    //Cylinder cyObject(10, 20);

    //std::cout << cyObject.get_baseRadius() << std::endl;

    //asio::io_context io_context;
    //asio::steady_timer timer(io_context, std::chrono::seconds(3));

    //std::cout << "Starting asynchronous timer for 3 seconds..." << std::endl;

    //// Initiate asynchronous wait, providing a callback function
    //timer.async_wait(timer_callback);

    //std::cout << "Doing other work while waiting for the timer..." << std::endl;
    //// The io_context.run() call below is crucial to start the event loop
    //io_context.run(); // Run the Asio event loop (blocks until io_context is stopped)

    //std::cout << "io_context::run() finished." << std::endl; // Will reach here after timer expires

    // Initialize COM for WinRT (required for C++/WinRT)
    //init_apartment();

    // Run the asynchronous main coroutine and block until it completes
    //run_async_main().get(); // .get() on IAsyncAction blocks until completion

    //std::cout << "Main: Example finished." << std::endl;
//
//    return 0;
//}

//template <typename T>
//concept MyConcept = requires (T a, T b) {
//    a+=1;
//    ++a;
//    a++;
//    a*b;
//    b<=3;
//    requires sizeof(b) <= 3;
//    { a + b } noexcept -> std::convertible_to<int>;
//};
//
//template <typename T>
//concept MyConcept2 = requires (T a, T b) {
//    a += 1;
//    ++a;
//    a++;
//    a* b;
//    b <= 3;
//        requires sizeof(b) <= 3;
//    { a + b } noexcept -> std::convertible_to<int>;
//};
//
//template <typename T>
//requires MyConcept<T> || MyConcept<T>
//T add(T a, T b) {
//    return a + b;
//}
//
//static void timer_callback(const asio::error_code& error) {
//    if (!error) {
//        std::cout << "Asynchronous timer expired!" << std::endl;
//    }
//    else {
//        std::cerr << "Timer error: " << error.message() << std::endl;
//    }
//}
//
//static IAsyncOperation<int> async_calculate_value_coroutine() {
//    std::cout << "Coroutine: Starting asynchronous calculation..." << std::endl;
//    co_await ThreadPool::RunAsync([](IAsyncAction) {
//        std::cout << "Coroutine: Doing work in thread pool..." << std::endl;
//        std::this_thread::sleep_for(seconds(1)); // Simulate work
//        });
//    std::cout << "Coroutine: Calculation complete." << std::endl;
//    co_return 42; // Return the result
//}
//
//// Helper coroutine to wrap main (since main can't directly be a coroutine in standard C++)
//static IAsyncAction run_async_main() {
//    std::cout << "Main: Starting coroutine example..." << std::endl;
//
//    IAsyncOperation<int> async_op =  async_calculate_value_coroutine();
//
//    std::cout << "Main: Doing other work while waiting..." << std::endl;
//    co_await ThreadPool::RunAsync([](IAsyncAction) {
//        std::this_thread::sleep_for(seconds(2)); // Simulate main thread work
//        });
//
//    std::cout << "Main: Awaiting coroutine result..." << std::endl;
//    int result = co_await async_op; // Await the result of the coroutine
//    std::cout << "Main: Coroutine result: " << result << std::endl;
//}