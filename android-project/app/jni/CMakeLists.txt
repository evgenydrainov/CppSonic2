cmake_minimum_required(VERSION 3.6)

project(GAME)

# armeabi-v7a requires cpufeatures library
# include(AndroidNdkModules)
# android_ndk_import_module_cpufeatures()


# SDL sources are in a subfolder named "SDL"
add_subdirectory(C:/Users/Username/source/repos/SDL-release-2.32.2 SDL)

# Compilation of companion libraries
#add_subdirectory(C:/Users/Username/source/repos/SDL_image SDL_image)

set(SDL2MIXER_MOD OFF)
set(SDL2MIXER_MIDI OFF)
set(SDL2MIXER_OPUS OFF)
set(SDL2MIXER_WAVPACK OFF)
add_subdirectory(C:/Users/Username/source/repos/SDL_mixer-release-2.8.0 SDL_mixer)

#add_subdirectory(C:/Users/Username/source/repos/SDL_ttf SDL_ttf)

# Your game and its CMakeLists.txt are in a subfolder named "src"
add_subdirectory(src)

