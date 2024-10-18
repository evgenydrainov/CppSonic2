#pragma once

#include "common.h"

#include "font.h"

struct Game {
	Font font;

	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);
};

extern Game game;
