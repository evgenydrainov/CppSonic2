#pragma once

#include "common.h"

#include "font.h"
#include "texture.h"

struct Player {
	vec2 pos;
	vec2 speed;

	float ground_speed;
	float ground_angle;

	vec2 radius = {9, 19};
};

struct Game {
	Player player;

	Font font;

	Texture tileset_texture;

	Texture texture_crouch;
	Texture texture_idle;
	Texture texture_look_up;
	Texture texture_peelout;
	Texture texture_roll;
	Texture texture_run;
	Texture texture_skid;
	Texture texture_spindash;
	Texture texture_walk;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);

	void load_level(const char* path);
};

extern Game game;
