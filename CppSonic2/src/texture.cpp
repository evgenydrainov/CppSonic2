#include "texture.h"

#include "package.h"
#include <stb/stb_image.h>

static bool is_png(u8* filedata, size_t filesize) {
	static u8 magic[] = {137, 80, 78, 71, 13, 10, 26, 10};
	if (filesize < sizeof(magic)) {
		return false;
	}
	for (size_t i = 0; i < sizeof(magic); i++) {
		if (filedata[i] != magic[i]) {
			return false;
		}
	}
	return true;
}

static bool is_qoi(u8* filedata, size_t filesize) {
	static u8 magic[] = {'q', 'o', 'i', 'f'};
	if (filesize < sizeof(magic)) {
		return false;
	}
	for (size_t i = 0; i < sizeof(magic); i++) {
		if (filedata[i] != magic[i]) {
			return false;
		}
	}
	return true;
}

Texture load_texture_from_file(const char* fname,
							   int filter, int wrap) {
	auto create_texture = [&](void* pixel_data, int width, int height, int num_channels) -> Texture {
		u32 texture;
		glGenTextures(1, &texture);

		glBindTexture(GL_TEXTURE_2D, texture);
		defer { glBindTexture(GL_TEXTURE_2D, 0); };

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

		Assert(num_channels == 4);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

		return {texture, width, height};
	};

	Texture result = {};

	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (filedata) {
		if (is_png(filedata, filesize)) {
			int width;
			int height;
			int num_channels;
			void* pixel_data = stbi_load_from_memory(filedata, (int)filesize, &width, &height, &num_channels, 4);
			defer { if (pixel_data) stbi_image_free(pixel_data); };

			result = create_texture(pixel_data, width, height, num_channels);
		} else if (is_qoi(filedata, filesize)) {
			//qoi_desc desc;
			//void* pixel_data = qoi_decode(filedata, (int)filesize, &desc, 4);
			//defer { if (pixel_data) free(pixel_data); };

			//// Assert(desc.colorspace == QOI_SRGB);

			//result = create_texture(pixel_data, desc.width, desc.height, desc.channels);
		}
	}

	if (result.ID != 0) {
		log_info("Loaded texture %s", fname);
	} else {
		log_info("Couldn't load texture %s", fname);

		// Create a 1x1 stub texture
		u32 white[1 * 1] = {0xffffffff};

		result = create_texture(white, 1, 1, 4);
	}

	return result;
}

SDL_Surface* load_surface_from_file(const char* fname) {
	SDL_Surface* result = nullptr;

	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (filedata) {
		if (is_png(filedata, filesize)) {
			int width;
			int height;
			int num_channels;
			void* pixel_data = stbi_load_from_memory(filedata, (int)filesize, &width, &height, &num_channels, 4);

			// @Leak: pixel_data must be alive
			result = SDL_CreateRGBSurfaceWithFormatFrom(pixel_data, width, height, 32, width * 4, SDL_PIXELFORMAT_ABGR8888);
		} else if (is_qoi(filedata, filesize)) {

		}
	}

	return result;
}

void free_texture(Texture* t) {
	t->width  = 0;
	t->height = 0;

	if (t->ID) glDeleteTextures(1, &t->ID);
	t->ID = 0;
}

vec4 surface_get_pixel(SDL_Surface* surface, int x, int y) {
	if (x < 0) return {};
	if (y < 0) return {};
	if (x >= surface->w) return {};
	if (y >= surface->h) return {};

	u32* pixels = (u32*) surface->pixels;
	u8 r, g, b, a;
	u32* target_pixel = (u32*) ((u8*) surface->pixels
								+ y * surface->pitch
								+ x * surface->format->BytesPerPixel);
	SDL_GetRGBA(*target_pixel, surface->format, &r, &g, &b, &a);

	return get_color(r, g, b, a);
}
