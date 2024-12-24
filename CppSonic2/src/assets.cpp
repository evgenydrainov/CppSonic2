#include "assets.h"

static Texture textures[NUM_TEXTURES];
static Sprite  sprites [NUM_SPRITES];
static Font    fonts   [NUM_FONTS];

static Mix_Chunk* sounds[NUM_SOUNDS];

void load_global_assets() {
	textures[tex_sonic_sprites]  = load_texture_from_file("textures/sonic_sprites.png");
	textures[tex_global_objects] = load_texture_from_file("textures/global_objects.png");

	{
		const Texture& t = get_texture(tex_sonic_sprites);

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
		const Texture& t = get_texture(tex_global_objects);

		sprites[spr_spindash_smoke] = make_sprite(t, 0,  0, 32, 32, 32, 11, 7);
		sprites[spr_ring]           = make_sprite(t, 0, 32, 16, 16,  8,  8, 4);
		sprites[spr_ring_disappear] = make_sprite(t, 0, 48, 16, 16,  8,  8, 4, 4, 1.0f / 6.0f);
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

	sounds[snd_jump_cd]      = load_sound("sounds/jump_cd.wav");
	sounds[snd_jump_s2]      = load_sound("sounds/jump_s2.wav");
	sounds[snd_ring]         = load_sound("sounds/ring.wav");
	sounds[snd_spindash]     = load_sound("sounds/spindash.wav");
	sounds[snd_spindash_end] = load_sound("sounds/spindash_end.wav");
	sounds[snd_skid]         = load_sound("sounds/skid.wav");

	{
		textures[tex_title_medal] = load_texture_from_file("textures/title_medal.png");
		const Texture& t = get_texture(tex_title_medal);
		sprites[spr_title_medal] = make_sprite(t, 0, 0, t.width, t.height, t.width, t.height / 2);
	}

	{
		textures[tex_title_sonic] = load_texture_from_file("textures/title_sonic.png");
		const Texture& t = get_texture(tex_title_sonic);
		sprites[spr_title_sonic] = make_sprite(t, 0, 0, 103, 120, 103 / 2, 120 / 2, 7);
	}

	{
		textures[tex_title_label] = load_texture_from_file("textures/title_label.png");
		const Texture& t = get_texture(tex_title_label);
		sprites[spr_title_label] = make_sprite(t, 0, 0, t.width, t.height, t.width / 2, t.height / 2);
	}

	{
		textures[tex_title_mountains_left] = load_texture_from_file("textures/title_mountains_left.png");
		const Texture& t = get_texture(tex_title_mountains_left);
		sprites[spr_title_mountains_left] = make_sprite(t, 0, 0, t.width, t.height, 0, t.height);
	}

	{
		textures[tex_title_mountains_right] = load_texture_from_file("textures/title_mountains_right.png");
		const Texture& t = get_texture(tex_title_mountains_right);
		sprites[spr_title_mountains_right] = make_sprite(t, 0, 0, t.width, t.height, t.width, t.height);
	}

	textures[tex_title_water] = load_texture_from_file("textures/title_water.png");
	textures[tex_title_water_palette] = load_texture_from_file("textures/title_water_palette.png");
	textures[tex_title_clouds] = load_texture_from_file("textures/title_clouds.png", GL_NEAREST, GL_REPEAT);
}

void load_assets_for_editor() {
	textures[tex_editor_sprites] = load_texture_from_file("textures/editor_sprites.png");

	{
		const Texture& t = get_texture(tex_editor_sprites);

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

	for (int i = 0; i < NUM_SOUNDS; i++) {
		if (sounds[i]) {
			Mix_FreeChunk(sounds[i]);
			sounds[i] = nullptr;
		}
	}
}

const Texture& get_texture(u32 texture_index) {
	Assert(texture_index < NUM_TEXTURES);

	if (textures[texture_index].ID == 0) {
		log_error("Trying to access texture %u that hasn't been loaded.", texture_index);
	}

	return textures[texture_index];
}

const Sprite& get_sprite(u32 sprite_index) {
	Assert(sprite_index < NUM_SPRITES);

	if (sprites[sprite_index].frames.count == 0) {
		log_error("Trying to access sprite %u that hasn't been loaded.", sprite_index);
	}

	return sprites[sprite_index];
}

const Font& get_font(u32 font_index) {
	Assert(font_index < NUM_FONTS);

	if (fonts[font_index].glyphs.count == 0) {
		log_error("Trying to access font %u that hasn't been loaded.", font_index);
	}

	return fonts[font_index];
}

Mix_Chunk* get_sound(u32 sound_index) {
	Assert(sound_index < NUM_SOUNDS);

	if (!sounds[sound_index]) {
		log_error("Trying to access sound %u that hasn't been loaded.", sound_index);
	}

	return sounds[sound_index];
}
