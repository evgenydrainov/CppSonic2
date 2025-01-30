cmake_minimum_required(VERSION 3.6)

project(MY_APP)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

#add_compile_definitions(EDITOR)
add_compile_definitions(DEVELOPER)

include_directories(${PROJECT_NAME} C:/Users/Username/source/repos/CppSonic2/external/glad/include)
include_directories(${PROJECT_NAME} C:/Users/Username/source/repos/CppSonic2/external/glm/include)
include_directories(${PROJECT_NAME} C:/Users/Username/source/repos/CppSonic2/external/stb/include)

find_library(SDL2 SDL2)
find_library(SDL2_mixer SDL2_mixer)

add_library(main SHARED)

target_compile_options(main PRIVATE -Wno-c++11-narrowing)

target_sources(main PRIVATE
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/renderer.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/font.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/game.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/main.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/package.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/stdafx.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/texture.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/window_creation.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/imgui_glue.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/editor.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/console.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/single_header.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/assets.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/sprite.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/particle_system.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/sound_mixer.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/main_menu.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/program.cpp
        C:/Users/Username/source/repos/CppSonic2/CppSonic2/src/title_screen.cpp
        )

target_link_libraries(main SDL2 SDL2_mixer)


