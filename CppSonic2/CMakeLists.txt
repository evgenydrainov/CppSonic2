cmake_minimum_required(VERSION 3.20)
project(CppSonic2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wno-c++11-narrowing")

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# add_compile_definitions(EDITOR)
add_compile_definitions(DEVELOPER)

include_directories(${PROJECT_NAME} ../external/glad/include)
include_directories(${PROJECT_NAME} ../external/glm/include)
include_directories(${PROJECT_NAME} ../external/stb/include)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
pkg_check_modules(FREETYPE REQUIRED freetype2)

MESSAGE(STATUS "INFO: SDL2_INCLUDE_DIRS is ${SDL2_INCLUDE_DIRS}")
MESSAGE(STATUS "INFO: SDL2_MIXER_INCLUDE_DIRS is ${SDL2_MIXER_INCLUDE_DIRS}")
MESSAGE(STATUS "INFO: FREETYPE_INCLUDE_DIRS is ${FREETYPE_INCLUDE_DIRS}")

include_directories(${PROJECT_NAME} ${SDL2_INCLUDE_DIRS})
include_directories(${PROJECT_NAME} ${SDL2_MIXER_INCLUDE_DIRS})
include_directories(${PROJECT_NAME} ${FREETYPE_INCLUDE_DIRS})

set(SOURCES
	src/renderer.cpp
	src/font.cpp
	src/game.cpp
	src/main.cpp
	src/package.cpp
	src/stdafx.cpp
	src/texture.cpp
	src/window_creation.cpp
	src/imgui_glue.cpp
	src/editor.cpp
	src/console.cpp
	src/single_header.cpp
	src/assets.cpp
	src/sprite.cpp
	src/particle_system.cpp
	src/sound_mixer.cpp
	src/main_menu.cpp
	src/program.cpp
	src/title_screen.cpp
	src/imgui/imgui_single_file.cpp
	src/nfd/nfd_zenity.cpp
	src/nfd/nfd_common.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

MESSAGE(STATUS "INFO: SDL2_CFLAGS_OTHER is ${SDL2_CFLAGS_OTHER}")
MESSAGE(STATUS "INFO: SDL2_MIXER_CFLAGS_OTHER is ${SDL2_MIXER_CFLAGS_OTHER}")
MESSAGE(STATUS "INFO: FREETYPE_CFLAGS_OTHER is ${FREETYPE_CFLAGS_OTHER}")

target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_CFLAGS_OTHER})
target_compile_options(${PROJECT_NAME} PRIVATE ${SDL2_MIXER_CFLAGS_OTHER})
target_compile_options(${PROJECT_NAME} PRIVATE ${FREETYPE_CFLAGS_OTHER})

MESSAGE(STATUS "INFO: SDL2_LIBRARIES is ${SDL2_LIBRARIES}")
MESSAGE(STATUS "INFO: SDL2_MIXER_LIBRARIES is ${SDL2_MIXER_LIBRARIES}")
MESSAGE(STATUS "INFO: FREETYPE_LIBRARIES is ${FREETYPE_LIBRARIES}")

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL2_MIXER_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${FREETYPE_LIBRARIES})

target_precompile_headers(${PROJECT_NAME} PRIVATE src/stdafx.h)
