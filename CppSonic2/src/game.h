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
	anim_balance,
	anim_balance2,
	anim_push,

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

#define OBJ_TYPE_ENUM(X) \
	X(OBJ_PLAYER_INIT_POS) \
	X(OBJ_LAYER_SET) \
	X(OBJ_LAYER_FLIP) \
	X(OBJ_RING)

DEFINE_NAMED_ENUM(ObjType, OBJ_TYPE_ENUM)

typedef u32 instance_id;

enum {
	FLAG_LAYER_FLIP_GROUNDED = 1 << 16,
};

struct Object {
	instance_id id;
	ObjType type;
	u32 flags;

	vec2 pos;

	union {
		struct {
			int layer;
			vec2 radius;
		} layset; // OBJ_LAYER_SET

		struct {
			vec2 radius;
		} layflip; // OBJ_LAYER_FLIP
	};
};

struct Sprite;
const Sprite& get_object_sprite(ObjType type);

vec2 get_object_size(const Object& o);

constexpr size_t MAX_OBJECTS = 10'000;

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
	array<u8> heights;
	array<u8> widths;
	array<float> angles;
};

struct Tilemap {
	int width;
	int height;

	// Solid and drawn
	array<Tile> tiles_a;

	// Solid and not drawn. For alternate collision layer (for loops).
	array<Tile> tiles_b;

	// Not solid and drawn. For visuals (grass).
	array<Tile> tiles_c;
};

struct Game {
	Player player;

	int player_score;
	float player_time;
	int player_rings;

	bump_array<Object> objects;

	vec2 camera_pos;
	float camera_lock;

	Tileset ts;
	Texture tileset_texture;
	int tileset_width;
	int tileset_height;

	Texture heightmap;
	Texture widthmap;

	Tilemap tm;

	vec2 mouse_world_pos;

	bool collision_test;
	bool show_height;
	bool show_width;
	bool show_debug_info;
	bool show_player_hitbox;

	bool skip_frame;
	bool frame_advance;

	void init(int argc, char* argv[]);
	void deinit();

	void update(float delta);
	void draw(float delta);
	void late_draw(float delta);

	void load_level(const char* path);
};

extern Game game;

#ifdef DEVELOPER
extern array<string> g_ConsoleCommands;

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

void write_objects(array<Object>       objects, const char* fname);
void read_objects (bump_array<Object>* objects, const char* fname);

void gen_heightmap_texture(Texture* heightmap, const Tileset& ts, const Texture& tileset_texture);
void gen_widthmap_texture (Texture* widthmap,  const Tileset& ts, const Texture& tileset_texture);

inline Tile get_tile(const Tilemap& tm, int tile_x, int tile_y, int layer) {
	Assert((0 <= tile_x && tile_x < tm.width) && (0 <= tile_y && tile_y < tm.height));
	Assert(layer >= 0 && layer < 3);
	if (layer == 0) {
		return tm.tiles_a[tile_x + tile_y * tm.width];
	} else if (layer == 1) {
		return tm.tiles_b[tile_x + tile_y * tm.width];
	} else {
		return tm.tiles_c[tile_x + tile_y * tm.width];
	}
}

inline Tile get_tile_safe(const Tilemap& tm, int tile_x, int tile_y, int layer) {
	if ((0 <= tile_x && tile_x < tm.width) && (0 <= tile_y && tile_y < tm.height)) {
		if (layer == 0) {
			return tm.tiles_a[tile_x + tile_y * tm.width];
		} else if (layer == 1) {
			return tm.tiles_b[tile_x + tile_y * tm.width];
		} else {
			return tm.tiles_c[tile_x + tile_y * tm.width];
		}
	}
	return {};
}

inline void set_tile(Tilemap* tm, int tile_x, int tile_y, int layer, Tile tile) {
	Assert((0 <= tile_x && tile_x < tm->width) && (0 <= tile_y && tile_y < tm->height));
	Assert(layer >= 0 && layer < 3);
	if (layer == 0) {
		tm->tiles_a[tile_x + tile_y * tm->width] = tile;
	} else if (layer == 1) {
		tm->tiles_b[tile_x + tile_y * tm->width] = tile;
	} else {
		tm->tiles_c[tile_x + tile_y * tm->width] = tile;
	}
}

inline void set_tile_safe(Tilemap* tm, int tile_x, int tile_y, int layer, Tile tile) {
	if ((0 <= tile_x && tile_x < tm->width) && (0 <= tile_y && tile_y < tm->height)) {
		if (layer == 0) {
			tm->tiles_a[tile_x + tile_y * tm->width] = tile;
		} else if (layer == 1) {
			tm->tiles_b[tile_x + tile_y * tm->width] = tile;
		} else {
			tm->tiles_c[tile_x + tile_y * tm->width] = tile;
		}
	}
}

inline array<u8> get_tile_heights(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.heights.count);

	array<u8> result = {};
	result.data = ts.heights.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline array<u8> get_tile_widths(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.widths.count);

	array<u8> result = {};
	result.data = ts.widths.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline float get_tile_angle(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.angles.count);
	return ts.angles[tile_index];
}

