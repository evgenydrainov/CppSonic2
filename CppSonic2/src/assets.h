#pragma once

#include "common.h"

#include "texture.h"
#include "font.h"
#include "sprite.h"
#include "sound_mixer.h"

enum {
	tex_fnt_menu,

	tex_sonic_sprites,
	tex_global_objects,
	tex_editor_sprites,

	tex_title_medal,
	tex_title_sonic,
	tex_title_label,
	tex_title_water,
	tex_title_water_palette,
	tex_title_mountains_left,
	tex_title_mountains_right,
	tex_title_clouds,

	NUM_TEXTURES,
};

enum {
	spr_sonic_crouch,
	spr_sonic_idle,
	spr_sonic_look_up,
	spr_sonic_peelout,
	spr_sonic_roll,
	spr_sonic_run,
	spr_sonic_skid,
	spr_sonic_spindash,
	spr_sonic_walk,
	spr_sonic_balance,
	spr_sonic_balance2,
	spr_sonic_push,

	spr_spindash_smoke,

	spr_ring,
	spr_ring_disappear,

	spr_layer_set,
	spr_layer_flip,

	spr_title_medal,
	spr_title_sonic,
	spr_title_label,
	spr_title_mountains_left,
	spr_title_mountains_right,

	NUM_SPRITES,
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

enum {
	snd_jump_cd,
	snd_jump_s2,
	snd_ring,
	snd_spindash,
	snd_spindash_end,
	snd_skid,

	NUM_SOUNDS,
};

void load_global_assets();
void load_assets_for_game();
void load_assets_for_editor();

void free_all_assets();

const Texture& get_texture(u32 texture_index);
const Sprite&  get_sprite (u32 sprite_index);
const Font&    get_font   (u32 font_index);

Mix_Chunk* get_sound(u32 sound_index);
