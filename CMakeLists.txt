cmake_minimum_required(VERSION 3.15)

# Vcpkg integration — must be set before project(), which loads the toolchain
if (DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
            CACHE STRING "Vcpkg toolchain file")
endif ()

# Copy dependent vcpkg DLLs into bin/ during install so CPack ZIPs are self-contained
set(X_VCPKG_APPLOCAL_DEPS_INSTALL ON)

project(CGR-Engine CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enable folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Gather source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/*.h
)

# Define executable
add_executable(CGR-Engine ${SOURCES}
        include/Config.h
        include/InputHandler.h
        src/Profilers.cpp
        include/Profilers.h
        src/CsvTelemetryWriter.cpp
        include/CsvTelemetryWriter.h
        src/CameraController.cpp
        include/CameraController.h
        src/BenchmarkOrchestrator.cpp
        include/BenchmarkOrchestrator.h
        src/SceneManager.cpp
        include/SceneManager.h
        include/EngineState.h
        src/TelemetryDashboard.cpp
        include/TelemetryDashboard.h
        src/InputController.cpp
        include/InputController.h
        src/RenderContext.cpp
        include/RenderContext.h)

# set output name to HyDra
set_target_properties(CGR-Engine PROPERTIES OUTPUT_NAME "HyDra")

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

# Force CMake to find the absolute path for raylib because vcpkg config is defective.
# Resolve per-configuration: a bare find_library returns the debug lib on
# multi-config generators, which mixes CRTs on MSVC.
find_library(RAYLIB_LIB_RELEASE NAMES raylib
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib" NO_DEFAULT_PATH)
find_library(RAYLIB_LIB_DEBUG NAMES raylib
        PATHS "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib" NO_DEFAULT_PATH)
if (NOT RAYLIB_LIB_RELEASE AND NOT RAYLIB_LIB_DEBUG)
    message(FATAL_ERROR "raylib library not found in vcpkg installed tree")
endif ()
if (NOT RAYLIB_LIB_DEBUG)
    set(RAYLIB_LIB_DEBUG "${RAYLIB_LIB_RELEASE}")
endif ()
if (NOT RAYLIB_LIB_RELEASE)
    set(RAYLIB_LIB_RELEASE "${RAYLIB_LIB_DEBUG}")
endif ()
target_include_directories(CGR-Engine PRIVATE ${raylib_INCLUDE_DIRS})

# link packages
# glad must precede raylib: raylib's DLL re-exports glad symbols, and the linker
# has to resolve ours from the static glad or the two definitions collide (LNK2005)
target_link_libraries(CGR-Engine PRIVATE
        glad::glad
        optimized "${RAYLIB_LIB_RELEASE}"
        debug "${RAYLIB_LIB_DEBUG}"
        glfw
        OpenGL::GL
        glm::glm
)

# Inject required sanitizer runtimes for Debug builds to satisfy the vcpkg binary.
# GCC/Clang only: MSVC has no UBSan and uses different ASan plumbing.
if (NOT MSVC AND (CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE STREQUAL ""))
    target_compile_options(CGR-Engine PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(CGR-Engine PRIVATE -fsanitize=address -fsanitize=undefined)
endif ()

# DEPLOYMENT & PACKAGING (CPack)
# --------------------------------------------------------------------------------------------------

# Install executable and assets into a portable folder structure
install(TARGETS CGR-Engine DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION .)

# CPack Configuration for System-Agnostic Packaging
set(CPACK_PACKAGE_NAME "HyDra")
set(CPACK_PACKAGE_VENDOR "CGR")
set(CPACK_PACKAGE_VERSION "1.0.0")

# Use ZIP on Windows, TGZ on Linux/macOS
if(WIN32)
    set(CPACK_GENERATOR "ZIP")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

# Include CPack
include(CPack)
