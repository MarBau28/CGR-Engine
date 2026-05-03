cmake_minimum_required(VERSION 3.15)
project(CGR-Engine CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Vcpkg integration
if (DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
endif ()

# Gather source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

# Define executable
add_executable(CGR-Engine ${SOURCES}
        include/Config.h
        include/InputHandler.h)

# Include directories
target_include_directories(CGR-Engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find Packages
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(raylib CONFIG REQUIRED)

# Force CMake to find the absolute path for raylib because vcpkg config is defective
find_library(RAYLIB_ABSOLUTE_PATH NAMES raylib REQUIRED)
target_include_directories(CGR-Engine PRIVATE ${raylib_INCLUDE_DIRS})

# link packages
target_link_libraries(CGR-Engine PRIVATE
        ${RAYLIB_ABSOLUTE_PATH}
        glfw
        glad::glad
        OpenGL::GL
        glm::glm
)

# Inject required sanitizer runtimes for Debug builds to satisfy the vcpkg binary
if (CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE STREQUAL "")
    target_compile_options(CGR-Engine PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(CGR-Engine PRIVATE -fsanitize=address -fsanitize=undefined)
endif ()
