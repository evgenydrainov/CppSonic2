#pragma once

#include "common.h"

#include "font.h"
#include "texture.h"

struct Game {
	Font font;

	Texture tileset_texture;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);

	void load_level(const char* path);
};

extern Game game;
