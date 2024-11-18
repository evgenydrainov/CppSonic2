#pragma once

#include "common.h"

#include "font.h"
#include "texture.h"

#define PLAYER_STATE_ENUM(X) \
	X(STATE_GROUND) \
	X(STATE_ROLL) \
	X(STATE_AIR) \
	X(STATE_DEBUG)

DEFINE_NAMED_ENUM(PlayerState, PLAYER_STATE_ENUM)

enum PlayerMode {
	MODE_FLOOR,
	MODE_RIGHT_WALL,
	MODE_CEILING,
	MODE_LEFT_WALL,
};

enum anim_index {
	anim_crouch,
	anim_idle,
	anim_look_up,
	anim_peelout,
	anim_roll,
	anim_run,
	anim_skid,
	anim_spindash,
	anim_walk,

	NUM_ANIMS,
};

struct Player {
	vec2 pos;
	vec2 speed;

	float ground_speed;
	float ground_angle;

	PlayerState state;

	int layer;

	anim_index anim;
	anim_index next_anim;
	int frame_index;
	int frame_duration;
	float frame_timer;

	int facing = 1;

	float spinrev;
	float control_lock;
	bool jumped;
	bool peelout;

	PlayerMode prev_mode;
	vec2 prev_radius;
};

struct Tile {
	unsigned int index : 16;

	unsigned int hflip : 1;
	unsigned int vflip : 1;
	unsigned int top_solid : 1;
	unsigned int lrb_solid : 1; // left right bottom
};

static_assert(sizeof(Tile) == 4);

struct SensorResult {
	Tile tile;
	int dist;
	int tile_x;
	int tile_y;
	bool found;
};

struct Tileset {
	int count;

	array<u8> heights;
	array<u8> widths;
	array<float> angles;
};

struct Tilemap {
	int width;
	int height;

	array<Tile> tiles_a;
	array<Tile> tiles_b;
};

struct Game {
	Player player;

	vec2 camera_pos;
	float camera_lock;

	Font font;
	Font font_consolas;

	Tileset ts;
	Texture tileset_texture;
	int tileset_width;
	int tileset_height;

	Texture heightmap;
	Texture widthmap;

	Tilemap tm;

	Texture anim_textures[NUM_ANIMS];

	vec2 mouse_world_pos;

	bool collision_test;
	bool show_height;
	bool show_width;

	bool skip_frame;
	bool frame_advance;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);

	void load_level(const char* path);
};

extern Game game;

#if defined(DEVELOPER) && !defined(EDITOR)
bool console_callback(string str, void* userdata);
#endif

void free_tilemap(Tilemap* tm);
void free_tileset(Tileset* ts);

void read_tilemap_old_format(Tilemap* tm, const char* fname);
void read_tileset_old_format(Tileset* ts, const char* fname);

void write_tilemap(const Tilemap& tm, const char* fname);
void write_tileset(const Tileset& ts, const char* fname);

void read_tilemap(Tilemap* tm, const char* fname);
void read_tileset(Tileset* ts, const char* fname);

void gen_heightmap_texture(Texture* heightmap, const Tileset& ts, const Texture& tileset_texture);
void gen_widthmap_texture (Texture* widthmap,  const Tileset& ts, const Texture& tileset_texture);

inline Tile get_tile_a(const Tilemap& tm, int tile_x, int tile_y) {
	if ((0 <= tile_x && tile_x < tm.width) && (0 <= tile_y && tile_y < tm.height)) {
		return tm.tiles_a[tile_x + tile_y * tm.width];
	}
	return {};
}

inline Tile get_tile_b(const Tilemap& tm, int tile_x, int tile_y) {
	if ((0 <= tile_x && tile_x < tm.width) && (0 <= tile_y && tile_y < tm.height)) {
		return tm.tiles_b[tile_x + tile_y * tm.width];
	}
	return {};
}

inline Tile get_tile(const Tilemap& tm, int tile_x, int tile_y, int layer) {
	if (layer == 0) {
		return get_tile_a(tm, tile_x, tile_y);
	} else if (layer == 1) {
		return get_tile_b(tm, tile_x, tile_y);
	}
	return {};
}

inline array<u8> get_tile_heights(const Tileset& ts, int tile_index) {
	Assert(tile_index < ts.count);

	array<u8> result = {};
	result.data = ts.heights.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline array<u8> get_tile_widths(const Tileset& ts, int tile_index) {
	Assert(tile_index < ts.count);

	array<u8> result = {};
	result.data = ts.widths.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline float get_tile_angle(const Tileset& ts, int tile_index) {
	Assert(tile_index < ts.count);
	return ts.angles[tile_index];
}
