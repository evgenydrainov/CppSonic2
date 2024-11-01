#pragma once

#include "common.h"

#include "font.h"
#include "texture.h"

enum PlayerState {
	PLAYER_STATE_GROUND,
	PLAYER_STATE_ROLL,
	PLAYER_STATE_AIR,
};

struct Player {
	vec2 pos;
	vec2 speed;

	float ground_speed;
	float ground_angle;

	vec2 radius = {9, 19};

	PlayerState state;
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

	Texture texture_crouch;
	Texture texture_idle;
	Texture texture_look_up;
	Texture texture_peelout;
	Texture texture_roll;
	Texture texture_run;
	Texture texture_skid;
	Texture texture_spindash;
	Texture texture_walk;

	vec2 mouse_world_pos;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);

	void load_level(const char* path);

	void load_tilemap_old_format(const char* fname);
	void load_tileset_old_format(const char* fname);

	Tile get_tile_a(int tile_x, int tile_y) {
		if ((0 <= tile_x && tile_x < tilemap_width) && (0 <= tile_y && tile_y < tilemap_height)) {
			return tiles_a[tile_x + tile_y * tilemap_width];
		}
		return {};
	}

	Tile get_tile_b(int tile_x, int tile_y) {
		if ((0 <= tile_x && tile_x < tilemap_width) && (0 <= tile_y && tile_y < tilemap_height)) {
			return tiles_b[tile_x + tile_y * tilemap_width];
		}
		return {};
	}

	Tile get_tile(int tile_x, int tile_y, int layer) {
		if (layer == 0) {
			return get_tile_a(tile_x, tile_y);
		} else if (layer == 1) {
			return get_tile_b(tile_x, tile_y);
		}
		return {};
	}

	array<u8> get_tile_heights(int tile_index) {
		Assert(tile_index < tileset_width * tileset_height);
		array<u8> result;
		result.data = &tile_heights[tile_index * 16];
		result.count = 16;
		return result;
	}

	array<u8> get_tile_widths(int tile_index) {
		Assert(tile_index < tileset_width * tileset_height);
		array<u8> result;
		result.data = &tile_widths[tile_index * 16];
		result.count = 16;
		return result;
	}

	float get_tile_angle(int tile_index) {
		Assert(tile_index < tileset_width * tileset_height);
		return tile_angles[tile_index];
	}

	SensorResult sensor_check_down(float x, float y, int layer);
};

extern Game game;
