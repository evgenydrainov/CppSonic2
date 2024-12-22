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
	snd_jump,
	snd_ring,

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
