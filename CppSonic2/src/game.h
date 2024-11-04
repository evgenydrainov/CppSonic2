#pragma once

#include "common.h"

#include "font.h"
#include "texture.h"

enum PlayerState {
	STATE_GROUND,
	STATE_ROLL,
	STATE_AIR,
	STATE_DEBUG,
};

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

struct Game {
	Player player;

	vec2 camera_pos;
	float camera_lock;

	Font font;

	Texture tileset_texture;
	int tileset_width;
	int tileset_height;

	array<u8> tile_heights;
	array<u8> tile_widths;
	array<float> tile_angles;

	Texture heightmap;
	Texture widthmap;

	array<Tile> tiles_a;
	array<Tile> tiles_b;
	int tilemap_width;
	int tilemap_height;

	Texture anim_textures[NUM_ANIMS];

	vec2 mouse_world_pos;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);

	void load_level(const char* path);

	void load_tilemap_old_format(const char* fname);
	void load_tileset_old_format(const char* fname);
};

extern Game game;

inline Tile get_tile_a(int tile_x, int tile_y) {
	if ((0 <= tile_x && tile_x < game.tilemap_width) && (0 <= tile_y && tile_y < game.tilemap_height)) {
		return game.tiles_a[tile_x + tile_y * game.tilemap_width];
	}
	return {};
}

inline Tile get_tile_b(int tile_x, int tile_y) {
	if ((0 <= tile_x && tile_x < game.tilemap_width) && (0 <= tile_y && tile_y < game.tilemap_height)) {
		return game.tiles_b[tile_x + tile_y * game.tilemap_width];
	}
	return {};
}

inline Tile get_tile(int tile_x, int tile_y, int layer) {
	if (layer == 0) {
		return get_tile_a(tile_x, tile_y);
	} else if (layer == 1) {
		return get_tile_b(tile_x, tile_y);
	}
	return {};
}

inline array<u8> get_tile_heights(int tile_index) {
	Assert(tile_index < game.tileset_width * game.tileset_height);
	array<u8> result;
	result.data = &game.tile_heights[tile_index * 16];
	result.count = 16;
	return result;
}

inline array<u8> get_tile_widths(int tile_index) {
	Assert(tile_index < game.tileset_width * game.tileset_height);
	array<u8> result;
	result.data = &game.tile_widths[tile_index * 16];
	result.count = 16;
	return result;
}

inline float get_tile_angle(int tile_index) {
	Assert(tile_index < game.tileset_width * game.tileset_height);
	return game.tile_angles[tile_index];
}
