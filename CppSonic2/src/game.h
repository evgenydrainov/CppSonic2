#pragma once

#include "common.h"

#include "renderer.h"
#include "font.h"

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
	anim_rise,
	anim_hurt,

	NUM_ANIMS,
};

struct Player {
	vec2 pos;
	vec2 speed;

	float ground_speed;
	float ground_angle;

	PlayerState state;

	int layer; // collision layer
	int priority = 1; // visual priority (high by default)

	anim_index anim      = anim_idle;
	anim_index next_anim = anim_idle;
	int frame_index;
	int frame_duration;
	float frame_timer;

	int facing = 1;

	float spinrev;
	float control_lock;
	float invulnerable;
	float ignore_rings;

	bool jumped;
	bool peelout;
	bool pushing;
	bool landed_on_solid_object;

	PlayerMode prev_mode;
	vec2 prev_radius;

	PlayerMode old_mode;
	PlayerMode new_mode;
	float change_mode_timer;
	float dont_change_mode_timer;

	u32 input;
	u32 input_press;
	u32 input_release;
};

// serialized
#define OBJ_TYPE_ENUM(X) \
	X(OBJ_PLAYER_INIT_POS,            0) \
	X(OBJ_LAYER_SET_DEPRECATED,       1) \
	X(OBJ_LAYER_FLIP_DEPRECATED,      2) \
	X(OBJ_RING,                       3) \
	X(OBJ_MONITOR,                    4) \
	X(OBJ_SPRING,                     5) \
	X(OBJ_MONITOR_BROKEN,             6) \
	X(OBJ_MONITOR_ICON,               7) \
	X(OBJ_SPIKE,                      8) \
	X(OBJ_RING_DROPPED,               9) \
	X(OBJ_MOVING_PLATFORM,           10) \
	X(OBJ_LAYER_SWITCHER_VERTICAL,   11) \
	X(OBJ_LAYER_SWITCHER_HORIZONTAL, 12) \
	X(OBJ_SPRING_DIAGONAL,           13) \
	X(OBJ_MOSQUI,                    14) \
	X(OBJ_FLOWER,                    15)

DEFINE_NAMED_ENUM_WITH_VALUES(ObjType, OBJ_TYPE_ENUM)

typedef u32 instance_id;

enum {
	FLAG_INSTANCE_DEAD = 1 << 0,

	FLAG_MONITOR_ICON_GOT_REWARD = 1 << 16,

	FLAG_MOSQUI_IS_DIVING = 1 << 16,

	// serialized
	FLAG_SPRING_SMALL_HITBOX = 1 << 16,

	// serialized
	FLAG_LAYER_SWITCHER_GROUND_ONLY = 1 << 16,

	// serialized
	FLAG_PLATFORM_CIRCULAR_MOVEMENT = 1 << 16,
};

// serialized
#define MONITOR_ICON_ENUM(X) \
	X(MONITOR_ICON_ROBOTNIK) \
	X(MONITOR_ICON_SUPER_RING) \
	X(MONITOR_ICON_POWER_SNEAKERS) \
	X(MONITOR_ICON_SHIELD) \
	X(MONITOR_ICON_BUBBLE_SHIELD) \
	X(MONITOR_ICON_LIGHTNING_SHIELD) \
	X(MONITOR_ICON_FIRE_SHIELD) \
	X(MONITOR_ICON_INVINCIBILITY) \
	X(MONITOR_ICON_SUPER) \
	X(MONITOR_ICON_1UP) \
	\
	X(NUM_MONITOR_ICONS)

DEFINE_NAMED_ENUM(MonitorIcon, MONITOR_ICON_ENUM)

// serialized
#define SPRING_COLOR_ENUM(X) \
	X(SPRING_COLOR_YELLOW) \
	X(SPRING_COLOR_RED) \
	\
	X(NUM_SPING_COLORS)

DEFINE_NAMED_ENUM(SpringColor, SPRING_COLOR_ENUM)

// serialized
#define DIRECTION_ENUM(X) \
	X(DIR_RIGHT) \
	X(DIR_UP) \
	X(DIR_LEFT) \
	X(DIR_DOWN) \
	\
	X(NUM_DIRS)

DEFINE_NAMED_ENUM(Direction, DIRECTION_ENUM)

struct Object {
	instance_id id;
	ObjType type;
	u32 flags;

	vec2 pos;
	vec2 speed;
	vec2 radius;
	float frame_index;

	vec2 start_pos;

	union {
		struct {
			int layer;
		} layset; // OBJ_LAYER_SET

		struct {
			MonitorIcon icon;
			float timer; // for OBJ_MONITOR_ICON
		} monitor; // OBJ_MONITOR and OBJ_MONITOR_ICON

		struct {
			SpringColor color;
			Direction direction;

			bool animating;
			float frame_index;
		} spring; // OBJ_SPRING and OBJ_SPRING_DIAGONAL

		struct {
			Direction direction;
		} spike; // OBJ_SPIKE

		struct {
			vec2 speed;
			float anim_spd;
			float frame_index;
			float lifetime;
		} ring_dropped; // OBJ_RING_DROPPED

		struct {
			u32 sprite_index;
			vec2 offset;
			float time_multiplier;

			vec2 init_pos;
			vec2 prev_pos;

			instance_id mounts[2];
		} mplatform; // OBJ_MOVING_PLATFORM

		struct {
			int layer_1;
			int layer_2;
			int priority_1;
			int priority_2;
			int current_side;
		} layswitch; // OBJ_LAYER_SWITCHER_HORIZONTAL and OBJ_LAYER_SWITCHER_VERTICAL

		struct {
			float fly_distance;
		} mosqui; // OBJ_MOSQUI

		struct {
			float timer;
		} flower; // OBJ_FLOWER
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
	int tile_x; // @Unused
	int tile_y; // @Unused
	bool found; // @Unused
};

struct Tileset {
	array<u8> heights;
	array<u8> widths;
	array<float> angles;
};

struct Tilemap {
	int width;
	int height;

	// has collision, visible
	array<Tile> tiles_a;

	// has collision, not visible
	// for alternate collision layer
	array<Tile> tiles_b;

	// no collision, visible
	// for grass
	array<Tile> tiles_c;

	// no collision, visible
	// for background
	array<Tile> tiles_d;
};

struct Game {
	Player player;

	int player_score;
	float player_time;
	int player_rings;
	int player_lives = 3;

	bump_array<Object> objects;

	instance_id next_id = 1;

	vec2 camera_pos;
	vec2 camera_pos_real;
	float camera_lock;

	Tileset ts;
	Texture tileset_texture;
	int tileset_width;
	int tileset_height;

	Texture heightmap;
	Texture widthmap;

	Tilemap tm;

	float water_pos_y;

	bump_array<Rectf> debug_rects;

	float time_seconds;
	float time_frames;

	enum Titlecard_State {
		TITLECARD_IN,
		TITLECARD_WAIT,
		TITLECARD_OUT,
		TITLECARD_FINISHED,
	};

	static constexpr float TITLECARD_IN_TIME = 30;
	static constexpr float TITLECARD_WAIT_TIME = 70;

	Titlecard_State titlecard_state;
	float titlecard_timer;
	float titlecard_t;

	enum Pause_State {
		PAUSE_NOT_PAUSED,
		PAUSE_IN,
		PAUSE_PAUSED,
		PAUSE_BLINK,
		PAUSE_OUT,
	};

	static constexpr float PAUSE_IN_TIME = 15;
	static constexpr float PAUSE_BLINK_TIME = 30;

#ifdef DEVELOPER
	static constexpr int PAUSE_MENU_NUM_ITEMS = 4;
#else
	static constexpr int PAUSE_MENU_NUM_ITEMS = 3;
#endif

	Pause_State pause_state;
	float pause_menu_timer;
	float pause_menu_t;
	int pause_menu_cursor;
	u32 pause_last_pressed_time;

#if defined(__ANDROID__) || defined(PRETEND_MOBILE)
	u32 mobile_input_state;
	u32 mobile_input_state_press;
	u32 mobile_input_state_release;
#endif

	bool collision_test;
	bool show_height;
	bool show_width;
	bool show_player_hitbox;
	bool show_hitboxes;

	void init(int argc, char* argv[]);
	void deinit();

	void update(float delta);
	void update_camera(float delta);
	void update_gameplay(float delta);
	void update_touch_input();
	void update_pause_menu(float delta);

	void draw(float delta);

	void load_level(const char* path);
	Object* find_object(instance_id id);
};

extern Game game;

void draw_tilemap_layer(const Tilemap& tm,
						int layer_index,
						const Texture& tileset_texture,
						int xfrom, int yfrom,
						int xto, int yto,
						vec4 color);

void draw_objects(array<Object> objects,
				  float time_frames,
				  bool show_editor_objects,
				  bool cull_objects);

void free_tilemap(Tilemap* tm);
void free_tileset(Tileset* ts);

void read_tilemap_old_format(Tilemap* tm, const char* fname);
void read_tileset_old_format(Tileset* ts, const char* fname);

void write_tilemap(const Tilemap& tm, const char* fname);
void write_tileset(const Tileset& ts, const char* fname);

bool read_tilemap(Tilemap* tm, const char* fname);
void read_tileset(Tileset* ts, const char* fname);

void write_objects(array<Object>       objects, const char* fname);
bool read_objects (bump_array<Object>* objects, const char* fname);

void gen_heightmap_texture(Texture* heightmap, const Tileset& ts, const Texture& tileset_texture);
void gen_widthmap_texture (Texture* widthmap,  const Tileset& ts, const Texture& tileset_texture);

inline array<Tile> get_tiles_array(const Tilemap& tm, int layer_index) {
	switch (layer_index) {
		case 0: return tm.tiles_a;
		case 1: return tm.tiles_b;
		case 2: return tm.tiles_c;
		case 3: return tm.tiles_d;
	}

	Assert(!"Invalid layer index.");
	return {};
}

inline Tile get_tile(const Tilemap& tm, int tile_x, int tile_y, int layer_index) {
	Assert(tile_x >= 0
		   && tile_x < tm.width
		   && tile_y >= 0
		   && tile_y < tm.height);

	return get_tiles_array(tm, layer_index)[tile_x + tile_y * tm.width];
}

inline Tile get_tile_safe(const Tilemap& tm, int tile_x, int tile_y, int layer_index) {
	if (!(tile_x >= 0
		  && tile_x < tm.width
		  && tile_y >= 0
		  && tile_y < tm.height))
	{
		return {};
	}

	return get_tiles_array(tm, layer_index)[tile_x + tile_y * tm.width];
}

inline Tile get_tile_by_index(const Tilemap& tm, int tile_index, int layer_index) {
	Assert(tile_index >= 0
		   && tile_index < tm.width * tm.height);

	return get_tiles_array(tm, layer_index)[tile_index];
}

inline void set_tile(Tilemap* tm, int tile_x, int tile_y, int layer_index, Tile tile) {
	Assert(tile_x >= 0
		   && tile_x < tm->width
		   && tile_y >= 0
		   && tile_y < tm->height);

	get_tiles_array(*tm, layer_index)[tile_x + tile_y * tm->width] = tile;
}

inline void set_tile_safe(Tilemap* tm, int tile_x, int tile_y, int layer_index, Tile tile) {
	if (!(tile_x >= 0
		   && tile_x < tm->width
		   && tile_y >= 0
		   && tile_y < tm->height))
	{
		return;
	}

	get_tiles_array(*tm, layer_index)[tile_x + tile_y * tm->width] = tile;
}

inline void set_tile_by_index(Tilemap* tm, int tile_index, int layer_index, Tile tile) {
	Assert(tile_index >= 0
		   && tile_index < tm->width * tm->height);

	get_tiles_array(*tm, layer_index)[tile_index] = tile;
}

inline array<u8> get_tile_heights(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.heights.count/16);

	array<u8> result = {};
	result.data = ts.heights.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline array<u8> get_tile_widths(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.widths.count/16);

	array<u8> result = {};
	result.data = ts.widths.data + tile_index * 16;
	result.count = 16;

	return result;
}

inline float get_tile_angle(const Tileset& ts, int tile_index) {
	Assert(tile_index >= 0 && tile_index < ts.angles.count);
	return ts.angles[tile_index];
}

