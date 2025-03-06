#pragma once

#include "common.h"
#include "renderer.h"

// result must be free()'d
u8* decode_image_data(array<u8> buffer, int* out_width, int* out_height);

// buffer: array of bytes of an image encoded in PNG, JPG, etc.
Texture load_texture_from_memory(array<u8> buffer,
								 int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);

Texture load_texture_from_file(const char* fname,
							   int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_EDGE);

Texture create_texture_stub();

SDL_Surface* load_surface_from_file(const char* fname);

void free_surface(SDL_Surface** s);

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y);
