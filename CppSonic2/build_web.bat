@echo off
if not exist build (mkdir build)

set SOURCES=src/renderer.cpp^
 src/font.cpp^
 src/game.cpp^
 src/main.cpp^
 src/package.cpp^
 src/texture.cpp^
 src/window_creation.cpp^
 src/imgui_glue.cpp^
 src/editor.cpp^
 src/console.cpp^
 src/single_header.cpp^
 src/assets.cpp^
 src/sprite.cpp^
 src/particle_system.cpp^
 src/sound_mixer.cpp^
 src/main_menu.cpp^
 src/program.cpp^
 src/title_screen.cpp^
 src/util.cpp^
 src/input.cpp^
 src/input_bindings.cpp^
 src/bunnymark.cpp^
 src/tests.cpp^
 src/common.cpp

set INCLUDES=-I../external/glad/include -I../external/glm/include -I../external/stb/include

set DEFINES=-DNDEBUG -DDEVELOPER

set COMPILE_FLAGS=-o build/index.html -O3 -Wno-c++11-narrowing -Wno-switch

set EMSCRIPTEN_FLAGS=-sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sWASM=1 -sASYNCIFY=1 -sUSE_WEBGL2=1 -sASSERTIONS=1 -sINITIAL_MEMORY=33554432

set PRELOADS=--preload-file fonts --preload-file levels --preload-file sounds --preload-file textures --preload-file music --preload-file shaders

emcc %COMPILE_FLAGS% %DEFINES% %SOURCES% %INCLUDES% %EMSCRIPTEN_FLAGS% %PRELOADS%
