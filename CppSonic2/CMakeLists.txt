cmake_minimum_required(VERSION 3.20)
project(CppSonic2)

# 
# On Windows, use visual studio
# On Linux and Mac, use Cmake
# 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

#set(CMAKE_CXX_FLAGS "-pg")
#set(CMAKE_EXE_LINKER_FLAGS "-pg")
#set(CMAKE_SHARED_LINKER_FLAGS "-pg")

add_compile_definitions($<$<CONFIG:Debug>:_DEBUG>)
add_compile_definitions($<$<CONFIG:Debug>:DEBUG>)
add_compile_definitions($<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

add_compile_definitions(EDITOR)
add_compile_definitions(DEVELOPER)

include_directories(${PROJECT_NAME} ../external/glad/include)
include_directories(${PROJECT_NAME} ../external/glm/include)
include_directories(${PROJECT_NAME} ../external/stb/include)

# 
# You can set SDL2_INCLUDE_DIRS, SDL2_LIBRARIES, SDL2_MIXER_INCLUDE_DIRS and SDL2_MIXER_LIBRARIES.
# If you change SDL2_LIBRARIES, you'll probably also want to setup LD_LIBRARY_PATH.
# See .vscode/settings.json and .vscode/launch.json
# 
if(NOT SDL2_INCLUDE_DIRS)
	find_package(SDL2 REQUIRED)
endif()

MESSAGE(STATUS "INFO: SDL2_INCLUDE_DIRS is ${SDL2_INCLUDE_DIRS}")
include_directories(${PROJECT_NAME} ${SDL2_INCLUDE_DIRS})

if(SDL2_MIXER_INCLUDE_DIRS)
	MESSAGE(STATUS "INFO: SDL2_MIXER_INCLUDE_DIRS is ${SDL2_MIXER_INCLUDE_DIRS}")
	include_directories(${PROJECT_NAME} ${SDL2_MIXER_INCLUDE_DIRS})
endif()

include_directories(${PROJECT_NAME} /usr/include/freetype2)

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
	src/imgui/imgui_single_file.cpp
	src/nfd/nfd_zenity.cpp
	src/nfd/nfd_common.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

MESSAGE(STATUS "INFO: SDL2_LIBRARIES is ${SDL2_LIBRARIES}")
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARIES})

if(SDL2_MIXER_LIBRARIES)
	MESSAGE(STATUS "INFO: SDL2_MIXER_LIBRARIES is ${SDL2_MIXER_LIBRARIES}")
	target_link_libraries(${PROJECT_NAME} ${SDL2_MIXER_LIBRARIES})
else()
	target_link_libraries(${PROJECT_NAME} SDL2_mixer)
endif()

target_link_libraries(${PROJECT_NAME} freetype)

#target_precompile_headers(${PROJECT_NAME} PRIVATE src/stdafx.h)
