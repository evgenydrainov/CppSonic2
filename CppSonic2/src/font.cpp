#include "font.h"

#include "package.h"
#include "texture.h"

Font load_bmfont_file(const char* fnt_filepath, const char* png_filepath) {
	Font f = {};

	string text = get_file_str(fnt_filepath);
	if (text.count == 0) {
		log_error("Couldn't open font \"%s\"", fnt_filepath);
		free_font(&f);
		return {};
	}

	string line = eat_line(&text); // info

	if (string_contains(line, '\r')) {
		log_warn("File %s has CRLF line endings!", fnt_filepath);
	}

	// allow comments at top of file
	while (line.count > 0 && line[0] == '#') {
		line = eat_line(&text);
	}

	// 
	// TODO: Parse 'face="MS Gothic"' somehow
	// 

	while (line.count > 0) {
		eat_whitespace(&line);
		string word = eat_non_whitespace(&line);

		string prefix = "size=";
		if (starts_with(word, prefix)) {
			advance(&word, prefix.count);

			bool done;
			int size = string_to_int(word, &done);
			if (!done) {
				log_error("Couldn't parse size for font \"%s\"", fnt_filepath);
				free_font(&f);
				return {};
			}

			f.size = size;
		}
	}

	line = eat_line(&text); // common

	while (line.count > 0) {
		eat_whitespace(&line);
		string word = eat_non_whitespace(&line);

		string prefix = "lineHeight=";
		if (starts_with(word, prefix)) {
			advance(&word, prefix.count);

			bool done;
			int line_height = string_to_int(word, &done);
			if (!done) {
				log_error("Couldn't parse line height for font \"%s\"", fnt_filepath);
				free_font(&f);
				return {};
			}

			f.line_height = line_height;
		}
	}

	line = eat_line(&text); // page
	line = eat_line(&text); // chars

	f.glyphs = calloc_array<Glyph>(95);
	f.should_free_glyphs = true;

	while (starts_with(text, "char ")) {
		line = eat_line(&text);

		eat_whitespace(&line);
		string word = eat_non_whitespace(&line);
		Assert(word == "char");

		int id = -1;

		auto eat_value_int = [&](string prefix) -> int {
			Assert(prefix.count >= 2);

			eat_whitespace(&line);
			string word = eat_non_whitespace(&line);

			if (!starts_with(word, prefix)) {
				string p = {prefix.data, prefix.count-1};
				log_warn("Couldn't read value \"" Str_Fmt "\" for char %d in font %s", Str_Arg(p), id, fnt_filepath);
				return 0;
			}

			advance(&word, prefix.count);

			bool done;
			int value = string_to_int(word, &done);
			if (!done) {
				string p = {prefix.data, prefix.count-1};
				log_warn("Couldn't parse value \"" Str_Fmt "\" for char %d in font %s", Str_Arg(p), id, fnt_filepath);
				return 0;
			}

			return value;
		};

		id = eat_value_int("id=");
		int glyph_index = id - 32;

		if (glyph_index < 0 || glyph_index >= f.glyphs.count) {
			continue;
		}

		Glyph glyph = {};

		glyph.u        = eat_value_int("x=");
		glyph.v        = eat_value_int("y=");
		glyph.width    = eat_value_int("width=");
		glyph.height   = eat_value_int("height=");
		glyph.xoffset  = eat_value_int("xoffset=");
		glyph.yoffset  = eat_value_int("yoffset=");
		glyph.xadvance = eat_value_int("xadvance=");

		f.glyphs[glyph_index] = glyph;
	}

	f.atlas = load_texture_from_file(png_filepath);

	if (f.atlas.id == 0) {
		log_error("Couldn't load font %s: couldn't load texture.", fnt_filepath);
		free_font(&f);
		return {};
	}

	f.should_free_atlas = true;

	log_info("Loaded font %s", fnt_filepath);

	return f;
}

Font load_font_from_texture(const char* filepath,
							int size, int line_height, int char_width,
							int xoffset, int yoffset) {
	Font f = {};

	f.atlas = load_texture_from_file(filepath);
	f.should_free_atlas = true;

	if (f.atlas.id == 0) {
		log_error("Couldn't create font: invalid texture.");
		free_font(&f);
		return {};
	}

	if (xoffset == 0) xoffset = char_width;
	if (yoffset == 0) yoffset = size;

	if (f.atlas.width % xoffset != 0) {
		log_error("Couldn't create font: texture width must be divisible by xoffset.");
		free_font(&f);
		return {};
	}

	int stride = f.atlas.width / xoffset;

	f.glyphs = calloc_array<Glyph>(95); // [32..126]
	f.should_free_glyphs = true;

	f.size = size;
	f.line_height = line_height;

	for (size_t i = 0; i < f.glyphs.count; i++) {
		int tile_x = i % stride;
		int tile_y = i / stride;

		Glyph glyph = {};
		glyph.u = tile_x * xoffset;
		glyph.v = tile_y * yoffset;
		glyph.width = char_width;
		glyph.height = size;
		glyph.xadvance = char_width;

		f.glyphs[i] = glyph;
	}

	log_info("Loaded font (%d x %d) from texture (%d x %d).", char_width, size, f.atlas.width, f.atlas.height);

	return f;
}

void free_font(Font* f) {
	if (f->should_free_atlas)  free_texture(&f->atlas);
	if (f->should_free_glyphs) free(f->glyphs.data);

	*f = {};
}

vec2 draw_text(const Font& font, string text, vec2 text_pos,
			   HAlign halign, VAlign valign, vec4 color) {
	if (font.glyphs.count == 0) return text_pos;

	if (valign == VALIGN_MIDDLE) {
		text_pos.y -= measure_text(font, text).y / 2.0f;
	} else if (valign == VALIGN_BOTTOM) {
		text_pos.y -= measure_text(font, text).y;
	}

	float ch_x = text_pos.x;
	float ch_y = text_pos.y;

	if (halign == HALIGN_CENTER) {
		ch_x -= measure_text(font, text, true).x / 2.0f;
	} else if (halign == HALIGN_RIGHT) {
		ch_x -= measure_text(font, text, true).x;
	}

	for (size_t i = 0; i < text.count; i++) {
		u8 ch = (u8) text[i];

		// If char isn't valid, set it to '?'
		if (!((ch >= 32 && ch <= 127) || ch == '\n')) {
			ch = '?';
		}

		if (ch == '\n') {
			ch_x = text_pos.x;
			ch_y += font.line_height;

			if (halign == HALIGN_CENTER) {
				string str = text;
				advance(&str, i + 1);

				ch_x -= measure_text(font, str, true).x / 2.0f;
			} else if (halign == HALIGN_RIGHT) {
				string str = text;
				advance(&str, i + 1);

				ch_x -= measure_text(font, str, true).x;
			}
		} else {
			Assert(font.glyphs.count == 95);
			Glyph glyph = font.glyphs[ch - 32];

			// If char isn't whitespace, draw it
			if (ch != ' ') {
				Rect src = {glyph.u, glyph.v, glyph.width, glyph.height};

				vec2 pos;
				pos.x = ch_x + glyph.xoffset;
				pos.y = ch_y + glyph.yoffset;

				pos = glm::floor(pos);

				draw_texture(font.atlas, src, pos, {1, 1}, {}, 0, color);
			}

			ch_x += glyph.xadvance;
		}
	}

	return {ch_x, ch_y};
}

vec2 draw_text_shadow(const Font& font, string text, vec2 text_pos,
					  HAlign halign, VAlign valign, vec4 color) {
	vec4 shadow_color = {0, 0, 0, color.a};
	draw_text(font, text, {text_pos.x + 1, text_pos.y + 1}, halign, valign, shadow_color);

	vec2 result = draw_text(font, text, text_pos, halign, valign, color);
	return result;
}

vec2 measure_text(const Font& font, string text, bool only_one_line) {
	if (font.glyphs.count == 0) return {};

	float w = 0;
	float h = (float) font.size;

	float ch_x = 0;
	float ch_y = 0;

	for (size_t i = 0; i < text.count; i++) {
		u8 ch = (u8) text[i];

		// If char isn't valid, set it to '?'
		if (!((ch >= 32 && ch <= 127) || ch == '\n')) {
			ch = '?';
		}

		if (ch == '\n') {
			if (only_one_line) return {w, h};

			ch_x = 0;
			ch_y += font.line_height;

			h = max(h, ch_y + font.size);
		} else {
			Assert(font.glyphs.count == 95);
			Glyph glyph = font.glyphs[ch - 32];

			// don't skip whitespace
			vec2 pos;
			pos.x = ch_x; // don't add xoffset and yoffset
			pos.y = ch_y;

			pos = glm::floor(pos);

			// On practise, I always happen to prefer glyph.xadvance here,
			// instead of the actual "correct" text size.
			w = max(w, pos.x + glyph.xadvance /*glyph.width*/);
			h = max(h, ch_y + font.size);

			ch_x += glyph.xadvance;
		}
	}

	return {w, h};
}
