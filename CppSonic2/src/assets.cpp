#include "assets.h"

static Texture textures[NUM_TEXTURES] = {};
static Sprite  sprites[NUM_SPRITES]   = {};
static Font    fonts[NUM_FONTS]       = {};

void load_global_assets() {
	textures[tex_sonic_sprites]  = load_texture_from_file("textures/sonic_sprites.png");
	textures[tex_global_objects] = load_texture_from_file("textures/global_objects.png");

	{
		u32 t = tex_sonic_sprites;

		sprites[spr_sonic_crouch]   = make_sprite(t, 0,  0 * 59, 59, 59, 30, 30, 1);
		sprites[spr_sonic_idle]     = make_sprite(t, 0,  1 * 59, 59, 59, 30, 30, 1);
		sprites[spr_sonic_look_up]  = make_sprite(t, 0,  2 * 59, 59, 59, 30, 30, 1);
		sprites[spr_sonic_peelout]  = make_sprite(t, 0,  3 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_roll]     = make_sprite(t, 0,  4 * 59, 59, 59, 30, 30, 5);
		sprites[spr_sonic_run]      = make_sprite(t, 0,  5 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_skid]     = make_sprite(t, 0,  6 * 59, 59, 59, 30, 30, 2);
		sprites[spr_sonic_spindash] = make_sprite(t, 0,  7 * 59, 59, 59, 30, 30, 6);
		sprites[spr_sonic_walk]     = make_sprite(t, 0,  8 * 59, 59, 59, 30, 30, 6);
		sprites[spr_sonic_balance]  = make_sprite(t, 0,  9 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_balance2] = make_sprite(t, 0, 10 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_push]     = make_sprite(t, 0, 11 * 59, 59, 59, 30, 30, 4);
	}

	{
		u32 t = tex_global_objects;

		sprites[spr_spindash_smoke] = make_sprite(t, 0,  0, 32, 32, 32, 11, 7);
		sprites[spr_ring]           = make_sprite(t, 0, 32, 16, 16,  8,  8, 4);
		sprites[spr_ring_disappear] = make_sprite(t, 0, 48, 16, 16,  8,  8, 4);
	}
}

void load_assets_for_game() {
	fonts[fnt_ms_gothic]     = load_bmfont_file("fonts/ms_gothic.fnt",      "fonts/ms_gothic_0.png");
	fonts[fnt_ms_mincho]     = load_bmfont_file("fonts/ms_mincho.fnt",      "fonts/ms_mincho_0.png");
	fonts[fnt_consolas]      = load_bmfont_file("fonts/consolas.fnt",       "fonts/consolas_0.png");
	fonts[fnt_consolas_bold] = load_bmfont_file("fonts/consolas_bold.fnt",  "fonts/consolas_bold_0.png");
	fonts[fnt_hud]           = load_bmfont_file("fonts/fnt_hud.fnt",        "fonts/fnt_hud.png");

	textures[tex_fnt_menu] = load_texture_from_file("fonts/fnt_menu.png");
	fonts[fnt_menu] = load_font_from_texture(get_texture(tex_fnt_menu), 16, 16, 8, 9, 17);
}

void load_assets_for_editor() {
	textures[tex_editor_sprites] = load_texture_from_file("textures/editor_sprites.png");

	{
		u32 t = tex_editor_sprites;

		sprites[spr_layer_flip] = make_sprite(t,  0,  0, 16, 48, 8, 24);
		sprites[spr_layer_set]  = make_sprite(t, 16,  0, 16, 48, 8, 24);
	}
}

void free_all_assets() {
	for (int i = 0; i < NUM_FONTS; i++) {
		free_font(&fonts[i]);
	}

	for (int i = 0; i < NUM_TEXTURES; i++) {
		free_texture(&textures[i]);
	}
}

const Texture& get_texture(u32 texture_index) {
	Assert(texture_index < NUM_TEXTURES);

	Assert(textures[texture_index].ID != 0
		   && "Trying to access a texture that hasn't been loaded.");

	return textures[texture_index];
}

const Sprite& get_sprite(u32 sprite_index) {
	Assert(sprite_index < NUM_SPRITES);

	Assert(sprites[sprite_index].frames.count != 0
		   && "Trying to access a sprite that hasn't been loaded.");

	return sprites[sprite_index];
}

const Font& get_font(u32 font_index) {
	Assert(font_index < NUM_FONTS);

	Assert(fonts[font_index].glyphs.count != 0
		   && "Trying to access a font that hasn't been loaded.");

	return fonts[font_index];
}
