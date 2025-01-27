#include "game.h"

#include "renderer.h"
#include "window_creation.h"
#include "package.h"
#include "console.h"
#include "assets.h"
#include "particle_system.h"
#include "program.h"

Game game;

void Game::load_level(const char* path) {
	log_info("Loading level %s...", path);

	// load tileset texture
	static char buf[512];
	stb_snprintf(buf, sizeof(buf), "%s/Tileset.png", path);

	tileset_texture = load_texture_from_file(buf);
	if (tileset_texture.ID == 0) {
		log_error("Couldn't load tileset texture for level %s", path);
		return;
	}

	Assert(tileset_texture.width  % 16 == 0);
	Assert(tileset_texture.height % 16 == 0);

	tileset_width  = tileset_texture.width  / 16;
	tileset_height = tileset_texture.height / 16;

	// load tilemap data
	stb_snprintf(buf, sizeof(buf), "%s/Tilemap.bin", path);
	read_tilemap(&tm, buf);

	// load tileset data
	stb_snprintf(buf, sizeof(buf), "%s/Tileset.bin", path);
	read_tileset(&ts, buf);

	// load object data
	objects = malloc_bump_array<Object>(MAX_OBJECTS);
	stb_snprintf(buf, sizeof(buf), "%s/Objects.bin", path);
	read_objects(&objects, buf);

	// give IDs to the objects
	For (it, objects) {
		it->id = next_id++;
	}

	// search for player init pos
	{
		bool found = false;

		For (it, objects) {
			if (it->type == OBJ_PLAYER_INIT_POS) {
				player.pos = it->pos;
				found = true;
				break;
			}
		}

		if (!found) {
			log_error("Couldn't find OBJ_PLAYER_INIT_POS object.");

			player.pos = {80, 944};
		}
	}

	gen_heightmap_texture(&heightmap, ts, tileset_texture);
	gen_widthmap_texture (&widthmap,  ts, tileset_texture);
}

void Game::init(int argc, char* argv[]) {
	init_particles();

	{
		const char* path = "levels/EEZ_Act1_tiled";

		if (argc >= 2 && strcmp(argv[1], "--game") == 0) {
			if (argc >= 3) path = argv[2];
		}

		load_level(path);
	}

	// Set camera pos after loading the level.
	camera_pos.x = player.pos.x - window.game_width  / 2;
	camera_pos.y = player.pos.y - window.game_height / 2;

	show_debug_info = true;
	// show_player_hitbox = true;
}

void Game::deinit() {
	free(objects.data);

	free_tileset(&ts);
	free_texture(&tileset_texture);

	free_texture(&heightmap);
	free_texture(&widthmap);

	free_tilemap(&tm);

	deinit_particles();
}

constexpr float PLAYER_ACC = 0.046875f;
constexpr float PLAYER_DEC = 0.5f;
constexpr float PLAYER_FRICTION = 0.046875f;
constexpr float PLAYER_TOP_SPEED = 6.0f;
constexpr float PLAYER_PUSH_RADIUS = 10.0f;

static bool player_is_grounded(Player* p) {
	return (p->state == STATE_GROUND
			|| p->state == STATE_ROLL);
}

static PlayerMode player_get_mode(Player* p) {
	if (!player_is_grounded(p)) {
		return MODE_FLOOR;
	}

	// The Floor and Ceiling mode ranges are slightly bigger than the wall ranges.
	float a = angle_wrap(p->ground_angle);
	if (a < 46 || a >= 315) {
		return MODE_FLOOR;
	}
	if (a >= 135 && a < 226) {
		return MODE_CEILING;
	}

	if (a < 180) {
		return MODE_RIGHT_WALL;
	} else {
		return MODE_LEFT_WALL;
	}
}

static PlayerMode player_get_push_mode(Player* p) {
	if (!player_is_grounded(p)) {
		return MODE_FLOOR;
	}

	// The Floor mode range is slightly smaller than the wall ranges.
	float a = angle_wrap(p->ground_angle);
	if (a >= 45 && a < 136) {
		return MODE_RIGHT_WALL;
	}
	if (a >= 255 && a < 316) {
		return MODE_LEFT_WALL;
	}

	if (90 < a && a < 270) {
		return MODE_CEILING;
	} else {
		return MODE_FLOOR;
	}
}

static bool player_is_on_a_wall(Player* p) {
	PlayerMode mode = player_get_mode(p);
	if (mode == MODE_LEFT_WALL || mode == MODE_RIGHT_WALL) {
		return true;
	}
	return false;
}

static void apply_slope_factor(Player* p, float delta) {
	// Adjust Ground Speed based on current Ground Angle (Slope Factor).

	const float PLAYER_SLOPE_FACTOR_NORMAL = 0.125f;
	const float PLAYER_SLOPE_FACTOR_ROLLUP = 0.078125f;
	const float PLAYER_SLOPE_FACTOR_ROLLDOWN = 0.3125f;

	if (player_get_mode(p) != MODE_CEILING && p->ground_speed != 0.0f) {
		float factor = PLAYER_SLOPE_FACTOR_NORMAL;

		if (p->state == STATE_ROLL) {
			if (signf(p->ground_speed) == signf(dsin(p->ground_angle))) {
				factor = PLAYER_SLOPE_FACTOR_ROLLUP;
			} else {
				factor = PLAYER_SLOPE_FACTOR_ROLLDOWN;
			}
		}

		p->ground_speed -= factor * dsin(p->ground_angle) * delta;
	}
}

static bool player_is_moving_mostly_right(Player* p) {
	if (p->speed.x == 0.0f && p->speed.y == 0.0f) return false;
	float player_move_dir = angle_wrap(point_direction(0.0f, 0.0f, p->speed.x, p->speed.y));
	return (player_move_dir < 46.0f) || (316.0f <= player_move_dir);
}

static bool player_is_moving_mostly_up(Player* p) {
	if (p->speed.x == 0.0f && p->speed.y == 0.0f) return false;
	float player_move_dir = angle_wrap(point_direction(0.0f, 0.0f, p->speed.x, p->speed.y));
	return 46.0f <= player_move_dir && player_move_dir < 136.0f;
}

static bool player_is_moving_mostly_left(Player* p) {
	if (p->speed.x == 0.0f && p->speed.y == 0.0f) return false;
	float player_move_dir = angle_wrap(point_direction(0.0f, 0.0f, p->speed.x, p->speed.y));
	return 136.0f <= player_move_dir && player_move_dir < 226.0f;
}

static bool player_is_moving_mostly_down(Player* p) {
	if (p->speed.x == 0.0f && p->speed.y == 0.0f) return false;
	float player_move_dir = angle_wrap(point_direction(0.0f, 0.0f, p->speed.x, p->speed.y));
	return 226.0f <= player_move_dir && player_move_dir < 316.0f;
}

static bool are_ground_sensors_active(Player* p) {
	if (player_is_grounded(p)) {
		return true;
	} else {
		// return (player_is_moving_mostly_right(p)
		// 		|| player_is_moving_mostly_left(p)
		// 		|| player_is_moving_mostly_down(p));

		return p->speed.y >= 0.0f;
	}
}

static bool are_ceiling_sensors_active(Player* p) {
	if (player_is_grounded(p)) {
		return false;
	} else {
		return p->speed.y < 0.0f;
	}
}

static bool is_push_sensor_e_active(Player* p) {
	if (player_is_grounded(p)) {
		float a = angle_wrap(p->ground_angle);
		if (!(a <= 90.0f || a >= 270.0f)) {
			return false;
		}

		return (p->ground_speed < 0.0f);
	} else {
		// return (player_is_moving_mostly_left(p)
		// 		|| player_is_moving_mostly_up(p)
		// 		|| player_is_moving_mostly_down(p));

		return p->speed.x < 0.0f;
	}
}

static bool is_push_sensor_f_active(Player* p) {
	if (player_is_grounded(p)) {
		float a = angle_wrap(p->ground_angle);
		if (!(a <= 90.0f || a >= 270.0f)) {
			return false;
		}

		return (p->ground_speed > 0.0f);
	} else {
		// return (player_is_moving_mostly_right(p)
		// 		|| player_is_moving_mostly_up(p)
		// 		|| player_is_moving_mostly_down(p));

		return p->speed.x > 0.0f;
	}
}

static bool player_is_small_radius(Player* p) {
	if (p->state == STATE_GROUND) {
		return false;
	}

	if (p->state == STATE_ROLL) {
		return true;
	}

	return p->anim == anim_roll;
}

static vec2 player_get_radius(Player* p) {
	if (player_is_small_radius(p)) {
		return {7, 14};
	} else {
		return {9, 19};
	}
}

// pass radius and mode because when drawing we want prev_radius and prev_mode
static Rectf player_get_rect(Player* p, vec2 radius, PlayerMode mode) {
	Rectf rect = {};

	switch (mode) {
		case MODE_FLOOR:
		case MODE_CEILING: {
			rect.x = p->pos.x - radius.x;
			rect.y = p->pos.y - radius.y;
			rect.w = radius.x * 2;
			rect.h = radius.y * 2;
			break;
		}
		case MODE_RIGHT_WALL:
		case MODE_LEFT_WALL: {
			rect.x = p->pos.x - radius.y;
			rect.y = p->pos.y - radius.x;
			rect.w = radius.y * 2;
			rect.h = radius.x * 2;
			break;
		}
	}

	return rect;
}

// common case
static Rectf player_get_rect(Player* p) {
	auto radius = player_get_radius(p);
	auto mode   = player_get_mode(p);
	return player_get_rect(p, radius, mode);
}

static void get_ground_sensors_pos(Player* p, vec2* sensor_a, vec2* sensor_b) {
	vec2 radius = player_get_radius(p);
	switch (player_get_mode(p)) {
		case MODE_FLOOR: {
			sensor_a->x = p->pos.x - radius.x;
			sensor_a->y = p->pos.y + radius.y;
			sensor_b->x = p->pos.x + radius.x;
			sensor_b->y = p->pos.y + radius.y;
			break;
		}
		case MODE_RIGHT_WALL: {
			sensor_a->x = p->pos.x + radius.y;
			sensor_a->y = p->pos.y + radius.x;
			sensor_b->x = p->pos.x + radius.y;
			sensor_b->y = p->pos.y - radius.x;
			break;
		}
		case MODE_CEILING: {
			sensor_a->x = p->pos.x + radius.x;
			sensor_a->y = p->pos.y - radius.y;
			sensor_b->x = p->pos.x - radius.x;
			sensor_b->y = p->pos.y - radius.y;
			break;
		}
		case MODE_LEFT_WALL: {
			sensor_a->x = p->pos.x - radius.y;
			sensor_a->y = p->pos.y - radius.x;
			sensor_b->x = p->pos.x - radius.y;
			sensor_b->y = p->pos.y + radius.x;
			break;
		}
	}
}

static void get_ceiling_sensors_pos(Player* p, vec2* sensor_c, vec2* sensor_d) {
	vec2 radius = player_get_radius(p);

	radius.y = -radius.y;

	// same as ground sensors
	switch (player_get_mode(p)) {
		case MODE_FLOOR: {
			sensor_c->x = p->pos.x - radius.x;
			sensor_c->y = p->pos.y + radius.y;
			sensor_d->x = p->pos.x + radius.x;
			sensor_d->y = p->pos.y + radius.y;
			break;
		}
		case MODE_RIGHT_WALL: {
			sensor_c->x = p->pos.x + radius.y;
			sensor_c->y = p->pos.y + radius.x;
			sensor_d->x = p->pos.x + radius.y;
			sensor_d->y = p->pos.y - radius.x;
			break;
		}
		case MODE_CEILING: {
			sensor_c->x = p->pos.x + radius.x;
			sensor_c->y = p->pos.y - radius.y;
			sensor_d->x = p->pos.x - radius.x;
			sensor_d->y = p->pos.y - radius.y;
			break;
		}
		case MODE_LEFT_WALL: {
			sensor_c->x = p->pos.x - radius.y;
			sensor_c->y = p->pos.y - radius.x;
			sensor_d->x = p->pos.x - radius.y;
			sensor_d->y = p->pos.y + radius.x;
			break;
		}
	}
}

static void get_push_sensors_pos(Player* p, vec2* sensor_e, vec2* sensor_f) {

	switch (player_get_push_mode(p)) {
		case MODE_FLOOR: {
			sensor_e->x = p->pos.x - PLAYER_PUSH_RADIUS;
			sensor_e->y = p->pos.y;
			sensor_f->x = p->pos.x + PLAYER_PUSH_RADIUS;
			sensor_f->y = p->pos.y;
			if (player_is_grounded(p) && p->ground_angle == 0.0f && !player_is_small_radius(p)) {
				sensor_e->y += 8.0f;
				sensor_f->y += 8.0f;
			}
			break;
		}
		case MODE_RIGHT_WALL: {
			sensor_e->x = p->pos.x;
			sensor_e->y = p->pos.y + PLAYER_PUSH_RADIUS;
			sensor_f->x = p->pos.x;
			sensor_f->y = p->pos.y - PLAYER_PUSH_RADIUS;
			if (player_is_grounded(p) && p->ground_angle == 0.0f && !player_is_small_radius(p)) {
				sensor_e->x += 8.0f;
				sensor_f->x += 8.0f;
			}
			break;
		}
		case MODE_CEILING: {
			sensor_e->x = p->pos.x + PLAYER_PUSH_RADIUS;
			sensor_e->y = p->pos.y;
			sensor_f->x = p->pos.x - PLAYER_PUSH_RADIUS;
			sensor_f->y = p->pos.y;
			if (player_is_grounded(p) && p->ground_angle == 0.0f && !player_is_small_radius(p)) {
				sensor_e->y -= 8.0f;
				sensor_f->y -= 8.0f;
			}
			break;
		}
		case MODE_LEFT_WALL: {
			sensor_e->x = p->pos.x;
			sensor_e->y = p->pos.y - PLAYER_PUSH_RADIUS;
			sensor_f->x = p->pos.x;
			sensor_f->y = p->pos.y + PLAYER_PUSH_RADIUS;
			if (player_is_grounded(p) && p->ground_angle == 0.0f && !player_is_small_radius(p)) {
				sensor_e->x -= 8.0f;
				sensor_f->x -= 8.0f;
			}
			break;
		}
	}
}

static SensorResult sensor_check_down(vec2 pos, int layer) {
	auto get_height = [&](Tile tile, int ix, int iy) -> int {
		if (!tile.top_solid) {
			return 0;
		}

		int result = 0;
		auto heights = get_tile_heights(game.ts, tile.index);

		if (tile.hflip && tile.vflip) {
			int h = heights[15 - ix % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (!tile.hflip && tile.vflip) {
			int h = heights[ix % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (tile.hflip && !tile.vflip) {
			int h = heights[15 - ix % 16];
			if (h <= 0x10) result = h;
		} else if (!tile.hflip && !tile.vflip) {
			int h = heights[ix % 16];
			if (h <= 0x10) result = h;
		}

		return result;
	};

	SensorResult result = {};

	// 
	// NOTE: To make this work correctly with negative numbers,
	// we would have to replace these casts for ix, iy, tile_x, tile_y with floorf's
	// and replace the modulo operator in get_height with calls to wrap()
	// 

	int ix = max((int)pos.x, 0);
	int iy = max((int)pos.y, 0);

	int tile_x = pos.x / 16;
	int tile_y = pos.y / 16;

	Tile tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y++;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		// NOTE: this should be wrap(iy,16) instead of modulo if we care about negative numbers. Also change it in get_height.
		result.dist = (32 - (iy % 16)) - (height + 1);
	} else if (height == 16) {
		tile_y--;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = -(iy % 16) - (height + 1);
	} else {
		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (16 - (iy % 16)) - (height + 1);
	}

	return result;
}

static SensorResult sensor_check_right(vec2 pos, int layer) {
	auto get_height = [&](Tile tile, int ix, int iy) -> int {
		if (!tile.lrb_solid) {
			return 0;
		}

		int result = 0;
		auto heights = get_tile_widths(game.ts, tile.index);

		if (tile.hflip && tile.vflip) {
			int h = heights[15 - iy % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (!tile.hflip && tile.vflip) {
			int h = heights[15 - iy % 16];
			if (h <= 0x10) result = h;
		} else if (tile.hflip && !tile.vflip) {
			int h = heights[iy % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (!tile.hflip && !tile.vflip) {
			int h = heights[iy % 16];
			if (h <= 0x10) result = h;
		}

		return result;
	};

	SensorResult result = {};

	int ix = max((int)pos.x, 0);
	int iy = max((int)pos.y, 0);

	int tile_x = pos.x / 16;
	int tile_y = pos.y / 16;

	Tile tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x++;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (32 - (ix % 16)) - (height + 1);
	} else if (height == 16) {
		tile_x--;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = -(ix % 16) - (height + 1);
	} else {
		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (16 - (ix % 16)) - (height + 1);
	}

	return result;
}

static SensorResult sensor_check_up(vec2 pos, int layer) {
	auto get_height = [&](Tile tile, int ix, int iy) {
		if (!tile.lrb_solid) {
			return 0;
		}

		int result = 0;
		auto heights = get_tile_heights(game.ts, tile.index);

		if (tile.hflip && tile.vflip) {
			int h = heights[15 - ix % 16];
			if (h <= 0x10) result = h;
		} else if (!tile.hflip && tile.vflip) {
			int h = heights[ix % 16];
			if (h <= 0x10) result = h;
		} else if (tile.hflip && !tile.vflip) {
			int h = heights[15 - ix % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (!tile.hflip && !tile.vflip) {
			int h = heights[ix % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		}

		return result;
	};

	SensorResult result = {};

	int ix = max((int)pos.x, 0);
	int iy = max((int)pos.y, 0);

	int tile_x = pos.x / 16;
	int tile_y = pos.y / 16;

	Tile tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y--;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (iy % 16) - (height);
	} else if (height == 16) {
		tile_y++;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = -16 + (iy % 16) - (height);
	} else {
		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (iy % 16) - (height);
	}

	return result;
}

static SensorResult sensor_check_left(vec2 pos, int layer) {
	auto get_height = [&](Tile tile, int ix, int iy) {
		if (!tile.lrb_solid) {
			return 0;
		}

		int result = 0;
		auto heights = get_tile_widths(game.ts, tile.index);

		if (tile.hflip && tile.vflip) {
			int h = heights[15 - iy % 16];
			if (h <= 0x10) result = h;
		} else if (!tile.hflip && tile.vflip) {
			int h = heights[15 - iy % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		} else if (tile.hflip && !tile.vflip) {
			int h = heights[iy % 16];
			if (h <= 0x10) result = h;
		} else if (!tile.hflip && !tile.vflip) {
			int h = heights[iy % 16];
			if (h >= 0xF0) result = 16 - (h - 0xF0);
			else if (h == 16) result = 16;
		}

		return result;
	};

	SensorResult result = {};

	int ix = max((int)pos.x, 0);
	int iy = max((int)pos.y, 0);

	int tile_x = pos.x / 16;
	int tile_y = pos.y / 16;

	Tile tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x--;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (ix % 16) - (height);
	} else if (height == 16) {
		tile_x++;
		tile = get_tile_safe(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = -16 + (ix % 16) - (height);
	} else {
		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (ix % 16) - (height);
	}

	return result;
}

static SensorResult ground_sensor_check(Player* p, vec2 pos) {
	switch (player_get_mode(p)) {
		case MODE_FLOOR:
			return sensor_check_down(pos, p->layer);
		case MODE_RIGHT_WALL:
			return sensor_check_right(pos, p->layer);
		case MODE_CEILING:
			return sensor_check_up(pos, p->layer);
		case MODE_LEFT_WALL:
			return sensor_check_left(pos, p->layer);
	}
	return {};
}

static SensorResult ceiling_sensor_check(Player* p, vec2 pos) {
	switch (player_get_mode(p)) {
		case MODE_FLOOR:
			return sensor_check_up(pos, p->layer);
		case MODE_RIGHT_WALL:
			return sensor_check_left(pos, p->layer);
		case MODE_CEILING:
			return sensor_check_down(pos, p->layer);
		case MODE_LEFT_WALL:
			return sensor_check_right(pos, p->layer);
	}
	return {};
}

static SensorResult push_sensor_e_check(Player* p, vec2 pos) {
	switch (player_get_mode(p)) {
		case MODE_FLOOR:
			return sensor_check_left(pos, p->layer);
		case MODE_RIGHT_WALL:
			return sensor_check_down(pos, p->layer);
		case MODE_CEILING:
			return sensor_check_right(pos, p->layer);
		case MODE_LEFT_WALL:
			return sensor_check_up(pos, p->layer);
	}
	return {};
}

static SensorResult push_sensor_f_check(Player* p, vec2 pos) {
	switch (player_get_mode(p)) {
		case MODE_FLOOR:
			return sensor_check_right(pos, p->layer);
		case MODE_RIGHT_WALL:
			return sensor_check_up(pos, p->layer);
		case MODE_CEILING:
			return sensor_check_left(pos, p->layer);
		case MODE_LEFT_WALL:
			return sensor_check_down(pos, p->layer);
	}
	return {};
}

static bool player_roll_condition(Player* p) {
	// if (p->control_lock > 0) return false;

	int input_h = 0;
	if (p->input & INPUT_RIGHT) input_h++;
	if (p->input & INPUT_LEFT)  input_h--;

	return (fabsf(p->ground_speed) >= 0.5f
			&& (p->input & INPUT_DOWN)
			&& input_h == 0);
}

static void ground_sensor_collision(Player* p) {
	if (!are_ground_sensors_active(p)) {
		return;
	}

	if (p->landed_on_solid_object) {
		return;
	}

	vec2 sensor_a;
	vec2 sensor_b;
	get_ground_sensors_pos(p, &sensor_a, &sensor_b);

	float check_dist;
	if (player_is_grounded(p)) {
		check_dist = 14;
	} else {
		float check_speed = player_is_on_a_wall(p) ? p->speed.y : p->speed.x;
		check_dist = fminf(fabsf(check_speed) + 4, 14);
	}

	SensorResult res_a = ground_sensor_check(p, sensor_a);
	SensorResult res_b = ground_sensor_check(p, sensor_b);

	bool sensor_a_found_tile = res_a.dist <= check_dist;
	bool sensor_b_found_tile = res_b.dist <= check_dist;

	// balancing animation
	if (p->state == STATE_GROUND) {
		if (p->ground_speed == 0) {
			if ((sensor_a_found_tile && !sensor_b_found_tile) || (sensor_b_found_tile && !sensor_a_found_tile)) {
				vec2 extra_sensor = p->pos;
				extra_sensor.y += player_get_radius(p).y;

				SensorResult extra_res = sensor_check_down(extra_sensor, p->layer);

				bool extra_sensor_found_tile = extra_res.dist <= check_dist;

				if (!extra_sensor_found_tile) {
					if (sensor_a_found_tile) {
						p->next_anim = (p->facing > 0) ? anim_balance : anim_balance2;
					} else {
						p->next_anim = (p->facing < 0) ? anim_balance : anim_balance2;
					}
					p->frame_duration = 15;
				}
			}
		}
	}

	SensorResult res = (res_a.dist < res_b.dist) ? res_a : res_b;

	if (res.dist > check_dist) {
		p->state = STATE_AIR;
		return;
	}

	//if (res.dist < -14) {
	//	return;
	//}

	switch (player_get_mode(p)) {
		case MODE_FLOOR:      p->pos.y += res.dist; break;
		case MODE_RIGHT_WALL: p->pos.x += res.dist; break;
		case MODE_CEILING:    p->pos.y -= res.dist; break;
		case MODE_LEFT_WALL:  p->pos.x -= res.dist; break;
	}

	float angle = get_tile_angle(game.ts, res.tile.index);
	if (angle == -1.0f) { // flagged
		// float a = angle_wrap(p->ground_angle);
		// if (a <= 45.0f) {
		// 	p->ground_angle = 0.0f;
		// } else if (a <= 134.0f) {
		// 	p->ground_angle = 90.0f;
		// } else if (a <= 225.0f) {
		// 	p->ground_angle = 180.0f;
		// } else if (a <= 314.0f) {
		// 	p->ground_angle = 270.0f;
		// } else {
		// 	p->ground_angle = 0.0f;
		// }

		switch (player_get_mode(p)) {
			case MODE_FLOOR:      p->ground_angle = 0.0f;   break;
			case MODE_RIGHT_WALL: p->ground_angle = 90.0f;  break;
			case MODE_CEILING:    p->ground_angle = 180.0f; break;
			case MODE_LEFT_WALL:  p->ground_angle = 270.0f; break;
		}
	} else {
		// if (res.tile_x * 16 <= (int)sensor_x && (int)sensor_x < (res.tile_x + 1) * 16
		// 	&& res.tile_y * 16 <= (int)sensor_y && (int)sensor_y < (res.tile_y + 1) * 16)
		{
			if (res.tile.hflip && res.tile.vflip) p->ground_angle = -(180.0f - angle);
			else if (!res.tile.hflip && res.tile.vflip) p->ground_angle = 180.0f - angle;
			else if (res.tile.hflip && !res.tile.vflip) p->ground_angle = -angle;
			else p->ground_angle = angle;
		}
	}

	if (p->state == STATE_AIR) {
		float a = angle_wrap(p->ground_angle);
		if (a <= 22.0f /*23.0f*/ || a >= 339.0f) {
			// flat
			p->ground_speed = p->speed.x;
		} else if (a <= 45.0f || a >= 316.0f) {
			// slope
			if (player_is_moving_mostly_right(p) || player_is_moving_mostly_left(p)) {
				p->ground_speed = p->speed.x;
			} else {
				p->ground_speed = p->speed.y * 0.5f * -signf(dsin(p->ground_angle));
			}
		} else {
			// steep
			if (player_is_moving_mostly_right(p) || player_is_moving_mostly_left(p)) {
				p->ground_speed = p->speed.x;
			} else {
				p->ground_speed = p->speed.y * -signf(dsin(p->ground_angle));
			}
		}

		if (p->ground_speed != 0) {
			p->facing = sign_int(p->ground_speed);
		}

		if (player_roll_condition(p)) {
			p->state = STATE_ROLL;

			play_sound(get_sound(snd_spindash));
		} else {
			p->state = STATE_GROUND;
		}
	}
}

static void ceiling_sensor_collision(Player* p) {
	if (!are_ceiling_sensors_active(p)) {
		return;
	}

	Assert(player_get_mode(p) == MODE_FLOOR);

	vec2 sensor_c;
	vec2 sensor_d;
	get_ceiling_sensors_pos(p, &sensor_c, &sensor_d);

	SensorResult res_c = ceiling_sensor_check(p, sensor_c);
	SensorResult res_d = ceiling_sensor_check(p, sensor_d);

	SensorResult res = (res_c.dist < res_d.dist) ? res_c : res_d;

	if (res.dist > 0) {
		return;
	}

	p->pos.y -= res.dist;

	p->speed.y = 0;

	// TODO: Landing on ceilings.
}

static void push_sensor_collision(Player* p) {
	vec2 sensor_e;
	vec2 sensor_f;
	get_push_sensors_pos(p, &sensor_e, &sensor_f);

	if (is_push_sensor_f_active(p)) {
		SensorResult res = push_sensor_f_check(p, sensor_f);
		if (res.dist <= 0) {
			switch (player_get_push_mode(p)) {
				case MODE_FLOOR:      p->pos.x += res.dist; break;
				case MODE_RIGHT_WALL: p->pos.y += res.dist; break;
				case MODE_CEILING:    p->pos.x -= res.dist; break;
				case MODE_LEFT_WALL:  p->pos.y -= res.dist; break;
			}

			p->ground_speed = 0.0f;
			p->speed.x = 0.0f;

			if (p->input & INPUT_RIGHT) {
				p->pushing = true;
			}
		}
	}

	if (is_push_sensor_e_active(p)) {
		SensorResult res = push_sensor_e_check(p, sensor_e);
		if (res.dist <= 0) {
			switch (player_get_push_mode(p)) {
				case MODE_FLOOR:      p->pos.x -= res.dist; break;
				case MODE_RIGHT_WALL: p->pos.y -= res.dist; break;
				case MODE_CEILING:    p->pos.x += res.dist; break;
				case MODE_LEFT_WALL:  p->pos.y += res.dist; break;
			}

			p->ground_speed = 0.0f;
			p->speed.x = 0.0f;

			if (p->input & INPUT_LEFT) {
				p->pushing = true;
			}
		}
	}
}

static bool player_try_slip(Player* p) {
	// Check for slipping/falling when Ground Speed is too low on walls/ceilings.

	float a = angle_wrap(p->ground_angle);
	if (a >= 46.0f && a < 316.0f) {
		if (fabsf(p->ground_speed) < 2.5f) {
			p->state = STATE_AIR;
			p->ground_speed = 0.0f;
			p->control_lock = 30.0f;

			return true;
		}
	}

	return false;
}

static bool player_try_jump(Player* p) {
	constexpr float PLAYER_JUMP_FORCE = 6.5f;

	// if (p->control_lock > 0) return false;

	{
		// don't check if they're active cause they're not
		vec2 sensor_c;
		vec2 sensor_d;
		get_ceiling_sensors_pos(p, &sensor_c, &sensor_d);

		SensorResult res_c = ceiling_sensor_check(p, sensor_c);
		SensorResult res_d = ceiling_sensor_check(p, sensor_d);

		SensorResult res = (res_c.dist < res_d.dist) ? res_c : res_d;

		if (res.dist < 6) {
			return false;
		}
	}

	p->speed.x -= PLAYER_JUMP_FORCE * dsin(p->ground_angle);
	p->speed.y -= PLAYER_JUMP_FORCE * dcos(p->ground_angle);
	p->state = STATE_AIR;
	p->ground_angle = 0;
	p->next_anim = anim_roll;
	p->frame_duration = fmaxf(0, 4 - fabsf(p->ground_speed));
	p->jumped = true;

	play_sound(get_sound(snd_jump_cd));

	return true;
}

static void player_keep_in_bounds(Player* p) {
	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	vec2 radius = player_get_radius(p);

	float left = radius.x + 2;
	float top  = radius.y + 2;
	float right  = game.tm.width  * 16 - 3 - radius.x;
	float bottom = game.tm.height * 16 - 3 - radius.y;

	if (p->pos.x < left)   {p->pos.x = left;  p->ground_speed = 0; p->speed.x = 0;}
	if (p->pos.x > right)  {p->pos.x = right; p->ground_speed = 0; p->speed.x = 0;}
	// if (p->pos.y < top)    {p->pos.y = top;}
	if (p->pos.y > bottom) {p->pos.y = bottom;}
}

static bool object_is_solid(ObjType type) {
	switch (type) {
		case OBJ_MONITOR: return true;
		case OBJ_SPRING:  return true;
	}
	return false;
}

static Direction opposite_dir(Direction dir) {
	/*switch (dir) {
		case DIR_RIGHT: return DIR_LEFT;
		case DIR_UP:    return DIR_DOWN;
		case DIR_LEFT:  return DIR_RIGHT;
		case DIR_DOWN:  return DIR_UP;
	}*/
	return (Direction) ((dir + 2) % 4);
}

static float get_spring_force(const Object& o) {
	float force = 10.5f; // just enough to reach 16 tiles

	if (o.spring.color == SPRING_COLOR_RED) {
		force = 16;
	}

	return force;
}

static void player_collide_with_objects(Player* p) {
	vec2 player_radius = player_get_radius(p);
	
	// reset the flag
	p->landed_on_solid_object = false;

	auto player_land_on_object = [&](Player* p, Object* o) -> bool {
		switch (o->type) {
			case OBJ_MONITOR: {
				if (p->anim != anim_roll) return false;

				float bounce_speed = 0;

				if (p->input & INPUT_JUMP) {
					bounce_speed = fabsf(p->speed.y);
				}

				if (bounce_speed < 2) bounce_speed = 2;

				p->state   = STATE_AIR;
				p->speed.y = -bounce_speed;

				o->flags |= FLAG_INSTANCE_DEAD;

				{
					Object broken_monitor = {};
					broken_monitor.id = game.next_id++;
					broken_monitor.type = OBJ_MONITOR_BROKEN;
					broken_monitor.pos = o->pos;

					array_add(&game.objects, broken_monitor);
				}

				{
					Particle p = {};
					p.pos = o->pos;
					p.sprite_index = spr_explosion;
					p.lifespan = 30;

					add_particle(p);
				}

				play_sound(get_sound(snd_destroy_monitor));
				
				return true;
			}

			case OBJ_SPRING: {
				if (o->spring.direction != DIR_UP) return false;

				float force = get_spring_force(*o);

				p->state   = STATE_AIR;
				p->speed.y = -force;
				p->jumped  = false;

				p->next_anim = anim_rise;
				p->frame_duration = 5;

				o->spring.animating = true;
				o->spring.frame_index = 0;

				play_sound(get_sound(snd_spring_bounce));

				return true;
			}
		}

		return false;
	};

	auto player_collide_object_side = [&](Player* p, Object* o, Direction dir) {
		switch (o->type) {
			case OBJ_MONITOR: {
				if (p->anim != anim_roll) return false;

				o->flags |= FLAG_INSTANCE_DEAD;

				{
					Object broken_monitor = {};
					broken_monitor.id = game.next_id++;
					broken_monitor.type = OBJ_MONITOR_BROKEN;
					broken_monitor.pos = o->pos;

					array_add(&game.objects, broken_monitor);
				}

				{
					Particle p = {};
					p.pos = o->pos;
					p.sprite_index = spr_explosion;
					p.lifespan = 30;

					add_particle(p);
				}

				play_sound(get_sound(snd_destroy_monitor));
				
				return true;
			}

			case OBJ_SPRING: {
				if (o->spring.direction != opposite_dir(dir)) return false;

				float force = get_spring_force(*o);

				if (o->spring.direction == DIR_RIGHT) {
					p->ground_speed = force;
					p->speed.x = force;
				} else {
					p->ground_speed = -force;
					p->speed.x = -force;
				}
				p->control_lock = 30;

				o->spring.animating = true;
				o->spring.frame_index = 0;

				play_sound(get_sound(snd_spring_bounce));

				return true;
			}
		}

		return false;
	};

	// because we add objects while iterating
	int object_count = game.objects.count;

	for (int i = 0; i < object_count; i++) {
		Object* it = &game.objects[i];

		if (!object_is_solid(it->type)) continue;

		vec2 obj_size = get_object_size(*it);

		float combined_x_radius = obj_size.x/2 + (player_radius.x + 1) + 1;
		float combined_y_radius = obj_size.y/2 + player_radius.y       + 1;

		float combined_x_diameter = combined_x_radius * 2;
		float combined_y_diameter = combined_y_radius * 2;

		float left_difference = (p->pos.x - it->pos.x) + combined_x_radius;

		// the Player is too far to the left to be touching?
		if (left_difference < 0) continue;
		// the Player is too far to the right to be touching?
		if (left_difference > combined_x_diameter) continue;

		float top_difference = (p->pos.y - it->pos.y) + 4 + combined_y_radius;

		// the Player is too far above to be touching
		if (top_difference < 0) continue;
		// the Player is too far down to be touching
		if (top_difference > combined_y_diameter) continue;

		float x_distance;
		if (p->pos.x > it->pos.x) {
			// player is on the right
			x_distance = left_difference - combined_x_diameter;
		} else {
			// player is on the left
			x_distance = left_difference;
		}

		//bool should_collide_vertically_anyway = false;

		float y_distance;
		if (p->pos.y > it->pos.y) {
			// player is on the bottom
			y_distance = top_difference - 4 - combined_y_diameter;
		} else {
			// player is on the top
			y_distance = top_difference;

			//if (fabsf(top_difference - 4) <= 2) should_collide_vertically_anyway = true;
		}

		if (fabsf(x_distance) > fabsf(y_distance)
			|| /*should_collide_vertically_anyway*/0)
		{
			// collide vertically

			if (y_distance >= 0) {
				// land

				//if (y_distance >= 16) continue;

				y_distance -= 4;

				if (!(it->pos.x - obj_size.x/2 < p->pos.x && p->pos.x < it->pos.x + obj_size.x/2)) {
					continue;
				}

				if (p->speed.y < 0) continue;

				p->pos.y -= y_distance;

				if (!player_land_on_object(p, it)) {
					if (p->state == STATE_AIR) {
						if (player_roll_condition(p)) {
							p->state = STATE_ROLL;
						} else {
							p->state = STATE_GROUND;
						}
						p->ground_speed = p->speed.x;
						p->ground_angle = 0;
					}

					p->speed.y = 0;
					p->landed_on_solid_object = true;
				}

				// we could remove dead objects at the end of the frame but idk
				if (it->flags & FLAG_INSTANCE_DEAD) {
					array_remove(&game.objects, i);
					i--;
					object_count--;
					continue;
				}
			} else {
				// TODO: bump the ceiling

				p->speed.y = 0;
				p->pos.y -= y_distance;
			}
		} else {
			// collide horizontally

			if (fabsf(y_distance) <= 4) continue;

			p->pos.x -= x_distance;

			if (x_distance != 0) {
				// if player is moving towards the object
				if ((x_distance > 0 && p->speed.x > 0)
					|| (x_distance < 0 && p->speed.x < 0))
				{
					Direction dir = (x_distance > 0) ? DIR_RIGHT : DIR_LEFT;

					if (!player_collide_object_side(p, it, dir)) {
						p->ground_speed = 0;
						p->speed.x = 0;
						p->pushing = true;
					}

					// we could remove dead objects at the end of the frame but idk
					if (it->flags & FLAG_INSTANCE_DEAD) {
						array_remove(&game.objects, i);
						i--;
						object_count--;
						continue;
					}
				}
			}
		}
	}
}

static void player_state_ground(Player* p, float delta) {
	int input_h = 0;
	if (p->control_lock == 0) {
		if (p->input & INPUT_RIGHT) input_h++;
		if (p->input & INPUT_LEFT)  input_h--;
	}

	// Adjust Ground Speed based on current Ground Angle (Slope Factor).
	apply_slope_factor(p, delta);

	// Update Ground Speed based on directional input and apply friction/deceleration.
	if (p->anim != anim_spindash
		&& !p->peelout
		&& p->anim != anim_crouch
		&& p->anim != anim_look_up)
	{
		if (input_h == 0) {
			Approach(&p->ground_speed, 0.0f, PLAYER_FRICTION * delta);
		} else {
			if (input_h == -sign_int(p->ground_speed)) {
				p->ground_speed += input_h * PLAYER_DEC * delta;

				if (fabsf(p->ground_speed) >= 4) {
					if (p->anim != anim_skid) {
						p->next_anim = anim_skid;
						p->frame_duration = 8;

						play_sound(get_sound(snd_skid));
					}
				}
			} else {
				if (fabsf(p->ground_speed) < PLAYER_TOP_SPEED) {
					p->ground_speed += input_h * PLAYER_ACC * delta;
					Clamp(&p->ground_speed, -PLAYER_TOP_SPEED, PLAYER_TOP_SPEED);
				}
			}
		}
	}

	auto physics_step = [&](float delta) {
		// Move the Player object
		p->speed.x =  dcos(p->ground_angle) * p->ground_speed;
		p->speed.y = -dsin(p->ground_angle) * p->ground_speed;

		p->pos += p->speed * delta;

		// Push Sensor collision occurs.
		push_sensor_collision(p);

		// Grounded Ground Sensor collision occurs.
		ground_sensor_collision(p);
	};

	constexpr int num_physics_steps = 4;
	for (int i = 0; i < num_physics_steps; i++) {
		physics_step(delta / (float)num_physics_steps);
	}

	// collide with objects
	player_collide_with_objects(p);

	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	player_keep_in_bounds(p);

	if (p->pushing) {
		const int anim_push_frame_duration = fmaxf(0, 8 - fabsf(p->ground_speed)) * 4;

		p->next_anim = anim_push;
		p->frame_duration = anim_push_frame_duration;
	} else {
		bool dont_update_anim = false;

		if (p->anim == anim_spindash) {
			dont_update_anim = true;
		}

		if ((p->next_anim == anim_balance || p->next_anim == anim_balance2) && p->ground_speed == 0) {
			dont_update_anim = true;
		}

		if (p->next_anim == anim_skid) {
			if (sign_int(p->ground_speed) == p->facing) {
				if (input_h != p->facing) {
					dont_update_anim = true;
				}
			}
		}

		if (p->peelout) {
			dont_update_anim = true;
		}

		if (!dont_update_anim) {
			if (p->ground_speed == 0.0f) {
				if (p->input & INPUT_DOWN) {
					p->next_anim = anim_crouch;
					p->frame_duration = 1;
				} else if (p->input & INPUT_UP) {
					p->next_anim = anim_look_up;
					p->frame_duration = 1;
				} else {
					p->next_anim = anim_idle;
					p->frame_duration = 1;
				}
			} else {
				if (fabsf(p->ground_speed) >= 12) {
					p->next_anim = anim_peelout;
				} else if (fabsf(p->ground_speed) >= PLAYER_TOP_SPEED) {
					p->next_anim = anim_run;
				} else {
					p->next_anim = anim_walk;
				}
				p->frame_duration = fmaxf(0, 8 - fabsf(p->ground_speed));
			}
		}
	}

	if (p->ground_speed != 0) {
		p->facing = sign_int(p->ground_speed);
	}

	// Check for slipping/falling when Ground Speed is too low on walls/ceilings.
	if (player_try_slip(p)) {
		return;
	}

	// Check for starting a jump.
	if (p->input_press & INPUT_JUMP) {
		if (p->anim == anim_crouch) {
			// start spindash
			p->next_anim = anim_spindash;
			p->frame_duration = 1;
			p->spinrev = 0;

			play_sound(get_sound(snd_spindash));
		} else if (p->anim == anim_look_up) {
			// start peelout
			p->peelout = true;
			p->spinrev = 0;

			play_sound(get_sound(snd_spindash));
		} else if (p->anim == anim_spindash) {
			// charge up spindash
			p->spinrev += 2;
			p->spinrev = fminf(p->spinrev, 8);
			p->frame_index = 0;

			play_sound(get_sound(snd_spindash));
		} else if (p->peelout) {
			// do nothing
		} else {
			if (player_try_jump(p)) {
				return;
			}
		}
	}

	if (p->anim == anim_spindash) {
		p->spinrev -= floorf(p->spinrev * 8) / 256 * delta;

		if (!(p->input & INPUT_DOWN)) {
			p->state = STATE_ROLL;
			p->ground_speed = (8 + floorf(p->spinrev) / 2) * p->facing;
			p->next_anim = anim_roll;
			p->frame_duration = fmaxf(0, 4 - fabsf(p->ground_speed));
			game.camera_lock = 24 - floorf(fabsf(p->ground_speed));

			stop_sound(get_sound(snd_spindash));
			play_sound(get_sound(snd_spindash_end));
			return;
		}
	}

	if (p->peelout) {
		p->spinrev += delta;
		p->spinrev = fminf(p->spinrev, 30);

		float speed = (4.5f + floorf(p->spinrev) / 4.0f) * p->facing;

		if (fabsf(speed) >= 12) {
			p->next_anim = anim_peelout;
		} else if (fabsf(speed) >= PLAYER_TOP_SPEED) {
			p->next_anim = anim_run;
		} else {
			p->next_anim = anim_walk;
		}
		p->frame_duration = fmaxf(0, 8 - fabsf(speed));

		if (!(p->input & INPUT_UP)) {
			p->peelout = false;

			if (p->spinrev >= 15) {
				p->state = STATE_GROUND;
				p->ground_speed = speed;
				game.camera_lock = 24 - floorf(fabsf(p->ground_speed));

				stop_sound(get_sound(snd_spindash));
				play_sound(get_sound(snd_spindash_end));
				return;
			}
		}
	}

	// Check for starting a roll.
	if (player_roll_condition(p)) {
		p->state = STATE_ROLL;
		// p->pos.y += 5;
		play_sound(get_sound(snd_spindash));
		return;
	}

	if (p->anim == anim_skid) {
		// @Cleanup
		static float t = 0;

		t += delta;
		while (t >= 4) {
			Particle dust_particle = {};
			dust_particle.sprite_index = spr_skid_dust;
			dust_particle.lifespan = 16;

			dust_particle.pos = p->pos;
			dust_particle.pos.y += player_get_radius(p).y;
			dust_particle.pos.y -= 8;

			add_particle(dust_particle);

			t -= 4;
		}
	}
}

static void player_state_roll(Player* p, float delta) {
	int input_h = 0;
	if (p->control_lock == 0) {
		if (p->input & INPUT_RIGHT) input_h++;
		if (p->input & INPUT_LEFT)  input_h--;
	}

	apply_slope_factor(p, delta);

	// Update Ground Speed based on directional input and apply friction/deceleration.
	const float roll_friction_speed = 0.0234375f;
	const float roll_deceleration_speed = 0.125f;

	Approach(&p->ground_speed, 0.0f, roll_friction_speed * delta);

	if (input_h == -sign_int(p->ground_speed)) {
		p->ground_speed += input_h * roll_deceleration_speed * delta;
	}

	p->speed.x =  dcos(p->ground_angle) * p->ground_speed;
	p->speed.y = -dsin(p->ground_angle) * p->ground_speed;

	auto physics_step = [&](float delta) {
		// Move the Player object
		p->pos += p->speed * delta;

		// Push Sensor collision occurs.
		push_sensor_collision(p);

		// Grounded Ground Sensor collision occurs.
		ground_sensor_collision(p);
	};

	constexpr int num_physics_steps = 4;
	for (int i = 0; i < num_physics_steps; i++) {
		physics_step(delta / (float)num_physics_steps);
	}

	// collide with objects
	player_collide_with_objects(p);

	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	player_keep_in_bounds(p);

	p->next_anim = anim_roll;
	p->frame_duration = fmaxf(0, 4 - fabsf(p->ground_speed));

	if (sign_int(p->ground_speed) != p->facing
		|| /*fabsf(p->ground_speed) < 0.5f*/0)
	{
		p->state = STATE_GROUND;
		return;
	}

	if (player_try_slip(p)) {
		return;
	}

	// Check for starting a jump.
	if (p->input_press & INPUT_JUMP) {
		if (player_try_jump(p)) {
			return;
		}
	}
}

static void player_state_air(Player* p, float delta) {
	int input_h = 0;
	// control_lock is ignored
	if (p->input & INPUT_RIGHT) input_h++;
	if (p->input & INPUT_LEFT)  input_h--;

	// Check for jump button release (variable jump velocity).
	if (p->jumped && !(p->input & INPUT_JUMP)) {
		if (p->speed.y < -4) {
			p->speed.y = -4;
		}
	}

	// Update X Speed based on directional input.
	const float air_acceleration_speed = 0.09375f;
	if (input_h != 0) {
		if (fabsf(p->speed.x) < PLAYER_TOP_SPEED || input_h == -sign_int(p->speed.x)) {
			p->speed.x += input_h * air_acceleration_speed * delta;
			Clamp(&p->speed.x, -PLAYER_TOP_SPEED, PLAYER_TOP_SPEED);
		}
	}

	// Apply air drag.
	if (-4 < p->speed.y && p->speed.y < 0) {
		p->speed.x -= floorf(fabsf(p->speed.x) * 8) / 256 * signf(p->speed.x) * delta;
	}

	// Apply gravity.
	const float GRAVITY = 0.21875f;
	p->speed.y += GRAVITY * delta;

	auto physics_step = [&](float delta) {
		// Move the Player object
		p->pos += p->speed * delta;

		// All air collision checks occur here.
		push_sensor_collision(p);

		ground_sensor_collision(p);

		ceiling_sensor_collision(p);
	};

	constexpr int num_physics_steps = 4;
	for (int i = 0; i < num_physics_steps; i++) {
		physics_step(delta / (float)num_physics_steps);
	}

	// collide with objects
	player_collide_with_objects(p);

	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	player_keep_in_bounds(p);

	// Rotate Ground Angle back to 0.
	p->ground_angle -= clamp(angle_difference(p->ground_angle, 0.0f), -2.8125f * delta, 2.8125f * delta);

	if (p->speed.x != 0) {
		p->facing = sign_int(p->speed.x);
	}

	if (p->anim == anim_rise) {
		if (p->speed.y >= 0) {
			p->next_anim = anim_walk;
			p->frame_duration = fmaxf(0, 8 - fabsf(p->speed.x));
		}
	}
}

static const Sprite& anim_get_sprite(anim_index anim) {
	return get_sprite(spr_sonic_crouch + anim);
};

#define MOBILE_DPAD_SIZE   64
#define MOBILE_DPAD_OFFSET 64

static void get_mobile_controls(glm::ivec2* dpad_pos,
								glm::ivec2* action_pos,
								Rect* action_rect,
								Rect* pause_rect) {
	dpad_pos->x	  = 48 - 32;
	dpad_pos->y   = window.game_height - 48 - 32;
	action_pos->x = window.game_width  - 48 - 24;
	action_pos->y = window.game_height - 48 - 24;
	
	action_rect->x = window.game_width  * 0.6;
	action_rect->y = window.game_height * 0.6;
	action_rect->w = window.game_width  - action_rect->x;
	action_rect->h = window.game_height - action_rect->y;

	pause_rect->x = window.game_width * 0.6;
	pause_rect->y = 0;
	pause_rect->w = window.game_width        - pause_rect->y;
	pause_rect->h = window.game_height * 0.4 - pause_rect->x;
}

static Rectf to_rectf(Rect r) {
	Rectf result;
	result.x = (float) r.x;
	result.y = (float) r.y;
	result.w = (float) r.w;
	result.h = (float) r.h;
	return result;
}

static void player_update(Player* p, float delta) {
	// clear flags
	if (p->state != STATE_AIR) {
		p->jumped = false;
	}
	if (p->state != STATE_GROUND) {
		p->peelout = false;
	}
	p->pushing = false;

	p->prev_mode   = player_get_mode(p);
	p->prev_radius = player_get_radius(p);

	// set input state
	{
		u32 prev = p->input;
		p->input = 0;

		// Keyboard

		if (is_key_held(SDL_SCANCODE_RIGHT)) {
			p->input |= INPUT_RIGHT;
		}

		if (is_key_held(SDL_SCANCODE_UP)) {
			p->input |= INPUT_UP;
		}

		if (is_key_held(SDL_SCANCODE_LEFT)) {
			p->input |= INPUT_LEFT;
		}

		if (is_key_held(SDL_SCANCODE_DOWN)) {
			p->input |= INPUT_DOWN;
		}

		if (is_key_held(SDL_SCANCODE_Z)) {
			p->input |= INPUT_Z;
		}

		if (is_key_held(SDL_SCANCODE_X)) {
			p->input |= INPUT_X;
		}

		// Controller

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) {
			p->input |= INPUT_RIGHT;
		}

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_DPAD_UP)) {
			p->input |= INPUT_UP;
		}

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) {
			p->input |= INPUT_LEFT;
		}

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) {
			p->input |= INPUT_DOWN;
		}

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_A)) {
			p->input |= INPUT_Z;
		}

		if (is_controller_button_held(SDL_CONTROLLER_BUTTON_B)) {
			p->input |= INPUT_X;
		}

		// Controller Axis
		{
			const float deadzone = 0.3f;

			float leftx = controller_get_axis(SDL_CONTROLLER_AXIS_LEFTX);
			if (leftx < -deadzone) {
				p->input |= INPUT_LEFT;
			}
			if (leftx > deadzone) {
				p->input |= INPUT_RIGHT;
			}

			float lefty = controller_get_axis(SDL_CONTROLLER_AXIS_LEFTY);
			if (lefty < -deadzone) {
				p->input |= INPUT_UP;
			}
			if (lefty > deadzone) {
				p->input |= INPUT_DOWN;
			}
		}

		// Touch
#if defined(__ANDROID__) || defined(PRETEND_MOBILE)
		{
			glm::ivec2 dpad_pos;
			glm::ivec2 action_pos;
			Rect action_rect;
			Rect pause_rect;
			get_mobile_controls(&dpad_pos, &action_pos, &action_rect, &pause_rect);

			int left   = dpad_pos.x;
			int top    = dpad_pos.y;
			int right  = left + MOBILE_DPAD_SIZE;
			int bottom = top  + MOBILE_DPAD_SIZE;
			int cent_x = left + MOBILE_DPAD_SIZE / 2;
			int cent_y = top  + MOBILE_DPAD_SIZE / 2;

			int act_x1 = action_rect.x;
			int act_y1 = action_rect.y;
			int act_x2 = action_rect.x + action_rect.w;
			int act_y2 = action_rect.y + action_rect.h;

			int pause_x1 = pause_rect.x;
			int pause_y1 = pause_rect.y;
			int pause_x2 = pause_rect.x + pause_rect.w;
			int pause_y2 = pause_rect.y + pause_rect.h;

			left   -= MOBILE_DPAD_OFFSET;
			top    -= MOBILE_DPAD_OFFSET;
			right  += MOBILE_DPAD_OFFSET;
			bottom += MOBILE_DPAD_OFFSET;

			int num_touch_devices = SDL_GetNumTouchDevices();
			for (int i = 0; i < num_touch_devices; i++) {
				SDL_TouchID touch_id = SDL_GetTouchDevice(i);

				int num_fingers = SDL_GetNumTouchFingers(touch_id);
				for (int i = 0; i < num_fingers; i++) {
					SDL_Finger* finger = SDL_GetTouchFinger(touch_id, i);

					if (!finger) {
						log_warn("SDL_GetTouchFinger returned null.");
						continue;
					}

					float mx = finger->x * renderer.backbuffer_width;
					float my = finger->y * renderer.backbuffer_height;

					auto rect = renderer.game_texture_rect;
					mx = (mx - rect.x) / (float)rect.w * (float)window.game_width;
					my = (my - rect.y) / (float)rect.h * (float)window.game_height;

					if (point_in_triangle({mx, my},
										  {left, top},
										  {right, top},
										  {cent_x, cent_y})) {
						p->input |= INPUT_UP;
					} else if (point_in_triangle({mx, my},
												 {cent_x, cent_y},
												 {left, bottom},
												 {right, bottom})) {
						p->input |= INPUT_DOWN;
					} else if (point_in_triangle({mx, my},
												 {left, top},
												 {left, bottom},
												 {cent_x, cent_y})) {
						p->input |= INPUT_LEFT;
					} else if (point_in_triangle({mx, my},
												 {cent_x, cent_y},
												 {right, top},
												 {right, bottom})) {
						p->input |= INPUT_RIGHT;
					}

					if (point_in_rect({mx, my},
									  to_rectf(action_rect))) {
						p->input |= INPUT_Z;
					}
				}
			}
		}
#endif

		p->input_press   = p->input & (~prev);
		p->input_release = (~p->input) & prev;
	}

	switch (p->state) {
		case STATE_GROUND: {
			player_state_ground(p, delta);
			break;
		}

		case STATE_ROLL: {
			player_state_roll(p, delta);
			break;
		}

		case STATE_AIR: {
			player_state_air(p, delta);
			break;
		}

		case STATE_DEBUG: {
			float spd = 8;

			p->speed = {};
			p->ground_speed = 0;
			p->ground_angle = 0;

			if (p->input & INPUT_UP)    { p->speed.y -= spd; }
			if (p->input & INPUT_DOWN)  { p->speed.y += spd; }
			if (p->input & INPUT_LEFT)  { p->speed.x -= spd; }
			if (p->input & INPUT_RIGHT) { p->speed.x += spd; }

			p->pos += p->speed * delta;
			break;
		}
	}

	if (p->state != STATE_AIR) {
		p->control_lock = fmaxf(p->control_lock - delta, 0);
	}

	auto player_collides_with_object = [](Player* p, const Object& o) -> bool {
		Rectf r1 = player_get_rect(p);

		vec2 size = get_object_size(o);
		vec2 pos = o.pos - size / 2.0f;
		Rectf r2 = {pos.x, pos.y, size.x, size.y};

		return rect_vs_rect(r1, r2);
	};

	For (it, game.objects) {
		if (player_collides_with_object(p, *it)) {
			if (it->type == OBJ_LAYER_SET) {
				p->layer = it->layset.layer;
			} else if (it->type == OBJ_LAYER_FLIP) {
				bool dont_set_layer = false;

				if (it->flags & FLAG_LAYER_FLIP_GROUNDED) {
					if (!player_is_grounded(p)) {
						dont_set_layer = true;
					}
				}

				if (!dont_set_layer) {
					if (sign_int(p->speed.x) == 1) {
						p->layer = 0;
					} else if (sign_int(p->speed.x) == -1) {
						p->layer = 1;
					}
				}
			} else if (it->type == OBJ_RING) {
				game.player_rings++;

				Particle p = {};
				p.pos = it->pos;
				p.sprite_index = spr_ring_disappear;

				const Sprite& s = get_sprite(p.sprite_index);
				p.lifespan = (1.0f / s.anim_spd) * s.frames.count;

				add_particle(p);

				play_sound(get_sound(snd_ring));

				Remove(it, game.objects);
				continue;
			}
		}
	}

	auto anim_get_frame_count = [](anim_index anim) -> int {
		const Sprite& s = anim_get_sprite(anim);
		return s.frames.count;
	};

	// animate player
	if (p->anim != p->next_anim) {
		p->anim = p->next_anim;
		p->frame_timer = p->frame_duration;
		p->frame_index = 0;
	} else {
		if (p->frame_timer > 0) {
			p->frame_timer -= delta;
		} else {
			p->frame_index++;
			if (p->frame_index >= anim_get_frame_count(p->anim)) p->frame_index = 0;

			p->frame_timer = p->frame_duration;
		}
	}

#ifdef DEVELOPER
	if (is_key_pressed(SDL_SCANCODE_A)
		|| is_controller_button_pressed(SDL_CONTROLLER_BUTTON_Y))
	{
		if (p->state == STATE_DEBUG) {
			p->state = STATE_AIR;
			p->speed = {};
		} else {
			p->state = STATE_DEBUG;
		}
	}
#endif
}

void Game::update(float delta) {
	skip_frame = frame_advance;

	if (is_key_pressed(SDL_SCANCODE_F5, true)) {
		frame_advance = true;
		skip_frame = false;
	}

	if (is_key_pressed(SDL_SCANCODE_F6)) {
		frame_advance = false;
	}

	if (is_key_pressed(SDL_SCANCODE_F1)) {
		show_debug_info ^= true;
	}

	if (is_key_pressed(SDL_SCANCODE_F4)) {
		set_fullscreen(!is_fullscreen());
	}

	if (program.transition_t != 0) {
		return;
	}

	if (!skip_frame) {
		player_update(&player, delta);

		player_time += delta;

		// update camera
		{
			Player* p = &player;
			vec2 radius = p->prev_radius;

			if (camera_lock == 0.0f) {
				float cam_target_x = p->pos.x - window.game_width / 2;
				float cam_target_y = p->pos.y + radius.y - 19 - window.game_height / 2;

				if (camera_pos.x < cam_target_x - 8) {
					Approach(&camera_pos.x, cam_target_x - 8, 16 * delta);
				}
				if (camera_pos.x > cam_target_x + 8) {
					Approach(&camera_pos.x, cam_target_x + 8, 16 * delta);
				}

				if (player_is_grounded(p)) {
					float camera_speed = 16;

					if (fabsf(p->ground_speed) < 8) {
						camera_speed = 6;
					}

					Approach(&camera_pos.y, cam_target_y, camera_speed * delta);
				} else {
					if (camera_pos.y < cam_target_y - 32) {
						Approach(&camera_pos.y, cam_target_y - 32, 16 * delta);
					}
					if (camera_pos.y > cam_target_y + 32) {
						Approach(&camera_pos.y, cam_target_y + 32, 16 * delta);
					}
				}

				Clamp(&camera_pos.x, cam_target_x - window.game_width  / 2, cam_target_x + window.game_width  / 2);
				Clamp(&camera_pos.y, cam_target_y - window.game_height / 2, cam_target_y + window.game_height / 2);

				camera_pos = floor(camera_pos);

				Clamp(&camera_pos.x, 0.0f, (float) (tm.width  * 16 - window.game_width));
				Clamp(&camera_pos.y, 0.0f, (float) (tm.height * 16 - window.game_height));
			}

			camera_lock = fmaxf(camera_lock - delta, 0);
		}

		// update game objects
		For (it, objects) {
			switch (it->type) {
				case OBJ_SPRING: {
					if (it->spring.animating) {
						const Sprite& s = get_sprite(spr_spring_bounce_yellow + it->spring.color);

						it->spring.frame_index += s.anim_spd * delta;
						if (it->spring.frame_index >= s.frames.count) {
							it->spring.animating = false;
						}
					}
					break;
				}
			}
		}

		update_particles(delta);
	}

	{
		int mouse_x;
		int mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		vec2 mouse_pos_rel;
		mouse_pos_rel.x = ((mouse_x - renderer.game_texture_rect.x) / (float)renderer.game_texture_rect.w) * (float)window.game_width;
		mouse_pos_rel.y = ((mouse_y - renderer.game_texture_rect.y) / (float)renderer.game_texture_rect.h) * (float)window.game_height;

		mouse_world_pos = floor(mouse_pos_rel + camera_pos);
	}
}

void Game::draw(float delta) {
	set_proj_mat(get_ortho(0, window.game_width, window.game_height, 0));
	defer { set_proj_mat({1}); };

	set_view_mat(get_translation({-camera_pos.x, -camera_pos.y, 0}));
	defer { set_view_mat({1}); };

	set_viewport(0, 0, window.game_width, window.game_height);

	render_clear_color(get_color(0x01abe8ff));

	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	//defer { glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); };

	// draw bg
	{
		auto draw_part = [&](float bg_height, float bg_pos_y, const Texture& t, Rect src, float parallax) {
			vec2 pos;
			pos.x = camera_pos.x * parallax;
			pos.y = lerp(0.0f, tm.height * 16.0f - bg_height, camera_pos.y / (tm.height * 16.0f - window.game_height));

			pos.y += src.y;
			pos.y += bg_pos_y;

			while (pos.x + t.width < camera_pos.x) {
				pos.x += t.width;
			}

			pos = floor(pos);

			draw_texture(t, src, pos);

			if (pos.x + t.width < camera_pos.x + window.game_width) {
				pos.x += t.width;
				draw_texture(t, src, pos);
			}
		};

		const Texture& back  = get_texture(tex_bg_EE_back);
		const Texture& front = get_texture(tex_bg_EE_front);

		float bg_height = 512;

		draw_part(bg_height, 0, back, {0, 0, 1024, 96}, 0.96f);

		for (int i = 0; i < 12; i++) {
			float f = i / 11.0f;
			float parallax = lerp(0.94f, 0.92f, f);
			int y = 16 * (i + 6);
			draw_part(bg_height, 0, back, {0, y, 1024, 16}, parallax);
		}

		draw_part(bg_height, 176, front, {0,   0, 1024, 208}, 0.90f);
		draw_part(bg_height, 176, front, {0, 208, 1024, 128}, 0.88f);
	}

	// draw tilemap
	{
		int xfrom = clamp((int)camera_pos.x / 16, 0, tm.width  - 1);
		int yfrom = clamp((int)camera_pos.y / 16, 0, tm.height - 1);

		int xto = clamp((int)(camera_pos.x + window.game_width  + 15) / 16, 0, tm.width);
		int yto = clamp((int)(camera_pos.y + window.game_height + 15) / 16, 0, tm.height);

		// draw layer A
		for (int y = yfrom; y < yto; y++) {
			for (int x = xfrom; x < xto; x++) {
				Tile tile = get_tile(tm, x, y, 0);

				if (tile.index == 0) {
					continue;
				}

				Rect src;
				src.x = (tile.index % tileset_width) * 16;
				src.y = (tile.index / tileset_width) * 16;
				src.w = 16;
				src.h = 16;

				draw_texture(tileset_texture, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
			}
		}

		// draw layer C
		for (int y = yfrom; y < yto; y++) {
			for (int x = xfrom; x < xto; x++) {
				Tile tile = get_tile(tm, x, y, 2);

				if (tile.index == 0) {
					continue;
				}

				Rect src;
				src.x = (tile.index % tileset_width) * 16;
				src.y = (tile.index / tileset_width) * 16;
				src.w = 16;
				src.h = 16;

				draw_texture(tileset_texture, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
			}
		}

		// show_height
#ifdef DEVELOPER
		if (show_height || show_width) {
			for (int y = yfrom; y < yto; y++) {
				for (int x = xfrom; x < xto; x++) {
					Tile tile = get_tile(tm, x, y, player.layer);

					if (tile.index == 0) {
						continue;
					}

					Rect src;
					src.x = (tile.index % tileset_width) * 16;
					src.y = (tile.index / tileset_width) * 16;
					src.w = 16;
					src.h = 16;

					if (tile.top_solid || tile.lrb_solid) {
						draw_texture(show_height ? heightmap : widthmap, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
					}
				}
			}
		}
#endif
	}

	Player* p = &player;

	// draw player
	{
		int frame_index = p->frame_index;
		if (p->anim == anim_roll || p->anim == anim_spindash) {
			if (frame_index % 2 == 0) {
				frame_index = 0;
			} else {
				frame_index = 1 + frame_index / 2;
			}
		}

		const Sprite& s = anim_get_sprite(p->anim);

		//float angle = round_to(p->ground_angle, 45.0f);
		float angle = p->ground_angle;

		if ((player_is_grounded(p) && p->ground_speed == 0) || p->anim == anim_roll) {
			angle = 0;
		}

		draw_sprite(s, frame_index, floor(player.pos), {p->facing, 1}, angle);

		// draw spindash smoke
		if (p->anim == anim_spindash) {
			const Sprite& s = get_sprite(spr_spindash_smoke);

			int frame_index = SDL_GetTicks() / (16.66 * 2);
			frame_index %= s.frames.count;

			vec2 pos = floor(p->pos);

			draw_sprite(s, frame_index, pos, {p->facing, 1});
		}
	}

	// draw objects
	For (it, objects) {
		switch (it->type) {
			case OBJ_PLAYER_INIT_POS:
			case OBJ_LAYER_FLIP:
			case OBJ_LAYER_SET: {
				// don't draw
				break;
			}

			case OBJ_RING: {
				const Sprite& s = get_object_sprite(it->type);
				int frame_index = (SDL_GetTicks() / (int)(16.66 * 10)) % s.frames.count;
				draw_sprite(s, frame_index, it->pos);
				break;
			}

			case OBJ_MONITOR: {
				const Sprite& s = get_object_sprite(it->type);
				int frame_index = (SDL_GetTicks() / (int)(16.66 * 4)) % s.frames.count;
				draw_sprite(s, frame_index, it->pos);

				// draw monitor icon
				if (SDL_GetTicks() % (int)(16.66 * 4) < (int)(16.66 * 3)) {
					const Sprite& s = get_sprite(spr_monitor_icon);
					int frame_index = it->monitor.icon;

					vec2 pos = it->pos;
					pos.y -= 3;
					draw_sprite(s, frame_index, pos);
				}
				break;
			}

			case OBJ_SPRING: {
				u32 spr = spr_spring_yellow + it->spring.color;
				float frame_index = 0;

				if (it->spring.animating) {
					spr = spr_spring_bounce_yellow + it->spring.color;
					frame_index = it->spring.frame_index;
				}

				float angle = it->spring.direction * 90 - 90;

				draw_sprite(get_sprite(spr), frame_index, it->pos, {1, 1}, angle);
				break;
			}

			default: {
				const Sprite& s = get_object_sprite(it->type);
				draw_sprite(s, 0, it->pos);
				break;
			}
		}
	}

	// draw object hitboxes
	if (show_hitboxes) {
		For (it, objects) {
			vec2 size = get_object_size(*it);

			Rectf rect;
			rect.x = it->pos.x - size.x / 2;
			rect.y = it->pos.y - size.y / 2;
			rect.w = size.x;
			rect.h = size.y;

			draw_rectangle_outline(rect, color_white);
		}
	}

	// draw particles
	draw_particles(delta);

#ifdef DEVELOPER
	// draw player hitbox
	if (show_player_hitbox) {
		{
			auto radius = p->prev_radius;
			auto mode = p->prev_mode;

			auto rect = player_get_rect(p, radius, mode);
			rect.x = floorf(rect.x);
			rect.y = floorf(rect.y);
			rect.w += 1;
			rect.h += 1;

			draw_rectangle_outline(rect, get_color(128, 128, 255, 255));
		}

		// draw sensors
		{
			const vec4 SENSOR_A_COLOR = get_color(0, 240, 0, 255);
			const vec4 SENSOR_B_COLOR = get_color(56, 255, 162, 255);
			const vec4 SENSOR_C_COLOR = get_color(0, 174, 239, 255);
			const vec4 SENSOR_D_COLOR = get_color(255, 242, 56, 255);
			const vec4 SENSOR_E_COLOR = get_color(255, 56, 255, 255);
			const vec4 SENSOR_F_COLOR = get_color(255, 84, 84, 255);

			if (are_ground_sensors_active(p)) {
				vec2 sensor_a;
				vec2 sensor_b;
				get_ground_sensors_pos(p, &sensor_a, &sensor_b);

				draw_point(floor(sensor_a), SENSOR_A_COLOR);
				draw_point(floor(sensor_b), SENSOR_B_COLOR);
			}

			if (are_ceiling_sensors_active(p)) {
				vec2 sensor_c;
				vec2 sensor_d;
				get_ceiling_sensors_pos(p, &sensor_c, &sensor_d);

				draw_point(floor(sensor_c), SENSOR_C_COLOR);
				draw_point(floor(sensor_d), SENSOR_D_COLOR);
			}

			vec2 sensor_e;
			vec2 sensor_f;
			get_push_sensors_pos(p, &sensor_e, &sensor_f);

			auto draw_push_sensor = [&](Player* p, vec2 sensor, vec4 color) {
				switch (player_get_push_mode(p)) {
					case MODE_FLOOR:
					case MODE_CEILING:
						draw_line(floor(vec2{p->pos.x, sensor.y}), floor(sensor), color);
						break;
					case MODE_RIGHT_WALL:
					case MODE_LEFT_WALL:
						draw_line(floor(vec2{sensor.x, p->pos.y}), floor(sensor), color);
						break;
				}
			};

			if (is_push_sensor_e_active(p)) {
				draw_push_sensor(p, sensor_e, SENSOR_E_COLOR);
			}

			if (is_push_sensor_f_active(p)) {
				draw_push_sensor(p, sensor_f, SENSOR_F_COLOR);
			}
		}
	}
#endif

#ifdef DEVELOPER
	if (collision_test) {
		SensorResult res;
		vec2 pos = mouse_world_pos;

		res = sensor_check_up(pos, p->layer);
		draw_line(pos, pos + vec2{0, -res.dist}, color_blue);

		res = sensor_check_left(pos, p->layer);
		draw_line(pos, pos + vec2{-res.dist, 0}, color_blue);

		res = sensor_check_down(pos, p->layer);
		draw_line(pos, pos + vec2{0, res.dist}, color_red);

		res = sensor_check_right(pos, p->layer);
		draw_line(pos, pos + vec2{res.dist, 0}, color_red);
	}
#endif

	// :ui
	set_view_mat({1});

	// draw hud
	{
		vec2 pos = {16, 8};

		draw_text(get_font(fnt_hud), "score", pos, HALIGN_LEFT, VALIGN_TOP, color_yellow);
		pos.y += 16;

		draw_text(get_font(fnt_hud), "time", pos, HALIGN_LEFT, VALIGN_TOP, color_yellow);
		pos.y += 16;

		vec4 color = color_yellow;
		if (player_rings == 0) {
			if ((SDL_GetTicks() / 500) % 2) {
				color = color_red;
			}
		}

		draw_text(get_font(fnt_hud), "rings", pos, HALIGN_LEFT, VALIGN_TOP, color);
		pos.y += 16;

		pos = {112, 8};

		char buf[32];
		string str;

		if (p->state == STATE_DEBUG) {
			str = Sprintf(buf, "%x", (int)p->pos.x);
		} else {
			str = Sprintf(buf, "%d", player_score);
		}
		draw_text(get_font(fnt_hud), str, pos, HALIGN_RIGHT);
		pos.y += 16;

		int min = (int)(player_time / 3600.0f);
		int sec = (int)(player_time / 60.0f) % 60;
		int ms  = (int)(player_time / 60.0f * 100.0f) % 100; // not actually milliseconds

		if (p->state == STATE_DEBUG) {
			str = Sprintf(buf, "%x", (int)p->pos.y);
		} else {
			str = Sprintf(buf, "%d'%02d\"%02d", min, sec, ms);
		}
		draw_text(get_font(fnt_hud), str, pos, HALIGN_RIGHT);
		pos.y += 16;
		pos.x -= 24;

		str = Sprintf(buf, "%d", player_rings);
		draw_text(get_font(fnt_hud), str, pos, HALIGN_RIGHT);
		pos.y += 16;
	}

	// draw mobile controls
#if defined(__ANDROID__) || defined(PRETEND_MOBILE)
	{
		glm::ivec2 dpad_pos;
		glm::ivec2 action_pos;
		Rect action_rect;
		Rect pause_rect;
		get_mobile_controls(&dpad_pos, &action_pos, &action_rect, &pause_rect);

		vec4 color = {1, 1, 1, 0.85f};

		// dpad
		draw_sprite(get_sprite(spr_mobile_dpad), 0, dpad_pos, {1,1}, 0, color);

		draw_sprite(get_sprite(spr_mobile_dpad_up),    (p->input & INPUT_UP)    > 0, dpad_pos + glm::ivec2{25,  0}, {1,1}, 0, color);
		draw_sprite(get_sprite(spr_mobile_dpad_down),  (p->input & INPUT_DOWN)  > 0, dpad_pos + glm::ivec2{25, 37}, {1,1}, 0, color);
		draw_sprite(get_sprite(spr_mobile_dpad_left),  (p->input & INPUT_LEFT)  > 0, dpad_pos + glm::ivec2{ 0, 24}, {1,1}, 0, color);
		draw_sprite(get_sprite(spr_mobile_dpad_right), (p->input & INPUT_RIGHT) > 0, dpad_pos + glm::ivec2{37, 24}, {1,1}, 0, color);

		// action button
		draw_sprite(get_sprite(spr_mobile_action_button), (p->input & INPUT_JUMP) > 0, action_pos, {1,1}, 0, color);
	}
#endif

	break_batch();
}

void Game::late_draw(float delta) {
	{
		int backbuffer_width;
		int backbuffer_height;
		SDL_GL_GetDrawableSize(window.handle, &backbuffer_width, &backbuffer_height);

		break_batch();
		renderer.proj_mat = glm::ortho<float>(0, backbuffer_width, backbuffer_height, 0);
		renderer.view_mat = {1};
		renderer.model_mat = {1};
		glViewport(0, 0, backbuffer_width, backbuffer_height);
	}

	vec2 pos = {};

#ifdef DEVELOPER
	if (show_debug_info) {
		{
			char buf[256];
			string str = Sprintf(buf,
								 "frame: %fms\n"
								 "update: %fms\n"
								 "draw: %fms\n"
								 "draw calls: %d\n"
								 "total triangles: %d\n",
								 window.frame_took * 1000.0,
								 (window.frame_took - renderer.draw_took) * 1000.0,
								 renderer.draw_took * 1000.0,
								 renderer.draw_calls,
								 renderer.total_triangles);
			pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);
		}

		{
			Player* p = &player;

			char buf[256];
			string str = Sprintf(buf,
								 "state: %s\n"
								 "ground speed: %f\n"
								 "ground angle: %f\n",
								 GetPlayerStateName(p->state),
								 p->ground_speed,
								 p->ground_angle);
			pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);
		}
	}
#endif

	if (frame_advance) {
		string str = "F5 - Next Frame\nF6 - Disable Frame Advance Mode\n";
		pos = draw_text_shadow(get_font(fnt_consolas_bold), str, pos);
	}

	break_batch();
}

void free_tilemap(Tilemap* tm) {
	free(tm->tiles_a.data);
	free(tm->tiles_b.data);
	free(tm->tiles_c.data);

	*tm = {};
}

void free_tileset(Tileset* ts) {
	free(ts->heights.data);
	free(ts->widths.data);
	free(ts->angles.data);

	*ts = {};
}

void read_tilemap_old_format(Tilemap* tm, const char* fname) {
	free_tilemap(tm);

	SDL_RWops* f = SDL_RWFromFile(fname, "rb");

	if (!f) {
		log_error("Couldn't open old tilemap \"%s\".", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	SDL_RWread(f, &tm->width,  sizeof(tm->width),  1);
	SDL_RWread(f, &tm->height, sizeof(tm->height), 1);

	if (tm->width <= 0 || tm->height <= 0) {
		log_error("Tilemap \"%s\" size is invalid.", fname);
		return;
	}

	float start_x;
	float start_y;
	SDL_RWread(f, &start_x, sizeof(start_x), 1);
	SDL_RWread(f, &start_y, sizeof(start_y), 1);

	if (start_x < 0.0f || start_y < 0.0f) {
		log_error("Tilemap \"%s\" start position is invalid.", fname);
		return;
	}

	struct OldTile {
		int index;
		bool hflip : 1;
		bool vflip : 1;
		bool top_solid : 1;
		bool left_right_bottom_solid : 1;
	};

	OldTile* old_tiles_a = (OldTile*) calloc(tm->width * tm->height, sizeof(old_tiles_a[0]));
	defer { free(old_tiles_a); };

	OldTile* old_tiles_b = (OldTile*) calloc(tm->width * tm->height, sizeof(old_tiles_b[0]));
	defer { free(old_tiles_b); };

	SDL_RWread(f, old_tiles_a, sizeof(old_tiles_a[0]), tm->width * tm->height);
	SDL_RWread(f, old_tiles_b, sizeof(old_tiles_b[0]), tm->width * tm->height);

	tm->tiles_a = calloc_array<Tile>(tm->width * tm->height);
	tm->tiles_b = calloc_array<Tile>(tm->width * tm->height);
	tm->tiles_c = calloc_array<Tile>(tm->width * tm->height);

	for (int i = 0; i < tm->width * tm->height; i++) {
		tm->tiles_a[i].index = old_tiles_a[i].index;
		tm->tiles_a[i].hflip = old_tiles_a[i].hflip;
		tm->tiles_a[i].vflip = old_tiles_a[i].vflip;
		tm->tiles_a[i].top_solid = old_tiles_a[i].top_solid;
		tm->tiles_a[i].lrb_solid = old_tiles_a[i].left_right_bottom_solid;
	}

	for (int i = 0; i < tm->width * tm->height; i++) {
		tm->tiles_b[i].index = old_tiles_b[i].index;
		tm->tiles_b[i].hflip = old_tiles_b[i].hflip;
		tm->tiles_b[i].vflip = old_tiles_b[i].vflip;
		tm->tiles_b[i].top_solid = old_tiles_b[i].top_solid;
		tm->tiles_b[i].lrb_solid = old_tiles_b[i].left_right_bottom_solid;
	}
}

void read_tileset_old_format(Tileset* ts, const char* fname) {
	free_tileset(ts);

	SDL_RWops* f = SDL_RWFromFile(fname, "rb");

	if (!f) {
		log_error("Couldn't open old tileset \"%s\".", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	int tile_count_;
	SDL_RWread(f, &tile_count_, sizeof(tile_count_), 1);

	game.tileset_width = 16; // @Temp
	game.tileset_height = 28;

	ts->heights = calloc_array<u8>(game.tileset_width * game.tileset_height * 16);
	ts->widths  = calloc_array<u8>(game.tileset_width * game.tileset_height * 16);
	ts->angles  = calloc_array<float>(game.tileset_width * game.tileset_height);

	SDL_RWread(f, ts->heights.data, 16 * sizeof(ts->heights[0]), tile_count_);
	SDL_RWread(f, ts->widths.data,  16 * sizeof(ts->widths[0]), tile_count_);
	SDL_RWread(f, ts->angles.data,  sizeof(ts->angles[0]), tile_count_);
}

void write_tilemap(const Tilemap& tm, const char* fname) {
	SDL_RWops* f = SDL_RWFromFile(fname, "wb");

	if (!f) {
		log_error("Couldn't open file \"%s\" for writing.", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	char magic[4] = {'T', 'M', 'A', 'P'};
	SDL_RWwrite(f, magic, sizeof magic, 1);

	u32 version = 2;
	SDL_RWwrite(f, &version, sizeof version, 1);

	int width = tm.width;
	SDL_RWwrite(f, &width, sizeof width, 1);

	int height = tm.height;
	SDL_RWwrite(f, &height, sizeof height, 1);

	Assert(tm.tiles_a.count == width * height);
	SDL_RWwrite(f, tm.tiles_a.data, sizeof(tm.tiles_a[0]), tm.tiles_a.count);

	Assert(tm.tiles_b.count == width * height);
	SDL_RWwrite(f, tm.tiles_b.data, sizeof(tm.tiles_b[0]), tm.tiles_b.count);

	Assert(tm.tiles_c.count == width * height);
	SDL_RWwrite(f, tm.tiles_c.data, sizeof(tm.tiles_c[0]), tm.tiles_c.count);
}

void write_tileset(const Tileset& ts, const char* fname) {
	SDL_RWops* f = SDL_RWFromFile(fname, "wb");

	if (!f) {
		log_error("Couldn't open file \"%s\" for writing.", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	char magic[4] = {'T', 'S', 'E', 'T'};
	SDL_RWwrite(f, magic, sizeof magic, 1);

	u32 version = 1;
	SDL_RWwrite(f, &version, sizeof version, 1);

	int count = ts.angles.count;
	SDL_RWwrite(f, &count, sizeof count, 1);

	auto heights = ts.heights;
	Assert(heights.count == count * 16);
	SDL_RWwrite(f, heights.data, sizeof(heights[0]), heights.count);

	auto widths = ts.widths;
	Assert(widths.count == count * 16);
	SDL_RWwrite(f, widths.data, sizeof(widths[0]), widths.count);

	auto angles = ts.angles;
	Assert(angles.count == count);
	SDL_RWwrite(f, angles.data, sizeof(angles[0]), angles.count);
}

void read_tilemap(Tilemap* tm, const char* fname) {
	free_tilemap(tm);

	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (!filedata) return;

	SDL_RWops* f = SDL_RWFromConstMem(filedata, filesize);
	defer { SDL_RWclose(f); };

	char magic[4];
	SDL_RWread(f, magic, sizeof magic, 1);
	Assert(magic[0] == 'T'
		   && magic[1] == 'M'
		   && magic[2] == 'A'
		   && magic[3] == 'P');
	
	u32 version;
	SDL_RWread(f, &version, sizeof version, 1);
	Assert(version >= 1 && version <= 2);

	int width;
	SDL_RWread(f, &width, sizeof width, 1);
	Assert(width > 0);
	tm->width = width;

	int height;
	SDL_RWread(f, &height, sizeof height, 1);
	Assert(height > 0);
	tm->height = height;

	tm->tiles_a = calloc_array<Tile>(width * height);
	SDL_RWread(f, tm->tiles_a.data, sizeof(tm->tiles_a[0]), tm->tiles_a.count);

	tm->tiles_b = calloc_array<Tile>(width * height);
	SDL_RWread(f, tm->tiles_b.data, sizeof(tm->tiles_b[0]), tm->tiles_b.count);

	tm->tiles_c = calloc_array<Tile>(width * height);
	if (version >= 2) {
		SDL_RWread(f, tm->tiles_c.data, sizeof(tm->tiles_c[0]), tm->tiles_c.count);
	}
}

void read_tileset(Tileset* ts, const char* fname) {
	free_tileset(ts);

	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (!filedata) return;

	SDL_RWops* f = SDL_RWFromConstMem(filedata, filesize);
	defer { SDL_RWclose(f); };

	char magic[4];
	SDL_RWread(f, magic, sizeof magic, 1);
	Assert(magic[0] == 'T'
				&& magic[1] == 'S'
				&& magic[2] == 'E'
				&& magic[3] == 'T');

	u32 version;
	SDL_RWread(f, &version, sizeof version, 1);
	Assert(version == 1);

	int count;
	SDL_RWread(f, &count, sizeof count, 1);
	Assert(count > 0);

	auto heights = calloc_array<u8>(count * 16); // TODO: add some kind of validation and free this if there's an error.
	SDL_RWread(f, heights.data, sizeof(heights[0]), heights.count);

	auto widths = calloc_array<u8>(count * 16); // TODO
	SDL_RWread(f, widths.data, sizeof(widths[0]), widths.count);

	auto angles = calloc_array<float>(count); // TODO
	SDL_RWread(f, angles.data, sizeof(angles[0]), angles.count);

	ts->heights = heights;
	ts->widths = widths;
	ts->angles = angles;
}

void write_objects(array<Object> objects, const char* fname) {
	SDL_RWops* f = SDL_RWFromFile(fname, "wb");

	if (!f) {
		log_error("Couldn't open file \"%s\" for writing.", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	char magic[4] = {'O', 'B', 'J', 'T'};
	SDL_RWwrite(f, magic, sizeof magic, 1);

	u32 version = 1;
	SDL_RWwrite(f, &version, sizeof version, 1);

	u32 num_objects = (u32) objects.count;
	SDL_RWwrite(f, &num_objects, sizeof num_objects, 1);

	auto serialize = [&](const Object& o) {
		ObjType type = o.type;
		SDL_RWwrite(f, &type, sizeof type, 1);

		u32 flags = o.flags;
		SDL_RWwrite(f, &flags, sizeof flags, 1);

		vec2 pos = o.pos;
		SDL_RWwrite(f, &pos, sizeof pos, 1);

		switch (o.type) {
			case OBJ_PLAYER_INIT_POS:
			case OBJ_RING:
				break;

			case OBJ_LAYER_SET: {
				vec2 radius = o.layset.radius;
				SDL_RWwrite(f, &radius, sizeof radius, 1);

				int layer = o.layset.layer;
				SDL_RWwrite(f, &layer, sizeof layer, 1);
				break;
			}

			case OBJ_LAYER_FLIP: {
				vec2 radius = o.layflip.radius;
				SDL_RWwrite(f, &radius, sizeof radius, 1);
				break;
			}

			case OBJ_MONITOR: {
				MonitorIcon icon = o.monitor.icon;
				SDL_RWwrite(f, &icon, sizeof icon, 1);
				break;
			}

			case OBJ_SPRING: {
				SpringColor color = o.spring.color;
				SDL_RWwrite(f, &color, sizeof color, 1);

				Direction direction = o.spring.direction;
				SDL_RWwrite(f, &direction, sizeof direction, 1);
				break;
			}

			default: {
				Assert(false);
				break;
			}
		}
	};

	For (it, objects) {
		serialize(*it);
	}
}

void read_objects(bump_array<Object>* objects, const char* fname) {
	objects->count = 0; // clear

	size_t filesize;
	u8* filedata = get_file(fname, &filesize);

	if (!filedata) return;

	SDL_RWops* f = SDL_RWFromConstMem(filedata, filesize);
	defer { SDL_RWclose(f); };

	char magic[4];
	SDL_RWread(f, magic, sizeof magic, 1);
	Assert(magic[0] == 'O'
		   && magic[1] == 'B'
		   && magic[2] == 'J'
		   && magic[3] == 'T');

	u32 version;
	SDL_RWread(f, &version, sizeof version, 1);
	Assert(version == 1);

	u32 num_objects;
	SDL_RWread(f, &num_objects, sizeof num_objects, 1);
	
	auto deserialize = [&](Object* o) {
		ObjType type;
		SDL_RWread(f, &type, sizeof type, 1);
		o->type = type;

		u32 flags;
		SDL_RWread(f, &flags, sizeof flags, 1);
		o->flags = flags;

		vec2 pos;
		SDL_RWread(f, &pos, sizeof pos, 1);
		o->pos = pos;

		switch (o->type) {
			case OBJ_PLAYER_INIT_POS:
			case OBJ_RING:
				break;

			case OBJ_LAYER_SET: {
				vec2 radius;
				SDL_RWread(f, &radius, sizeof radius, 1);
				o->layset.radius = radius;

				int layer;
				SDL_RWread(f, &layer, sizeof layer, 1);
				o->layset.layer = layer;
				break;
			}

			case OBJ_LAYER_FLIP: {
				vec2 radius;
				SDL_RWread(f, &radius, sizeof radius, 1);
				o->layflip.radius = radius;
				break;
			}

			case OBJ_MONITOR: {
				MonitorIcon icon;
				SDL_RWread(f, &icon, sizeof icon, 1);
				o->monitor.icon = icon;
				break;
			}

			case OBJ_SPRING: {
				SpringColor color;
				SDL_RWread(f, &color, sizeof color, 1);
				o->spring.color = color;

				Direction direction;
				SDL_RWread(f, &direction, sizeof direction, 1);
				o->spring.direction = direction;
				break;
			}

			default: {
				Assert(false);
				break;
			}
		}
	};

	for (u32 i = 0; i < num_objects; i++) {
		Object o = {};

		deserialize(&o);

		array_add(objects, o);
	}
}

void gen_heightmap_texture(Texture* heightmap, const Tileset& ts, const Texture& tileset_texture) {
	free_texture(heightmap);

	heightmap->width  = tileset_texture.width;
	heightmap->height = tileset_texture.height;

	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, heightmap->width, heightmap->height, 32, SDL_PIXELFORMAT_ABGR8888);
	defer { SDL_FreeSurface(surf); };

	SDL_FillRect(surf, nullptr, 0x00000000);

	int stride = tileset_texture.width / 16;

	for (int tile_index = 0; tile_index < ts.angles.count; tile_index++) {
		array<u8> heights = get_tile_heights(ts, tile_index);

		for (int i = 0; i < 16; i++) {
			if (heights[i] != 0) {
				if (heights[i] <= 0x10) {
					SDL_Rect line = {
						(tile_index % stride) * 16 + i,
						(tile_index / stride) * 16 + (16 - heights[i]),
						1,
						heights[i]
					};
					SDL_FillRect(surf, &line, SDL_MapRGB(surf->format, 255, 255, 255));
				} else if (heights[i] >= 0xF0) {
					SDL_Rect line = {
						(tile_index % stride) * 16 + i,
						(tile_index / stride) * 16,
						1,
						16 - (heights[i] - 0xF0)
					};
					SDL_FillRect(surf, &line, SDL_MapRGB(surf->format, 255, 0, 0));
				}
			}
		}
	}

	glGenTextures(1, &heightmap->ID);
	glBindTexture(GL_TEXTURE_2D, heightmap->ID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, heightmap->width, heightmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void gen_widthmap_texture(Texture* widthmap, const Tileset& ts, const Texture& tileset_texture) {
	free_texture(widthmap);

	widthmap->width  = tileset_texture.width;
	widthmap->height = tileset_texture.height;

	SDL_Surface* wsurf = SDL_CreateRGBSurfaceWithFormat(0, widthmap->width, widthmap->height, 32, SDL_PIXELFORMAT_ABGR8888);
	defer { SDL_FreeSurface(wsurf); };

	SDL_FillRect(wsurf, nullptr, 0x00000000);

	int stride = tileset_texture.width / 16;

	for (int tile_index = 0; tile_index < ts.angles.count; tile_index++) {
		array<u8> widths  = get_tile_widths(ts, tile_index);

		for (int i = 0; i < 16; i++) {
			if (widths[i] != 0) {
				if (widths[i] <= 0x10) {
					SDL_Rect line = {
						(tile_index % stride) * 16 + 16 - widths[i],
						(tile_index / stride) * 16 + i,
						widths[i],
						1
					};
					SDL_FillRect(wsurf, &line, SDL_MapRGB(wsurf->format, 255, 255, 255));
				} else if (widths[i] >= 0xF0) {
					SDL_Rect line = {
						(tile_index % stride) * 16,
						(tile_index / stride) * 16 + i,
						16 - (widths[i] - 0xF0),
						1
					};
					SDL_FillRect(wsurf, &line, SDL_MapRGB(wsurf->format, 0, 0, 255));
				}
			}
		}
	}

	glGenTextures(1, &widthmap->ID);
	glBindTexture(GL_TEXTURE_2D, widthmap->ID);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, widthmap->width, widthmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, wsurf->pixels);

	glBindTexture(GL_TEXTURE_2D, 0);
}

const Sprite& get_object_sprite(ObjType type) {
	switch (type) {
		case OBJ_PLAYER_INIT_POS: return get_sprite(spr_sonic_idle);
		case OBJ_LAYER_SET:       return get_sprite(spr_layer_set);
		case OBJ_LAYER_FLIP:      return get_sprite(spr_layer_flip);
		case OBJ_RING:            return get_sprite(spr_ring);
		case OBJ_MONITOR:         return get_sprite(spr_monitor);
		case OBJ_SPRING:          return get_sprite(spr_spring_yellow);
		case OBJ_MONITOR_BROKEN:  return get_sprite(spr_monitor_broken);
	}

	Assert(!"invalid object type");
	return get_sprite(0);
}

vec2 get_object_size(const Object& o) {
	switch (o.type) {
		case OBJ_PLAYER_INIT_POS: return {30, 44};
		case OBJ_LAYER_SET:       return {o.layset.radius.x  * 2, o.layset.radius.y  * 2};
		case OBJ_LAYER_FLIP:      return {o.layflip.radius.x * 2, o.layflip.radius.y * 2};
		case OBJ_MONITOR:         return {30, 30};
		case OBJ_SPRING: {
			vec2 size = {32, 16};
			if (o.spring.direction == DIR_LEFT || o.spring.direction == DIR_RIGHT) {
				float temp = size.x;
				size.x = size.y;
				size.y = temp;
			}
			return size;
		}
	}

	const Sprite& s = get_object_sprite(o.type);
	return {s.width, s.height};
}
