#include "assets.h"

static Texture textures[NUM_TEXTURES] = {};
static Font    fonts[NUM_FONTS]       = {};

void load_common_assets() {
	textures[tex_fnt_menu] = load_texture_from_file("fonts/fnt_main.png");

	textures[tex_crouch]   = load_texture_from_file("textures/crouch.png");
	textures[tex_idle]     = load_texture_from_file("textures/idle.png");
	textures[tex_look_up]  = load_texture_from_file("textures/look_up.png");
	textures[tex_peelout]  = load_texture_from_file("textures/peelout.png");
	textures[tex_roll]     = load_texture_from_file("textures/roll.png");
	textures[tex_run]      = load_texture_from_file("textures/run.png");
	textures[tex_skid]     = load_texture_from_file("textures/skid.png");
	textures[tex_spindash] = load_texture_from_file("textures/spindash.png");
	textures[tex_walk]     = load_texture_from_file("textures/walk.png");
	textures[tex_balance]  = load_texture_from_file("textures/balance.png");
	textures[tex_balance2] = load_texture_from_file("textures/balance2.png");
	textures[tex_push]     = load_texture_from_file("textures/push.png");

	fonts[fnt_ms_gothic]     = load_bmfont_file("fonts/ms_gothic.fnt",      "fonts/ms_gothic_0.png");
	fonts[fnt_ms_mincho]     = load_bmfont_file("fonts/ms_mincho.fnt",      "fonts/ms_mincho_0.png");
	fonts[fnt_consolas]      = load_bmfont_file("fonts/consolas.fnt",       "fonts/consolas_0.png");
	fonts[fnt_consolas_bold] = load_bmfont_file("fonts/consolas_bold.fnt",  "fonts/consolas_bold_0.png");
	fonts[fnt_hud]           = load_bmfont_file("fonts/fnt_hud.fnt",        "fonts/fnt_hud.png");

	fonts[fnt_menu] = load_font_from_texture(get_texture(tex_fnt_menu), 16, 16, 8, 17, 9);
}

void load_common_assets_for_game() {
	textures[tex_spindash_smoke] = load_texture_from_file("textures/spindash_smoke.png");
}

void load_common_assets_for_editor() {
	textures[tex_layer_flip] = load_texture_from_file("textures/layer_flip.png");
	textures[tex_layer_set]  = load_texture_from_file("textures/layer_set.png");
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
	return textures[texture_index];
}

const Font& get_font(u32 font_index) {
	Assert(font_index < NUM_FONTS);
	return fonts[font_index];
}
