#pragma once

#include "common.h"

struct Texture {
	u32 id;
	int width;
	int height;
};

// result must be free()'d
u8* decode_image_data(array<u8> buffer, int* out_width, int* out_height);

Texture load_texture_from_pixel_data(u8* pixel_data, int width, int height,
									 int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);

Texture load_texture_from_memory(array<u8> buffer,
								 int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);

Texture load_texture_from_file(const char* fname,
							   int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);

Texture create_texture_stub();

void free_texture(Texture* t);

SDL_Surface* load_surface_from_file(const char* fname);

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y);
