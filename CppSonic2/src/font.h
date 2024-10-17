#pragma once

#include "common.h"
#include "texture.h"

struct Glyph {
	int u;
	int v;
	int width;
	int height;
	int xoffset;
	int yoffset;
	int xadvance;
};

struct Font {
	Texture atlas;
	array<Glyph> glyphs;
	int size;
	int line_height;
};

enum HAlign {
	HALIGN_LEFT,
	HALIGN_CENTER,
	HALIGN_RIGHT,
};

enum VAlign {
	VALIGN_TOP,
	VALIGN_MIDDLE,
	VALIGN_BOTTOM,
};

Font load_bmfont_file(const char* fnt_filepath, const char* png_filepath);

// Returns the position of the next-to-be-drawn character.
vec2 draw_text(Font font, string text, vec2 text_pos,
			   HAlign halign = HALIGN_LEFT, VAlign valign = VALIGN_TOP, vec4 color = color_white);

// The height of an empty string is font.size
vec2 measure_text(Font font, string text, bool only_one_line = false);
