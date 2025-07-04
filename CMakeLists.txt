cmake_minimum_required(VERSION 3.15)
project(CGR-Engine CXX) # Add CXX to specify language

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # Useful for static analysis tools

# Enable folders in IDEs like CLion
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Detect and include vcpkg toolchain automatically (good fallback!)
# This block should ideally be near the top, after project()
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

# Define executable
add_executable(CGR-Engine ${SOURCES})

# Add include directory for your own headers, tied to the executable
target_include_directories(CGR-Engine PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Find packages (Vcpkg should provide these configs)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

# Manual include path for stb (no config file is available from vcpkg for header-only)
find_path(STB_INCLUDE_DIRS "stb.h")
if(NOT STB_INCLUDE_DIRS)
    message(FATAL_ERROR "stb.h not found. Make sure stb is installed via vcpkg.")
endif()
target_include_directories(CGR-Engine PRIVATE ${STB_INCLUDE_DIRS})

# Link libraries
target_link_libraries(CGR-Engine
        PRIVATE
        glfw
        glad::glad
        OpenGL::GL
        glm
)
