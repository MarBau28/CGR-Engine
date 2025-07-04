cmake_minimum_required(VERSION 3.15)
project(CGR-Engine)

set(CMAKE_CXX_STANDARD 17)

# Include the vcpkg toolchain
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "")
endif()

# Include your headers
include_directories(include)

# Gather all .cpp and .h files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        src/*.cpp
        include/*.h
)

# Create executable from collected sources
add_executable(CGR-Engine ${SOURCES})

# Link against vcpkg dependencies
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(stb CONFIG REQUIRED)

target_link_libraries(CGR-Engine
        PRIVATE
        glfw
        glad
        OpenGL::GL
        glm::glm
)
