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

void Game::update(float delta) {
	mouse_world_pos = glm::floor(renderer.mouse_pos + camera_pos);

	// update player
	{
		switch (player.state) {
			case PLAYER_STATE_GROUND: {
				
				break;
			}
			case PLAYER_STATE_ROLL: {

				break;
			}
			case PLAYER_STATE_AIR: {

				break;
			}
		}
	}

	{
		float spd = 5;
		if (is_key_held(SDL_SCANCODE_UP)) player.pos.y -= spd * delta;
		if (is_key_held(SDL_SCANCODE_DOWN)) player.pos.y += spd * delta;
		if (is_key_held(SDL_SCANCODE_LEFT)) player.pos.x -= spd * delta;
		if (is_key_held(SDL_SCANCODE_RIGHT)) player.pos.x += spd * delta;
	}

	camera_pos.x = player.pos.x - window.game_width / 2.0f;
	camera_pos.y = player.pos.y - window.game_height / 2.0f;
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
				draw_texture(heightmap, src, {x * 16.0f, y * 16.0f}, {1, 1}, {}, 0, color_white, {tile.hflip, tile.vflip});
#endif
			}
		}
	}

	draw_texture_centered(texture_idle, player.pos);

#ifdef DEVELOPER
	{
		SensorResult res = sensor_check_down(mouse_world_pos.x, mouse_world_pos.y, 0);
		draw_rectangle({mouse_world_pos.x, mouse_world_pos.y, 1, (float)res.dist}, color_red);
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

SensorResult Game::sensor_check_down(float x, float y, int layer) {
	auto get_height = [&](Tile tile, int ix, int iy) {
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

	int ix = floorf(x);
	int iy = floorf(y);

	int tile_x = floorf(x / 16.0f);
	int tile_y = floorf(y / 16.0f);

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
