#include "texture.h"

#include "package.h"
#include <stb/stb_image.h>

u8* decode_image_data(array<u8> buffer, int* out_width, int* out_height) {
	static const u8 png_magic[] = {137, 80, 78, 71, 13, 10, 26, 10};
	static const u8 qoi_magic[] = {'q', 'o', 'i', 'f'};
	
	if (starts_with(buffer, array<u8>{png_magic})) {
		int width;
		int height;
		int num_channels;
		u8* pixel_data = stbi_load_from_memory(buffer.data, (int)buffer.count, &width, &height, &num_channels, 4);

		if (!pixel_data) {
			log_error("Couldn't decode png image.");
			return nullptr;
		}

		Assert(num_channels == 4);

		*out_width = width;
		*out_height = height;
		return pixel_data;
	}

	if (starts_with(buffer, array<u8>{qoi_magic})) {
#if 0
		qoi_desc desc;
		u8* pixel_data = (u8*) qoi_decode(buffer.data, (int)buffer.count, &desc, 4);

		if (!pixel_data) {
			log_error("Couldn't decode qoi image.");
			return nullptr;
		}

		// Assert(desc.colorspace == QOI_SRGB);

		*out_width = desc.width;
		*out_height = desc.height;
		return pixel_data;
#endif
	}

	log_error("Unknown image format.");
	return {};
}

bool load_texture_from_pixel_data(Texture* t, u8* pixel_data, int width, int height,
								  int filter, int wrap) {
	free_texture(t);

	glGenTextures(1, &t->ID);
	glBindTexture(GL_TEXTURE_2D, t->ID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

	glBindTexture(GL_TEXTURE_2D, 0);

	t->width = width;
	t->height = height;
	return true;
}

bool load_texture_from_memory(Texture* t, array<u8> buffer,
							  int filter, int wrap) {
	free_texture(t);

	int width;
	int height;
	u8* pixel_data = decode_image_data(buffer, &width, &height);
	if (!pixel_data) {
		return false;
	}

	defer { free(pixel_data); };

	return load_texture_from_pixel_data(t, pixel_data, width, height, filter, wrap);
}

bool load_texture_from_file(Texture* t, const char* fname,
							int filter, int wrap) {
	free_texture(t);

	auto buffer = get_file_arr(fname);
	if (buffer.count == 0) {
		return false;
	}

	bool result = load_texture_from_memory(t, buffer, filter, wrap);

	if (result) {
		log_info("Loaded texture %s", fname);
	} else {
		log_info("Couldn't load texture %s", fname);
	}

	return result;
}

void free_texture(Texture* t) {
	if (t->ID != 0) {
		glDeleteTextures(1, &t->ID);
	}

	*t = {};
}

bool load_surface_from_file(SDL_Surface** surface, const char* fname) {
	if (*surface) SDL_FreeSurface(*surface);
	*surface = nullptr;

	auto buffer = get_file_arr(fname);
	if (buffer.count == 0) {
		return false;
	}

	int width;
	int height;
	// @Leak: pixel_data must be alive
	u8* pixel_data = decode_image_data(buffer, &width, &height);
	if (!pixel_data) {
		return false;
	}

	*surface = SDL_CreateRGBSurfaceWithFormatFrom(pixel_data, width, height, 32, width * 4, SDL_PIXELFORMAT_ABGR8888);
	return *surface != nullptr;
}

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y) {
	if (x < 0) return {};
	if (y < 0) return {};
	if (x >= surface->w) return {};
	if (y >= surface->h) return {};

	u8 r, g, b, a;
	u32* target_pixel = (u32*) ((u8*) surface->pixels
								+ y * surface->pitch
								+ x * surface->format->BytesPerPixel);
	SDL_GetRGBA(*target_pixel, surface->format, &r, &g, &b, &a);

	return get_color(r, g, b, a);
}
