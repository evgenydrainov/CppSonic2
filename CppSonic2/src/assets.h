#pragma once

#include "common.h"

#include "renderer.h"
#include "font.h"
#include "sprite.h"
#include "sound_mixer.h"

enum {
	tex_sonic_sprites,
	tex_sonic_palette,
	tex_global_objects,
	tex_editor_sprites,
	tex_mobile_controls,
	tex_titlecard_line,
	tex_pause_menu,
	tex_EEZ_objects,
	tex_editor_bg,

	tex_title_medal,
	tex_title_sonic,
	tex_title_label,
	tex_title_water,
	tex_title_water_palette,
	tex_title_mountains_left,
	tex_title_mountains_right,
	tex_title_clouds,

	tex_bg_EE_back_old,
	tex_bg_EE_front_old,

	tex_back_EE_highbg2,
	tex_back_EE_hindmountains,
	tex_back_EE_medbg2,

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
	spr_sonic_rise,
	spr_sonic_hurt,
	spr_sonic_die,
	spr_sonic_drown,

	spr_spindash_smoke,
	spr_ring,
	spr_ring_disappear,
	spr_monitor,
	spr_monitor_broken,
	spr_monitor_icon,
	spr_explosion,
	spr_skid_dust,
	spr_spike,
	spr_water_surface,
	spr_mosqui,
	spr_flower,
	spr_score_popup,
	spr_game_over_text,
	spr_sign_post,
	spr_text_sonic,
	spr_text_got,
	spr_text_through,
	spr_text_zone,
	spr_text_zone_number,
	spr_invincibility_sparkle,

	// has to be in order
	spr_spring_yellow,
	spr_spring_red,

	// has to be in order
	spr_spring_bounce_yellow,
	spr_spring_bounce_red,

	// has to be in order
	spr_spring_diagonal_yellow,
	spr_spring_diagonal_red,

	// has to be in order
	spr_spring_diagonal_bounce_yellow,
	spr_spring_diagonal_bounce_red,

	spr_EEZ_platform1,
	spr_EEZ_platform2,

	spr_layer_set,
	spr_layer_flip,
	spr_sonic_editor_preview,
	spr_layer_switcher_horizontal,
	spr_layer_switcher_vertical,
	spr_layer_switcher_layer_letter,
	spr_layer_switcher_priority_letter,
	spr_layer_switcher_grounded_flag_letter,
	spr_camera_region,

	spr_title_medal,
	spr_title_sonic,
	spr_title_label,
	spr_title_mountains_left,
	spr_title_mountains_right,

	spr_mobile_dpad,
	spr_mobile_dpad_up,
	spr_mobile_dpad_down,
	spr_mobile_dpad_left,
	spr_mobile_dpad_right,
	spr_mobile_action_button,
	spr_mobile_pause_button,

	spr_pause_menu_bg,
	spr_pause_menu_logo,
	spr_pause_menu_labels,
	spr_pause_menu_cursor,
	spr_hud_lives,

	NUM_SPRITES,
};

enum {
	fnt_ms_gothic,
	fnt_ms_mincho,
	fnt_consolas,
	fnt_consolas_bold,
	fnt_menu,
	fnt_hud,
	fnt_titlecard,

	NUM_FONTS,
};

enum {
	snd_jump_cd,
	snd_jump_s2,
	snd_ring,
	snd_spindash,
	snd_spindash_end,
	snd_skid,
	snd_destroy_monitor,
	snd_spring_bounce,
	snd_lose_rings,
	snd_die,

	NUM_SOUNDS,
};

enum {
	shd_palette,
	shd_sine,

	NUM_SHADERS,
};

void load_global_assets();
void load_assets_for_game();
void load_assets_for_editor();

void free_all_assets();

const Texture& get_texture(u32 texture_index);
const Sprite&  get_sprite (u32 sprite_index);
const Font&    get_font   (u32 font_index);
Mix_Chunk*     get_sound  (u32 sound_index);
const Shader&  get_shader (u32 shader_index);
