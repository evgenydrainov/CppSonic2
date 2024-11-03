#include "game.h"

#include "renderer.h"
#include "window_creation.h"

Game game;

void Game::load_level(const char* path) {
	{
		char buf[512];
		stb_snprintf(buf, sizeof(buf), "%s/Tileset.png", path);

		tileset_texture = load_texture_from_file(buf);

		Assert(tileset_texture.width % 16 == 0);
		Assert(tileset_texture.height % 16 == 0);

		tileset_width  = tileset_texture.width  / 16;
		tileset_height = tileset_texture.height / 16;
	}

	
}

void Game::init() {
	font = load_bmfont_file("fonts/ms_gothic.fnt", "fonts/ms_gothic_0.png");

	load_level("levels/GHZ_Act1");

	load_tilemap_old_format("levels/tilemap.bin"); // @Temp
	load_tileset_old_format("levels/tileset.bin");

	// generate heightmap texture
	{
		heightmap.width = tileset_width * 16;
		heightmap.height = tileset_height * 16;

		SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, heightmap.width, heightmap.height, 32, SDL_PIXELFORMAT_RGBA8888);
		defer { SDL_FreeSurface(surf); };

		SDL_FillRect(surf, nullptr, 0x00000000);

		for (int tile_index = 0; tile_index < tileset_width * tileset_height; tile_index++) {
			array<u8> heights = get_tile_heights(tile_index);

			for (int i = 0; i < 16; i++) {
				if (heights[i] != 0) {
					if (heights[i] <= 0x10) {
						SDL_Rect line;
						line.x = (tile_index % 16) * 16 + i;
						line.y = (tile_index / 16) * 16 + (16 - heights[i]);
						line.w = 1;
						line.h = heights[i];
						SDL_FillRect(surf, &line, 0xffffffff);
					} else if (heights[i] >= 0xF0) {
						SDL_Rect line;
						line.x = (tile_index % 16) * 16 + i;
						line.y = (tile_index / 16) * 16;
						line.w = 1;
						line.h = 16 - (heights[i] - 0xF0);
						SDL_FillRect(surf, &line, 0xff0000ff);
					}
				}
			}
		}

		glGenTextures(1, &heightmap.ID); // @Leak
		glBindTexture(GL_TEXTURE_2D, heightmap.ID);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, heightmap.width, heightmap.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, surf->pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// @Leak
	texture_crouch   = load_texture_from_file("textures/crouch.png");
	texture_idle     = load_texture_from_file("textures/idle.png");
	texture_look_up  = load_texture_from_file("textures/look_up.png");
	texture_peelout  = load_texture_from_file("textures/peelout.png");
	texture_roll     = load_texture_from_file("textures/roll.png");
	texture_run      = load_texture_from_file("textures/run.png");
	texture_skid     = load_texture_from_file("textures/skid.png");
	texture_spindash = load_texture_from_file("textures/spindash.png");
	texture_walk     = load_texture_from_file("textures/walk.png");

	player.pos.x = 80; // @Temp
	player.pos.y = 944;
}

void Game::deinit() {

}

constexpr float PLAYER_ACC = 0.046875f;
constexpr float PLAYER_DEC = 0.5f;
constexpr float PLAYER_FRICTION = 0.046875f;
constexpr float PLAYER_TOP_SPEED = 6.0f;
constexpr float PLAYER_JUMP_FORCE = 6.5f;

static bool player_is_grounded(Player* p) {
	return (p->state == STATE_GROUND
			|| p->state == STATE_ROLL);
}

static PlayerMode player_get_mode(Player* p) {
	PlayerMode result;

	if (player_is_grounded(p)) {
		float a = angle_wrap(p->ground_angle);
		if (a <= 45.0f) {
			result = MODE_FLOOR;
		} else if (a <= 134.0f) {
			result = MODE_RIGHT_WALL;
		} else if (a <= 225.0f) {
			result = MODE_CEILING;
		} else if (a <= 314.0f) {
			result = MODE_LEFT_WALL;
		} else {
			result = MODE_FLOOR;
		}
	} else {
		result = MODE_FLOOR;
	}

	return result;
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

static void get_ground_sensors_pos(Player* p, vec2* sensor_a, vec2* sensor_b) {
	switch (player_get_mode(p)) {
		case MODE_FLOOR: {
			sensor_a->x = p->pos.x - p->radius.x;
			sensor_a->y = p->pos.y + p->radius.y;
			sensor_b->x = p->pos.x + p->radius.x;
			sensor_b->y = p->pos.y + p->radius.y;
			break;
		}
		case MODE_RIGHT_WALL: {
			sensor_a->x = p->pos.x + p->radius.y;
			sensor_a->y = p->pos.y + p->radius.x;
			sensor_b->x = p->pos.x + p->radius.y;
			sensor_b->y = p->pos.y - p->radius.x;
			break;
		}
		case MODE_CEILING: {
			sensor_a->x = p->pos.x + p->radius.x;
			sensor_a->y = p->pos.y - p->radius.y;
			sensor_b->x = p->pos.x - p->radius.x;
			sensor_b->y = p->pos.y - p->radius.y;
			break;
		}
		case MODE_LEFT_WALL: {
			sensor_a->x = p->pos.x - p->radius.y;
			sensor_a->y = p->pos.y - p->radius.x;
			sensor_b->x = p->pos.x - p->radius.y;
			sensor_b->y = p->pos.y + p->radius.x;
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
		auto heights = get_tile_heights(tile.index);

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

	int ix = floorf(pos.x);
	int iy = floorf(pos.y);

	int tile_x = floorf(pos.x / 16.0f);
	int tile_y = floorf(pos.y / 16.0f);

	Tile tile = get_tile(tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y++;
		tile = get_tile(tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (32 - (iy % 16)) - (height + 1);
	} else if (height == 16) {
		tile_y--;
		tile = get_tile(tile_x, tile_y, layer);
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
		auto heights = get_tile_widths(tile.index);

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

	int ix = floorf(pos.x);
	int iy = floorf(pos.y);

	int tile_x = floorf(pos.x / 16.0f);
	int tile_y = floorf(pos.y / 16.0f);

	Tile tile = get_tile(tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x++;
		tile = get_tile(tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = (32 - (ix % 16)) - (height + 1);
	} else if (height == 16) {
		tile_x--;
		tile = get_tile(tile_x, tile_y, layer);
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
		auto heights = get_tile_heights(tile.index);

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

	int ix = floorf(pos.x);
	int iy = floorf(pos.y);

	int tile_x = floorf(pos.x / 16.0f);
	int tile_y = floorf(pos.y / 16.0f);

	Tile tile = get_tile(tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_y--;
		tile = get_tile(tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (iy % 16) - (height);
	} else if (height == 16) {
		tile_y++;
		tile = get_tile(tile_x, tile_y, layer);
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
		auto heights = get_tile_widths(tile.index);

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

	int ix = floorf(pos.x);
	int iy = floorf(pos.y);

	int tile_x = floorf(pos.x / 16.0f);
	int tile_y = floorf(pos.y / 16.0f);

	Tile tile = get_tile(tile_x, tile_y, layer);
	int height = get_height(tile, ix, iy);

	if (height == 0) {
		tile_x--;
		tile = get_tile(tile_x, tile_y, layer);
		height = get_height(tile, ix, iy);

		result.tile = tile;
		result.found = true;
		result.tile_x = tile_x;
		result.tile_y = tile_y;

		result.dist = 16 + (ix % 16) - (height);
	} else if (height == 16) {
		tile_x++;
		tile = get_tile(tile_x, tile_y, layer);
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

	float check_speed = player_is_on_a_wall(p) ? p->speed.y : p->speed.x;
	if (res.dist > fminf(fabsf(check_speed) + 4, 14)) {
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

	float angle = get_tile_angle(res.tile.index);
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
}

static void player_state_ground(Player* p, float delta) {
	int input_h = 0;
	if (is_key_held(SDL_SCANCODE_RIGHT)) input_h++;
	if (is_key_held(SDL_SCANCODE_LEFT))  input_h--;

	// TODO: Check for special animations that prevent control (such as balancing).

	// TODO: Check for starting a spindash while crouched.

	// Adjust Ground Speed based on current Ground Angle (Slope Factor).
	apply_slope_factor(p, delta);

	// TODO: Check for starting a jump.

	// Update Ground Speed based on directional input and apply friction/deceleration.
	if (p->anim != anim_spindash
		&& /*p->anim != anim_crouch*/1
		&& /*p->anim != anim_look_up*/1)
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

	// TODO: Push Sensor collision occurs.

	// TODO: Check for starting a roll.

	// TODO: Handle camera boundaries (keep the Player inside the view and kill them if they touch the kill plane).

	// Move the Player object
	p->speed.x =  dcos(p->ground_angle) * p->ground_speed;
	p->speed.y = -dsin(p->ground_angle) * p->ground_speed;

	p->pos += p->speed * delta;

	// Grounded Ground Sensor collision occurs.
	ground_sensor_collision(p);

	// TODO: Check for slipping/falling when Ground Speed is too low on walls/ceilings.
}

static void player_state_roll(Player* p, float delta) {
	
}

static void player_state_air(Player* p, float delta) {
	
}

static void player_update(Player* p, float delta) {
	if (game.debug_mode) {
		float spd = 4;

		if (is_key_held(SDL_SCANCODE_UP))    p->pos.y -= spd * delta;
		if (is_key_held(SDL_SCANCODE_DOWN))  p->pos.y += spd * delta;
		if (is_key_held(SDL_SCANCODE_LEFT))  p->pos.x -= spd * delta;
		if (is_key_held(SDL_SCANCODE_RIGHT)) p->pos.x += spd * delta;
	} else {
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
		}
	}

#ifdef DEVELOPER
	if (is_key_pressed(SDL_SCANCODE_A)) {
		game.debug_mode ^= true;
		p->speed = {};
		p->ground_speed = 0;
	}
#endif
}

void Game::update(float delta) {
	player_update(&player, delta);

	camera_pos.x = floorf(player.pos.x - window.game_width / 2.0f);
	camera_pos.y = floorf(player.pos.y - window.game_height / 2.0f);

	{
		int mouse_x;
		int mouse_y;
		SDL_GetMouseState(&mouse_x, &mouse_y);

		vec2 mouse_pos_rel;
		mouse_pos_rel.x = ((mouse_x - renderer.game_texture_rect.x) / (float)renderer.game_texture_rect.w) * (float)window.game_width;
		mouse_pos_rel.y = ((mouse_y - renderer.game_texture_rect.y) / (float)renderer.game_texture_rect.h) * (float)window.game_height;

		mouse_world_pos = glm::floor(mouse_pos_rel + camera_pos);
	}
}

void Game::draw(float delta) {
	renderer.proj_mat = glm::ortho<float>(0, window.game_width, window.game_height, 0);
	renderer.view_mat = glm::translate(mat4{1}, vec3{-camera_pos.x, -camera_pos.y, 0});

	// draw tilemap
	{
		int xfrom = clamp((int)camera_pos.x / 16, 0, tilemap_width  - 1);
		int yfrom = clamp((int)camera_pos.y / 16, 0, tilemap_height - 1);

		int xto = clamp((int)(camera_pos.x + window.game_width  + 15) / 16, 0, tilemap_width);
		int yto = clamp((int)(camera_pos.y + window.game_height + 15) / 16, 0, tilemap_height);

		for (int y = yfrom; y < yto; y++) {
			for (int x = xfrom; x < xto; x++) {
				Tile tile = get_tile_a(x, y);

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
				tile = get_tile(x, y, player.layer);

				src.x = (tile.index % tileset_width) * 16;
				src.y = (tile.index / tileset_width) * 16;
				src.w = 16;
				src.h = 16;
				
				if (tile.top_solid || tile.lrb_solid) {
					draw_texture(heightmap, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
				}
#endif
			}
		}
	}

	Player* p = &player;

	draw_texture_centered(texture_idle, glm::floor(player.pos));

#ifdef DEVELOPER
	// draw player hitbox
	{
		Rectf rect;
		switch (player_get_mode(p)) {
			case MODE_FLOOR:
			case MODE_CEILING: {
				rect.x = floorf(p->pos.x - p->radius.x);
				rect.y = floorf(p->pos.y - p->radius.y);
				rect.w = p->radius.x * 2 + 1;
				rect.h = p->radius.y * 2 + 1;
				break;
			}
			case MODE_RIGHT_WALL:
			case MODE_LEFT_WALL: {
				rect.x = floorf(p->pos.x - p->radius.y);
				rect.y = floorf(p->pos.y - p->radius.x);
				rect.w = p->radius.y * 2 + 1;
				rect.h = p->radius.x * 2 + 1;
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

			draw_point({floorf(sensor_a.x), floorf(sensor_a.y)}, SENSOR_A_COLOR);
			draw_point({floorf(sensor_b.x), floorf(sensor_b.y)}, SENSOR_B_COLOR);
		}
	}
#endif

#ifdef DEVELOPER
	{
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
}

void Game::load_tilemap_old_format(const char* fname) {
	SDL_RWops* f = SDL_RWFromFile(fname, "rb");

	if (!f) {
		log_error("Couldn't open old tilemap \"%s\".", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	SDL_RWread(f, &tilemap_width,  sizeof(tilemap_width),  1);
	SDL_RWread(f, &tilemap_height, sizeof(tilemap_height), 1);

	if (tilemap_width <= 0 || tilemap_height <= 0) {
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

	OldTile* old_tiles_a = (OldTile*) calloc(tilemap_width * tilemap_height, sizeof(old_tiles_a[0]));
	defer { free(old_tiles_a); };

	OldTile* old_tiles_b = (OldTile*) calloc(tilemap_width * tilemap_height, sizeof(old_tiles_b[0]));
	defer { free(old_tiles_b); };

	SDL_RWread(f, old_tiles_a, sizeof(old_tiles_a[0]), tilemap_width * tilemap_height);
	SDL_RWread(f, old_tiles_b, sizeof(old_tiles_b[0]), tilemap_width * tilemap_height);

	tiles_a.count = tilemap_width * tilemap_height;
	tiles_a.data = (Tile*)calloc(tiles_a.count, sizeof(tiles_a[0]));

	tiles_b.count = tilemap_width * tilemap_height;
	tiles_b.data = (Tile*)calloc(tiles_b.count, sizeof(tiles_b[0]));

	for (int i = 0; i < tilemap_width * tilemap_height; i++) {
		tiles_a[i].index = old_tiles_a[i].index;
		tiles_a[i].hflip = old_tiles_a[i].hflip;
		tiles_a[i].vflip = old_tiles_a[i].vflip;
		tiles_a[i].top_solid = old_tiles_a[i].top_solid;
		tiles_a[i].lrb_solid = old_tiles_a[i].left_right_bottom_solid;
	}

	for (int i = 0; i < tilemap_width * tilemap_height; i++) {
		tiles_b[i].index = old_tiles_b[i].index;
		tiles_b[i].hflip = old_tiles_b[i].hflip;
		tiles_b[i].vflip = old_tiles_b[i].vflip;
		tiles_b[i].top_solid = old_tiles_b[i].top_solid;
		tiles_b[i].lrb_solid = old_tiles_b[i].left_right_bottom_solid;
	}
}

void Game::load_tileset_old_format(const char* fname) {
	SDL_RWops* f = SDL_RWFromFile(fname, "rb");

	if (!f) {
		log_error("Couldn't open old tileset \"%s\".", fname);
		return;
	}

	defer { SDL_RWclose(f); };

	int tile_count_;
	SDL_RWread(f, &tile_count_, sizeof(tile_count_), 1);

	tileset_width = 16; // @Temp
	tileset_height = 28;

	tile_heights.count = tileset_width * tileset_height * 16;
	tile_heights.data = (u8*)calloc(tile_heights.count, sizeof(tile_heights[0]));

	tile_widths.count = tileset_width * tileset_height * 16;
	tile_widths.data = (u8*)calloc(tile_widths.count, sizeof(tile_widths[0]));

	tile_angles.count = tileset_width * tileset_height;
	tile_angles.data = (float*)calloc(tile_angles.count, sizeof(tile_angles[0]));

	SDL_RWread(f, tile_heights.data, 16 * sizeof(tile_heights[0]), tile_count_);
	SDL_RWread(f, tile_widths.data,  16 * sizeof(tile_widths[0]), tile_count_);
	SDL_RWread(f, tile_angles.data,  sizeof(tile_angles[0]), tile_count_);
}
