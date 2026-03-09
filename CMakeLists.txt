cmake_minimum_required(VERSION 3.16)

project(dfa_lexicon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(lexicon
    src/lexicon.cpp
)

target_include_directories(lexicon
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(lexicon
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_executable(lexicon_demo
    examples/main.cpp
)

target_link_libraries(lexicon_demo
    PRIVATE
        lexicon
)
