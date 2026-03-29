cmake_minimum_required(VERSION 3.14)
project(fastWAD VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure static MSVC runtime for portability on Windows
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

# Fetch STB libraries for image I/O and resizing
include(FetchContent)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)
FetchContent_MakeAvailable(stb)

# Core Library
add_library(fastwad_core
    src/image_processor.cpp
    src/wad_archive.cpp
    src/utils.cpp
)
target_include_directories(fastwad_core PUBLIC src ${stb_SOURCE_DIR})

# Main Executable
add_executable(fastwad src/main.cpp src/cli.hpp src/config.hpp)
target_link_libraries(fastwad PRIVATE fastwad_core)

# Tests
enable_testing()
add_executable(fastwad_tests src/tests.cpp src/cli.hpp src/config.hpp)
target_link_libraries(fastwad_tests PRIVATE fastwad_core)
add_test(NAME CoreLogicTest COMMAND fastwad_tests)