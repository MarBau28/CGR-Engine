# Install script for directory: C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/src/2f4fb681b3-78068df8f5.clean

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/pkgs/glad_x64-windows/debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "OFF")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/glad.lib")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/pkgs/glad_x64-windows/debug/include")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/pkgs/glad_x64-windows/debug" TYPE DIRECTORY FILES "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/include")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/glad" TYPE FILE FILES
    "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/gladConfig.cmake"
    "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/gladConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/glad/gladTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/glad/gladTargets.cmake"
         "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/CMakeFiles/Export/9f6d6e00f39627d63570f0f3437e2805/gladTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/glad/gladTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/glad/gladTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/glad" TYPE FILE FILES "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/CMakeFiles/Export/9f6d6e00f39627d63570f0f3437e2805/gladTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/glad" TYPE FILE FILES "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/CMakeFiles/Export/9f6d6e00f39627d63570f0f3437e2805/gladTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/Users/Marvi/Meine Ablage/Main/Development/Code/VS Code/CGR/Minimal-CGR-Engine/Minimal-CGR-Engine/vcpkg_installed/x64-windows/vcpkg/blds/glad/x64-windows-dbg/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
