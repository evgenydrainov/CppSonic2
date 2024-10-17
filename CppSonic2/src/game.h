#pragma once

#include "common.h"

struct Game {
	void init();
	void deinit();

	void update(float delta);
	void draw(float delta);
};

extern Game game;
