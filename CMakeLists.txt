cmake_minimum_required(VERSION 3.1)

# vcpkg: нужно задать до project()
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-static" CACHE STRING "")
endif()

project(yap_cursovaya C)

set(CMAKE_C_STANDARD 23)

find_package(glfw3 CONFIG REQUIRED)
find_path(STB_INCLUDE_DIRS "stb_image.h" REQUIRED)

add_executable(yap_cursovaya main.c)

target_include_directories(yap_cursovaya PRIVATE ${STB_INCLUDE_DIRS})
target_link_libraries(yap_cursovaya PRIVATE glfw opengl32)
