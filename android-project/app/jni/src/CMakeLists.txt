cmake_minimum_required(VERSION 3.6)

project(MY_APP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

#add_compile_definitions(EDITOR)
add_compile_definitions(DEVELOPER)

set(SolutionDir ../../../..)

include_directories(${PROJECT_NAME} ${SolutionDir}/external/glad/include)
include_directories(${PROJECT_NAME} ${SolutionDir}/external/glm/include)
include_directories(${PROJECT_NAME} ${SolutionDir}/external/stb/include)

find_library(SDL2 SDL2)
find_library(SDL2_mixer SDL2_mixer)

add_library(main SHARED)

target_compile_options(main PRIVATE -Wno-c++11-narrowing)

set(SourceDir ${SolutionDir}/CppSonic2/src)

target_sources(main PRIVATE
        ${SourceDir}/renderer.cpp
        ${SourceDir}/font.cpp
        ${SourceDir}/game.cpp
        ${SourceDir}/main.cpp
        ${SourceDir}/package.cpp
        ${SourceDir}/stdafx.cpp
        ${SourceDir}/texture.cpp
        ${SourceDir}/window_creation.cpp
        ${SourceDir}/imgui_glue.cpp
        ${SourceDir}/editor.cpp
        ${SourceDir}/console.cpp
        ${SourceDir}/single_header.cpp
        ${SourceDir}/assets.cpp
        ${SourceDir}/sprite.cpp
        ${SourceDir}/particle_system.cpp
        ${SourceDir}/sound_mixer.cpp
        ${SourceDir}/main_menu.cpp
        ${SourceDir}/program.cpp
        ${SourceDir}/title_screen.cpp
        )

target_link_libraries(main SDL2 SDL2_mixer)


