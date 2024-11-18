#pragma once

#include "common.h"

struct Texture {
	u32 ID;
	int width;
	int height;
};

// result must be free()'d
u8* decode_image_data(array<u8> buffer, int* out_width, int* out_height);

// Texture struct must be zero-initialized.
bool load_texture_from_pixel_data(Texture* t, u8* pixel_data, int width, int height,
								  int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_BORDER);

// Texture struct must be zero-initialized.
bool load_texture_from_memory(Texture* t, array<u8> buffer,
							  int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_BORDER);

// Texture struct must be zero-initialized.
bool load_texture_from_file(Texture* t, const char* fname,
							int filter = GL_NEAREST, int wrap = GL_CLAMP_TO_BORDER);

void free_texture(Texture* t);

// pointer must be zero-initialized.
bool load_surface_from_file(SDL_Surface** surface, const char* fname);

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y);
