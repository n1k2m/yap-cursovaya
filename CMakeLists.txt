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

# Бенчмарк ботов без окна: отдельный исполняемый файл на каждый размер поля
set(BENCH_SIZES 15 25 50 75 100 150 200 300 CACHE STRING "Размеры поля для бенчмарка")
option(BENCH_PROFILE "Собрать бенчмарк с -pg для gprof" OFF)

foreach(S ${BENCH_SIZES})
    add_executable(bench_${S} bench/bench.c)
    target_compile_definitions(bench_${S} PRIVATE SIZE=${S})
    target_include_directories(bench_${S} PRIVATE ${STB_INCLUDE_DIRS})
    target_link_libraries(bench_${S} PRIVATE glfw opengl32)
    if(BENCH_PROFILE)
        # без встраивания, чтобы gprof показывал время по каждой функции
        target_compile_options(bench_${S} PRIVATE -pg -fno-inline)
        target_link_options(bench_${S} PRIVATE -pg -no-pie -Wl,--disable-dynamicbase)
    endif()
endforeach()
