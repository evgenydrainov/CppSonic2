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

	log_error("Couldn't decode image data: Unknown image format.");
	return {};
}

Texture load_texture_from_pixel_data(u8* pixel_data, int width, int height,
									 int filter, int wrap) {
	Texture t = {};

	glGenTextures(1, &t.id);
	glBindTexture(GL_TEXTURE_2D, t.id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);

	glBindTexture(GL_TEXTURE_2D, 0);

	t.width = width;
	t.height = height;
	return t;
}

Texture load_texture_from_memory(array<u8> buffer,
								 int filter, int wrap) {
	int width;
	int height;
	u8* pixel_data = decode_image_data(buffer, &width, &height);
	if (!pixel_data) {
		return create_texture_stub();
	}

	defer { free(pixel_data); };

	return load_texture_from_pixel_data(pixel_data, width, height, filter, wrap);
}

Texture load_texture_from_file(const char* fname,
							   int filter, int wrap) {
	auto buffer = get_file_arr(fname);
	if (buffer.count == 0) {
		return create_texture_stub();
	}

	Texture t = load_texture_from_memory(buffer, filter, wrap);

	if (t.id != 0) {
		log_info("Loaded texture %s (%d x %d)", fname, t.width, t.height);
	}

	return t;
}

Texture create_texture_stub() {
	const int width = 2;
	const int height = 2;
	u8 pixel_data[width * height * 4] = {
		255, 0, 255, 255,    0,   0,   0, 255,
		0,   0,   0, 255,    255, 0, 255, 255,
	};

	return load_texture_from_pixel_data(pixel_data, width, height, GL_NEAREST, GL_REPEAT);
}

void free_texture(Texture* t) {
	if (t->id != 0) {
		glDeleteTextures(1, &t->id);
	}

	*t = {};
}

SDL_Surface* load_surface_from_file(const char* fname) {
	auto buffer = get_file_arr(fname);
	if (buffer.count == 0) {
		return nullptr;
	}

	int width;
	int height;
	// @Leak!!! pixel_data must be alive
	u8* pixel_data = decode_image_data(buffer, &width, &height);
	if (!pixel_data) {
		return nullptr;
	}

	return SDL_CreateRGBSurfaceWithFormatFrom(pixel_data, width, height, 32, width * 4, SDL_PIXELFORMAT_ABGR8888);
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
