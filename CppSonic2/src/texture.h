#pragma once

#include "common.h"

struct Texture {
	u32 ID;
	int width;
	int height;
};

Texture load_texture_from_file(const char* fname,
							   int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_BORDER);

SDL_Surface* load_surface_from_file(const char* fname);

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y);

void free_texture(Texture* t);
