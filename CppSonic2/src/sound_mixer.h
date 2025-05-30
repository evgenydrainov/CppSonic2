#pragma once

#include "common.h"

#include <SDL_mixer.h>

extern Mix_Music* g_Music;

void init_mixer();
void deinit_mixer();

Mix_Chunk* load_sound(const char* fname);

void play_sound(Mix_Chunk* chunk, bool stop_all_instances = true);
void stop_sound(Mix_Chunk* chunk);

void play_music(const char* fname);
void stop_music();
