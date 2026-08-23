cmake_minimum_required(VERSION 3.14)
project(fastwad VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler Warnings & Definitions
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/W4 /WX- /utf-8)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-missing-field-initializers)
    find_package(Threads REQUIRED)
endif()

# Core Library (Embeddable C++ SDK)
add_library(fastwad_core
    src/image_processor.cpp
    src/wad_archive.cpp
    src/utils.cpp
)
target_include_directories(fastwad_core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(fastwad_core SYSTEM PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb
)
if(NOT MSVC)
    target_link_libraries(fastwad_core PUBLIC Threads::Threads)
endif()

# Main Executable CLI
add_executable(fastwad 
    src/main.cpp 
    src/cli.hpp 
    src/config.hpp
)
target_link_libraries(fastwad PRIVATE fastwad_core)

# Tests & Showcase Generator
enable_testing()
add_executable(fastwad_tests 
    src/tests.cpp 
    src/cli.hpp 
    src/config.hpp
)
target_link_libraries(fastwad_tests PRIVATE fastwad_core)
add_test(NAME FastwadE2ETest COMMAND fastwad_tests)