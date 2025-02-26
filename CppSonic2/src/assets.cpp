#include "assets.h"

#include "package.h"
#include "util.h"

static Texture    textures[NUM_TEXTURES];
static Sprite     sprites [NUM_SPRITES];
static Font       fonts   [NUM_FONTS];
static Mix_Chunk* sounds  [NUM_SOUNDS];
static u32        shaders [NUM_SHADERS];

void load_global_assets() {
	{
		textures[tex_global_objects] = load_texture_from_file("textures/global_objects.png");
		const Texture& t = get_texture(tex_global_objects);

		sprites[spr_spindash_smoke] = create_sprite(t,  0,   0, 32, 32, 32, 11, 7);
		sprites[spr_ring]           = create_sprite(t,  0,  32, 16, 16,  8,  8, 4);
		sprites[spr_ring_disappear] = create_sprite(t,  0,  48, 16, 16,  8,  8, 4, 4, 1.0f / 6.0f);
		sprites[spr_monitor]        = create_sprite(t,  0,  64, 32, 32, 16, 16, 2);
		sprites[spr_monitor_broken] = create_sprite(t, 64,  64, 32, 32, 16, 16);
		sprites[spr_monitor_icon]   = create_sprite(t,  0,  96, 16, 16,  8,  8, 10);
		sprites[spr_explosion]      = create_sprite(t,  0, 176, 32, 32, 16, 16, 5, 5, 1.0f / 6.0f);
		sprites[spr_skid_dust]      = create_sprite(t,  0, 208, 16, 16,  8,  8, 4, 4, 1.0f / 4.0f);
		sprites[spr_spike]          = create_sprite(t,  0, 224, 32, 32, 16, 16);
		sprites[spr_water_surface]  = create_sprite(t, 64,  32, 32, 16,  0,  8, 4);

		sprites[spr_spring_yellow]  = create_sprite(t, 32, 112, 32, 32, 16, 24);
		sprites[spr_spring_red]     = create_sprite(t, 32, 144, 32, 32, 16, 24);

		sprites[spr_spring_bounce_yellow]  = create_sprite(t,  0, 112, 32, 32, 16, 24, 3, 3, 1.0f / 3.0f);
		sprites[spr_spring_bounce_red]     = create_sprite(t,  0, 144, 32, 32, 16, 24, 3, 3, 1.0f / 3.0f);
	}

	{
		textures[tex_EEZ_objects] = load_texture_from_file("textures/EEZ_objects.png");
		const Texture& t = get_texture(tex_EEZ_objects);

		sprites[spr_EEZ_platform1] = create_sprite(t, 0,  0,  64, 32, 32, 16);
		sprites[spr_EEZ_platform2] = create_sprite(t, 0, 32, 128, 48, 64, 24);
	}
}

void load_assets_for_game() {
	{
		textures[tex_sonic_sprites] = load_texture_from_file("textures/sonic_sprites_indexed.png");
		const Texture& t = get_texture(tex_sonic_sprites);

		sprites[spr_sonic_crouch]   = create_sprite(t, 0,  0 * 59, 59, 59, 30, 30);
		sprites[spr_sonic_idle]     = create_sprite(t, 0,  1 * 59, 59, 59, 30, 30);
		sprites[spr_sonic_look_up]  = create_sprite(t, 0,  2 * 59, 59, 59, 30, 30);
		sprites[spr_sonic_peelout]  = create_sprite(t, 0,  3 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_roll]     = create_sprite(t, 0,  4 * 59, 59, 59, 30, 30, 5);
		sprites[spr_sonic_run]      = create_sprite(t, 0,  5 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_skid]     = create_sprite(t, 0,  6 * 59, 59, 59, 30, 30, 2);
		sprites[spr_sonic_spindash] = create_sprite(t, 0,  7 * 59, 59, 59, 30, 30, 6);
		sprites[spr_sonic_walk]     = create_sprite(t, 0,  8 * 59, 59, 59, 30, 30, 6);
		sprites[spr_sonic_balance]  = create_sprite(t, 0,  9 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_balance2] = create_sprite(t, 0, 10 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_push]     = create_sprite(t, 0, 11 * 59, 59, 59, 30, 30, 4);
		sprites[spr_sonic_rise]     = create_sprite(t, 0, 12 * 59, 59, 59, 30, 30, 5);
		sprites[spr_sonic_hurt]     = create_sprite(t, 0, 13 * 59, 59, 59, 30, 30);
	}

	textures[tex_sonic_palette] = load_texture_from_file("textures/sonic_palette.png");

	fonts[fnt_ms_gothic]     = load_bmfont_file("fonts/ms_gothic.fnt",      "fonts/ms_gothic_0.png");
	fonts[fnt_ms_mincho]     = load_bmfont_file("fonts/ms_mincho.fnt",      "fonts/ms_mincho_0.png");
	fonts[fnt_consolas]      = load_bmfont_file("fonts/consolas.fnt",       "fonts/consolas_0.png");
	fonts[fnt_consolas_bold] = load_bmfont_file("fonts/consolas_bold.fnt",  "fonts/consolas_bold_0.png");
	fonts[fnt_hud]           = load_bmfont_file("fonts/fnt_hud.fnt",        "fonts/fnt_hud.png");
	fonts[fnt_titlecard]     = load_bmfont_file("fonts/fnt_titlecard.fnt",  "fonts/fnt_titlecard.png");

	fonts[fnt_menu] = load_font_from_texture("fonts/fnt_menu.png", 16, 16, 8, 9, 17);

	sounds[snd_jump_cd]         = load_sound("sounds/jump_cd.wav");
	sounds[snd_jump_s2]         = load_sound("sounds/jump_s2.wav");
	sounds[snd_ring]            = load_sound("sounds/ring.wav");
	sounds[snd_spindash]        = load_sound("sounds/spindash.wav");
	sounds[snd_spindash_end]    = load_sound("sounds/spindash_end.wav");
	sounds[snd_skid]            = load_sound("sounds/skid.wav");
	sounds[snd_destroy_monitor] = load_sound("sounds/destroy_monitor.wav");
	sounds[snd_spring_bounce]   = load_sound("sounds/spring_bounce.wav");
	sounds[snd_lose_rings]      = load_sound("sounds/lose_rings.wav");

	{
		textures[tex_title_medal] = load_texture_from_file("textures/title_medal.png");
		const Texture& t = get_texture(tex_title_medal);
		sprites[spr_title_medal] = create_sprite(t, 0, 0, t.width, t.height, t.width, t.height / 2);
	}

	{
		textures[tex_title_sonic] = load_texture_from_file("textures/title_sonic.png");
		const Texture& t = get_texture(tex_title_sonic);
		sprites[spr_title_sonic] = create_sprite(t, 0, 0, 103, 120, 103 / 2, 120 / 2, 7);
	}

	{
		textures[tex_title_label] = load_texture_from_file("textures/title_label.png");
		const Texture& t = get_texture(tex_title_label);
		sprites[spr_title_label] = create_sprite(t, 0, 0, t.width, t.height, t.width / 2, t.height / 2);
	}

	{
		textures[tex_title_mountains_left] = load_texture_from_file("textures/title_mountains_left.png");
		const Texture& t = get_texture(tex_title_mountains_left);
		sprites[spr_title_mountains_left] = create_sprite(t, 0, 0, t.width, t.height, 0, t.height);
	}

	{
		textures[tex_title_mountains_right] = load_texture_from_file("textures/title_mountains_right.png");
		const Texture& t = get_texture(tex_title_mountains_right);
		sprites[spr_title_mountains_right] = create_sprite(t, 0, 0, t.width, t.height, t.width, t.height);
	}

	textures[tex_title_water] = load_texture_from_file("textures/title_water.png");
	textures[tex_title_water_palette] = load_texture_from_file("textures/title_water_palette.png");
	textures[tex_title_clouds] = load_texture_from_file("textures/title_clouds.png", GL_NEAREST, GL_REPEAT);

	textures[tex_bg_EE_back]  = load_texture_from_file("textures/bg_EE_back.png");
	textures[tex_bg_EE_front] = load_texture_from_file("textures/bg_EE_front.png");

	textures[tex_titlecard_line] = load_texture_from_file("textures/titlecard_line.png", GL_NEAREST, GL_REPEAT);

	{
		textures[tex_pause_menu] = load_texture_from_file("textures/pause_menu.png");
		const Texture& t = get_texture(tex_pause_menu);

		sprites[spr_pause_menu_bg]     = create_sprite(t, 0,   0, 128, 32, 0, 0);
		sprites[spr_pause_menu_logo]   = create_sprite(t, 0,  32, 128, 32, 0, 0);
		sprites[spr_pause_menu_labels] = create_sprite(t, 0,  64,  65, 12, 0, 0, 4, 1);
		sprites[spr_pause_menu_cursor] = create_sprite(t, 0, 112, 128,  3, 0, 0);
		sprites[spr_hud_lives]         = create_sprite(t, 0, 128,  24, 16, 0, 0, 3);
	}

#if defined(__ANDROID__) || defined(PRETEND_MOBILE)
	{
		textures[tex_mobile_controls] = load_texture_from_file("textures/mobile_controls.png");
		const Texture& t = get_texture(tex_mobile_controls);

		sprites[spr_mobile_dpad]       = create_sprite(t,  0,  0, 62, 62, 0, 0);
		sprites[spr_mobile_dpad_up]    = create_sprite(t, 64,  0, 13, 25, 0, 0, 2);
		sprites[spr_mobile_dpad_down]  = create_sprite(t, 64, 26, 13, 25, 0, 0, 2);
		sprites[spr_mobile_dpad_left]  = create_sprite(t, 90, 14, 27, 13, 0, 0, 2);
		sprites[spr_mobile_dpad_right] = create_sprite(t, 90,  0, 27, 13, 0, 0, 2);

		sprites[spr_mobile_action_button] = create_sprite(t,  0, 64, 48, 48, 0, 0, 2);
		sprites[spr_mobile_pause_button]  = create_sprite(t, 96, 32, 16, 16, 0, 0);
	}
#endif

	{
		u32 shd_palette_vert = compile_shader(GL_VERTEX_SHADER, get_file_str("shaders/palette.vert"), "shd_palette_vert");
		defer { glDeleteShader(shd_palette_vert); };

		u32 shd_palette_frag = compile_shader(GL_FRAGMENT_SHADER, get_file_str("shaders/palette.frag"), "shd_palette_frag");
		defer { glDeleteShader(shd_palette_frag); };

		u32 shd_sine_vert = compile_shader(GL_VERTEX_SHADER, get_file_str("shaders/sine.vert"), "shd_sine_vert");
		defer { glDeleteShader(shd_sine_vert); };

		u32 shd_sine_frag = compile_shader(GL_FRAGMENT_SHADER, get_file_str("shaders/sine.frag"), "shd_sine_frag");
		defer { glDeleteShader(shd_sine_frag); };

		shaders[shd_palette] = link_program(shd_palette_vert, shd_palette_frag, "shd_palette");
		shaders[shd_sine]    = link_program(shd_sine_vert,    shd_sine_frag,    "shd_sine");
	}
}

void load_assets_for_editor() {
	textures[tex_editor_bg] = load_texture_from_file("textures/editor_bg.png", GL_NEAREST, GL_REPEAT);

	{
		textures[tex_editor_sprites] = load_texture_from_file("textures/editor_sprites.png");
		const Texture& t = get_texture(tex_editor_sprites);

		sprites[spr_layer_flip]           = create_sprite(t,  0,  0, 16, 48,  8, 24);
		sprites[spr_layer_set]            = create_sprite(t, 16,  0, 16, 48,  8, 24);
		sprites[spr_sonic_editor_preview] = create_sprite(t, 32,  0, 32, 48, 16, 24);
	}
}

void free_all_assets() {
	for (int i = 0; i < NUM_FONTS; i++) {
		free_font(&fonts[i]);
	}

	for (int i = 0; i < NUM_TEXTURES; i++) {
		free_texture(&textures[i]);
	}

	for (int i = 0; i < NUM_SOUNDS; i++) {
		if (sounds[i]) {
			Mix_FreeChunk(sounds[i]);
			sounds[i] = nullptr;
		}
	}

	for (int i = 0; i < NUM_SHADERS; i++) {
		if (shaders[i] != 0) {
			glDeleteProgram(shaders[i]);
			shaders[i] = 0;
		}
	}
}

const Texture& get_texture(u32 texture_index) {
	Assert(texture_index < NUM_TEXTURES);

	if (textures[texture_index].id == 0) {
		log_warn("Trying to access texture %u that hasn't been loaded.", texture_index);
	}

	return textures[texture_index];
}

const Sprite& get_sprite(u32 sprite_index) {
	Assert(sprite_index < NUM_SPRITES);

	if (sprites[sprite_index].frames.count == 0) {
		log_warn("Trying to access sprite %u that hasn't been loaded.", sprite_index);
	}

	return sprites[sprite_index];
}

const Font& get_font(u32 font_index) {
	Assert(font_index < NUM_FONTS);

	if (fonts[font_index].glyphs.count == 0) {
		log_warn("Trying to access font %u that hasn't been loaded.", font_index);
	}

	return fonts[font_index];
}

Mix_Chunk* get_sound(u32 sound_index) {
	Assert(sound_index < NUM_SOUNDS);

	if (!sounds[sound_index]) {
		log_warn("Trying to access sound %u that hasn't been loaded.", sound_index);
	}

	return sounds[sound_index];
}

u32 get_shader(u32 shader_index) {
	Assert(shader_index < NUM_SHADERS);

	if (shaders[shader_index] == 0) {
		log_warn("Trying to access shader %u that hasn't been loaded.", shader_index);
	}

	return shaders[shader_index];
}
