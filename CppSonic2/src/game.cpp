#include "game.h"

#include "renderer.h"
#include "window_creation.h"
#include "package.h"
#include "console.h"

Game game;

void Game::load_level(const char* path) {
	// load tileset texture
	char buf[512];
	stb_snprintf(buf, sizeof(buf), "%s/Tileset.png", path);

	load_texture_from_file(&tileset_texture, buf);

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

	gen_heightmap_texture(&heightmap, ts, tileset_texture);
	gen_widthmap_texture(&widthmap, ts, tileset_texture);
}

void Game::init() {
	load_bmfont_file(&font,          "fonts/ms_gothic.fnt",     "fonts/ms_gothic_0.png");
	load_bmfont_file(&consolas,      "fonts/consolas.fnt",      "fonts/consolas_0.png");
	load_bmfont_file(&consolas_bold, "fonts/consolas_bold.fnt", "fonts/consolas_bold_0.png");

	load_level("levels/GHZ_Act1");

	// @Leak
	load_texture_from_file(&anim_textures[anim_crouch],   "textures/crouch.png");
	load_texture_from_file(&anim_textures[anim_idle],     "textures/idle.png");
	load_texture_from_file(&anim_textures[anim_look_up],  "textures/look_up.png");
	load_texture_from_file(&anim_textures[anim_peelout],  "textures/peelout.png");
	load_texture_from_file(&anim_textures[anim_roll],     "textures/roll.png");
	load_texture_from_file(&anim_textures[anim_run],      "textures/run.png");
	load_texture_from_file(&anim_textures[anim_skid],     "textures/skid.png");
	load_texture_from_file(&anim_textures[anim_spindash], "textures/spindash.png");
	load_texture_from_file(&anim_textures[anim_walk],     "textures/walk.png");

	player.pos.x = 80; // @Temp
	player.pos.y = 944;

	camera_pos.x = player.pos.x - window.game_width / 2;
	camera_pos.y = player.pos.y - window.game_height / 2;
}

void Game::deinit() {

}

constexpr float PLAYER_ACC = 0.046875f;
constexpr float PLAYER_DEC = 0.5f;
constexpr float PLAYER_FRICTION = 0.046875f;
constexpr float PLAYER_TOP_SPEED = 6.0f;

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

static void get_push_sensors_pos(Player* p, vec2* sensor_e, vec2* sensor_f) {
	const float PLAYER_PUSH_RADIUS = 10.0f;

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

	Tile tile = get_tile(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y++;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		// NOTE: this should be wrap(iy,16) instead of modulo if we care about negative numbers. Also change it in get_height.
		result.dist = (32 - (iy % 16)) - (height + 1);
	} else if (height == 16) {
		tile_y--;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
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

	Tile tile = get_tile(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x++;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (32 - (ix % 16)) - (height + 1);
	} else if (height == 16) {
		tile_x--;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
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

	Tile tile = get_tile(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y--;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (iy % 16) - (height);
	} else if (height == 16) {
		tile_y++;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
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

	Tile tile = get_tile(game.tm, tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x--;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (ix % 16) - (height);
	} else if (height == 16) {
		tile_x++;
		tile = get_tile(game.tm, tile_x, tile_y, layer);
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
	int input_h = 0;
	if (p->control_lock == 0) {
		if (is_key_held(SDL_SCANCODE_RIGHT)) input_h++;
		if (is_key_held(SDL_SCANCODE_LEFT))  input_h--;
	}

	return (fabsf(p->ground_speed) >= 0.5f
			&& is_key_held(SDL_SCANCODE_DOWN)
			&& input_h == 0);
}

static void ground_sensor_collision(Player* p) {
	if (!are_ground_sensors_active(p)) {
		return;
	}

	vec2 sensor_a;
	vec2 sensor_b;
	get_ground_sensors_pos(p, &sensor_a, &sensor_b);

	SensorResult res   = ground_sensor_check(p, sensor_a);
	SensorResult res_b = ground_sensor_check(p, sensor_b);
	if (res_b.dist < res.dist) {
		res = res_b;
	}

	float check_dist;
	if (player_is_grounded(p)) {
		check_dist = 14;
	} else {
		float check_speed = player_is_on_a_wall(p) ? p->speed.y : p->speed.x;
		check_dist = fminf(fabsf(check_speed) + 4, 14);
	}

	if (res.dist > check_dist) {
		p->state = STATE_AIR;
		return;
	}
	if (res.dist < -14) {
		return;
	}

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
		} else {
			p->state = STATE_GROUND;
		}
	}
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

	if (p->control_lock > 0) {
		return false;
	}

	p->speed.x -= PLAYER_JUMP_FORCE * dsin(p->ground_angle);
	p->speed.y -= PLAYER_JUMP_FORCE * dcos(p->ground_angle);
	p->state = STATE_AIR;
	p->ground_angle = 0;
	p->next_anim = anim_roll;
	p->frame_duration = fmaxf(0, 4 - fabsf(p->ground_speed));
	p->jumped = true;

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

static void player_state_ground(Player* p, float delta) {
	int input_h = 0;
	if (p->control_lock == 0) {
		if (is_key_held(SDL_SCANCODE_RIGHT)) input_h++;
		if (is_key_held(SDL_SCANCODE_LEFT))  input_h--;
	}

	// TODO: Check for special animations that prevent control (such as balancing).

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
			} else {
				if (fabsf(p->ground_speed) < PLAYER_TOP_SPEED) {
					p->ground_speed += input_h * PLAYER_ACC * delta;
					Clamp(&p->ground_speed, -PLAYER_TOP_SPEED, PLAYER_TOP_SPEED);
				}
			}
		}
	}

	// TODO: Check for starting crouching, balancing on ledges, etc.

	// Move the Player object
	p->speed.x =  dcos(p->ground_angle) * p->ground_speed;
	p->speed.y = -dsin(p->ground_angle) * p->ground_speed;

	p->pos += p->speed * delta;

	// Push Sensor collision occurs.
	push_sensor_collision(p);

	// Grounded Ground Sensor collision occurs.
	ground_sensor_collision(p);

	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	player_keep_in_bounds(p);

	if (p->anim != anim_spindash && !p->peelout) {
		if (p->ground_speed == 0.0f) {
			if (is_key_held(SDL_SCANCODE_DOWN)) {
				p->next_anim = anim_crouch;
				p->frame_duration = 1;
			} else if (is_key_held(SDL_SCANCODE_UP)) {
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

	if (p->ground_speed != 0) {
		p->facing = sign_int(p->ground_speed);
	}

	// Check for slipping/falling when Ground Speed is too low on walls/ceilings.
	if (player_try_slip(p)) {
		return;
	}

	// Check for starting a jump.
	if (is_key_pressed(SDL_SCANCODE_Z)) {
		if (p->anim == anim_crouch) {
			// start spindash
			p->next_anim = anim_spindash;
			p->frame_duration = 1;
			p->spinrev = 0;
		} else if (p->anim == anim_look_up) {
			// start peelout
			p->peelout = true;
			p->spinrev = 0;
		} else if (p->anim == anim_spindash) {
			// charge up spindash
			p->spinrev += 2;
			p->spinrev = fminf(p->spinrev, 8);
			p->frame_index = 0;
		} else {
			if (!p->peelout) {
				if (player_try_jump(p)) {
					return;
				}
			}
		}
	}

	if (p->anim == anim_spindash) {
		p->spinrev -= floorf(p->spinrev * 8) / 256 * delta;

		if (!is_key_held(SDL_SCANCODE_DOWN)) {
			p->state = STATE_ROLL;
			p->ground_speed = (8 + floorf(p->spinrev) / 2) * p->facing;
			p->next_anim = anim_roll;
			p->frame_duration = fmaxf(0, 4 - fabsf(p->ground_speed));
			game.camera_lock = 24 - floorf(fabsf(p->ground_speed));
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

		if (!is_key_held(SDL_SCANCODE_UP)) {
			p->state = STATE_GROUND;
			if (p->spinrev >= 15) {
				p->ground_speed = speed;
				game.camera_lock = 24 - floorf(fabsf(p->ground_speed));
			}
			p->peelout = false;
			return;
		}
	}

	// Check for starting a roll.
	if (player_roll_condition(p)) {
		p->state = STATE_ROLL;
		return;
	}
}

static void player_state_roll(Player* p, float delta) {
	int input_h = 0;
	if (p->control_lock == 0) {
		if (is_key_held(SDL_SCANCODE_RIGHT)) input_h++;
		if (is_key_held(SDL_SCANCODE_LEFT))  input_h--;
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

	// Move the Player object
	p->pos += p->speed * delta;

	// Push Sensor collision occurs.
	push_sensor_collision(p);

	// Grounded Ground Sensor collision occurs.
	ground_sensor_collision(p);

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
	if (is_key_pressed(SDL_SCANCODE_Z)) {
		if (player_try_jump(p)) {
			return;
		}
	}
}

static void player_state_air(Player* p, float delta) {
	int input_h = 0;
	if (p->control_lock == 0) {
		if (is_key_held(SDL_SCANCODE_RIGHT)) input_h++;
		if (is_key_held(SDL_SCANCODE_LEFT))  input_h--;
	}

	// Check for jump button release (variable jump velocity).
	if (p->jumped && !is_key_held(SDL_SCANCODE_Z)) {
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

	// Move the Player object
	p->pos += p->speed * delta;

	// All air collision checks occur here.
	push_sensor_collision(p);
	ground_sensor_collision(p);

	// Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).
	player_keep_in_bounds(p);

	// Rotate Ground Angle back to 0.
	p->ground_angle -= clamp(angle_difference(p->ground_angle, 0.0f), -2.8125f * delta, 2.8125f * delta);

	if (p->speed.x != 0) {
		p->facing = sign_int(p->speed.x);
	}
}

static void player_update(Player* p, float delta) {
	// clear flags
	if (p->state != STATE_AIR) {
		p->jumped = false;
	}
	if (p->state != STATE_GROUND) {
		p->peelout = false;
	}

	p->prev_mode = player_get_mode(p);
	p->prev_radius = player_get_radius(p);

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

			if (is_key_held(SDL_SCANCODE_UP))    { p->pos.y -= spd * delta; }
			if (is_key_held(SDL_SCANCODE_DOWN))  { p->pos.y += spd * delta; }
			if (is_key_held(SDL_SCANCODE_LEFT))  { p->pos.x -= spd * delta; }
			if (is_key_held(SDL_SCANCODE_RIGHT)) { p->pos.x += spd * delta; }
			break;
		}
	}

	if (p->state != STATE_AIR) {
		p->control_lock = fmaxf(p->control_lock - delta, 0);
	}

	auto anim_get_frame_count = [&](anim_index anim) -> int {
		Texture t = game.anim_textures[anim];
		return t.width / 59;
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
	if (is_key_pressed(SDL_SCANCODE_A)) {
		if (p->state == STATE_DEBUG) {
			p->state = STATE_AIR;
			p->speed = {};
			p->ground_speed = 0;
			p->jumped = false;
			p->ground_angle = 0;
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

	if (!skip_frame) {
		player_update(&player, delta);

		// update camera
		{
			Player* p = &player;
			vec2 radius = p->prev_radius;

			if (camera_lock == 0.0f) {
				float cam_target_x = p->pos.x - window.game_width / 2;
				float cam_target_y = p->pos.y + radius.y - 19 - window.game_height / 2;

				if (camera_pos.x < cam_target_x - 8) {
					camera_pos.x = fminf(camera_pos.x + 16 * delta, cam_target_x - 8);
				}
				if (camera_pos.x > cam_target_x + 8) {
					camera_pos.x = fmaxf(camera_pos.x - 16 * delta, cam_target_x + 8);
				}

				if (player_is_grounded(p)) {
					float camera_speed = 16;

					if (fabsf(p->ground_speed) < 8) {
						camera_speed = 6;
					}

					Approach(&camera_pos.y, cam_target_y, camera_speed * delta);
				} else {
					if (camera_pos.y < cam_target_y - 32) {
						camera_pos.y = fminf(camera_pos.y + 16 * delta, cam_target_y - 32);
					}
					if (camera_pos.y > cam_target_y + 32) {
						camera_pos.y = fmaxf(camera_pos.y - 16 * delta, cam_target_y + 32);
					}
				}

				camera_pos = glm::floor(camera_pos);

				Clamp(&camera_pos.x, 0.0f, (float) (tm.width  * 16 - window.game_width));
				Clamp(&camera_pos.y, 0.0f, (float) (tm.height * 16 - window.game_height));
			}

			camera_lock = fmaxf(camera_lock - delta, 0);
		}
	}

	{
		int mouse_x;
		int mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		vec2 mouse_pos_rel;
		mouse_pos_rel.x = ((mouse_x - renderer.game_texture_rect.x) / (float)renderer.game_texture_rect.w) * (float)window.game_width;
		mouse_pos_rel.y = ((mouse_y - renderer.game_texture_rect.y) / (float)renderer.game_texture_rect.h) * (float)window.game_height;

		mouse_world_pos = glm::floor(mouse_pos_rel + camera_pos);
	}

	if (is_key_pressed(SDL_SCANCODE_F4)) {
		set_fullscreen(!is_fullscreen());
	}
}

void Game::draw(float delta) {
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = glm::translate(mat4{1}, vec3{-camera_pos.x, -camera_pos.y, 0});

	// draw tilemap
	{
		int xfrom = clamp((int)camera_pos.x / 16, 0, tm.width  - 1);
		int yfrom = clamp((int)camera_pos.y / 16, 0, tm.height - 1);

		int xto = clamp((int)(camera_pos.x + window.game_width  + 15) / 16, 0, tm.width);
		int yto = clamp((int)(camera_pos.y + window.game_height + 15) / 16, 0, tm.height);

		for (int y = yfrom; y < yto; y++) {
			for (int x = xfrom; x < xto; x++) {
				Tile tile = get_tile_a(tm, x, y);

				if (tile.index == 0) {
					continue;
				}

				Rect src;
				src.x = (tile.index % tileset_width) * 16;
				src.y = (tile.index / tileset_width) * 16;
				src.w = 16;
				src.h = 16;

				draw_texture(tileset_texture, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});

#ifdef DEVELOPER
				if (show_height || show_width) {
					if (is_key_held(SDL_SCANCODE_TAB)) {
						tile = get_tile(tm, x, y, 1);
					} else {
						tile = get_tile(tm, x, y, 0);
					}

					src.x = (tile.index % tileset_width) * 16;
					src.y = (tile.index / tileset_width) * 16;
					src.w = 16;
					src.h = 16;
				
					if (tile.top_solid || tile.lrb_solid) {
						draw_texture(show_height ? heightmap : widthmap, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
					}
				}
#endif
			}
		}
	}

	Player* p = &player;

	{
		int frame_index = p->frame_index;
		if (p->anim == anim_roll || p->anim == anim_spindash) {
			if (frame_index % 2 == 0) {
				frame_index = 0;
			} else {
				frame_index = 1 + frame_index / 2;
			}
		}

		Texture t = anim_textures[p->anim];

		Rect src;
		src.x = frame_index * 59;
		src.y = 0;
		src.w = 59;
		src.h = 59;

		//float angle = round_to(p->ground_angle, 45.0f);
		float angle = p->ground_angle;

		if ((player_is_grounded(p) && p->ground_speed == 0) || p->anim == anim_roll) {
			angle = 0;
		}

		draw_texture(t, src, glm::floor(player.pos), {p->facing, 1}, {30, 30}, angle);
	}

#ifdef DEVELOPER
	// draw player hitbox
	{
		vec2 radius = p->prev_radius;

		Rectf rect;
		switch (p->prev_mode) {
			case MODE_FLOOR:
			case MODE_CEILING: {
				rect.x = floorf(p->pos.x - radius.x);
				rect.y = floorf(p->pos.y - radius.y);
				rect.w = radius.x * 2 + 1;
				rect.h = radius.y * 2 + 1;
				break;
			}
			case MODE_RIGHT_WALL:
			case MODE_LEFT_WALL: {
				rect.x = floorf(p->pos.x - radius.y);
				rect.y = floorf(p->pos.y - radius.x);
				rect.w = radius.y * 2 + 1;
				rect.h = radius.x * 2 + 1;
				break;
			}
		}

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

			draw_point(glm::floor(sensor_a), SENSOR_A_COLOR);
			draw_point(glm::floor(sensor_b), SENSOR_B_COLOR);
		}

		vec2 sensor_e;
		vec2 sensor_f;
		get_push_sensors_pos(p, &sensor_e, &sensor_f);

		auto draw_push_sensor = [&](Player* p, vec2 sensor, vec4 color) {
			switch (player_get_push_mode(p)) {
				case MODE_FLOOR:
				case MODE_CEILING:
					draw_line(glm::floor(vec2{p->pos.x, sensor.y}), glm::floor(sensor), color);
					break;
				case MODE_RIGHT_WALL:
				case MODE_LEFT_WALL:
					draw_line(glm::floor(vec2{sensor.x, p->pos.y}), glm::floor(sensor), color);
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

	break_batch();
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = {1};

	// draw fps
	{
		char buf[16];
		string str = Sprintf(buf, "%.0f", roundf(window.avg_fps));
		draw_text_shadow(font, str, {window.game_width, window.game_height}, HALIGN_RIGHT, VALIGN_BOTTOM);
	}
}

void Game::late_draw(float delta) {
	vec2 pos = {};

#ifdef DEVELOPER
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
		pos = draw_text_shadow(consolas_bold, str, pos);
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
		pos = draw_text_shadow(consolas_bold, str, pos);
	}
#endif

	if (frame_advance) {
		string str = "F5 - Next Frame\nF6 - Disable Frame Advance Mode\n";
		pos = draw_text_shadow(consolas_bold, str, pos);
	}

	break_batch();
}

void free_tilemap(Tilemap* tm) {
	free(tm->tiles_a.data);
	free(tm->tiles_b.data);

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

	ts->count = tile_count_;
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

	u32 version = 1;
	SDL_RWwrite(f, &version, sizeof version, 1);

	int width = tm.width;
	SDL_RWwrite(f, &width, sizeof width, 1);

	int height = tm.height;
	SDL_RWwrite(f, &height, sizeof height, 1);

	auto tiles_a = tm.tiles_a;
	Assert(tiles_a.count == width * height);
	SDL_RWwrite(f, tiles_a.data, sizeof(tiles_a[0]), tiles_a.count);

	auto tiles_b = tm.tiles_b;
	Assert(tiles_b.count == width * height);
	SDL_RWwrite(f, tiles_b.data, sizeof(tiles_b[0]), tiles_b.count);
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

	int count = ts.count;
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
	Assert(version == 1);

	int width;
	SDL_RWread(f, &width, sizeof width, 1);
	Assert(width > 0);

	int height;
	SDL_RWread(f, &height, sizeof height, 1);
	Assert(height > 0);

	auto tiles_a = calloc_array<Tile>(width * height); // TODO: add some kind of validation and free this if there's an error.
	SDL_RWread(f, tiles_a.data, sizeof(tiles_a[0]), tiles_a.count);

	auto tiles_b = calloc_array<Tile>(width * height); // TODO
	SDL_RWread(f, tiles_b.data, sizeof(tiles_b[0]), tiles_b.count);

	tm->width = width;
	tm->height = height;
	tm->tiles_a = tiles_a;
	tm->tiles_b = tiles_b;
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

	ts->count = count;
	ts->heights = heights;
	ts->widths = widths;
	ts->angles = angles;
}

void gen_heightmap_texture(Texture* heightmap, const Tileset& ts, const Texture& tileset_texture) {
	free_texture(heightmap);

	heightmap->width  = tileset_texture.width;
	heightmap->height = tileset_texture.height;

	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, heightmap->width, heightmap->height, 32, SDL_PIXELFORMAT_RGBA8888);
	defer { SDL_FreeSurface(surf); };

	SDL_FillRect(surf, nullptr, 0x00000000);

	int stride = tileset_texture.width / 16;

	for (int tile_index = 0; tile_index < ts.count; tile_index++) {
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
					SDL_FillRect(surf, &line, 0xffffffff);
				} else if (heights[i] >= 0xF0) {
					SDL_Rect line = {
						(tile_index % stride) * 16 + i,
						(tile_index / stride) * 16,
						1,
						16 - (heights[i] - 0xF0)
					};
					SDL_FillRect(surf, &line, 0xff8080ff);
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

	SDL_Surface* wsurf = SDL_CreateRGBSurfaceWithFormat(0, widthmap->width, widthmap->height, 32, SDL_PIXELFORMAT_RGBA8888);
	defer { SDL_FreeSurface(wsurf); };

	SDL_FillRect(wsurf, nullptr, 0x00000000);

	int stride = tileset_texture.width / 16;

	for (int tile_index = 0; tile_index < ts.count; tile_index++) {
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
					SDL_FillRect(wsurf, &line, 0xffffffff);
				} else if (widths[i] >= 0xF0) {
					SDL_Rect line = {
						(tile_index % stride) * 16,
						(tile_index / stride) * 16 + i,
						16 - (widths[i] - 0xF0),
						1
					};
					SDL_FillRect(wsurf, &line, 0xffff8080);
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

#if defined(DEVELOPER) && !defined(EDITOR)

bool console_callback(string str, void* userdata) {
	eat_whitespace(&str);
	string command = eat_non_whitespace(&str);

	if (command == "h" || command == "help") {
		console.write("Commands: collision_test show_width show_height\n");
		return true;
	}

	if (command == "collision_test") {
		game.collision_test ^= true;
		return true;
	}

	if (command == "show_height") {
		game.show_height ^= true;
		if (game.show_height) game.show_width = false;
		return true;
	}

	if (command == "show_width") {
		game.show_width ^= true;
		if (game.show_width) game.show_height = false;
		return true;
	}

	return false;
}

#endif
