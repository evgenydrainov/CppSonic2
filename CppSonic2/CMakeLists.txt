cmake_minimum_required(VERSION 3.20)
project(CppSonic2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

add_compile_definitions(EDITOR)
add_compile_definitions(DEVELOPER)

include_directories(${PROJECT_NAME} ../external/glad/include)
include_directories(${PROJECT_NAME} ../external/glm/include)
include_directories(${PROJECT_NAME} ../external/stb/include)

if(NOT SDL2_INCLUDE_DIRS)
	find_package(SDL2 REQUIRED)
endif()
if(NOT FREETYPE_INCLUDE_DIRS)
	find_package(Freetype REQUIRED)
endif()

if(NOT SDL2_MIXER_INCLUDE_DIRS)
	if(EXISTS /usr/local/include/SDL2/SDL_mixer.h)
		set(SDL2_MIXER_INCLUDE_DIRS /usr/local/include/SDL2)
		set(SDL2_MIXER_LIBRARIES "-L/usr/local/lib -lSDL2_mixer")
	endif()
endif()

if(NOT SDL2_MIXER_LIBRARIES)
	set(SDL2_MIXER_LIBRARIES SDL2_mixer)
endif()

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

MESSAGE(STATUS "INFO: SDL2_LIBRARIES is ${SDL2_LIBRARIES}")
MESSAGE(STATUS "INFO: SDL2_MIXER_LIBRARIES is ${SDL2_MIXER_LIBRARIES}")
MESSAGE(STATUS "INFO: FREETYPE_LIBRARIES is ${FREETYPE_LIBRARIES}")

target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${SDL2_MIXER_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${FREETYPE_LIBRARIES})

target_precompile_headers(${PROJECT_NAME} PRIVATE src/stdafx.h)
