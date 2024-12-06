#pragma once

#include "common.h"

#include "texture.h"
#include "font.h"
// #include "sprite.h"

enum {
	tex_fnt_menu,
	tex_layer_flip,
	tex_layer_set,

	tex_crouch,
	tex_idle,
	tex_look_up,
	tex_peelout,
	tex_roll,
	tex_run,
	tex_skid,
	tex_spindash,
	tex_walk,
	tex_balance,
	tex_balance2,
	tex_push,

	tex_spindash_smoke,

	NUM_TEXTURES,
};

enum {
	fnt_ms_gothic,
	fnt_ms_mincho,
	fnt_consolas,
	fnt_consolas_bold,
	fnt_menu,
	fnt_hud,

	NUM_FONTS,
};

void load_common_assets();
void load_common_assets_for_game();
void load_common_assets_for_editor();

void free_all_assets();

const Texture& get_texture(u32 texture_index);
const Font&    get_font(u32 font_index);
